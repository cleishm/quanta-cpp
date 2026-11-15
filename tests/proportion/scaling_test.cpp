// Unit tests for scaling the linear quantities of every domain by a proportion
// Uses Catch2 test framework with compile-time static_asserts

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <quanta/data>
#include <quanta/decibel>
#include <quanta/electro>
#include <quanta/frequency>
#include <quanta/proportion>
#include <quanta/spatial>
#include <quanta/thermo>
#include <ratio>
#include <type_traits>
#include <utility>

// electro, spatial and data each have a quantity, so no domain namespace is
// imported: names are qualified through these aliases.
namespace data = quanta::data;
namespace electro = quanta::electro;
namespace frequency = quanta::frequency;
namespace proportion = quanta::proportion;
namespace spatial = quanta::spatial;
namespace thermo = quanta::thermo;

using namespace quanta::literals;

// =============================================================================
// Compile-time tests (static_assert)
// These verify correctness at compile time - if this file compiles, they pass.
// =============================================================================

namespace {

template<typename A, typename B>
concept multipliable = requires(const A& a, const B& b) { a * b; };

template<typename A, typename B>
concept divisible = requires(const A& a, const B& b) { a / b; };

// q * p and p * q are both exactly Expected, and q / p is Quotient. A domain's
// scalar overloads (quantity * Rep2) must never take the proportion as the
// scalar: that would be ambiguous here, or give a quantity whose rep is a
// proportion.
template<typename Q, typename P, typename Expected, typename Quotient>
constexpr bool scales_to = std::is_same_v<decltype(std::declval<const Q&>() * std::declval<const P&>()), Expected> &&
    std::is_same_v<decltype(std::declval<const P&>() * std::declval<const Q&>()), Expected> &&
    std::is_same_v<decltype(std::declval<const Q&>() / std::declval<const P&>()), Quotient>;

using fraction_d = proportion::proportion<double>;
using percent_d = proportion::proportion<double, std::centi>;

} // namespace

// The result type for an integer and a double rep, in both operand orders
static_assert(scales_to<
              electro::millivolts,
              proportion::percent,
              electro::quantity<electro::volt_unit, int64_t, std::ratio<1, 100000>>,
              electro::quantity<electro::volt_unit, int64_t, std::deci>>);
static_assert(scales_to<electro::voltage<double>, fraction_d, electro::voltage<double>, electro::voltage<double>>);
static_assert(scales_to<
              electro::millivolts,
              percent_d,
              electro::quantity<electro::volt_unit, double, std::ratio<1, 100000>>,
              electro::quantity<electro::volt_unit, double, std::deci>>);
static_assert(scales_to<
              spatial::meters,
              proportion::ppm,
              spatial::length<int64_t, std::micro>,
              spatial::length<int64_t, std::mega>>);
static_assert(scales_to<spatial::length<double>, fraction_d, spatial::length<double>, spatial::length<double>>);
static_assert(
    scales_to<frequency::megahertz, proportion::ppm, frequency::hertz, frequency::frequency<int64_t, std::tera>>
);
static_assert(
    scales_to<frequency::frequency<double>, fraction_d, frequency::frequency<double>, frequency::frequency<double>>
);
static_assert(scales_to<
              thermo::delta_celsius,
              proportion::percent,
              thermo::delta<int64_t, std::centi>,
              thermo::delta<int64_t, std::hecto>>);
static_assert(scales_to<thermo::delta<double>, fraction_d, thermo::delta<double>, thermo::delta<double>>);
static_assert(scales_to<
              data::megabytes,
              proportion::percent,
              data::size<int64_t, std::ratio<10000>>,
              data::size<int64_t, std::ratio<100000000>>>);
static_assert(scales_to<data::size<double>, fraction_d, data::size<double>, data::size<double>>);
static_assert(scales_to<
              data::megabits_per_second,
              proportion::percent,
              data::rate<int64_t, std::ratio<1250>>,
              data::rate<int64_t, std::ratio<12500000>>>);
static_assert(scales_to<data::rate<double>, fraction_d, data::rate<double>, data::rate<double>>);

