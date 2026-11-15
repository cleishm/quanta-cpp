// Regression test: the std::formatter specialization must be usable when
// compiled with exceptions disabled (-fno-exceptions), as on embedded targets
// such as ESP-IDF with CONFIG_COMPILER_CXX_EXCEPTIONS unset.
//
// This is a separate target from proportion_tests because Catch2 requires
// exceptions. It deliberately avoids any test framework so the whole
// translation unit builds without them.

#include <cstdio>
#include <quanta/proportion>
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

using namespace quanta::proportion;

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
    check(std::format("{}", percent(25)), "25%", "{} percent");
    check(std::format("{}", permille(5)), "5‰", "{} permille");
    check(std::format("{}", ppm(50)), "50ppm", "{} ppm");
    check(std::format("{}", ppb(3)), "3ppb", "{} ppb");
    check(std::format("{}", proportion<double>(0.25)), "0.25", "{} plain fraction");

    // Formatting with an explicit spec.
    check(std::format("{:.1f}", percent(25)), "25.0%", "{:.1f} percent");
    check(std::format("{:.2f}", ppm(50)), "50.00ppm", "{:.2f} ppm");

    // Unit markers.
    check(std::format("{:b}", percent(25)), "0.25", "{:b} percent");
    check(std::format("{:b.3f}", percent(25)), "0.250", "{:b.3f} percent");
    check(std::format("{:s}", permille(4)), "0.4%", "{:s} permille");
    check(std::format("{:s}", ppm(200)), "200ppm", "{:s} ppm");
    check(std::format("{:s.1f}", ppb(3)), "3.0ppb", "{:s.1f} ppb");

    // A precision with no unit symbol is still usable with an explicit spec or
    // a marker.
    using basis_points = proportion<int64_t, std::ratio<1, 10000>>;
    check(std::format("{:.4f}", basis_points(2500)), "0.2500", "{:.4f} basis points");
    check(std::format("{:s}", basis_points(2500)), "25%", "{:s} basis points");

    // to_string, which has no formatter.
    check(to_string(percent(25)), "25%", "to_string percent");
    check(to_string(ppm(200), scaled), "200ppm", "to_string scaled ppm");

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}

#endif // CONFIG_QUANTA_STD_FORMAT
