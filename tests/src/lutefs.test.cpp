#include "lute/fileutils.h"

#include "Luau/FileUtils.h"

#include <string>

#include "cliruntimefixture.h"
#include "doctest.h"
#include "luteprojectroot.h"

TEST_CASE_FIXTURE(CliRuntimeFixture, "fs_open_write_read_close")
{
    std::string testFile = "./tmp/lute_test_file.txt";
    std::string luteProjectRoot = getLuteProjectRootAbsolute();
    std::string localTmpDir = joinPaths(luteProjectRoot, "./tmp");

    // Clean up any existing file
    Lute::removeFile(testFile);
    Lute::createDirectories(localTmpDir);
    runCode(
        R"(
        local fs = require("@lute/fs")
        local stream = require("@lute/stream")
        local path = ")" +
        testFile + R"("

        -- Open file for writing
        local h = fs.open(path, "w+")

        -- Write some data
        stream.write(h, "Hello, World!")

        -- Close the file
        stream.close(h)

        -- Open file for reading
        local hr = fs.open(path, "r")
        assert(hr ~= nil, "File handle for reading should not be nil")

        -- Read the data (chunked — loop until nil)
        local chunks = {}
        while true do
            local chunk = stream.read(hr)
            if not chunk then
                break
            end
            table.insert(chunks, buffer.tostring(chunk))
        end
        local content = table.concat(chunks)
        local correctness = content == "Hello, World!"

        -- Close the read handle
        stream.close(hr)

        report(content)
        report(correctness)
    )"
    );

    CHECK(getReporter().getOutputs()[0] == "Hello, World!");
    CHECK(getReporter().getOutputs()[1] == "true");

    // Clean up
    Lute::removeFile(testFile);
    Lute::removeDirectory(localTmpDir);
}
