#include "Download.h"
#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <unistd.h>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
    std::exit(EXIT_FAILURE); \
} } while (false)

using File = std::unique_ptr<FILE, int (*)(FILE *)>;

static File temporary_file(const std::string &contents = "")
{
    File file(std::tmpfile(), std::fclose);
    CHECK(file != nullptr);
    CHECK(std::fwrite(contents.data(), 1, contents.size(), file.get()) == contents.size());
    CHECK(std::fflush(file.get()) == 0);
    return file;
}

static std::string read_file(FILE *file)
{
    CHECK(std::fseek(file, 0, SEEK_SET) == 0);
    std::string contents;
    char buffer[256];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof buffer, file)) != 0)
        contents.append(buffer, count);
    CHECK(std::ferror(file) == 0);
    return contents;
}

static void test_bytes()
{
    auto file = temporary_file("..........");
    ThreadData data = {nullptr, file.get(), 2, 7};
    char first[] = "abcd", second[] = "ef";
    CHECK(Download::writeData(first, 2, 2, &data) == 4);
    CHECK(data.startPos == 6);
    CHECK(Download::writeData(second, 1, 2, &data) == 2);
    CHECK(data.startPos == 8);
    CHECK(read_file(file.get()) == "..abcdef..");
}

static void test_boundary()
{
    auto file = temporary_file(".......");
    ThreadData data = {nullptr, file.get(), 2, 2};
    char value = 'Z';
    CHECK(Download::writeData(&value, 1, 1, &data) == 1);
    CHECK(data.startPos == 3);
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    CHECK(data.startPos == 3);
    CHECK(read_file(file.get()) == "..Z....");
}

static void test_overrun()
{
    auto file = temporary_file("guarded");
    ThreadData data = {nullptr, file.get(), 1, 3};
    char values[] = "1234";
    CHECK(Download::writeData(values, 1, 4, &data) == 0);
    CHECK(data.startPos == 1);
    CHECK(read_file(file.get()) == "guarded");
}

static void test_invalid()
{
    auto file = temporary_file("safe");
    ThreadData data = {nullptr, file.get(), 0, 3};
    char value = 'X';
    CHECK(Download::writeData(&value, 2, std::numeric_limits<size_t>::max(), &data) == 0);
    CHECK(Download::writeData(&value, 1, 1, nullptr) == 0);
    CHECK(Download::writeData(nullptr, 1, 1, &data) == 0);
    CHECK(data.startPos == 0);
    data.startPos = -1;
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    data.startPos = 4;
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    data.startPos = data.endPos = LLONG_MAX;
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    CHECK(data.startPos == LLONG_MAX);
    data = {nullptr, nullptr, 0, 3};
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    CHECK(read_file(file.get()) == "safe");
}

static void test_empty()
{
    CHECK(Download::writeData(nullptr, 0, 1, nullptr) == 0);
    CHECK(Download::writeData(nullptr, 1, 0, nullptr) == 0);
    auto file = temporary_file("same");
    ThreadData data = {nullptr, file.get(), 0, 3};
    CHECK(Download::writeData(nullptr, 0, 0, &data) == 0);
    CHECK(data.startPos == 0);
    CHECK(read_file(file.get()) == "same");
}

static void test_seek_error()
{
    int descriptors[2];
    CHECK(pipe(descriptors) == 0);
    File file(fdopen(descriptors[1], "wb"), std::fclose);
    CHECK(file != nullptr);
    ThreadData data = {nullptr, file.get(), 0, 3};
    char value = 'X';
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    CHECK(data.startPos == 0);
    file.reset();
    CHECK(read(descriptors[0], &value, 1) == 0);
    CHECK(close(descriptors[0]) == 0);
}

static void test_write_error()
{
    char path[] = "/tmp/download-write-XXXXXX";
    const int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    File file(std::fopen(path, "rb"), std::fclose);
    const int removed = unlink(path), closed = close(descriptor);
    CHECK(file != nullptr && removed == 0 && closed == 0);
    CHECK(std::fseek(file.get(), 0, SEEK_SET) == 0);
    ThreadData data = {nullptr, file.get(), 0, 3};
    char value = 'X';
    CHECK(Download::writeData(&value, 1, 1, &data) == 0);
    CHECK(std::ferror(file.get()) != 0);
    CHECK(data.startPos == 0);
}

static void test_concurrent()
{
    auto file = temporary_file();
    const long long length = 2048;
    ThreadData left = {nullptr, file.get(), 0, length - 1};
    ThreadData right = {nullptr, file.get(), length, 2 * length - 1};
    auto write_range = [](ThreadData *data, char value) {
        char block[8];
        std::memset(block, value, sizeof block);
        for (int i = 0; i < 256; ++i)
            CHECK(Download::writeData(block, 2, 4, data) == sizeof block);
    };
    std::thread first(write_range, &left, 'a');
    std::thread second(write_range, &right, 'b');
    first.join();
    second.join();
    CHECK(left.startPos == length && right.startPos == 2 * length);
    CHECK(read_file(file.get()) == std::string(length, 'a') + std::string(length, 'b'));
}

int main(int argc, char **argv)
{
    struct Case { const char *name; void (*run)(); };
    const Case cases[] = {
        {"bytes", test_bytes}, {"boundary", test_boundary}, {"overrun", test_overrun},
        {"invalid", test_invalid}, {"empty", test_empty}, {"seek-error", test_seek_error},
        {"write-error", test_write_error}, {"concurrent", test_concurrent}
    };
    if (argc == 2) {
        for (const auto &test : cases) {
            if (std::strcmp(argv[1], test.name) == 0) {
                test.run();
                return EXIT_SUCCESS;
            }
        }
    }
    std::fputs("Expected a write callback test name.\n", stderr);
    return EXIT_FAILURE;
}
