// Unit tests for proportion library
// Uses Catch2 test framework with compile-time static_asserts

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <limits>
#include <quanta/proportion>
#include <ratio>
#include <string>
#include <type_traits>

using namespace quanta::proportion;
using namespace quanta::proportion_literals;

using fraction = proportion<int64_t>;
using fraction_d = proportion<double>;
using percent_d = proportion<double, std::ratio<1, 100>>;
using ppm_d = proportion<double, std::ratio<1, 1'000'000>>;
using basis_points = proportion<int64_t, std::ratio<1, 10000>>;

// =============================================================================
// Compile-time tests (static_assert)
// These verify correctness at compile time - if this file compiles, they pass.
// =============================================================================

// Trait
static_assert(is_proportion_v<percent>);
static_assert(is_proportion_v<proportion<double>>);
static_assert(!is_proportion_v<int64_t>);
static_assert(!is_proportion_v<double>);
static_assert(!is_proportion_v<std::chrono::seconds>);

// The shared traits and tags are re-exported, not redefined
static_assert(std::is_same_v<treat_as_inexact<double>, quanta::treat_as_inexact<double>>);
static_assert(&base_unit == &quanta::base_unit);
static_assert(&scaled == &quanta::scaled);

// Proportion construction and arithmetic
static_assert(percent(10).count() == 10);
static_assert(percent(5) + percent(3) == percent(8));
static_assert(percent(5) - percent(3) == percent(2));
static_assert(percent(5) * 2 == percent(10));
static_assert(2 * percent(5) == percent(10));
static_assert(percent(10) / 2 == percent(5));

// Proportion precision conversions
static_assert(fraction(1) == percent(100));
static_assert(percent(1) == permille(10));
static_assert(permille(1) == ppm(1000));
static_assert(ppm(1) == ppb(1000));
static_assert(percent(1) == ppm(10000));

// Construction and count
static_assert(percent(25).count() == 25);
static_assert(permille(5).count() == 5);
static_assert(ppm(50).count() == 50);
static_assert(ppb(3).count() == 3);
static_assert(fraction_d(0.25).count() == 0.25);

// Precision conversions (implicit lossless)
constexpr permille pm_from_pct = percent(1);
static_assert(pm_from_pct.count() == 10);
constexpr ppm ppm_from_pct = percent(1);
static_assert(ppm_from_pct.count() == 10000);
constexpr ppb ppb_from_ppm = ppm(1);
static_assert(ppb_from_ppm.count() == 1000);
constexpr percent pct_from_fraction = fraction(1);
static_assert(pct_from_fraction.count() == 100);
constexpr fraction_d fraction_from_pct = percent(25);
static_assert(fraction_from_pct.count() == 0.25);

// Explicit lossy conversions
static_assert(percent(ppm(12345)).count() == 1);
static_assert(percent(permille(15)).count() == 1);
static_assert(percent(percent_d(12.5)).count() == 12);

// Comparison operators
static_assert(percent(10) == percent(10));
static_assert(percent(10) != percent(11));
static_assert(percent(10) < percent(11));
static_assert(percent(11) > percent(10));
static_assert(percent(10) <= percent(11));
static_assert(percent(11) >= percent(10));
static_assert(percent(1) > ppm(9999));
static_assert(percent(1) < ppm(10001));
static_assert((percent(1) <=> ppm(10000)) == 0);

// Literals
static_assert((25_pct).count() == 25);
static_assert((5_permille).count() == 5);
static_assert((50_ppm).count() == 50);
static_assert((3_ppb).count() == 3);
static_assert(std::is_same_v<decltype(25_pct), percent>);
static_assert(std::is_same_v<decltype(5_permille), permille>);
static_assert(std::is_same_v<decltype(50_ppm), ppm>);
static_assert(std::is_same_v<decltype(3_ppb), ppb>);

// zero/min/max
static_assert(percent::zero().count() == 0);
static_assert(percent::min().count() == std::numeric_limits<int64_t>::min());
static_assert(percent::max().count() == std::numeric_limits<int64_t>::max());
static_assert(fraction_d::min().count() == std::numeric_limits<double>::lowest());

// Modulo operations
static_assert((percent(10) % 3).count() == 1);
static_assert((percent(10) % percent(3)).count() == 1);
static_assert((percent(1) % ppm(3000)) == ppm(1000));

// Division returning scalar
static_assert(percent(50) / percent(25) == 2);
static_assert(percent(1) / ppm(100) == 100);

// Proportion x proportion: precisions multiply and the product is exact
static_assert(50_pct * 50_pct == basis_points(2500));
static_assert((50_pct * 50_pct).count() == 2500);
static_assert(50_pct * 50_pct == 25_pct);
static_assert(10_pct * 5_permille == 500_ppm);
static_assert(1_ppm * 1_ppm == proportion<int64_t, std::ratio<1, 1'000'000'000'000>>(1));

// Rounding functions
static_assert(floor<percent>(permille(15)).count() == 1);
static_assert(floor<percent>(permille(19)).count() == 1);
static_assert(floor<percent>(permille(20)).count() == 2);
static_assert(ceil<percent>(permille(15)).count() == 2);
static_assert(ceil<percent>(permille(11)).count() == 2);
static_assert(ceil<percent>(permille(10)).count() == 1);
static_assert(round<percent>(permille(15)).count() == 2); // ties to even
static_assert(round<percent>(permille(25)).count() == 2); // ties to even
static_assert(round<percent>(permille(14)).count() == 1);
static_assert(round<percent>(permille(16)).count() == 2);
static_assert(trunc<percent>(permille(-15)).count() == -1);
static_assert(abs(percent(5)).count() == 5);
static_assert(abs(percent(-5)).count() == 5);
static_assert(abs(percent(0)).count() == 0);

