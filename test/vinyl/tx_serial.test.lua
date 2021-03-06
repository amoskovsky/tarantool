-- The test runs loop of given number of rounds.
-- Every round does the following:
-- The test starts several concurrent transactions in vinyl.
-- The transactions make some read/write operations over several keys in
--  a random order and commit at a random moment.
-- After that all transactions are sorted in order of commit
-- With the sublist of read-write transactions committed w/o conflict:
-- Test tries to make these transactions in memtex, one tx after another,
--  without interleaving and compares select results with vinyl to make sure
--  if the transaction could be serialized in order of commit or not
-- With the sublist of read-write transactions committed with conflict:
-- Test does nothing
-- With the sublist of read only transactions:
-- Test tries to insert these transactions between other transactions and checks
--  that it possible to get same results.

test_run = require('test_run').new()
txn_proxy = require('txn_proxy')
--settings
num_tx = 10 --number of concurrent transactions
num_key = 5 --number of keys that transactions use
num_tests = 60 --number of test rounds to run

txs = {}
order_of_commit = {}
num_committed = 0
stmts = {}
errors = {}
initial_data = {}
initial_repro = ""
ops = {'begin', 'commit', 'select', 'replace', 'upsert', 'delete'}

test_run:cmd("setopt delimiter ';'")

s1 = box.schema.create_space('test1', { engine = 'vinyl' })
i1 = s1:create_index('test', { type = 'TREE', parts = {1, 'uint'} })
s2 = box.schema.create_space('test2', { engine = 'memtx' })
i2 = s2:create_index('test', { type = 'TREE', parts = {1, 'uint'} })

for i=1,num_tx do
    txs[i] = {con = txn_proxy.new()}
end;

function my_equal(a, b)
    local typea = box.tuple.is(a) and 'table' or type(a)
    local typeb = box.tuple.is(b) and 'table' or type(b)
    if typea ~= typeb then
        return false
    elseif typea ~= 'table' then
        return a == b
    end
    for k,v in pairs(a) do if not my_equal(b[k], v) then return false end end
    for k,v in pairs(b) do if not my_equal(a[k], v) then return false end end
    return true
end;

unique_value = 0
function get_unique_value()
    unique_value = unique_value + 1
    return unique_value
end;

function prepare()
    order_of_commit = {}
    num_committed = 0
    stmts = {}
    for i=1,num_tx do
        txs[i].started = false
        txs[i].ended = false
        if math.random(3) == 1 then
            txs[i].read_only = true
        else
            txs[i].read_only = false
        end
        txs[i].read_only_checked = false
        txs[i].conflicted = false
        txs[i].possible = nil
        txs[i].num_writes = 0
    end
    s1:truncate()
    s2:truncate()
    for i=1,num_key do
        local r = math.random(5)
        local v = get_unique_value()
        if (r >= 2) then
            s1:replace{i, v}
            s2:replace{i, v }
        end
        if (r == 2) then
            s1:delete{i}
            s2:delete{i}
        end
    end
    initial_data = s1:select{}
    initial_repro = ""
    initial_repro = initial_repro .. "s = box.schema.space.create('test', {engine = 'vinyl', if_not_exists = true})\n"
    initial_repro = initial_repro .. "i1 = s:create_index('test', {parts = {1, 'uint'}, if_not_exists = true})\n"
    initial_repro = initial_repro .. "txn_proxy = require('txn_proxy')\n"
    for _,tuple in pairs(initial_data) do
        initial_repro = initial_repro .. "s:replace{" .. tuple[1] .. ", " .. tuple[2] .. "} "
    end
end;

