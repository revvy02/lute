-- lute-lint-global-ignore(unused_variable)
local stream = require("@lute/stream")

export type StdioKind = "default" | "piped" | "inherit" | "none" | "tee"

export type ProcessRunOptions = {
	cwd: string?,
	stdio: StdioKind?,
	stdin: StdioKind?,
	stdout: StdioKind?,
	stderr: StdioKind?,

	env: { [string]: string }?,
}

export type ProcessSystemOptions = {
	system: string?,
	cwd: string?,
	stdio: StdioKind?,
	stdin: StdioKind?,
	stdout: StdioKind?,
	stderr: StdioKind?,

	env: { [string]: string }?,
}

export type Process = {
	ok: boolean?,
	exitcode: number?,
	signal: string?,
	out: string,
	err: string,
	stdin: stream.Stream?,
	stdout: stream.Stream?,
	stderr: stream.Stream?,
}

local process = {}

process.env = {} :: { [string]: string }

function process.homedir(): string
	error("not implemented")
end

function process.cwd(): string
	error("not implemented")
end

process.run = (nil :: any) :: (({ string }, ProcessRunOptions?) -> Process) & ((Process) -> Process)

function process.system(command: string, options: ProcessSystemOptions?): Process
	error("not implemented")
end

function process.exit(exitcode: number): never
	error("not implemented")
end

function process.execpath(): string
	error("not implemented")
end

function process.create(args: { string }, options: ProcessRunOptions?): Process
	error("not implemented")
end

function process.kill(handle: Process)
	error("not implemented")
end

function process.attach(handle: Process)
	error("not implemented")
end

function process.detach(handle: Process)
	error("not implemented")
end

return process