// =============================================================================
// Conversion rules
// =============================================================================

// To a finer precision: lossless, so implicit
static_assert(std::is_convertible_v<percent, permille>);
static_assert(std::is_convertible_v<percent, ppm>);
static_assert(std::is_convertible_v<percent, ppb>);
static_assert(std::is_convertible_v<permille, ppm>);
static_assert(std::is_convertible_v<ppm, ppb>);
static_assert(std::is_convertible_v<fraction, percent>);
static_assert(std::is_convertible_v<percent, basis_points>);

// To a coarser precision: lossy, so explicit only
static_assert(!std::is_convertible_v<ppm, percent>);
static_assert(std::is_constructible_v<percent, ppm>);
static_assert(!std::is_convertible_v<ppb, ppm>);
static_assert(std::is_constructible_v<ppm, ppb>);
static_assert(!std::is_convertible_v<permille, percent>);
static_assert(std::is_constructible_v<percent, permille>);
static_assert(!std::is_convertible_v<percent, fraction>);
static_assert(std::is_constructible_v<fraction, percent>);

// To a floating-point rep: implicit at any precision
static_assert(std::is_convertible_v<ppm, fraction_d>);
static_assert(std::is_convertible_v<ppb, percent_d>);
static_assert(std::is_convertible_v<fraction, ppm_d>);

// From a floating-point rep to an integral one: explicit only
static_assert(!std::is_convertible_v<percent_d, percent>);
static_assert(std::is_constructible_v<percent, percent_d>);
static_assert(!std::is_convertible_v<fraction_d, ppm>);
static_assert(std::is_constructible_v<ppm, fraction_d>);

// A count converts explicitly, and a floating-point count does not make an
// integral proportion
static_assert(!std::is_convertible_v<int, percent>);
static_assert(std::is_constructible_v<percent, int>);
static_assert(!std::is_constructible_v<percent, double>);
static_assert(std::is_constructible_v<percent_d, int>);

// Proportions are not interchangeable with their counts
static_assert(!std::is_convertible_v<percent, int64_t>);

// common_type follows the std::chrono rules
static_assert(std::is_same_v<std::common_type_t<percent, ppm>, ppm>);
static_assert(std::is_same_v<std::common_type_t<ppm, percent>, ppm>);
static_assert(std::is_same_v<std::common_type_t<percent, permille>, permille>);
static_assert(std::is_same_v<std::common_type_t<fraction, ppb>, ppb>);
static_assert(std::is_same_v<std::common_type_t<percent, percent_d>, percent_d>);
static_assert(std::is_same_v<decltype(1_pct + 50_ppm), ppm>);
static_assert(1_pct + 50_ppm == ppm(10050));
static_assert(std::is_same_v<
              std::common_type_t<proportion<int, std::ratio<1, 4>>, proportion<long, std::ratio<1, 6>>>,
              proportion<long, std::ratio<1, 12>>>);

// =============================================================================
// Overload selection: proportion x proportion vs proportion x scalar
// =============================================================================

// A proportion is never taken as the scalar of the scalar overloads, so the
// product of two proportions is a proportion at the product precision, not a
// proportion with a proportion rep.
static_assert(std::is_same_v<decltype(50_pct * 50_pct), basis_points>);
static_assert(std::is_same_v<decltype(1_ppm * 1_pct), proportion<int64_t, std::ratio<1, 100'000'000>>>);
static_assert(std::is_same_v<decltype(percent_d(50) * 50_pct), proportion<double, std::ratio<1, 10000>>>);
static_assert(std::is_same_v<decltype(fraction_d(0.5) * fraction_d(0.5)), fraction_d>);
static_assert(std::is_same_v<
              decltype(proportion<int, std::ratio<1, 100>>(1) * proportion<long, std::ratio<1, 10>>(1)),
              proportion<long, std::ratio<1, 1000>>>);

// Scalars on either side keep the precision
static_assert(std::is_same_v<decltype(25_pct * 2), percent>);
static_assert(std::is_same_v<decltype(2 * 25_pct), percent>);
static_assert(std::is_same_v<decltype(25_pct * 2.0), percent_d>);
static_assert(std::is_same_v<decltype(2.0 * 25_pct), percent_d>);

// Division of two proportions is a scalar; by a scalar, a proportion
static_assert(std::is_same_v<decltype(50_pct / 25_pct), int64_t>);
static_assert(std::is_same_v<decltype(50_pct / 2), percent>);

template<typename A, typename B>
concept multipliable = requires(A a, B b) { a * b; };

template<typename A, typename B>
concept modulable = requires(A a, B b) { a % b; };

static_assert(multipliable<percent, int>);
static_assert(multipliable<int, percent>);
static_assert(multipliable<percent, ppm>);
static_assert(!modulable<percent_d, int>);
static_assert(!modulable<percent_d, percent_d>);
static_assert(!modulable<percent, double>);

// =============================================================================
// Runtime tests (Catch2 TEST_CASE)
// =============================================================================

TEST_CASE("proportion construction", "[proportion]") {
    REQUIRE(percent(25).count() == 25);
    REQUIRE(percent::zero().count() == 0);
    REQUIRE(percent().count() == 0);
    REQUIRE(fraction_d(0.25).count() == 0.25);
    REQUIRE(fraction_d().count() == 0.0);
}

TEST_CASE("proportion arithmetic", "[proportion]") {
    REQUIRE((percent(5) + percent(3)).count() == 8);
    REQUIRE((percent(5) - percent(3)).count() == 2);
    REQUIRE((percent(5) * 2).count() == 10);
    REQUIRE((2 * percent(5)).count() == 10);
    REQUIRE((percent(10) / 2).count() == 5);
    REQUIRE((percent(10) % 3).count() == 1);
    REQUIRE((-percent(5)).count() == -5);
    REQUIRE((+percent(5)).count() == 5);
}

