// Every domain in one translation unit: the headers must coexist, and the
// shared names (treat_as_inexact, base_unit, scaled) must not be ambiguous
// when several domains are brought in with using-directives. Those
// using-directives are confined to the all_domains namespace so that the
// literals-only test below sees nothing but `using namespace quanta::literals;`.

#include <catch2/catch_test_macros.hpp>
#include <quanta/decibel>
#include <quanta/electro>
#include <quanta/frequency>
#include <quanta/spatial>
#include <quanta/thermo>
#include <string>

namespace all_domains {

using namespace quanta::thermo;
using namespace quanta::frequency;
using namespace quanta::electro;
using namespace quanta::spatial;
using namespace quanta::literals;

static_assert(treat_as_inexact_v<double>);
static_assert(!treat_as_inexact_v<int>);

TEST_CASE("literals from every domain", "[combined]") {
    CHECK(25_c == celsius(25));
    CHECK(2400_MHz == megahertz(2400));
    CHECK(3300_mV == millivolts(3300));
    CHECK(-73_dBm == dbm(-73));
    CHECK(250_mm == millimeters(250));
}

TEST_CASE("shared formatting tags", "[combined]") {
    CHECK(to_string(millicelsius(22534), base_unit) == "22.534°C");
    CHECK(to_string(megahertz(2400), scaled) == "2.4GHz");
    CHECK(to_string(millivolts(3300), base_unit) == "3.3V");
    CHECK(to_string(millimeters(1500), scaled) == "1.5m");
}

TEST_CASE("frequency to spatial wavelength", "[combined]") {
    CHECK(megahertz(300).wavelength<quanta::spatial::millimeters>() == quanta::spatial::millimeters(999));
}

#if CONFIG_QUANTA_STD_FORMAT
TEST_CASE("std::format from every domain", "[combined]") {
    CHECK(std::format("{}", celsius(20)) == "20°C");
    CHECK(std::format("{}", megahertz(868)) == "868MHz");
    CHECK(std::format("{}", millivolts(3300)) == "3300mV");
    CHECK(std::format("{}", dbm(-73)) == "-73dBm");
    CHECK(std::format("{}", millimeters(250)) == "250mm");
}
#endif

} // namespace all_domains

// Only the umbrella literals namespace is in scope here: no domain namespace
// is imported, so every literal must come through quanta::literals.
TEST_CASE("quanta::literals alone provides every domain's literals", "[combined]") {
    using namespace quanta::literals;

    CHECK(25_c == quanta::thermo::celsius(25));
    CHECK(2400_MHz == quanta::frequency::megahertz(2400));
    CHECK(3300_mV == quanta::electro::millivolts(3300));
    CHECK(-73_dBm == quanta::electro::dbm(-73));
    CHECK(250_mm == quanta::spatial::millimeters(250));
}

// A quantity whose rep is another domain's quantity converts to nothing that
// the rep itself does not convert to: neither converting constructor applies.
static_assert(!std::is_constructible_v<
              quanta::electro::volts,
              quanta::electro::quantity<quanta::electro::volt_unit, quanta::spatial::meters>>);
static_assert(!std::is_constructible_v<
              quanta::electro::quantity<quanta::electro::volt_unit, quanta::spatial::meters>,
              quanta::electro::volts>);
static_assert(
    !std::is_constructible_v<quanta::frequency::hertz, quanta::frequency::frequency<quanta::thermo::delta_celsius>>
);

#if CONFIG_QUANTA_STD_FORMAT
// The formatters expose only parse and format.
template<typename T>
concept exposes_internals =
    requires(const std::formatter<T>& f, const T& t, std::format_context& ctx) { f.format_quantity(t, ctx); };
static_assert(!exposes_internals<quanta::thermo::delta_celsius>);
static_assert(!exposes_internals<quanta::thermo::celsius>);
static_assert(!exposes_internals<quanta::frequency::hertz>);
static_assert(!exposes_internals<quanta::electro::volts>);
static_assert(!exposes_internals<quanta::spatial::meters>);
#endif

// A rep constructible only explicitly from double (a fixed-point type) is
// reached by explicit conversion from a double-rep quantity, at the same and
// at a lossy precision, in every domain.
namespace explicit_rep {
struct fixed {
    long long hundredths = 0;
    constexpr fixed() = default;
    constexpr explicit fixed(double d)
        : hundredths(static_cast<long long>(d * 100)) {}
    constexpr operator double() const { return static_cast<double>(hundredths) / 100.0; }
};

template<typename To, typename Same, typename Lossy>
constexpr bool explicit_only = std::is_constructible_v<To, Same> && !std::is_convertible_v<Same, To> &&
    std::is_constructible_v<To, Lossy> && !std::is_convertible_v<Lossy, To>;

static_assert(explicit_only<
              quanta::thermo::delta<fixed>,
              quanta::thermo::delta<double>,
              quanta::thermo::delta<double, std::milli>>);
static_assert(explicit_only<
              quanta::frequency::frequency<fixed>,
              quanta::frequency::frequency<double>,
              quanta::frequency::frequency<double, std::milli>>);
static_assert(explicit_only<
              quanta::electro::voltage<fixed>,
              quanta::electro::voltage<double>,
              quanta::electro::voltage<double, std::milli>>);
static_assert(explicit_only<
              quanta::spatial::length<fixed>,
              quanta::spatial::length<double>,
              quanta::spatial::length<double, std::milli>>);

TEST_CASE("explicit conversion to an explicit-only rep", "[combined]") {
    CHECK(double(quanta::thermo::delta<fixed>(quanta::thermo::delta<double>(-0.5)).count()) == -0.5);
    CHECK(double(quanta::frequency::frequency<fixed>(quanta::frequency::frequency<double>(1.234)).count()) == 1.23);
    CHECK(
        double(quanta::frequency::frequency<fixed>(quanta::frequency::frequency<double, std::milli>(1500.0)).count()) ==
        1.5
    );
    CHECK(double(quanta::electro::voltage<fixed>(quanta::electro::voltage<double, std::milli>(2500.0)).count()) == 2.5);
    CHECK(double(quanta::spatial::length<fixed>(quanta::spatial::length<double, std::milli>(750.0)).count()) == 0.75);
}
} // namespace explicit_rep

