#include <cstdio>
#include <sstream>
#include <string>

int main()
{
    // Growth past the short-string buffer, then integer and floating point
    // formatting: both report use-of-uninitialized-value when the standard
    // library itself is not instrumented.
    std::string text("abc");
    text += "0123456789012345678901234567890123";

    std::ostringstream formatted;
    formatted << 12345 << ' ' << 3.5 << ' ' << text.size();
    std::string result = formatted.str();

    if (text.size() != 37) {
        std::fprintf(stderr, "unexpected string size %zu\n", text.size());
        return 1;
    }
    if (result != "12345 3.5 37") {
        std::fprintf(stderr, "unexpected formatting \"%s\"\n", result.c_str());
        return 2;
    }

    std::printf("msan-cxx-standard-library-ok\n");
    return 0;
}