TEST_CASE("proportion arithmetic across precisions", "[proportion]") {
    // Results are in the common precision, as for std::chrono durations
    auto sum = 1_pct + 50_ppm;
    REQUIRE(sum == ppm(10050));
    REQUIRE(sum.count() == 10050);

    auto diff = 1_pct - 5_permille;
    REQUIRE(diff == permille(5));

    REQUIRE((25_pct + 50_permille).count() == 300);
    REQUIRE((1_ppm - 1_ppb).count() == 999);
    REQUIRE((1_pct % 3_permille) == permille(1));
    REQUIRE(1_pct / 100_ppm == 100);
}

TEST_CASE("proportion increment/decrement", "[proportion]") {
    percent p(10);

    ++p;
    REQUIRE(p.count() == 11);

    --p;
    REQUIRE(p.count() == 10);

    p++;
    REQUIRE(p.count() == 11);

    p--;
    REQUIRE(p.count() == 10);
}

TEST_CASE("proportion compound assignment", "[proportion]") {
    percent p(10);

    p += percent(5);
    REQUIRE(p.count() == 15);

    p -= percent(3);
    REQUIRE(p.count() == 12);

    p *= 2;
    REQUIRE(p.count() == 24);

    p /= 4;
    REQUIRE(p.count() == 6);

    p %= 4;
    REQUIRE(p.count() == 2);

    p = percent(10);
    p %= percent(3);
    REQUIRE(p.count() == 1);

    // A lossless proportion converts implicitly in compound assignment
    ppm q(100);
    q += 1_pct;
    REQUIRE(q.count() == 10100);
}

TEST_CASE("proportion comparison", "[proportion]") {
    REQUIRE(percent(10) == percent(10));
    REQUIRE(percent(10) != percent(11));
    REQUIRE(percent(10) < percent(11));
    REQUIRE(percent(11) > percent(10));
    REQUIRE(percent(10) <= percent(10));
    REQUIRE(percent(10) <= percent(11));
    REQUIRE(percent(11) >= percent(11));
    REQUIRE(percent(11) >= percent(10));
}

TEST_CASE("proportion cross-precision comparison", "[proportion]") {
    REQUIRE(fraction(1) == percent(100));
    REQUIRE(percent(1) == permille(10));
    REQUIRE(permille(1) == ppm(1000));
    REQUIRE(ppm(1) == ppb(1000));
    REQUIRE(percent(1) > ppm(9999));
    REQUIRE(fraction_d(0.25) == percent(25));
}

TEST_CASE("proportion times proportion is exact", "[proportion][product]") {
    auto both = 50_pct * 50_pct;
    STATIC_REQUIRE(std::is_same_v<decltype(both), basis_points>);
    REQUIRE(both.count() == 2500);
    REQUIRE(both == 25_pct);
    REQUIRE(percent(both) == 25_pct);

    // Precisions multiply: no conversion, so no truncation
    auto tiny = 3_pct * 7_ppm;
    STATIC_REQUIRE(std::is_same_v<decltype(tiny), proportion<int64_t, std::ratio<1, 100'000'000>>>);
    REQUIRE(tiny.count() == 21);
    REQUIRE(tiny == ppb(210));

    // A product that a coarser precision could not hold
    REQUIRE((1_pct * 1_pct).count() == 1);
    REQUIRE(1_pct * 1_pct == ppm(100));
    REQUIRE(percent(1_pct * 1_pct).count() == 0);

    // Signs
    REQUIRE((percent(-50) * 50_pct).count() == -2500);
    REQUIRE((percent(-50) * percent(-50)).count() == 2500);

    // Floating-point and mixed reps
    REQUIRE((fraction_d(0.5) * fraction_d(0.25)).count() == 0.125);
    auto mixed = percent_d(12.5) * 2_pct;
    STATIC_REQUIRE(std::is_same_v<decltype(mixed), proportion<double, std::ratio<1, 10000>>>);
    REQUIRE(mixed.count() == 25.0);
    REQUIRE(fraction_d(mixed).count() == Catch::Approx(0.0025));

    // Scalars multiply the count without changing the precision
    REQUIRE((50_pct * 2).count() == 100);
    REQUIRE((2 * 50_pct).count() == 100);
    REQUIRE((50_pct * 2) == fraction(1));
}

TEST_CASE("proportion literals", "[proportion][literals]") {
    REQUIRE((25_pct).count() == 25);
    REQUIRE((5_permille).count() == 5);
    REQUIRE((50_ppm).count() == 50);
    REQUIRE((3_ppb).count() == 3);
    REQUIRE(25_pct == percent(25));
    REQUIRE(1_pct == 10_permille);
    REQUIRE(1_permille == 1000_ppm);
    REQUIRE(1_ppm == 1000_ppb);
}

