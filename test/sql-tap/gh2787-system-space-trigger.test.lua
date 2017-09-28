#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(20)

local sys_spaces = {
	'_schema',
	'_space',
	'_index',
	'_trigger',
	'_truncate',
}

local query = ' BEGIN SELECT 1; END;'
local trigger_type = {
	'BEFORE UPDATE ON ',
	'AFTER UPDATE ON ',
	'BEFORE INSERT ON ',
	'AFTER INSERT ON '
}

for i, type in pairs(trigger_type) do
	for _, v in pairs(sys_spaces) do
		create_stmt = 'CREATE TRIGGER tt1 '
		create_stmt = create_stmt .. type .. v .. query
		test:do_catchsql_test(
			"trigger-1."..i,
			create_stmt, {
				1, 'cannot create trigger on system table "' .. v .. '"'
			})
	end
end

test:finish_test()
