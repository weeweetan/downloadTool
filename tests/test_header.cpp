#include "HttpDownload.h"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
    std::exit(EXIT_FAILURE); \
} } while (false)

static void deliver(const std::string &line, std::string &name)
{
    // libcurl gives a byte count, not a NUL-terminated C string.
    std::unique_ptr<char[]> buffer(new char[line.size()]);
    std::memcpy(buffer.get(), line.data(), line.size());
    CHECK(HttpDownload::parseHeader(buffer.get(), 1, line.size(), &name) == line.size());
}

static void test_valid()
{
    std::string name = "previous.txt";
    deliver("Content-Disposition: attachment; filename=report.txt\r\n", name);
    CHECK(name == "report.txt");

    deliver("cOnTeNt-DiSpOsItIoN: attachment; creation-date=now; FiLeNaMe = \"report; 2026.txt\"\r\n", name);
    CHECK(name == "report; 2026.txt");

    deliver("Content-Disposition: attachment; filename*=UTF-8''other.txt; filename=plain.txt\r\n", name);
    CHECK(name == "plain.txt");

    deliver("Content-Disposition: attachment; filename=\"quote\\\".txt\"\r\n", name);
    CHECK(name == "quote\".txt");
}

static void test_invalid()
{
    std::string name = "unchanged.txt";
    const char *ignored[] = {
        "X-Content-Disposition: attachment; filename=wrong.txt\r\n",
        "Content-Disposition-Extra: attachment; filename=wrong.txt\r\n",
        "Content-Disposition: attachment; filename*=UTF-8''ignored.txt\r\n",
        "Content-Disposition: attachment; filename=\r\n",
        "Content-Disposition: attachment; filename=../outside.txt\r\n",
        "Content-Disposition: attachment; filename=..\\outside.txt\r\n",
        "Content-Disposition: attachment; filename=\"unfinished.txt\r\n",
        "Content-Disposition: attachment\r\n; filename=hidden.txt\r\n"
    };
    for (const char *line : ignored) {
        deliver(line, name);
        CHECK(name == "unchanged.txt");
    }

    std::string with_nul = "Content-Disposition: attachment; filename=bad";
    with_nul += '\0';
    with_nul += "name.txt\r\n";
    deliver(with_nul, name);
    CHECK(name == "unchanged.txt");

    std::string with_nul_before_parameter = "Content-Disposition: attachment";
    with_nul_before_parameter += '\0';
    with_nul_before_parameter += "; filename=hidden.txt\r\n";
    deliver(with_nul_before_parameter, name);
    CHECK(name == "unchanged.txt");

    char byte = 'x';
    CHECK(HttpDownload::parseHeader(&byte, std::numeric_limits<size_t>::max(), 2, &name) == 0);
    CHECK(HttpDownload::parseHeader(nullptr, 0, 1, nullptr) == 0);
    CHECK(HttpDownload::parseHeader(nullptr, 1, 1, &name) == 0);
    CHECK(HttpDownload::parseHeader(&byte, 1, 1, nullptr) == 0);
    CHECK(name == "unchanged.txt");
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (std::strcmp(argv[1], "valid") == 0) test_valid();
    else if (std::strcmp(argv[1], "invalid") == 0) test_invalid();
    else CHECK(false);
    return EXIT_SUCCESS;
}