TEST_CASE("literals follow C++ integer-literal rules", "[proportion][literals]") {
    // Digit separators
    REQUIRE((1'000_ppm).count() == 1000);
    REQUIRE((2'500'000_ppb).count() == 2500000);
    // Hexadecimal, binary and octal, as for std::chrono literals (0123h is 83 h)
    REQUIRE((0x10_pct).count() == 16);
    REQUIRE((0XfF_ppm).count() == 255);
    REQUIRE((0b101_permille).count() == 5);
    REQUIRE((0B1'0000_ppb).count() == 16);
    REQUIRE((0123_pct).count() == 83);
    REQUIRE((0123_pct).count() == std::chrono::hours(0123).count());
    REQUIRE((0'17_ppm).count() == 15);
    // Zero in every base
    REQUIRE((0_pct).count() == 0);
    REQUIRE((00_pct).count() == 0);
    REQUIRE((0x0_pct).count() == 0);
    REQUIRE((0b0_pct).count() == 0);
    // The largest representable count (one more is a compile error)
    REQUIRE((9'223'372'036'854'775'807_pct).count() == std::numeric_limits<int64_t>::max());
    REQUIRE((0x7fff'ffff'ffff'ffff_ppb).count() == std::numeric_limits<int64_t>::max());
}

TEST_CASE("proportion modulo", "[proportion]") {
    REQUIRE((percent(10) % 3).count() == 1);
    REQUIRE((percent(10) % percent(3)).count() == 1);
}

TEST_CASE("proportion division returning scalar", "[proportion]") {
    REQUIRE(percent(50) / percent(25) == 2);
    REQUIRE(ppm(10) / ppm(2) == 5);
    REQUIRE(fraction_d(0.5) / fraction_d(0.25) == 2.0);
}

TEST_CASE("proportion_cast", "[proportion][conversion]") {
    REQUIRE(proportion_cast<percent>(ppm(12345)) == percent(1));
    REQUIRE(proportion_cast<percent>(ppm(-12345)) == percent(-1));
    REQUIRE(proportion_cast<ppm>(percent(1)) == ppm(10000));
    REQUIRE(proportion_cast<fraction_d>(percent(25)).count() == 0.25);
    REQUIRE(proportion_cast<percent_d>(ppm(12345)).count() == Catch::Approx(1.2345));
    REQUIRE(proportion_cast<percent>(fraction_d(0.256)) == percent(25));
    REQUIRE(proportion_cast<permille>(percent_d(12.34)) == permille(123));
}

TEST_CASE("trunc", "[proportion][rounding]") {
    REQUIRE(trunc<percent>(permille(15)) == percent(1));
    REQUIRE(trunc<percent>(permille(-15)) == percent(-1));
    REQUIRE(trunc<percent>(permille(19)) == percent(1));
    REQUIRE(trunc<percent>(permille(0)) == percent(0));
    REQUIRE(trunc<percent>(percent_d(1.7)) == percent(1));
    REQUIRE(trunc<percent>(percent_d(-1.7)) == percent(-1));
    REQUIRE(trunc<ppm>(percent(5)) == ppm(50000));
    REQUIRE(trunc<percent_d>(permille(15)).count() == 1.5);
}

TEST_CASE("proportion floor", "[proportion][rounding]") {
    REQUIRE(floor<percent>(permille(15)).count() == 1);
    REQUIRE(floor<percent>(permille(9)).count() == 0);
    REQUIRE(floor<percent>(permille(-15)).count() == -2);
    REQUIRE(floor<percent>(permille(-10)).count() == -1);
    REQUIRE(floor<percent>(permille(-9)).count() == -1);
    REQUIRE(floor<ppm>(ppb(1999)).count() == 1);
    REQUIRE(floor<ppm>(ppb(-1)).count() == -1);
    REQUIRE(floor<percent>(1999_ppm) == 0_pct);
    REQUIRE(floor<percent>(percent_d(1.5)) == percent(1));
    REQUIRE(floor<percent>(percent_d(-1.5)) == percent(-2));
}

TEST_CASE("proportion ceil", "[proportion][rounding]") {
    REQUIRE(ceil<percent>(permille(15)).count() == 2);
    REQUIRE(ceil<percent>(permille(11)).count() == 2);
    REQUIRE(ceil<percent>(permille(10)).count() == 1);
    REQUIRE(ceil<percent>(permille(-15)).count() == -1);
    REQUIRE(ceil<percent>(permille(-11)).count() == -1);
    REQUIRE(ceil<ppm>(ppb(1001)).count() == 2);
    REQUIRE(ceil<percent>(1_ppm) == 1_pct);
    REQUIRE(ceil<percent>(percent_d(1.2)) == percent(2));
    REQUIRE(ceil<percent>(percent_d(-1.2)) == percent(-1));
}

TEST_CASE("proportion round", "[proportion][rounding]") {
    SECTION("basic rounding") {
        REQUIRE(round<percent>(permille(14)).count() == 1);
        REQUIRE(round<percent>(permille(16)).count() == 2);
        REQUIRE(round<percent>(permille(10)).count() == 1);
        REQUIRE(round<percent>(permille(20)).count() == 2);
    }

    SECTION("ties to even") {
        REQUIRE(round<percent>(permille(15)).count() == 2);
        REQUIRE(round<percent>(permille(25)).count() == 2);
        REQUIRE(round<percent>(permille(35)).count() == 4);
        REQUIRE(round<percent>(permille(45)).count() == 4);
        REQUIRE(round<ppm>(ppb(2500)).count() == 2);
    }

    SECTION("negative values") {
        REQUIRE(round<percent>(permille(-14)).count() == -1);
        REQUIRE(round<percent>(permille(-16)).count() == -2);
        REQUIRE(round<percent>(permille(-15)).count() == -2); // ties to even
        REQUIRE(round<percent>(permille(-25)).count() == -2); // ties to even
    }

    SECTION("larger precision gaps") {
        REQUIRE(round<percent>(ppm(14999)).count() == 1);
        REQUIRE(round<percent>(ppm(15000)).count() == 2); // ties to even
        REQUIRE(round<percent>(ppm(15001)).count() == 2);
        REQUIRE(round<percent>(ppb(25'000'000)).count() == 2); // ties to even
    }

    SECTION("from a floating-point rep") {
        REQUIRE(round<percent>(percent_d(1.7)) == percent(2));
        REQUIRE(round<percent>(percent_d(-1.7)) == percent(-2));
        REQUIRE(round<percent>(percent_d(2.5)) == percent(2));   // tie to even
        REQUIRE(round<percent>(percent_d(-2.5)) == percent(-2)); // tie to even
        REQUIRE(round<percent>(fraction_d(0.125)) == percent(12));
    }

    SECTION("a floating-point target is not rounded") {
        REQUIRE(floor<percent_d>(permille(15)).count() == 1.5);
        REQUIRE(ceil<percent_d>(permille(15)).count() == 1.5);
        REQUIRE(round<percent_d>(permille(15)).count() == 1.5);
    }

    SECTION("rounding a product back to a named precision") {
        REQUIRE(round<percent>(33_pct * 33_pct) == 11_pct); // 10.89%
        REQUIRE(floor<percent>(33_pct * 33_pct) == 10_pct);
    }
}

TEST_CASE("proportion abs", "[proportion][rounding]") {
    REQUIRE(abs(percent(5)).count() == 5);
    REQUIRE(abs(percent(-5)).count() == 5);
    REQUIRE(abs(percent(0)).count() == 0);
    REQUIRE(abs(percent::zero()).count() == 0);
    REQUIRE(abs(ppm(-50)).count() == 50);
    REQUIRE(abs(fraction_d(-0.25)).count() == 0.25);

    percent p = abs(percent(-5));
    ppb q = abs(ppb(-3));
    REQUIRE(p.count() == 5);
    REQUIRE(q.count() == 3);
}

TEST_CASE("floating-point proportions", "[proportion][floating-point]") {
    fraction_d efficiency{0.875};
    REQUIRE(efficiency.count() == 0.875);

    // Implicit lossless conversion to a floating-point rep at any precision
    fraction_d f = 25_pct;
    REQUIRE(f.count() == 0.25);
    ppm_d p = 1_pct;
    REQUIRE(p.count() == 10000.0);
    percent_d q = ppm(12345);
    REQUIRE(q.count() == Catch::Approx(1.2345));

    // Arithmetic
    REQUIRE(fraction_d(0.5) + 25_pct == fraction_d(0.75));
    REQUIRE((fraction_d(0.5) + 25_pct).count() == 75.0); // in the common precision, percent
    REQUIRE((fraction_d(0.5) * 1.5).count() == 0.75);
    REQUIRE((1.5 * fraction_d(0.5)).count() == 0.75);
    REQUIRE((fraction_d(0.5) / 4.0).count() == 0.125);

    // Floating-point to integral is explicit and truncates
    REQUIRE(percent(percent_d(12.9)).count() == 12);
    REQUIRE(percent(fraction_d(0.129)).count() == 12);
    REQUIRE(ppm(fraction_d(-0.0000015)).count() == -1);
}

TEST_CASE("to_string", "[proportion][string]") {
    REQUIRE(to_string(percent(25)) == "25%");
    REQUIRE(to_string(permille(5)) == "5‰");
    REQUIRE(to_string(ppm(50)) == "50ppm");
    REQUIRE(to_string(ppb(3)) == "3ppb");
    REQUIRE(to_string(percent(-25)) == "-25%");
    REQUIRE(to_string(percent(0)) == "0%");
    // A plain fraction has no symbol
    REQUIRE(to_string(fraction(1)) == "1");
    REQUIRE(to_string(fraction(-2)) == "-2");
}

TEST_CASE("to_string floating-point uses shortest fixed notation", "[proportion][string]") {
    REQUIRE(to_string(fraction_d(0.25)) == "0.25");
    REQUIRE(to_string(fraction_d(1e-5)) == "0.00001");
    REQUIRE(to_string(fraction_d(1e6)) == "1000000");
    REQUIRE(to_string(percent_d(12.5)) == "12.5%");
    REQUIRE(to_string(ppm_d(-1.25)) == "-1.25ppm");
    REQUIRE(to_string(proportion<float>(0.5f)) == "0.5");
}

TEST_CASE("to_string base_unit", "[proportion][string]") {
    REQUIRE(to_string(percent(25), base_unit) == "0.25");
    REQUIRE(to_string(25_pct, base_unit) == "0.25");
    REQUIRE(to_string(percent(150), base_unit) == "1.5");
    REQUIRE(to_string(percent(-25), base_unit) == "-0.25");
    REQUIRE(to_string(permille(5), base_unit) == "0.005");
    REQUIRE(to_string(ppm(50), base_unit) == "0.00005");
    REQUIRE(to_string(ppb(3), base_unit) == "0.000000003");
    REQUIRE(to_string(percent(0), base_unit) == "0");
    REQUIRE(to_string(fraction(2), base_unit) == "2");
    REQUIRE(to_string(fraction_d(0.25), base_unit) == "0.25");
    REQUIRE(to_string(percent_d(12.5), base_unit) == "0.125");
    // Precisions with no symbol of their own still have a base unit.
    REQUIRE(to_string(50_pct * 50_pct, base_unit) == "0.25");
    REQUIRE(to_string(proportion<int64_t, std::ratio<1, 4>>(3), base_unit) == "0.75");
}

TEST_CASE("to_string scaled", "[proportion][string]") {
    SECTION("examples") {
        REQUIRE(to_string(fraction_d(0.5), scaled) == "50%");
        REQUIRE(to_string(fraction_d(0.004), scaled) == "0.4%");
        REQUIRE(to_string(fraction_d(0.0002), scaled) == "200ppm");
        REQUIRE(to_string(fraction_d(3e-9), scaled) == "3ppb");
        REQUIRE(to_string(percent(150), scaled) == "150%");
        REQUIRE(to_string(percent(50), scaled) == "50%");
        REQUIRE(to_string(permille(4), scaled) == "0.4%");
        REQUIRE(to_string(ppm(200), scaled) == "200ppm");
        REQUIRE(to_string(ppb(3), scaled) == "3ppb");
        REQUIRE(to_string(fraction(2), scaled) == "200%");
    }

    SECTION("% from 0.001 (0.1%) up") {
        REQUIRE(to_string(1_permille, scaled) == "0.1%");
        REQUIRE(to_string(1000_ppm, scaled) == "0.1%");
        REQUIRE(to_string(1'000'000_ppb, scaled) == "0.1%");
        REQUIRE(to_string(fraction_d(0.001), scaled) == "0.1%");
        REQUIRE(to_string(1001_ppm, scaled) == "0.1001%");
        REQUIRE(to_string(1_pct, scaled) == "1%");
        REQUIRE(to_string(100_pct, scaled) == "100%");
    }

    SECTION("ppm from 0.000001 up to below 0.001") {
        REQUIRE(to_string(999_ppm, scaled) == "999ppm");
        REQUIRE(to_string(999'999_ppb, scaled) == "999.999ppm");
        REQUIRE(to_string(fraction_d(0.0009765625), scaled) == "976.5625ppm"); // 2^-10
        REQUIRE(to_string(1_ppm, scaled) == "1ppm");
        REQUIRE(to_string(1000_ppb, scaled) == "1ppm");
        REQUIRE(to_string(fraction_d(0.000001), scaled) == "1ppm");
        REQUIRE(to_string(1500_ppb, scaled) == "1.5ppm");
    }

    SECTION("ppb below 0.000001") {
        REQUIRE(to_string(999_ppb, scaled) == "999ppb");
        REQUIRE(to_string(1_ppb, scaled) == "1ppb");
        REQUIRE(to_string(fraction_d(0.00000095367431640625), scaled) == "953.67431640625ppb"); // 2^-20
        // Below the smallest unit the smallest is used.
        REQUIRE(to_string(proportion<double, std::ratio<1, 1'000'000'000>>(0.5), scaled) == "0.5ppb");
    }

    SECTION("negative values") {
        REQUIRE(to_string(fraction_d(-0.5), scaled) == "-50%");
        REQUIRE(to_string(percent(-150), scaled) == "-150%");
        REQUIRE(to_string(-1_permille, scaled) == "-0.1%");
        REQUIRE(to_string(-999_ppm, scaled) == "-999ppm");
        REQUIRE(to_string(-1_ppm, scaled) == "-1ppm");
        REQUIRE(to_string(-999_ppb, scaled) == "-999ppb");
    }

    SECTION("zero is a plain fraction") {
        REQUIRE(to_string(percent(0), scaled) == "0");
        REQUIRE(to_string(ppb(0), scaled) == "0");
        REQUIRE(to_string(fraction_d(0.0), scaled) == "0");
        REQUIRE(to_string(fraction_d(-0.0), scaled) == "-0");
    }

    SECTION("precisions with no symbol of their own") {
        REQUIRE(to_string(50_pct * 50_pct, scaled) == "25%");
        REQUIRE(to_string(basis_points(50), scaled) == "0.5%");
        REQUIRE(to_string(basis_points(5), scaled) == "500ppm");
        REQUIRE(to_string(proportion<int64_t, std::ratio<1, 4>>(3), scaled) == "75%");
        REQUIRE(to_string(proportion<int64_t, std::ratio<1, 3>>(1), scaled) == "33.33333333333333%");
    }
}

TEST_CASE("scaled form of non-finite values is a plain fraction", "[proportion][string]") {
    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(to_string(fraction_d(inf), scaled) == "inf");
    REQUIRE(to_string(fraction_d(-inf), scaled) == "-inf");
    REQUIRE(to_string(fraction_d(nan), scaled) == "nan");
    REQUIRE(to_string(percent_d(inf), scaled) == "inf");
    REQUIRE(to_string(proportion<double, std::ratio<1, 3>>(nan), scaled) == "nan");
#if CONFIG_QUANTA_STD_FORMAT
    REQUIRE(std::format("{:s}", fraction_d(inf)) == "inf");
    REQUIRE(std::format("{:s}", fraction_d(-inf)) == "-inf");
    REQUIRE(std::format("{:s.1f}", percent_d(inf)) == "inf");
#endif
}

#if CONFIG_QUANTA_STD_FORMAT
TEST_CASE("std::format proportion", "[proportion][string]") {
    // Empty spec prints the exact stored count with the precision's symbol.
    REQUIRE(std::format("{}", percent(25)) == "25%");
    REQUIRE(std::format("{}", 25_pct) == "25%");
    REQUIRE(std::format("{}", permille(5)) == "5‰");
    REQUIRE(std::format("{}", ppm(50)) == "50ppm");
    REQUIRE(std::format("{}", ppb(3)) == "3ppb");
    REQUIRE(std::format("{}", percent(-25)) == "-25%");
    // A plain fraction has no symbol.
    REQUIRE(std::format("{}", fraction(1)) == "1");
}

TEST_CASE("std::format floating-point uses shortest fixed notation", "[proportion][string]") {
    REQUIRE(std::format("{}", fraction_d(0.25)) == "0.25");
    REQUIRE(std::format("{}", fraction_d(1e-5)) == "0.00001");
    REQUIRE(std::format("{}", fraction_d(1e6)) == "1000000");
    REQUIRE(std::format("{}", percent_d(12.5)) == "12.5%");
}

TEST_CASE("std::format with spec renders in the own unit", "[proportion][string]") {
    // A format spec changes only how the number is rendered, never the unit.
    REQUIRE(std::format("{:.1f}", percent(25)) == "25.0%");
    REQUIRE(std::format("{:.2f}", ppm(50)) == "50.00ppm");
    REQUIRE(std::format("{:g}", permille(5)) == "5‰");
    REQUIRE(std::format("{:.1f}", percent(-25)) == "-25.0%");
    REQUIRE(std::format("{:.1f}", percent(0)) == "0.0%");
    REQUIRE(std::format("{:.2f}", fraction_d(0.125)) == "0.12");
    REQUIRE(std::format("{:.3e}", percent_d(12.5)) == "1.250e+01%");

    // Width and alignment pass through to the numeric part.
    REQUIRE(std::format("{:6.1f}", percent(25)) == "  25.0%");
    REQUIRE(std::format("{:*<6.1f}", percent(25)) == "25.0**%");
}

TEST_CASE("std::format with no symbol renders a spec as a plain fraction", "[proportion][string]") {
    // There is no unit symbol for the precision, so a spec falls back to the
    // plain fraction.
    REQUIRE(std::format("{:.4f}", 50_pct * 50_pct) == "0.2500");
    REQUIRE(std::format("{:g}", basis_points(-5)) == "-0.0005");
}

TEST_CASE("std::format b marker renders a plain fraction", "[proportion][string]") {
    REQUIRE(std::format("{:b}", 25_pct) == "0.25");
    REQUIRE(std::format("{:b}", percent(150)) == "1.5");
    REQUIRE(std::format("{:b}", permille(5)) == "0.005");
    REQUIRE(std::format("{:b}", ppm(50)) == "0.00005");
    REQUIRE(std::format("{:b}", ppb(3)) == "0.000000003");
    REQUIRE(std::format("{:b}", percent(-25)) == "-0.25");
    REQUIRE(std::format("{:b}", percent(0)) == "0");
    REQUIRE(std::format("{:b.1f}", percent(0)) == "0.0");
    REQUIRE(std::format("{:b.3f}", 25_pct) == "0.250");
    REQUIRE(std::format("{:b}", percent_d(12.5)) == "0.125");
    REQUIRE(std::format("{:b}", 50_pct * 50_pct) == "0.25");

    // Width, fill and alignment follow the marker.
    REQUIRE(std::format("{:b6.2f}", 25_pct) == "  0.25");
    REQUIRE(std::format("{:b*>6.2f}", 25_pct) == "**0.25");
}

TEST_CASE("std::format s marker scales to %, ppm or ppb", "[proportion][string]") {
    // The examples
    REQUIRE(std::format("{:s}", fraction_d(0.5)) == "50%");
    REQUIRE(std::format("{:s}", fraction_d(0.004)) == "0.4%");
    REQUIRE(std::format("{:s}", fraction_d(0.0002)) == "200ppm");
    REQUIRE(std::format("{:s}", fraction_d(3e-9)) == "3ppb");
    REQUIRE(std::format("{:s}", percent(150)) == "150%");

    // The thresholds
    REQUIRE(std::format("{:s}", 1_permille) == "0.1%");
    REQUIRE(std::format("{:s}", 1000_ppm) == "0.1%");
    REQUIRE(std::format("{:s}", 999_ppm) == "999ppm");
    REQUIRE(std::format("{:s}", 1_ppm) == "1ppm");
    REQUIRE(std::format("{:s}", 1000_ppb) == "1ppm");
    REQUIRE(std::format("{:s}", 999_ppb) == "999ppb");
    REQUIRE(std::format("{:s}", -1_permille) == "-0.1%");
    REQUIRE(std::format("{:s}", -999_ppb) == "-999ppb");

    // Zero is a plain fraction
    REQUIRE(std::format("{:s}", percent(0)) == "0");
    REQUIRE(std::format("{:s.1f}", ppm(0)) == "0.0");

    // A spec after the marker
    REQUIRE(std::format("{:s.1f}", 4_permille) == "0.4%");
    REQUIRE(std::format("{:s.2f}", 200_ppm) == "200.00ppm");
    REQUIRE(std::format("{:s.0f}", 50_pct * 50_pct) == "25%");

    // Width, fill and alignment follow the marker.
    REQUIRE(std::format("{:s6.1f}", 25_pct) == "  25.0%");
    REQUIRE(std::format("{:s*<6.1f}", 25_pct) == "25.0**%");
}

TEST_CASE("std::format and to_string agree with no spec", "[proportion][string]") {
    const auto check = [](const auto& p) {
        REQUIRE(std::format("{}", p) == to_string(p));
        REQUIRE(std::format("{:b}", p) == to_string(p, base_unit));
        REQUIRE(std::format("{:s}", p) == to_string(p, scaled));
    };
    check(percent(25));
    check(percent(-150));
    check(percent(0));
    check(permille(4));
    check(ppm(200));
    check(ppm(999));
    check(ppb(3));
    check(fraction(1));
    check(fraction_d(0.5));
    check(fraction_d(0.004));
    check(fraction_d(3e-9));
    check(fraction_d(-0.0));
    check(percent_d(12.5));
    check(proportion<float>(0.1f));

    REQUIRE(std::format("{:b}", basis_points(2500)) == to_string(basis_points(2500), base_unit));
    REQUIRE(std::format("{:s}", basis_points(2500)) == to_string(basis_points(2500), scaled));
}
#endif

// =============================================================================
// Result types (pinned: they depend on the exact std::ratio spelling)
// =============================================================================

namespace pin {
using U = proportion<int, std::ratio<10, 2000>>;
static_assert(std::is_same_v<decltype(+U(1)), proportion<int, std::ratio<1, 200>>>);
static_assert(std::is_same_v<decltype(-U(1)), proportion<int, std::ratio<1, 200>>>);
static_assert(std::is_same_v<decltype(U(1) * 2L), proportion<long, std::ratio<10, 2000>>>);
static_assert(std::is_same_v<decltype(2L * U(1)), proportion<long, std::ratio<10, 2000>>>);
static_assert(std::is_same_v<decltype(U(1) / 2L), proportion<long, std::ratio<10, 2000>>>);
static_assert(std::is_same_v<decltype(U(1) % 2L), proportion<long, std::ratio<10, 2000>>>);
static_assert(std::is_same_v<decltype(U(1) / U(1)), int>);
static_assert(std::is_same_v<decltype(U(1) * U(1)), proportion<int, std::ratio<1, 40000>>>);
static_assert(std::is_same_v<std::common_type_t<U, U>, proportion<int, std::ratio<1, 200>>>);
static_assert(std::is_same_v<decltype(U(1) + U(1)), proportion<int, std::ratio<1, 200>>>);
static_assert(std::is_same_v<decltype(U(1) - U(1)), proportion<int, std::ratio<1, 200>>>);
static_assert(std::is_same_v<decltype(U(1) % U(1)), proportion<int, std::ratio<1, 200>>>);
static_assert(std::is_same_v<decltype(abs(U(1))), U>);
static_assert(std::is_same_v<decltype(U::zero()), U>);
static_assert(std::is_same_v<decltype(U::min()), U>);
static_assert(std::is_same_v<decltype(++std::declval<U&>()), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>()++), U>);
static_assert(std::is_same_v<decltype(std::declval<U&>() += U(1)), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() *= 2), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() %= 2), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() %= U(1)), U&>);
static_assert(sizeof(percent) == sizeof(int64_t));
static_assert(std::is_trivially_copyable_v<percent>);
} // namespace pin

TEST_CASE("explicit conversion covers every lossy case", "[proportion][conversion]") {
    // Floating-point rep to integral rep, same precision: truncates
    REQUIRE(percent(percent_d(1.5)).count() == 1);
    REQUIRE(percent(percent_d(-1.5)).count() == -1);
    // Floating-point rep to integral rep, finer precision: truncates
    REQUIRE(ppm(percent_d(1.5)).count() == 15000);
    REQUIRE(ppm(fraction_d(0.0000015)).count() == 1);
    // Integral rep to a coarser precision: truncates
    REQUIRE(percent(permille(15)).count() == 1);
    REQUIRE(percent(permille(-15)).count() == -1);
    REQUIRE(percent(ppm_d(19999.0)).count() == 1);
    REQUIRE(fraction(percent(150)).count() == 1);
}

TEST_CASE("abs of an unreduced precision", "[proportion]") {
    // A precision not in lowest terms (unary minus gives the reduced one)
    using QI = proportion<int64_t, std::ratio<4, 600>>;
    using QD = proportion<double, std::ratio<4, 600>>;
    STATIC_REQUIRE(std::is_same_v<decltype(abs(QI(-3))), QI>);
    REQUIRE(abs(QI(-3)).count() == 3);
    REQUIRE(abs(QI(3)).count() == 3);
    REQUIRE(abs(QD(-1.5)).count() == 1.5);
    STATIC_REQUIRE(abs(QI(-7)) == QI(7));
}

TEST_CASE("round with a narrow rep does not overflow", "[proportion][rounding]") {
    using percent8 = proportion<int8_t, std::ratio<1, 100>>;
    using permille8 = proportion<int8_t, std::ratio<1, 1000>>;
    REQUIRE(round<percent8>(permille8(0)).count() == 0);
    REQUIRE(round<percent8>(permille8(125)).count() == 12);
    REQUIRE(round<percent8>(permille8(-125)).count() == -12);
    REQUIRE(round<proportion<uint8_t, std::ratio<1, 10>>>(proportion<uint8_t, std::ratio<1, 1000>>(0)).count() == 0);
}

TEST_CASE("floor and ceil with a narrow rep compare exactly", "[proportion][rounding]") {
    // 16384 thirds is 10922.67 halves. Compared in their common type, sixths
    // of an int16_t, 16384 thirds would overflow.
    using thirds16 = proportion<int16_t, std::ratio<1, 3>>;
    using halves16 = proportion<int16_t, std::ratio<1, 2>>;
    REQUIRE(floor<halves16>(thirds16(16384)).count() == 10922);
    REQUIRE(ceil<halves16>(thirds16(16384)).count() == 10923);
    REQUIRE(round<halves16>(thirds16(16384)).count() == 10923);
    REQUIRE(floor<halves16>(thirds16(-16384)).count() == -10923);
    REQUIRE(ceil<halves16>(thirds16(-16384)).count() == -10922);

    // 254 thirds is 169.33 halves; as sixths it exceeds uint8_t
    using thirds_u8 = proportion<uint8_t, std::ratio<1, 3>>;
    using halves_u8 = proportion<uint8_t, std::ratio<1, 2>>;
    REQUIRE(floor<halves_u8>(thirds_u8(254)).count() == 169);
    REQUIRE(ceil<halves_u8>(thirds_u8(254)).count() == 170);
    REQUIRE(round<halves_u8>(thirds_u8(254)).count() == 169);
}

// Precisions whose conversion factor overflows intmax_t do not convert, and
// asking is not an error
static_assert(!std::is_convertible_v<proportion<double, std::atto>, proportion<double, std::exa>>);
static_assert(!std::is_constructible_v<proportion<double, std::atto>, proportion<double, std::exa>>);
static_assert(!std::is_constructible_v<proportion<double, std::exa>, proportion<double, std::atto>>);
static_assert(!std::is_convertible_v<proportion<int64_t, std::atto>, proportion<int64_t, std::exa>>);
static_assert(!std::is_constructible_v<proportion<int64_t, std::exa>, proportion<int64_t, std::atto>>);
static_assert(!std::is_constructible_v<proportion<int64_t, std::atto>, proportion<int64_t, std::exa>>);
