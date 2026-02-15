local stream = require("@lute/stream")
local process = require("@lute/process")
local task = require("@lute/task")
local test = require("@std/test")

test.suite("Stream", function(suite)
	-- Core stream operations (using process pipes)

	suite:case("write_and_read_through_cat", function(assert)
		local p = process.create({ "cat" }, { stdio = "piped" })
		task.spawn(process.run, p)

		stream.write(p.stdin, "hello world")
		stream.close(p.stdin)

		local chunks = {}
		while true do
			local chunk = stream.read(p.stdout)
			if not chunk then
				break
			end
			table.insert(chunks, buffer.tostring(chunk))
		end
		assert.eq(table.concat(chunks), "hello world")
	end)

	suite:case("read_returns_nil_on_eof", function(assert)
		local p = process.create({ "echo", "done" }, { stdio = "piped" })
		task.spawn(process.run, p)

		local gotEof = false
		while true do
			local chunk = stream.read(p.stdout)
			if not chunk then
				gotEof = true
				break
			end
		end
		assert.eq(gotEof, true)
	end)

	suite:case("write_accepts_string_and_buffer", function(assert)
		local p = process.create({ "cat" }, { stdio = "piped" })
		task.spawn(process.run, p)

		stream.write(p.stdin, "string ")
		stream.write(p.stdin, buffer.fromstring("buffer"))
		stream.close(p.stdin)

		local result = {}
		while true do
			local chunk = stream.read(p.stdout)
			if not chunk then
				break
			end
			table.insert(result, buffer.tostring(chunk))
		end
		assert.eq(table.concat(result), "string buffer")
	end)

	suite:case("close_is_idempotent", function()
		local p = process.create({ "cat" }, { stdio = "piped" })
		task.spawn(process.run, p)

		stream.close(p.stdin)
		stream.close(p.stdin) -- should not error
	end)

	suite:case("read_errors_on_write_only_stream", function(assert)
		local p = process.create({ "cat" }, { stdio = "piped" })
		task.spawn(process.run, p)

		assert.errors(function()
			stream.read(p.stdin) -- stdin is write-only
		end)
		stream.close(p.stdin)
	end)

	suite:case("write_errors_on_read_only_stream", function(assert)
		local p = process.create({ "echo", "hi" }, { stdio = "piped" })
		task.spawn(process.run, p)

		assert.errors(function()
			stream.write(p.stdout, "nope") -- stdout is read-only
		end)
	end)

	-- Parent stdio

	suite:case("write_to_parent_stdout", function()
		stream.write(stream.stdout, "stream test output\n")
	end)

	suite:case("write_to_parent_stderr", function()
		stream.write(stream.stderr, "stream test error\n")
	end)

	suite:case("stdin_stdout_stderr_exist", function(assert)
		assert.neq(stream.stdin, nil)
		assert.neq(stream.stdout, nil)
		assert.neq(stream.stderr, nil)
	end)

	-- ProcessHandle field access

	suite:case("process_handle_has_stdio_fields_when_piped", function(assert)
		local p = process.create({ "echo", "hi" }, { stdio = "piped" })
		assert.neq(p.stdin, nil)
		assert.neq(p.stdout, nil)
		assert.neq(p.stderr, nil)
	end)

	suite:case("process_handle_fields_nil_when_not_piped", function(assert)
		local p = process.create({ "echo", "hi" })
		assert.eq(p.stdin, nil)
		assert.eq(p.stdout, nil)
		assert.eq(p.stderr, nil)
	end)
end)
