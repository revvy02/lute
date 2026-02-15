-- lute-lint-global-ignore(unused_variable)
export type Stream = typeof(newproxy())

local stream = {}

stream.stdin = (nil :: any) :: Stream
stream.stdout = (nil :: any) :: Stream
stream.stderr = (nil :: any) :: Stream

function stream.read(_stream: Stream): buffer?
	error("unimplemented")
end

function stream.write(_stream: Stream, _data: buffer | string)
	error("unimplemented")
end

function stream.close(_stream: Stream)
	error("unimplemented")
end

return stream
