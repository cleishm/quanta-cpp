// Regression test: the std::formatter specialization must be usable when
// compiled with exceptions disabled (-fno-exceptions), as on embedded targets
// such as ESP-IDF with CONFIG_COMPILER_CXX_EXCEPTIONS unset.
//
// This is a separate target from data_tests because Catch2 requires
// exceptions. It deliberately avoids any test framework so the whole
// translation unit builds without them.

#include <cstdio>
#include <quanta/data>
#include <string>

#ifdef __cpp_exceptions
#error "this test must be compiled with exceptions disabled"
#endif

#if !CONFIG_QUANTA_STD_FORMAT

// Without std::format the formatter is not compiled and there is nothing to
// test; older standard libraries (e.g. g++-12's) lack <format> entirely.
int main() {
    std::printf("std::format unavailable; nothing to test\n");
    return 0;
}

#else

using namespace quanta::data;

namespace {

int failures = 0;

void check(const std::string& actual, const std::string& expected, const char* what) {
    if (actual != expected) {
        std::printf("FAIL: %s: expected \"%s\", got \"%s\"\n", what, expected.c_str(), actual.c_str());
        ++failures;
    }
}

} // namespace

int main() {
    // Formatting with no spec: instantiating parse() must not pull in a throw.
    check(std::format("{}", bytes(1500)), "1500B", "{} bytes");
    check(std::format("{}", bits(12)), "12bit", "{} bits");
    check(std::format("{}", kilobytes(64)), "64kB", "{} kilobytes");
    check(std::format("{}", mebibytes(16)), "16MiB", "{} mebibytes");
    check(std::format("{}", megabits_per_second(100)), "100Mbit/s", "{} megabits_per_second");
    check(std::format("{}", kibibytes_per_second(64)), "64KiB/s", "{} kibibytes_per_second");

    // Formatting with an explicit spec.
    check(std::format("{:.1f}", kilobytes(1500)), "1500.0kB", "{:.1f} kilobytes");
    check(std::format("{:.1f}", gigabits(2)), "2.0Gbit", "{:.1f} gigabits");
    check(std::format("{:>6.1f}", bytes_per_second(5)), "   5.0B/s", "{:>6.1f} bytes_per_second");

    // Base-unit and scaled markers, and the matching to_string tags.
    check(std::format("{:b}", bits(1)), "0.125B", "{:b} bits");
    check(std::format("{:b.1f}", kilobits_per_second(3)), "375.0B/s", "{:b.1f} kilobits_per_second");
    check(std::format("{:s}", kilobytes(1500)), "1.5MB", "{:s} kilobytes");
    check(std::format("{:s}", kibibytes(1536)), "1.5MiB", "{:s} kibibytes");
    check(std::format("{:s}", bytes(2048)), "2.048kB", "{:s} bytes");
    check(std::format("{:s}", kilobits_per_second(1500)), "1.5Mbit/s", "{:s} kilobits_per_second");
    check(std::format("{:s>6.1f}", kibibytes(-1536)), "  -1.5MiB", "{:s>6.1f} kibibytes");
    check(to_string(kibibytes(2), base_unit), "2048B", "to_string(kibibytes, base_unit)");
    check(to_string(gigabits(3), scaled), "3Gbit", "to_string(gigabits, scaled)");
    check(std::format("{}", size<double>(1e6)), "1000000B", "{} size<double>");
    check(to_string(rate<double>(2.5)), "2.5B/s", "to_string(rate<double>)");

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}

#endif // CONFIG_QUANTA_STD_FORMAT