function apply(t, k, op)
    local tx = txs[t]
    local v = nil
    local k = k
    local repro = nil
    if op == 'begin' then
        if tx.started then
            table.insert(errors, "assert #1")
        end
        tx.started = true
        tx.con:begin()
        k = nil
        repro = "c" .. t .. " = txn_proxy.new() c" .. t .. ":begin()"
        repro = "p(\"c" .. t .. ":begin()\") " .. repro
    elseif op == 'commit' then
        if tx.ended or not tx.started then
            table.insert(errors, "assert #2")
        end
        tx.ended = true
        table.insert(order_of_commit, t)
        num_committed = num_committed + 1
        local res = tx.con:commit()
        if res ~= "" and res[1]['error'] then
            tx.conflicted = true
        else
            tx.select_all = s1:select{}
            if tx.num_writes == 0 then
                tx.read_only = true
            end
        end
        k = nil
        repro = "c" .. t .. ":commit()"
        repro = "p(\"" .. repro .. "\", " .. repro .. ", s:select{})"
    elseif op == 'select' then
        v = tx.con('s1:select{'..k..'}')
        repro = "c" .. t .. "('s:select{" .. k .. "}')"
        repro = "p(\"" .. repro .. "\", " .. repro .. ")"
    elseif op == 'replace' then
        v = get_unique_value()
        tx.con('s1:replace{'..k..','..v..'}')
        tx.num_writes = tx.num_writes + 1
        repro = "c" .. t .. "('s:replace{" .. k .. ", " .. v .. "}')"
        repro = "p(\"" .. repro .. "\", " .. repro .. ")"
    elseif op == 'upsert' then
        v = math.random(100)
        tx.con('s1:upsert({'..k..','..v..'}, {{"+", 2,'..v..'}})')
        tx.num_writes = tx.num_writes + 1
        repro = "c" .. t .. "('s:upsert({" .. k .. ", " .. v .. "}, {{\\'+\\', 2, " .. v .. "}})')"
        repro = "p(\"" .. repro .. "\", " .. repro .. ")"
    elseif op == 'delete' then
        tx.con('s1:delete{'..k..'}')
        tx.num_writes = tx.num_writes + 1
        repro = "c" .. t .. "('s:delete{" .. k .. "}')"
        repro = "p(\"" .. repro .. "\", " .. repro .. ")"
    end
    table.insert(stmts, {t=t, k=k, op=op, v=v, repro=repro})
end;

function generate_random_operation()
    local t = math.random(num_tx)
    local k = math.random(num_key)
    local tx = txs[t]
    if tx.ended then
        return
    end

    local op_no = 0
    if (tx.read_only) then
        op_no = math.random(3)
    else
        op_no = math.random(6)
    end
    local op = ops[op_no]
    if op ~= 'commit' or tx.started then
        if not tx.started then
            apply(t, k, 'begin')
        end
        if op ~= 'begin' then
            apply(t, k, op)
        end
    end
end;

function is_rdonly_tx_possible(t)
    for _,s in pairs(stmts) do
        if s.t == t and s.op == 'select' then
            local cmp_with = {s2:select{s.k}}
            if not my_equal(s.v, cmp_with) then
                return false
            end
        end
    end
    return true
end;

function try_to_apply_tx(t)
    for _,s in pairs(stmts) do
        if s.t == t then
            if s.op == 'select' then
                local cmp_with = {s2:select{s.k}}
                if not my_equal(s.v, cmp_with) then
                    return false
                end
            elseif s.op == 'replace' then
                s2:replace{s.k, s.v}
            elseif s.op == 'upsert' then
                s2:upsert({s.k, s.v}, {{'+', 2, s.v}})
            elseif s.op == 'delete' then
                s2:delete{s.k}
            end
        end
    end
    return true
end;

function check_rdonly_possibility()
    for i=1,num_tx do
        if txs[i].read_only and not txs[i].possible then
            if is_rdonly_tx_possible(i) then
                txs[i].possible = true
            end
        end
    end
end;

function check()
    local had_errors = (errors[1] ~= nil)
    for i=1,num_tx do
        if txs[i].read_only then
            if txs[i].conflicted then
                table.insert(errors, "read-only conflicted " .. i)
            end
            txs[i].possible = false
        end
    end
    check_rdonly_possibility()
    for _,t in ipairs(order_of_commit) do
        if not txs[t].read_only then
            if not txs[t].conflicted then
                if not try_to_apply_tx(t) then
                    table.insert(errors, "not serializable " .. t)
                end
                if not my_equal(txs[t].select_all, s2:select{}) then
                    table.insert(errors, "results are different " .. t)
                end
                check_rdonly_possibility()
            end
        end
    end
    for i=1,num_tx do
        if txs[i].read_only and not txs[i].possible then
            table.insert(errors, "not valid read view " .. i)
        end
    end
    if errors[1] and not had_errors then
        print("p(\"" .. errors[1] .. "\")")
        print(initial_repro)
        print("p(\"" .. initial_repro .. "\")")
        print('----------------------')
        for _,stmt in ipairs(stmts) do
            print(stmt.repro)
        end
        io.flush()
    end
end;

for i = 1, num_tests do
    prepare()
    while num_committed ~= num_tx do
        generate_random_operation()
    end
    check()
end;

test_run:cmd("setopt delimiter ''");

errors

s1:drop()
s2:drop()