// Electrical: counts multiply exactly; conversion back is explicit where lossy
static_assert((3300_mV * 50_pct).count() == 165000);
static_assert((50_pct * 3300_mV).count() == 165000);
static_assert(3300_mV * 50_pct == 1650_mV);
static_assert(electro::millivolts(3300_mV * 50_pct) == 1650_mV);
static_assert(!std::is_convertible_v<decltype(3300_mV * 50_pct), electro::millivolts>);
static_assert((3300_mV / 50_pct).count() == 66);
static_assert(3300_mV / 50_pct == 6600_mV);
static_assert((20_mA * 10_pct).count() == 200);
static_assert(electro::milliamperes(20_mA * 10_pct) == 2_mA);
static_assert(electro::milliwatts(1_W * 25_pct) == 250_mW);
static_assert(electro::ohms(4_kOhm * 5_permille) == electro::ohms(20));

// Integer division truncates, as for cross-unit quotients
static_assert((10_mV / 3_pct).count() == 3);

// Spatial: lengths, areas and volumes
static_assert((1000_m * 20_ppm).count() == 20000);
static_assert(spatial::millimeters(1000_m * 20_ppm) == 20_mm);
static_assert(spatial::square_meters(10_m2 * 50_pct) == 5_m2);
static_assert(spatial::milliliters(2_L * 25_pct) == 500_mL);
static_assert(spatial::meters(150_m / 50_pct) == 300_m);

// Frequency
static_assert((2400_MHz * 50_ppm).count() == 120000);
static_assert(2400_MHz * 50_ppm == 120_kHz);
static_assert(std::is_same_v<decltype(2400_MHz * 50_ppm), frequency::hertz>);
static_assert((100_MHz / 50_pct).count() == 2);
static_assert(100_MHz / 50_pct == 200_MHz);
static_assert(frequency::megahertz(1_GHz * 25_pct) == 250_MHz);

// Temperature deltas
static_assert((thermo::delta_celsius(10) * 50_pct).count() == 500);
static_assert(thermo::delta_decicelsius(thermo::delta_celsius(10) * 50_pct) == thermo::delta_celsius(5));
static_assert((thermo::delta_millicelsius(1500) / 50_pct).count() == 30);
static_assert(thermo::delta_millicelsius(1500) / 50_pct == thermo::delta_celsius(3));
static_assert(thermo::delta_fahrenheit(9) * 50_pct == thermo::delta_kelvin(2) + thermo::delta_millikelvin(500));

// A precision not in lowest terms (5/90) multiplies as its reduced value
static_assert(
    std::is_same_v<decltype(thermo::delta_decifahrenheit(90) * 50_pct), thermo::delta<int64_t, std::ratio<1, 1800>>>
);
static_assert((thermo::delta_decifahrenheit(90) * 50_pct).count() == 4500);
static_assert(thermo::delta_decifahrenheit(90) * 50_pct == thermo::delta_fahrenheit(9) * 50_pct);

// Data sizes and rates
static_assert((1_MB * 25_pct).count() == 25);
static_assert(data::kilobytes(1_MB * 25_pct) == 250_kB);
static_assert((100_MB / 50_pct).count() == 2);
static_assert(100_MB / 50_pct == 200_MB);
static_assert((100_Mbit_per_s * 80_pct).count() == 8000);
static_assert(data::megabits_per_second(100_Mbit_per_s * 80_pct) == 80_Mbit_per_s);
static_assert((100_Mbit_per_s / 50_pct).count() == 2);
static_assert(100_Mbit_per_s / 50_pct == 200_Mbit_per_s);

// Double reps
static_assert((electro::voltage<double>(1.5) * 50_pct).count() == 75.0);
static_assert(fraction_d(0.5) * 3300_mV == electro::voltage<double, std::milli>(1650.0));
static_assert(3300_mV / fraction_d(0.5) == electro::voltage<double, std::milli>(6600.0));
static_assert((spatial::length<double>(3.0) * fraction_d(0.5)).count() == 1.5);
static_assert((spatial::length<double>(3.0) / fraction_d(0.5)).count() == 6.0);
static_assert((fraction_d(0.25) * frequency::frequency<double>(10.0)).count() == 2.5);
static_assert((thermo::delta<double>(5.0) * fraction_d(0.5)).count() == 2.5);
static_assert((data::size<double>(1000.0) * fraction_d(0.125)).count() == 125.0);
static_assert((data::rate<double>(1000.0) / fraction_d(0.5)).count() == 2000.0);

