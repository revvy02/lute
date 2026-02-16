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
        local path = ")" +
        testFile + R"("

        local h = fs.open(path, "w+")
        fs.write(h, "Hello, World!")
        fs.close(h)

        local hr = fs.open(path, "r")
        assert(hr ~= nil, "File handle for reading should not be nil")

        local buf = fs.read(hr)
        local content = if buf then buffer.tostring(buf) else ""
        local correctness = content == "Hello, World!"
        fs.close(hr)

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
