#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(3)

local result_prefix = 'object names beginning with "_" are reserved for internal use: '

test:execsql("CREATE TABLE t(a INT PRIMARY KEY);")

test:do_catchsql_test(
	"prefix-1.1",
	[[
		CREATE TABLE _t1(a INT PRIMARY KEY);
	]], {
		1, result_prefix .. '"_t1"'
	})

test:do_catchsql_test(
	"prefix-1.2",
	[[
		CREATE INDEX _t1ix1 ON t(a);
	]], {
		1, result_prefix .. '"_t1ix1"'
	})

test:do_catchsql_test(
	"prefix-1.3",
	[[
		CREATE TRIGGER _tt BEFORE UPDATE ON t BEGIN SELECT 1; END;
	]], {
		1, result_prefix .. '"_tt"'
	})

test:finish_test()