// Absolute temperatures, decibel levels and decibel gains are not scaled, in
// either order, and nothing is divided by a proportion or divides one.
static_assert(!multipliable<thermo::celsius, proportion::percent>);
static_assert(!multipliable<proportion::percent, thermo::celsius>);
static_assert(!divisible<thermo::celsius, proportion::percent>);
static_assert(!multipliable<electro::dbm, proportion::percent>);
static_assert(!multipliable<proportion::percent, electro::dbm>);
static_assert(!divisible<electro::dbm, proportion::percent>);
static_assert(!multipliable<electro::decibels, proportion::percent>);
static_assert(!multipliable<proportion::percent, electro::decibels>);
static_assert(!divisible<electro::decibels, proportion::percent>);
static_assert(!multipliable<electro::gain<double>, fraction_d>);
static_assert(!multipliable<fraction_d, electro::gain<double>>);
static_assert(!divisible<electro::gain<double>, fraction_d>);

// A proportion divided by a quantity is not defined
static_assert(!divisible<proportion::percent, electro::millivolts>);
static_assert(!divisible<proportion::percent, spatial::meters>);
static_assert(!divisible<proportion::percent, frequency::hertz>);
static_assert(!divisible<proportion::percent, thermo::delta_celsius>);
static_assert(!divisible<proportion::percent, data::bytes>);
static_assert(!divisible<fraction_d, electro::voltage<double>>);

// =============================================================================
// Runtime tests (Catch2 TEST_CASE)
// =============================================================================

TEST_CASE("scaling electrical quantities by a proportion", "[proportion][scaling]") {
    electro::millivolts supply{3300};
    proportion::percent duty{50};

    auto v = supply * duty;
    CHECK(v.count() == 165000);
    CHECK((duty * supply).count() == 165000);
    CHECK(electro::quantity_cast<electro::millivolts>(v) == 1650_mV);
    CHECK(electro::round<electro::volts>(v) == 2_V);

    auto full = supply / duty;
    CHECK(full.count() == 66);
    CHECK(electro::quantity_cast<electro::millivolts>(full) == 6600_mV);

    // Exact for integer reps, even at fine precisions
    CHECK(electro::microvolts(1_V * 3_ppm) == 3_uV);
}

TEST_CASE("scaling spatial quantities by a proportion", "[proportion][scaling]") {
    spatial::meters span{1000};
    CHECK(spatial::quantity_cast<spatial::millimeters>(span * 20_ppm) == 20_mm);
    CHECK(spatial::quantity_cast<spatial::millimeters>(20_ppm * span) == 20_mm);
    CHECK(spatial::quantity_cast<spatial::kilometers>(span / 50_pct) == 2_km);
}

TEST_CASE("scaling frequencies by a proportion", "[proportion][scaling]") {
    frequency::megahertz clock{2400};
    proportion::ppm tolerance{50};
    CHECK(clock * tolerance == 120_kHz);
    CHECK(tolerance * clock == 120_kHz);
    CHECK(frequency::frequency_cast<frequency::megahertz>(clock / 50_pct) == 4800_MHz);
}

TEST_CASE("scaling temperature deltas by a proportion", "[proportion][scaling]") {
    thermo::delta_celsius rise{10};
    CHECK(thermo::delta_cast<thermo::delta_celsius>(rise * 25_pct) == thermo::delta_celsius(2));
    CHECK(thermo::round<thermo::delta_celsius>(rise * 25_pct) == thermo::delta_celsius(2));
    CHECK(thermo::delta_cast<thermo::delta_decicelsius>(rise * 25_pct) == thermo::delta_decicelsius(25));
}

TEST_CASE("scaling data sizes and rates by a proportion", "[proportion][scaling]") {
    data::megabytes file{10};
    CHECK(data::quantity_cast<data::kilobytes>(file * 15_pct) == 1500_kB);
    CHECK(data::quantity_cast<data::kilobytes>(15_pct * file) == 1500_kB);

    data::megabits_per_second link{100};
    CHECK(data::quantity_cast<data::megabits_per_second>(link * 80_pct) == 80_Mbit_per_s);
}

TEST_CASE("scaling with floating-point reps", "[proportion][scaling][floating-point]") {
    electro::voltage<double> v{3.3};
    proportion::proportion<double> half{0.5};
    CHECK((v * half).count() == Catch::Approx(1.65));
    CHECK((half * v).count() == Catch::Approx(1.65));
    CHECK((v / half).count() == Catch::Approx(6.6));

    // An integer quantity and a floating-point proportion give a double rep
    auto mv = 3300_mV * proportion::proportion<double, std::centi>(12.5);
    CHECK(electro::quantity_cast<electro::voltage<double, std::milli>>(mv).count() == Catch::Approx(412.5));
    CHECK(electro::quantity_cast<electro::millivolts>(mv) == 412_mV);
}
