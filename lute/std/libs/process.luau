--!strict
-- @std/process
-- stdlib for `@lute/process`
-- Provides process-related utility functions that handles file paths as path types instead of strings

local process = require("@lute/process")
local pathlib = require("@std/path")

local processlib = {}

export type stdiokind = process.StdioKind
export type processrunoptions = process.ProcessRunOptions
export type processsystemoptions = process.ProcessSystemOptions
export type process = process.Process
export type path = pathlib.path
export type pathlike = pathlib.pathlike

function processlib.homedir(): path
	return pathlib.parse(process.homedir())
end

function processlib.cwd(): path
	return pathlib.parse(process.cwd())
end

processlib.run = process.run :: (({ string }, processrunoptions?) -> process.Process) & ((process.Process) -> process.Process)

function processlib.system(command: string, options: processsystemoptions?): process.Process
	return process.system(command, options)
end

function processlib.exit(exitcode: number): never
	return process.exit(exitcode)
end

function processlib.execpath(): path
	return pathlib.parse(process.execpath())
end

-- re-exports
processlib.env = process.env

return table.freeze(processlib)