// A rep that is explicitly constructible from the source rep but that the
// cast cannot reach (no common type with it) is not constructible either.
static_assert(!std::is_constructible_v<
              quanta::electro::quantity<quanta::electro::volt_unit, quanta::spatial::meters, std::kilo>,
              quanta::electro::volts>);

// The implicit converting constructor likewise applies only where the cast
// can perform the conversion: a rep implicitly constructible from int64_t
// but without the multiplication a finer precision needs is not convertible,
// while one with it is.
namespace implicit_rep {
struct wrapped {
    long long v = 0;
    constexpr wrapped() = default;
    constexpr wrapped(long long x)
        : v(x) {}
};

struct scaled_int {
    long long v = 0;
    constexpr scaled_int() = default;
    constexpr scaled_int(long long x)
        : v(x) {}
    friend constexpr scaled_int operator*(scaled_int a, scaled_int b) { return a.v * b.v; }
};

static_assert(!std::is_convertible_v<quanta::thermo::delta_celsius, quanta::thermo::delta<wrapped, std::milli>>);
static_assert(!std::is_constructible_v<quanta::thermo::delta<wrapped, std::milli>, quanta::thermo::delta_celsius>);
static_assert(std::is_convertible_v<quanta::thermo::delta_celsius, quanta::thermo::delta<scaled_int, std::milli>>);
static_assert(quanta::thermo::delta<scaled_int, std::milli>(quanta::thermo::delta_celsius(2)).count().v == 2000);
static_assert(!std::is_convertible_v<quanta::frequency::hertz, quanta::frequency::frequency<wrapped, std::milli>>);
static_assert(!std::is_convertible_v<quanta::electro::volts, quanta::electro::voltage<wrapped, std::milli>>);
static_assert(!std::is_convertible_v<quanta::spatial::meters, quanta::spatial::length<wrapped, std::milli>>);
} // namespace implicit_rep

// A lossless conversion to a rep that the source rep converts to only
// explicitly is explicit, not missing.
namespace explicit_lossless {
struct fx {
    long long v = 0;
    constexpr fx() = default;
    constexpr explicit fx(long long x)
        : v(x) {}
    constexpr operator long long() const { return v; }
};

static_assert(std::is_constructible_v<quanta::frequency::frequency<fx, std::milli>, quanta::frequency::hertz>);
static_assert(!std::is_convertible_v<quanta::frequency::hertz, quanta::frequency::frequency<fx, std::milli>>);
static_assert(quanta::frequency::frequency<fx, std::milli>(quanta::frequency::hertz(2)).count().v == 2000);
static_assert(std::is_constructible_v<quanta::thermo::delta<fx>, quanta::thermo::delta_celsius>);
static_assert(!std::is_convertible_v<quanta::thermo::delta_celsius, quanta::thermo::delta<fx>>);
static_assert(std::is_constructible_v<quanta::electro::voltage<fx, std::milli>, quanta::electro::volts>);
static_assert(std::is_constructible_v<quanta::spatial::length<fx, std::milli>, quanta::spatial::meters>);

// The fixed-point rep (explicit from double) from int64_t, at the same and a
// finer precision
static_assert(std::is_constructible_v<quanta::frequency::frequency<explicit_rep::fixed>, quanta::frequency::hertz>);
static_assert(!std::is_convertible_v<quanta::frequency::hertz, quanta::frequency::frequency<explicit_rep::fixed>>);
static_assert(
    std::is_constructible_v<quanta::frequency::frequency<explicit_rep::fixed, std::milli>, quanta::frequency::hertz>
);

TEST_CASE("lossless explicit conversion to an explicit-only rep", "[combined]") {
    CHECK(quanta::thermo::delta<fx, std::milli>(quanta::thermo::delta_celsius(3)).count().v == 3000);
    CHECK(double(quanta::frequency::frequency<explicit_rep::fixed>(quanta::frequency::hertz(7)).count()) == 7.0);
    CHECK(
        double(quanta::frequency::frequency<explicit_rep::fixed, std::milli>(quanta::frequency::hertz(2)).count()) ==
        2000.0
    );
}
} // namespace explicit_lossless

// The explicit constructor needs exactly what the cast does: the count, a
// prvalue, converts to the target rep (through their common type). A rep
// constructible only from an rvalue double is reached from a double-rep
// quantity.
namespace rvalue_rep {
struct from_rvalue {
    double v = 0;
    constexpr from_rvalue() = default;
    constexpr explicit from_rvalue(double&& d)
        : v(d) {}
    constexpr explicit from_rvalue(const double&) = delete;
    constexpr operator double() const { return v; } // for the cast's common type
};

static_assert(std::is_constructible_v<quanta::frequency::frequency<from_rvalue>, quanta::frequency::frequency<double>>);
static_assert(!std::is_convertible_v<quanta::frequency::frequency<double>, quanta::frequency::frequency<from_rvalue>>);
static_assert(quanta::frequency::frequency<from_rvalue>(quanta::frequency::frequency<double>(2.5)).count().v == 2.5);
} // namespace rvalue_rep
