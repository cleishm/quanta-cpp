// Unit tests for thermo library
// Uses Catch2 test framework with compile-time static_asserts

#include <catch2/catch_test_macros.hpp>
#include <quanta/thermo>
#include <type_traits>

using namespace quanta::thermo;
using namespace quanta::thermo_literals;

// =============================================================================
// Delta construction and arithmetic
// =============================================================================

// Compile-time tests
static_assert(delta_celsius(10).count() == 10);
static_assert(delta_celsius(5) + delta_celsius(3) == delta_celsius(8));
static_assert(delta_celsius(5) - delta_celsius(3) == delta_celsius(2));
static_assert(delta_celsius(5) * 2 == delta_celsius(10));
static_assert(delta_celsius(10) / 2 == delta_celsius(5));

// Runtime tests
TEST_CASE("delta construction", "[thermo][delta]") {
    REQUIRE(delta_celsius(10).count() == 10);
    REQUIRE(delta_celsius::zero().count() == 0);
}

TEST_CASE("delta arithmetic", "[thermo][delta]") {
    REQUIRE((delta_celsius(5) + delta_celsius(3)).count() == 8);
    REQUIRE((delta_celsius(5) - delta_celsius(3)).count() == 2);
    REQUIRE((delta_celsius(5) * 2).count() == 10);
    REQUIRE((delta_celsius(10) / 2).count() == 5);
}

// =============================================================================
// Delta precision conversions
// =============================================================================

// Compile-time tests
static_assert(delta_celsius(1) == delta_millicelsius(1000));
static_assert(delta_fahrenheit(9) == delta_celsius(5));

// Runtime tests
TEST_CASE("delta precision conversion", "[thermo][delta]") {
    // milli to base (explicit, lossy - divides by 1000)
    delta_celsius dc = delta_cast<delta_celsius>(delta_millicelsius(1000));
    REQUIRE(dc.count() == 1);

    // base to milli (implicit, lossless - multiplies by 1000)
    delta_millicelsius dmc = delta_celsius(1);
    REQUIRE(dmc.count() == 1000);

    // fahrenheit to celsius delta
    REQUIRE(delta_fahrenheit(9) == delta_celsius(5));
}

// A lossy conversion is explicit, as for the other domains' quantities
static_assert(std::is_constructible_v<delta_celsius, delta_millicelsius>);
static_assert(!std::is_convertible_v<delta_millicelsius, delta_celsius>);
static_assert(std::is_convertible_v<delta_celsius, delta_millicelsius>);
static_assert(delta_celsius(delta_millicelsius(2500)) == delta_celsius(2));

TEST_CASE("delta explicit lossy conversion", "[thermo][delta]") {
    REQUIRE(delta_celsius(delta_millicelsius(1500)).count() == 1);        // truncates
    REQUIRE(delta_celsius(delta_millicelsius(-1500)).count() == -1);      // toward zero
    REQUIRE(delta_fahrenheit(delta_celsius(1)).count() == 1);             // 1.8 truncated
    REQUIRE(delta_decicelsius(delta_millifahrenheit(1000)).count() == 5); // 0.555... truncated
}

// =============================================================================
// Temperature construction
// =============================================================================

// Compile-time tests
static_assert(celsius(0).count() == 0);
static_assert(celsius(100).count() == 100);
static_assert(kelvin(273).count() == 273);
static_assert(millicelsius(1000).count() == 1000);
static_assert(millikelvin(273150).count() == 273150);

// Runtime tests
TEST_CASE("temperature construction", "[thermo][temperature]") {
    REQUIRE(celsius(0).count() == 0);
    REQUIRE(celsius(100).count() == 100);
    REQUIRE(kelvin(273).count() == 273);
    REQUIRE(fahrenheit(32).count() == 32);
}

// =============================================================================
// Same-scale precision conversions
// =============================================================================

// Compile-time tests
constexpr millicelsius mc_from_c = celsius(1);
static_assert(mc_from_c.count() == 1000);
constexpr millikelvin mk_from_k = kelvin(273);
static_assert(mk_from_k.count() == 273000);
static_assert(celsius(millicelsius(1500)).count() == 1);

// Runtime tests
TEST_CASE("celsius to millicelsius conversion", "[thermo][temperature]") {
    millicelsius mc = celsius(1); // implicit, lossless
    REQUIRE(mc.count() == 1000);

    millicelsius mc2 = celsius(25);
    REQUIRE(mc2.count() == 25000);
}

TEST_CASE("millicelsius to celsius conversion", "[thermo][temperature]") {
    // explicit required (lossy)
    celsius c = celsius(millicelsius(1500));
    REQUIRE(c.count() == 1); // truncates

    celsius c2 = celsius(millicelsius(1999));
    REQUIRE(c2.count() == 1); // still truncates
}

// =============================================================================
// Cross-scale conversions
// =============================================================================

// Compile-time tests
static_assert(kelvin(celsius(0)).count() == 273);
static_assert(kelvin(celsius(100)).count() == 373);
static_assert(celsius(kelvin(273)).count() == 0);

// Millikelvin precision (lossless)
constexpr millikelvin mk_from_0c = temperature_cast<millikelvin>(celsius(0));
static_assert(mk_from_0c.count() == 273150);
constexpr millikelvin mk_from_mc = millicelsius(0);
static_assert(mk_from_mc.count() == 273150);

// Fahrenheit conversions
static_assert(fahrenheit(32).count() == 32);
static_assert(celsius(fahrenheit(32)).count() == 0);
static_assert(celsius(fahrenheit(212)).count() == 100);
static_assert(celsius(fahrenheit(0)).count() == -17);

// Lossless conversions are implicit; lossy ones are explicit only
static_assert(std::is_convertible_v<celsius, millicelsius>);
static_assert(!std::is_convertible_v<millicelsius, celsius>);
static_assert(std::is_constructible_v<celsius, millicelsius>);
static_assert(std::is_convertible_v<kelvin, millikelvin>);
static_assert(std::is_convertible_v<celsius, millikelvin>);
static_assert(!std::is_convertible_v<kelvin, celsius>);
static_assert(std::is_constructible_v<celsius, kelvin>);

// Runtime tests
TEST_CASE("celsius to kelvin conversion", "[thermo][temperature]") {
    // 0°C = 273.15 K -> 273 (truncated)
    kelvin k0 = kelvin(celsius(0));
    REQUIRE(k0.count() == 273);

    // 100°C = 373.15 K -> 373 (truncated)
    kelvin k100 = kelvin(celsius(100));
    REQUIRE(k100.count() == 373);

    // -40°C = 233.15 K -> 233 (truncated)
    kelvin km40 = kelvin(celsius(-40));
    REQUIRE(km40.count() == 233);
}

TEST_CASE("kelvin to celsius conversion", "[thermo][temperature]") {
    // 273 K = -0.15°C -> 0 (truncated)
    celsius c273 = celsius(kelvin(273));
    REQUIRE(c273.count() == 0);

    // 373 K = 99.85°C -> 99 (truncated)
    celsius c373 = celsius(kelvin(373));
    REQUIRE(c373.count() == 99);

    // 0 K = -273.15°C -> -273 (truncated)
    celsius c0 = celsius(kelvin(0));
    REQUIRE(c0.count() == -273);
}

TEST_CASE("celsius to millikelvin conversion", "[thermo][temperature]") {
    // With millikelvin precision, conversion is exact
    millikelvin mk = temperature_cast<millikelvin>(celsius(0));
    REQUIRE(mk.count() == 273150);

    millikelvin mk100 = temperature_cast<millikelvin>(celsius(100));
    REQUIRE(mk100.count() == 373150);
}

TEST_CASE("millicelsius to millikelvin implicit conversion", "[thermo][temperature]") {
    // Same precision, different scale - should be lossless and implicit
    millikelvin mk = millicelsius(0);
    REQUIRE(mk.count() == 273150);

    millikelvin mk1000 = millicelsius(1000); // 1°C
    REQUIRE(mk1000.count() == 274150);
}

TEST_CASE("fahrenheit to celsius conversion", "[thermo][temperature]") {
    // 32°F = 0°C
    celsius c32 = celsius(fahrenheit(32));
    REQUIRE(c32.count() == 0);

    // 212°F = 100°C
    celsius c212 = celsius(fahrenheit(212));
    REQUIRE(c212.count() == 100);

    // 0°F = -17.78°C -> -17 (truncated)
    celsius c0 = celsius(fahrenheit(0));
    REQUIRE(c0.count() == -17);

    // -40°F = -40°C (the scales meet)
    celsius cm40 = celsius(fahrenheit(-40));
    REQUIRE(cm40.count() == -40);
}

TEST_CASE("millifahrenheit conversion", "[thermo][temperature]") {
    // 0.001°F precision: 1°F = 1000 counts
    REQUIRE(temperature_cast<millifahrenheit>(fahrenheit(1)).count() == 1000);
    millifahrenheit mf = fahrenheit(1); // finer precision, implicit
    REQUIRE(mf.count() == 1000);
    REQUIRE(temperature_cast<millifahrenheit>(fahrenheit(72)).count() == 72000);

    // Cross-scale: 0°C = 32°F = 32000 millifahrenheit
    REQUIRE(temperature_cast<millifahrenheit>(celsius(0)).count() == 32000);

    // 72.5°F in decifahrenheit -> millifahrenheit
    REQUIRE(millifahrenheit(decifahrenheit(725)).count() == 72500);

    // Deltas
    REQUIRE(delta_millifahrenheit(delta_fahrenheit(1)).count() == 1000);
    REQUIRE(delta_millifahrenheit(9000) == delta_celsius(5));
    REQUIRE(delta_millifahrenheit(9000) == delta_millicelsius(5000));
}

// =============================================================================
// Temperature arithmetic
// =============================================================================

// Compile-time tests
static_assert((celsius(20) + delta_celsius(5)).count() == 25);
static_assert((celsius(20) - delta_celsius(5)).count() == 15);
static_assert((delta_celsius(5) + celsius(20)).count() == 25);
static_assert(celsius(20) - celsius(10) == delta_celsius(10));
static_assert(difference(celsius(25), celsius(20)).count() == 5);

// Runtime tests
TEST_CASE("temperature arithmetic with delta", "[thermo][temperature]") {
    celsius t = celsius(20);

    REQUIRE((t + delta_celsius(5)).count() == 25);
    REQUIRE((t - delta_celsius(5)).count() == 15);
    REQUIRE((delta_celsius(5) + t).count() == 25);
}

// The difference of two temperatures is a delta, exactly as difference()
// returns; adding two temperatures is ill-formed.
template<typename A, typename B>
concept addable = requires(const A& a, const B& b) { a + b; };
static_assert(std::is_same_v<decltype(celsius(20) - celsius(10)), delta_celsius>);
static_assert(std::is_same_v<decltype(celsius(20) - millicelsius(10)), delta_millicelsius>);
static_assert(std::is_same_v<
              decltype(fahrenheit(20) - decifahrenheit(10)),
              decltype(difference(fahrenheit(20), decifahrenheit(10)))>);
static_assert(fahrenheit(20) - decifahrenheit(10) == delta_decifahrenheit(190));
static_assert(std::is_same_v<decltype(celsius(20) - celsius(10)), decltype(difference(celsius(20), celsius(10)))>);
static_assert(!addable<celsius, celsius>);
static_assert(!addable<celsius, millicelsius>);
static_assert(!addable<kelvin, kelvin>);
static_assert(!addable<fahrenheit, decifahrenheit>);
static_assert(addable<celsius, delta_celsius>);
static_assert(addable<delta_celsius, celsius>);

TEST_CASE("temperature minus temperature is a delta", "[thermo][temperature]") {
    REQUIRE(celsius(20) - celsius(10) == delta_celsius(10));
    REQUIRE(celsius(10) - celsius(20) == delta_celsius(-10));
    REQUIRE(kelvin(300) - kelvin(273) == delta_kelvin(27));
    REQUIRE(fahrenheit(212) - fahrenheit(32) == delta_fahrenheit(180));
    REQUIRE(fahrenheit(212) - fahrenheit(32) == delta_celsius(100));
    REQUIRE((celsius(25) - celsius(20)) == difference(celsius(25), celsius(20)));
    REQUIRE((millicelsius(25500) - celsius(20)) == difference(millicelsius(25500), celsius(20)));

    // temperature - delta and temperature + delta are still temperatures
    REQUIRE(celsius(20) - delta_celsius(5) == celsius(15));
    REQUIRE(celsius(20) + (celsius(25) - celsius(20)) == celsius(25));
}

TEST_CASE("temperature difference", "[thermo][temperature]") {
    delta_celsius d = difference(celsius(25), celsius(20));
    REQUIRE(d.count() == 5);

    delta_celsius d2 = difference(celsius(20), celsius(25));
    REQUIRE(d2.count() == -5);
}

TEST_CASE("temperature increment/decrement", "[thermo][temperature]") {
    celsius t(10);

    ++t;
    REQUIRE(t.count() == 11);

    --t;
    REQUIRE(t.count() == 10);

    t++;
    REQUIRE(t.count() == 11);

    t--;
    REQUIRE(t.count() == 10);
}

TEST_CASE("temperature compound assignment", "[thermo][temperature]") {
    celsius t(10);

    t += delta_celsius(5);
    REQUIRE(t.count() == 15);

    t -= delta_celsius(3);
    REQUIRE(t.count() == 12);
}

// =============================================================================
// Mixed-precision temperature arithmetic and comparison
// =============================================================================

// Each operand is converted to the common precision before combining.
static_assert(celsius(20) + delta_millicelsius(500) == millicelsius(20500));
static_assert(celsius(20) == millicelsius(20000));
static_assert(millicelsius(20000) < celsius(25));
static_assert(difference(celsius(25), millicelsius(20000)) == delta_millicelsius(5000));

TEST_CASE("mixed-precision temperature and delta arithmetic", "[thermo][temperature]") {
    STATIC_REQUIRE(std::is_same_v<decltype(celsius(20) + delta_millicelsius(500)), millicelsius>);
    REQUIRE((celsius(20) + delta_millicelsius(500)).count() == 20500);
    REQUIRE((delta_millicelsius(500) + celsius(20)).count() == 20500);
    REQUIRE((millicelsius(20000) + delta_celsius(1)).count() == 21000);
    REQUIRE((delta_celsius(1) + millicelsius(20000)).count() == 21000);
    REQUIRE((celsius(20) - delta_millicelsius(500)).count() == 19500);
    REQUIRE((millicelsius(20500) - delta_celsius(1)).count() == 19500);

    // Fahrenheit and Celsius-sized steps meet at a common 1/9 degree precision.
    REQUIRE(fahrenheit(32) + delta_celsius(5) == fahrenheit(41));
    REQUIRE((fahrenheit(70) + delta_decifahrenheit(5)).count() == 705);

    // A floating-point temperature combined with an integral delta.
    using celsius_d = temperature<celsius_scale, delta<double>>;
    REQUIRE((celsius_d(20.0) + delta_millicelsius(500)).count() == 20500.0);
}

TEST_CASE("mixed-precision temperature arithmetic", "[thermo][temperature]") {
    REQUIRE((celsius(25) - millicelsius(20000)).count() == 5000);
    REQUIRE((millicelsius(20000) - celsius(25)).count() == -5000);
}

TEST_CASE("mixed-precision temperature difference", "[thermo][temperature]") {
    REQUIRE(difference(celsius(25), millicelsius(20000)).count() == 5000);
    REQUIRE(difference(millicelsius(20000), celsius(25)).count() == -5000);
    REQUIRE(difference(decifahrenheit(725), fahrenheit(70)).count() == 25);
}

TEST_CASE("mixed-precision temperature comparison", "[thermo][temperature]") {
    REQUIRE(celsius(20) == millicelsius(20000));
    REQUIRE(millicelsius(20000) == celsius(20));
    REQUIRE(celsius(20) != millicelsius(20001));
    REQUIRE(millicelsius(20001) != celsius(20));
    REQUIRE(fahrenheit(32) == decifahrenheit(320));

    REQUIRE_FALSE(celsius(25) < millicelsius(20000));
    REQUIRE(millicelsius(20000) < celsius(25));
    REQUIRE(celsius(25) > millicelsius(20000));
    REQUIRE_FALSE(millicelsius(20000) > celsius(25));
    REQUIRE(celsius(20) <= millicelsius(20000));
    REQUIRE(millicelsius(19999) <= celsius(20));
    REQUIRE(celsius(20) >= millicelsius(20000));
    REQUIRE(millicelsius(20001) >= celsius(20));
    REQUIRE((celsius(20) <=> millicelsius(20000)) == std::strong_ordering::equal);
}

// =============================================================================
// Comparison operators
// =============================================================================

// Compile-time tests
static_assert(celsius(20) == celsius(20));
static_assert(celsius(20) != celsius(21));
static_assert(celsius(20) < celsius(21));
static_assert(celsius(21) > celsius(20));
static_assert(celsius(20) <= celsius(21));
static_assert(celsius(21) >= celsius(20));
static_assert(millicelsius(celsius(1)) == millicelsius(1000));

// Runtime tests
TEST_CASE("temperature comparison", "[thermo][temperature]") {
    REQUIRE(celsius(20) == celsius(20));
    REQUIRE(celsius(20) != celsius(21));
    REQUIRE(celsius(20) < celsius(21));
    REQUIRE(celsius(21) > celsius(20));
    REQUIRE(celsius(20) <= celsius(20));
    REQUIRE(celsius(20) <= celsius(21));
    REQUIRE(celsius(21) >= celsius(21));
    REQUIRE(celsius(21) >= celsius(20));
}

// =============================================================================
// Literals
// =============================================================================

// Compile-time tests
static_assert((20_c).count() == 20);
static_assert((1000_mc).count() == 1000);
static_assert((273_k).count() == 273);
static_assert((273000_mk).count() == 273000);
static_assert((32_f).count() == 32);
// ASCII delta literals: always defined, same types as the _Δ forms.
static_assert(std::is_same_v<decltype(5_delta_c), delta_celsius>);
static_assert(std::is_same_v<decltype(5000_delta_mc), delta_millicelsius>);
static_assert(std::is_same_v<decltype(5_delta_k), delta_kelvin>);
static_assert(std::is_same_v<decltype(5000_delta_mk), delta_millikelvin>);
static_assert(std::is_same_v<decltype(9_delta_f), delta_fahrenheit>);
static_assert(std::is_same_v<decltype(9000_delta_mf), delta_millifahrenheit>);
static_assert((5_delta_c).count() == 5);
static_assert((5000_delta_mc).count() == 5000);
static_assert((5_delta_k).count() == 5);
static_assert((5000_delta_mk).count() == 5000);
static_assert((9_delta_f).count() == 9);
static_assert((9000_delta_mf).count() == 9000);
static_assert(9_delta_f == delta_celsius(5));
#if QUANTA_HAS_UTF8_LITERALS
static_assert((5_Δc).count() == 5);
static_assert((1000_Δmc).count() == 1000);
static_assert((5_Δk).count() == 5);
static_assert((1000_Δmk).count() == 1000);
static_assert((9_Δf).count() == 9);
static_assert((9000_Δmf).count() == 9000);
#endif

// Runtime tests
TEST_CASE("temperature literals", "[thermo][literals]") {
    REQUIRE((20_c).count() == 20);
    REQUIRE((1000_mc).count() == 1000);
    REQUIRE((273_k).count() == 273);
    REQUIRE((273000_mk).count() == 273000);
    REQUIRE((32_f).count() == 32);
}

TEST_CASE("literals follow C++ integer-literal rules", "[thermo][literals]") {
    REQUIRE((22'534_mc).count() == 22534);
    REQUIRE((0x10_c).count() == 16);
    REQUIRE((0b1010_dk).count() == 10);
    REQUIRE((010_f).count() == 8);
    REQUIRE((1'000_delta_mc).count() == 1000);
    REQUIRE((0x1F_delta_c).count() == 31);
    REQUIRE((-0x10_c).count() == -16);
}

TEST_CASE("delta literals", "[thermo][literals]") {
    // ASCII spellings, always available.
    REQUIRE((5_delta_c).count() == 5);
    REQUIRE((5000_delta_mc).count() == 5000);
    REQUIRE((5_delta_k).count() == 5);
    REQUIRE((5000_delta_mk).count() == 5000);
    REQUIRE((9_delta_f).count() == 9);
    REQUIRE((9000_delta_mf).count() == 9000);
#if QUANTA_HAS_UTF8_LITERALS
    REQUIRE((5_Δc).count() == 5);
    REQUIRE((1000_Δmc).count() == 1000);
    REQUIRE((5_Δk).count() == 5);
    REQUIRE((1000_Δmk).count() == 1000);
    REQUIRE((9_Δf).count() == 9);
    REQUIRE((9000_Δmf).count() == 9000);
    REQUIRE(5_Δc == 5_delta_c);
    REQUIRE(1000_Δmc == 1000_delta_mc);
    REQUIRE(5_Δk == 5_delta_k);
    REQUIRE(1000_Δmk == 1000_delta_mk);
    REQUIRE(9_Δf == 9_delta_f);
    REQUIRE(9000_Δmf == 9000_delta_mf);
    static_assert(std::is_same_v<decltype(5_Δc), decltype(5_delta_c)>);
    static_assert(std::is_same_v<decltype(9000_Δmf), decltype(9000_delta_mf)>);
#endif
}

// =============================================================================
// Negation (enables negative literals, e.g. -123_dc == -(123_dc))
// =============================================================================

template<typename T>
concept has_unary_minus = requires(T t) { -t; };

// Available on relative scales (zero above absolute zero)...
static_assert(has_unary_minus<celsius>);
static_assert(has_unary_minus<decicelsius>);
static_assert(has_unary_minus<fahrenheit>);
// ...but not on the absolute scale, where a negated value is below absolute zero.
static_assert(!has_unary_minus<kelvin>);
static_assert(!has_unary_minus<decikelvin>);
static_assert(!has_unary_minus<millikelvin>);

// Compile-time tests
static_assert((-123_dc).count() == -123);
static_assert((-40_f).count() == -40);
static_assert((-20_c).count() == -20);
static_assert(-(-5_c) == 5_c);

// Runtime tests
TEST_CASE("temperature negation", "[thermo][temperature]") {
    REQUIRE((-123_dc).count() == -123);
    REQUIRE((-40_f).count() == -40);
    REQUIRE((-20_c).count() == -20);
    REQUIRE((-(-5_c)).count() == 5);
}

// =============================================================================
// String formatting
// =============================================================================

// Runtime tests
TEST_CASE("to_string", "[thermo][string]") {
    REQUIRE(to_string(celsius(20)) == "20°C");
    REQUIRE(to_string(kelvin(273)) == "273K");
    REQUIRE(to_string(fahrenheit(32)) == "32°F");
    REQUIRE(to_string(millicelsius(1000)) == "1000m°C");
    REQUIRE(to_string(millikelvin(273150)) == "273150mK");
    REQUIRE(to_string(decicelsius(200)) == "200d°C");
    REQUIRE(to_string(decikelvin(2955)) == "2955dK");
    REQUIRE(to_string(decifahrenheit(725)) == "725d°F");
    REQUIRE(to_string(millifahrenheit(72500)) == "72500m°F");
    REQUIRE(to_string(delta_celsius(5)) == "5Δ°C");
    REQUIRE(to_string(delta_millifahrenheit(36000)) == "36000Δm°F");
    REQUIRE(to_string(millicelsius(-1250)) == "-1250m°C");
}

TEST_CASE("to_string floating-point counts", "[thermo][string][real]") {
    using celsius_d = temperature<celsius_scale, delta<double>>;
    using delta_millicelsius_d = delta<double, std::milli>;
    // Shortest round-trip fixed notation, never std::to_string's six decimals.
    REQUIRE(to_string(celsius_d(22.5)) == "22.5°C");
    REQUIRE(to_string(celsius_d(1000000.0)) == "1000000°C");
    REQUIRE(to_string(celsius_d(-0.125)) == "-0.125°C");
    REQUIRE(to_string(delta_millicelsius_d(1500.25)) == "1500.25Δm°C");
}

TEST_CASE("to_string base_unit", "[thermo][string][marker]") {
    REQUIRE(to_string(millicelsius(22534), base_unit) == "22.534°C");
    REQUIRE(to_string(celsius(22), base_unit) == "22°C");
    REQUIRE(to_string(decikelvin(2955), base_unit) == "295.5K");
    REQUIRE(to_string(millikelvin(24), base_unit) == "0.024K");
    REQUIRE(to_string(millifahrenheit(-40250), base_unit) == "-40.25°F");
    REQUIRE(to_string(decifahrenheit(725), base_unit) == "72.5°F");
    REQUIRE(to_string(millicelsius(0), base_unit) == "0°C");
    REQUIRE(to_string(delta_millicelsius(1500), base_unit) == "1.5Δ°C");
    REQUIRE(to_string(delta_millikelvin(-1500), base_unit) == "-1.5Δ°C");
    REQUIRE(to_string(delta_millifahrenheit(9000), base_unit) == "9Δ°F");
}

TEST_CASE("to_string scaled", "[thermo][string][marker]") {
    // Temperatures are not rescaled: the scaled form is the type's own unit.
    REQUIRE(to_string(millicelsius(22534), scaled) == "22534m°C");
    REQUIRE(to_string(celsius(22), scaled) == "22°C");
    REQUIRE(to_string(decikelvin(2955), scaled) == "2955dK");
    REQUIRE(to_string(millifahrenheit(-40250), scaled) == "-40250m°F");
    REQUIRE(to_string(millicelsius(0), scaled) == "0m°C");
    REQUIRE(to_string(delta_millicelsius(1500), scaled) == "1500Δm°C");
    REQUIRE(to_string(delta_decifahrenheit(-95), scaled) == "-95Δd°F");
}

#if CONFIG_QUANTA_STD_FORMAT
TEST_CASE("std::format delta", "[thermo][string][delta]") {
    REQUIRE(std::format("{}", delta_celsius(20)) == "20Δ°C");
    REQUIRE(std::format("{}", delta_decicelsius(200)) == "200Δd°C");
    REQUIRE(std::format("{}", delta_millicelsius(20000)) == "20000Δm°C");
    REQUIRE(std::format("{}", delta_fahrenheit(36)) == "36Δ°F");
    REQUIRE(std::format("{}", delta_decifahrenheit(360)) == "360Δd°F");
    REQUIRE(std::format("{}", delta_millifahrenheit(36000)) == "36000Δm°F");
}

TEST_CASE("std::format temperature", "[thermo][string][temperature]") {
    // Empty spec prints the exact stored count with a precision-qualified unit.
    REQUIRE(std::format("{}", celsius(20)) == "20°C");
    REQUIRE(std::format("{}", decicelsius(200)) == "200d°C");
    REQUIRE(std::format("{}", millicelsius(22500)) == "22500m°C");
    REQUIRE(std::format("{}", kelvin(273)) == "273K");
    REQUIRE(std::format("{}", decikelvin(2955)) == "2955dK");
    REQUIRE(std::format("{}", millikelvin(273150)) == "273150mK");
    REQUIRE(std::format("{}", fahrenheit(72)) == "72°F");
    REQUIRE(std::format("{}", decifahrenheit(725)) == "725d°F");
    REQUIRE(std::format("{}", millifahrenheit(72500)) == "72500m°F");
}

TEST_CASE("std::format with spec renders in the type's own unit", "[thermo][string]") {
    // A spec changes only the number formatting, never the unit: the count is
    // rendered as a double with the same unit as "{}".
    REQUIRE(std::format("{:.1f}", millicelsius(22534)) == "22534.0m°C");
    REQUIRE(std::format("{:g}", millicelsius(22500)) == "22500m°C");
    REQUIRE(std::format("{:.2f}", decicelsius(225)) == "225.00d°C");
    REQUIRE(std::format("{:.1f}", celsius(22)) == "22.0°C");
    REQUIRE(std::format("{:.2f}", millikelvin(295650)) == "295650.00mK");
    REQUIRE(std::format("{:.1f}", decikelvin(2955)) == "2955.0dK");
    REQUIRE(std::format("{:.1f}", fahrenheit(72)) == "72.0°F");
    REQUIRE(std::format("{:.1f}", decifahrenheit(725)) == "725.0d°F");
    REQUIRE(std::format("{:.2f}", millifahrenheit(72530)) == "72530.00m°F");
    REQUIRE(std::format("{:.1f}", millicelsius(-1250)) == "-1250.0m°C");

    // Width and alignment pass through to the numeric part.
    REQUIRE(std::format("{:9.1f}", millicelsius(22500)) == "  22500.0m°C");
    REQUIRE(std::format("{:*<8}", celsius(22)) == "22******°C");

    // Deltas render the same way, keeping the Δ marker.
    REQUIRE(std::format("{:.1f}", delta_millicelsius(1500)) == "1500.0Δm°C");
    REQUIRE(std::format("{:.1f}", delta_decicelsius(15)) == "15.0Δd°C");
    REQUIRE(std::format("{:.1f}", delta_millifahrenheit(9000)) == "9000.0Δm°F");
    REQUIRE(std::format("{:.1f}", delta_fahrenheit(36)) == "36.0Δ°F");
}

TEST_CASE("std::format base-unit marker", "[thermo][string][marker]") {
    // 'b' renders in the unprefixed degree of the same scale (no scale change).
    REQUIRE(std::format("{:b.1f}", millicelsius(22534)) == "22.5°C");
    REQUIRE(std::format("{:b.2f}", decicelsius(225)) == "22.50°C");
    REQUIRE(std::format("{:b.1f}", celsius(22)) == "22.0°C");
    REQUIRE(std::format("{:b.2f}", millikelvin(295650)) == "295.65K");
    REQUIRE(std::format("{:b.1f}", decikelvin(2955)) == "295.5K");
    REQUIRE(std::format("{:b.1f}", decifahrenheit(725)) == "72.5°F");
    REQUIRE(std::format("{:b.2f}", millifahrenheit(72530)) == "72.53°F");
    REQUIRE(std::format("{:b.1f}", delta_millicelsius(1500)) == "1.5Δ°C");
    REQUIRE(std::format("{:b.1f}", delta_millifahrenheit(9000)) == "9.0Δ°F");

    // With no trailing spec: shortest round-trip fixed notation.
    REQUIRE(std::format("{:b}", millicelsius(22534)) == "22.534°C");
    REQUIRE(std::format("{:b}", celsius(22)) == "22°C");
    REQUIRE(std::format("{:b}", millikelvin(24)) == "0.024K");
    REQUIRE(std::format("{:b}", decifahrenheit(725)) == "72.5°F");
    REQUIRE(std::format("{:b}", delta_millicelsius(1500)) == "1.5Δ°C");

    // Negative values and zero.
    REQUIRE(std::format("{:b}", millicelsius(-1250)) == "-1.25°C");
    REQUIRE(std::format("{:b.1f}", millifahrenheit(-40250)) == "-40.2°F");
    REQUIRE(std::format("{:b}", delta_millicelsius(-1500)) == "-1.5Δ°C");
    REQUIRE(std::format("{:b}", millicelsius(0)) == "0°C");
    REQUIRE(std::format("{:b.1f}", delta_decicelsius(0)) == "0.0Δ°C");

    // Width and fill after the marker apply to the numeric part.
    REQUIRE(std::format("{:b8.2f}", millicelsius(22534)) == "   22.53°C");
    REQUIRE(std::format("{:b*>8.2f}", millicelsius(22534)) == "***22.53°C");
    REQUIRE(std::format("{:b<6}", celsius(22)) == "22    °C");
}

TEST_CASE("std::format scaled marker", "[thermo][string][marker]") {
    // 's' keeps the type's own unit: temperatures are not rescaled.
    REQUIRE(std::format("{:s.1f}", millicelsius(22534)) == "22534.0m°C");
    REQUIRE(std::format("{:s.1f}", decikelvin(2955)) == "2955.0dK");
    REQUIRE(std::format("{:s.1f}", fahrenheit(72)) == "72.0°F");
    REQUIRE(std::format("{:s.1f}", delta_millicelsius(1500)) == "1500.0Δm°C");

    // With no trailing spec: the count as a double in shortest fixed notation.
    REQUIRE(std::format("{:s}", millicelsius(22534)) == "22534m°C");
    REQUIRE(std::format("{:s}", millifahrenheit(72561)) == "72561m°F");
    REQUIRE(std::format("{:s}", delta_millikelvin(1500)) == "1500Δm°C");

    // Negative values and zero.
    REQUIRE(std::format("{:s}", millicelsius(-1250)) == "-1250m°C");
    REQUIRE(std::format("{:s.1f}", delta_decifahrenheit(-95)) == "-95.0Δd°F");
    REQUIRE(std::format("{:s}", millicelsius(0)) == "0m°C");
    REQUIRE(std::format("{:s}", celsius(0)) == "0°C");

    // Width and fill after the marker apply to the numeric part.
    REQUIRE(std::format("{:s>8}", celsius(22)) == "      22°C");
    REQUIRE(std::format("{:s0>8.1f}", millicelsius(22534)) == "022534.0m°C");
}

TEST_CASE("std::format of a precision with no SI prefix", "[thermo][string][marker]") {
    // No unit label of its own: any spec renders in the base unit (as before).
    using centicelsius = temperature<celsius_scale, delta<int64_t, std::centi>>;
    REQUIRE(std::format("{:.2f}", centicelsius(2250)) == "22.50°C");
    REQUIRE(std::format("{:b}", centicelsius(2250)) == "22.5°C");
    REQUIRE(std::format("{:s}", centicelsius(2250)) == "22.5°C");
    REQUIRE(std::format("{:s.1f}", centicelsius(-2250)) == "-22.5°C");
    REQUIRE(to_string(centicelsius(2250), base_unit) == "22.5°C");
    REQUIRE(to_string(centicelsius(2250), scaled) == "22.5°C");
}

TEST_CASE("std::format with no spec agrees with to_string", "[thermo][string]") {
    using celsius_d = temperature<celsius_scale, delta<double>>;
    using millikelvin_d = temperature<kelvin_scale, delta<double, std::milli>>;
    using delta_millicelsius_d = delta<double, std::milli>;

    const auto agree = [](const auto& q) {
        REQUIRE(std::format("{}", q) == to_string(q));
        REQUIRE(std::format("{:b}", q) == to_string(q, base_unit));
        REQUIRE(std::format("{:s}", q) == to_string(q, scaled));
    };
    agree(celsius(22));
    agree(millicelsius(22534));
    agree(millicelsius(-1250));
    agree(decikelvin(2955));
    agree(millifahrenheit(72561));
    agree(delta_millicelsius(1500));
    agree(delta_decifahrenheit(-95));
    agree(celsius_d(22.5));
    agree(celsius_d(1000000.0));
    agree(celsius_d(0.000001));
    agree(millikelvin_d(24.25));
    agree(delta_millicelsius_d(-0.5));

    // Floating-point counts use shortest round-trip fixed notation, never
    // scientific notation.
    REQUIRE(std::format("{}", celsius_d(1000000.0)) == "1000000°C");
    REQUIRE(std::format("{}", celsius_d(0.000001)) == "0.000001°C");
    REQUIRE(std::format("{}", celsius_d(22.5)) == "22.5°C");
    REQUIRE(std::format("{}", millikelvin_d(24.25)) == "24.25mK");
    REQUIRE(std::format("{:b}", millikelvin_d(24.25)) == "0.02425K");
    REQUIRE(std::format("{}", delta_millicelsius_d(-0.5)) == "-0.5Δm°C");

    // An explicit spec is unaffected.
    REQUIRE(std::format("{:.3e}", celsius_d(1000000.0)) == "1.000e+06°C");
    REQUIRE(std::format("{:b.2f}", millikelvin_d(24.25)) == "0.02K");
}

#endif

TEST_CASE("floating-point representation conversion", "[thermo][real]") {
    // Inexact reps accept lossy conversions implicitly; count() reads in
    // precision steps, so with the default precision that is scale degrees.
    using celsius_d = temperature<celsius_scale, delta<double>>;
    REQUIRE(celsius_d(millicelsius(22500)).count() == 22.5); // implicit
    REQUIRE(temperature_cast<celsius_d>(millicelsius(22500)).count() == 22.5);
    REQUIRE(temperature_cast<celsius_d>(decicelsius(225)).count() == 22.5);
}

TEST_CASE("large integral casts do not overflow early", "[thermo][conversion]") {
    // delta_fahrenheit -> delta_millicelsius scales by 5000/9, and fahrenheit ->
    // millikelvin also adds an offset in ninths. These counts fit in the result
    // but overflowed int64 in the intermediate product.
    REQUIRE(
        delta_cast<delta_millicelsius>(delta_fahrenheit(9'000'000'000'000'000)).count() == 5'000'000'000'000'000'000
    );
    REQUIRE(
        delta_cast<delta_millicelsius>(delta_fahrenheit(-9'000'000'000'000'000)).count() == -5'000'000'000'000'000'000
    );
    REQUIRE(temperature_cast<millikelvin>(fahrenheit(1'000'000'000'000'000)).count() == 555'555'555'555'810'927);
#ifdef __SIZEOF_INT128__
    // Without a 128-bit intermediate these still overflow.
    REQUIRE(
        delta_cast<delta_millicelsius>(delta_fahrenheit(2'000'000'000'000'000)).count() == 1'111'111'111'111'111'111
    );
    REQUIRE(
        delta_cast<delta_millicelsius>(delta_fahrenheit(16'000'000'000'000'000)).count() == 8'888'888'888'888'888'888
    );
    REQUIRE(
        delta_cast<delta_millicelsius>(delta_fahrenheit(-16'000'000'000'000'000)).count() == -8'888'888'888'888'888'888
    );
    REQUIRE(temperature_cast<millikelvin>(fahrenheit(16'000'000'000'000'000)).count() == 8'888'888'888'889'144'261);
    REQUIRE(temperature_cast<millikelvin>(fahrenheit(-16'000'000'000'000'000)).count() == -8'888'888'888'888'633'516);
#endif
}

TEST_CASE("integral casts between signed and unsigned reps", "[thermo][conversion]") {
    // -1/3 is -0.67 halves, truncated to 0: a signed count stays negative in
    // the intermediate even when the target rep is unsigned.
    REQUIRE(delta_cast<delta<uint64_t, std::ratio<1, 2>>>(delta<int8_t, std::ratio<1, 3>>(-1)).count() == 0);
    REQUIRE(delta_cast<delta<uint32_t, std::kilo>>(delta<int64_t>(-999)).count() == 0);
    STATIC_REQUIRE(delta_cast<delta<uint64_t, std::ratio<1, 2>>>(delta<int8_t, std::ratio<1, 3>>(-1)).count() == 0);

    // Two unsigned reps compute unsigned, so counts above INTMAX_MAX convert
    // exactly with or without a 128-bit intermediate.
    REQUIRE(
        delta_cast<delta<uint64_t, std::kilo>>(delta<uint64_t>(18'446'744'073'709'551'000U)).count() ==
        18'446'744'073'709'551U
    );
    REQUIRE(
        delta_cast<delta<uint64_t>>(delta<uint64_t, std::kilo>(18'446'744'073'709'551U)).count() ==
        18'446'744'073'709'551'000U
    );
}

TEST_CASE("casts across scales from unsigned reps keep the offset's sign", "[thermo][conversion]") {
    using kelvin_u64 = temperature<kelvin_scale, delta<uint64_t>>;
    using celsius_u64 = temperature<celsius_scale, delta<uint64_t>>;
    using fahrenheit_u64 = temperature<fahrenheit_scale, delta<uint64_t, std::ratio<5, 9>>>;
    using millikelvin_u32 = temperature<kelvin_scale, delta<uint32_t, std::milli>>;

    // The offset from Kelvin to Celsius is negative; computing in the reps'
    // unsigned common type wrapped it.
    REQUIRE(temperature_cast<celsius>(kelvin_u64(200)).count() == -73);
    REQUIRE(temperature_cast<celsius>(kelvin_u64(0)).count() == -273);
    REQUIRE(temperature_cast<celsius>(fahrenheit_u64(0)).count() == -17);
    REQUIRE(temperature_cast<millicelsius>(millikelvin_u32(200'000)).count() == -73'150);
    REQUIRE(temperature_cast<fahrenheit>(kelvin_u64(0)).count() == -459);

    // A signed source into an unsigned target converts the signed result.
    REQUIRE(temperature_cast<kelvin_u64>(celsius(-200)).count() == 73);
    REQUIRE(temperature_cast<celsius_u64>(fahrenheit(212)).count() == 100);

    // Results that are not negative are unchanged.
    REQUIRE(temperature_cast<celsius_u64>(kelvin_u64(1000)).count() == 726);
    REQUIRE(temperature_cast<kelvin_u64>(celsius_u64(20)).count() == 293);
    REQUIRE(temperature_cast<fahrenheit_u64>(celsius_u64(100)).count() == 212);
    REQUIRE(temperature_cast<kelvin_u64>(fahrenheit_u64(212)).count() == 373);
#ifdef __SIZEOF_INT128__
    REQUIRE(
        temperature_cast<celsius_u64>(kelvin_u64(18'446'744'073'709'551'615U)).count() == 18'446'744'073'709'551'341U
    );
#endif

    static_assert(temperature_cast<celsius>(kelvin_u64(200)).count() == -73);
}

// =============================================================================
// Min/max values
// =============================================================================

// Compile-time tests
static_assert(celsius::min().count() < 0);
static_assert(celsius::max().count() > 0);

// =============================================================================
// Rounding functions
// =============================================================================

// Compile-time tests for ceil
static_assert(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(12345)).count() == 13);
static_assert(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(12000)).count() == 12);
static_assert(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
static_assert(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(0)).count() == 0);

// Compile-time tests for floor
static_assert(quanta::thermo::floor<delta_celsius>(delta_millicelsius(12345)).count() == 12);
static_assert(quanta::thermo::floor<delta_celsius>(delta_millicelsius(12000)).count() == 12);
static_assert(quanta::thermo::floor<delta_celsius>(delta_millicelsius(-12345)).count() == -13);
static_assert(quanta::thermo::floor<delta_celsius>(delta_millicelsius(0)).count() == 0);

// Compile-time tests for trunc
static_assert(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(12345)).count() == 12);
static_assert(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(12000)).count() == 12);
static_assert(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
static_assert(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(0)).count() == 0);

// Compile-time tests for round
static_assert(quanta::thermo::round<delta_celsius>(delta_millicelsius(12345)).count() == 12);
static_assert(quanta::thermo::round<delta_celsius>(delta_millicelsius(12500)).count() == 12); // tie to even
static_assert(quanta::thermo::round<delta_celsius>(delta_millicelsius(12600)).count() == 13);
static_assert(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
static_assert(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12500)).count() == -12); // tie to even
static_assert(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12600)).count() == -13);

// Runtime tests
TEST_CASE("delta ceil", "[thermo][delta][rounding]") {
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(12345)).count() == 13);
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(12000)).count() == 12);
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(12001)).count() == 13);
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(-12000)).count() == -12);
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(-12001)).count() == -12);
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(quanta::thermo::ceil<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

TEST_CASE("delta floor", "[thermo][delta][rounding]") {
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(12345)).count() == 12);
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(12000)).count() == 12);
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(12999)).count() == 12);
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(-12345)).count() == -13);
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(-12000)).count() == -12);
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(-12001)).count() == -13);
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(quanta::thermo::floor<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

TEST_CASE("delta trunc", "[thermo][delta][rounding]") {
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(12345)).count() == 12);
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(12000)).count() == 12);
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(12999)).count() == 12);
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(-12000)).count() == -12);
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(-12999)).count() == -12);
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(quanta::thermo::trunc<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(quanta::thermo::trunc<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

TEST_CASE("delta round", "[thermo][delta][rounding]") {
    // Basic rounding
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(12345)).count() == 12);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(12499)).count() == 12);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(12500)).count() == 12); // Tie: round to even
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(12501)).count() == 13);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(12999)).count() == 13);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(12000)).count() == 12);

    // Negative values
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12345)).count() == -12);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12499)).count() == -12);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12500)).count() == -12); // Tie: round to even
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12501)).count() == -13);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12999)).count() == -13);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(-12000)).count() == -12);

    // Ties round to the even neighbour, as std::chrono::round does
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_decicelsius(25)).count() == 2);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_decicelsius(35)).count() == 4);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_decicelsius(-25)).count() == -2);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_decicelsius(-35)).count() == -4);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_decicelsius(5)).count() == 0);
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_decicelsius(-5)).count() == 0);
    REQUIRE(quanta::thermo::round<delta_fahrenheit>(delta_decifahrenheit(25)).count() == 2);

    // Zero
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_millicelsius(0)).count() == 0);

    // Same precision should return identical value
    REQUIRE(quanta::thermo::round<delta_celsius>(delta_celsius(42)).count() == 42);

    // Finer precision should return lossless conversion
    REQUIRE(quanta::thermo::round<delta_millicelsius>(delta_celsius(5)).count() == 5000);
}

static_assert(abs(delta_celsius(-5)) == delta_celsius(5));
static_assert(std::is_same_v<decltype(abs(delta_decifahrenheit(-5))), delta_decifahrenheit>);

TEST_CASE("delta abs", "[thermo][delta]") {
    REQUIRE(abs(delta_millicelsius(-1500)) == delta_millicelsius(1500));
    REQUIRE(abs(delta_millicelsius(1500)) == delta_millicelsius(1500));
    REQUIRE(abs(delta_celsius(0)) == delta_celsius(0));
    REQUIRE(abs(delta_fahrenheit(-9)).count() == 9);
    REQUIRE(abs(delta<double>(-2.5)).count() == 2.5);
    REQUIRE(abs(-5_delta_c) == 5_delta_c);
}

TEST_CASE("delta rounding from a floating-point rep", "[thermo][delta][rounding]") {
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta<double>(1.5)) == delta_celsius(1));
    REQUIRE(quanta::thermo::floor<delta_celsius>(delta<double>(-1.5)) == delta_celsius(-2));
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta<double>(1.2)) == delta_celsius(2));
    REQUIRE(quanta::thermo::ceil<delta_celsius>(delta<double>(-1.2)) == delta_celsius(-1));
    REQUIRE(quanta::thermo::round<delta_celsius>(delta<double>(1.7)) == delta_celsius(2));
    REQUIRE(quanta::thermo::round<delta_celsius>(delta<double>(2.5)) == delta_celsius(2)); // tie to even
}

TEST_CASE("delta rounding to a floating-point target is a plain cast", "[thermo][delta][rounding]") {
    REQUIRE(quanta::thermo::floor<delta<double>>(delta_millicelsius(1500)).count() == 1.5);
    REQUIRE(quanta::thermo::ceil<delta<double>>(delta_millicelsius(1500)).count() == 1.5);
    REQUIRE(quanta::thermo::round<delta<double>>(delta_millicelsius(1500)).count() == 1.5);

    // 2.007 and 1.001 are inexact in double: comparing them back against the
    // source must not step the result by a whole degree.
    REQUIRE(quanta::thermo::floor<delta<double>>(delta_millicelsius(2007)).count() == 2007 / 1000.0);
    REQUIRE(quanta::thermo::ceil<delta<double>>(delta_millicelsius(1001)).count() == 1001 / 1000.0);
}

// =============================================================================
// Result types (pinned: they depend on the exact std::ratio spelling)
// =============================================================================

namespace pin {
using U = delta<int, std::ratio<10, 20>>;
static_assert(std::is_same_v<decltype(+U(1)), delta<int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(-U(1)), delta<int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) * 2L), delta<long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(2L * U(1)), delta<long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(U(1) / 2L), delta<long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(U(1) % 2L), delta<long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(U(1) / U(1)), int>);
static_assert(std::is_same_v<std::common_type_t<U, U>, delta<int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) + U(1)), delta<int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) - U(1)), delta<int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) % U(1)), delta<int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<std::common_type_t<U, delta<long, std::ratio<1, 3>>>, delta<long, std::ratio<1, 6>>>);
static_assert(std::is_same_v<
              std::common_type_t<delta<int, std::milli>, delta<long long, std::ratio<1>>>,
              delta<long long, std::ratio<1, 1000>>>);
static_assert(std::is_same_v<decltype(abs(U(1))), U>);
static_assert(std::is_same_v<decltype(U::zero()), U>);
static_assert(std::is_same_v<decltype(U::min()), U>);
static_assert(std::is_same_v<decltype(++std::declval<U&>()), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>()++), U>);
static_assert(std::is_same_v<decltype(std::declval<U&>() += U(1)), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() *= 2), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() %= 2), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() %= U(1)), U&>);
static_assert(sizeof(delta_celsius) == sizeof(int64_t));
static_assert(sizeof(celsius) == sizeof(int64_t));
static_assert(std::is_trivially_copyable_v<delta_celsius>);
static_assert(std::is_trivially_copyable_v<celsius>);
static_assert(std::is_same_v<decltype(-decifahrenheit(1)), decifahrenheit>);
static_assert(std::is_same_v<decltype(-delta_decifahrenheit(1)), delta<int64_t, std::ratio<1, 18>>>);
} // namespace pin

// Every conversion between two specializations is either implicit (lossless)
// or explicit (lossy), never ill-formed
static_assert(std::is_constructible_v<delta_millicelsius, delta<double, std::milli>>);
static_assert(!std::is_convertible_v<delta<double, std::milli>, delta_millicelsius>);
static_assert(std::is_constructible_v<delta_millicelsius, delta<double>>);
static_assert(!std::is_convertible_v<delta<double>, delta_millicelsius>);
static_assert(std::is_constructible_v<delta_celsius, delta_millicelsius>);
static_assert(!std::is_convertible_v<delta_millicelsius, delta_celsius>);
static_assert(std::is_convertible_v<delta_millicelsius, delta<double, std::milli>>);

TEST_CASE("delta explicit conversion covers every lossy case", "[thermo][conversion]") {
    // Floating-point rep to integral rep, same precision: truncates
    REQUIRE(delta_millicelsius(delta<double, std::milli>(1.5)).count() == 1);
    REQUIRE(delta_millicelsius(delta<double, std::milli>(-1.5)).count() == -1);
    // Floating-point rep to integral rep, finer precision: truncates
    REQUIRE(delta_millicelsius(delta<double>(1.5)).count() == 1500);
    REQUIRE(delta_millicelsius(delta<double>(0.0015)).count() == 1);
    // Integral rep to a coarser precision: truncates
    REQUIRE(delta_celsius(delta_millicelsius(1500)).count() == 1);
    REQUIRE(delta_celsius(delta_millicelsius(-1500)).count() == -1);
    REQUIRE(delta_celsius(delta<double, std::milli>(1999.0)).count() == 1);
}

TEST_CASE("delta abs of an unreduced precision", "[thermo][delta]") {
    // A precision not in lowest terms (unary minus gives the reduced one)
    using QI = delta<int64_t, std::ratio<4, 6>>;
    using QD = delta<double, std::ratio<4, 6>>;
    STATIC_REQUIRE(std::is_same_v<decltype(abs(QI(-3))), QI>);
    REQUIRE(abs(QI(-3)).count() == 3);
    REQUIRE(abs(QI(3)).count() == 3);
    REQUIRE(abs(QD(-1.5)).count() == 1.5);
    STATIC_REQUIRE(abs(QI(-7)) == QI(7));
}

TEST_CASE("delta round with a narrow rep does not overflow", "[thermo][delta][rounding]") {
    // 1 kilodegree is 1000 degrees, beyond int8_t
    using kilo8 = delta<int8_t, std::kilo>;
    REQUIRE(quanta::thermo::round<kilo8>(delta<int8_t>(0)).count() == 0);
    REQUIRE(quanta::thermo::round<kilo8>(delta<int8_t>(100)).count() == 0);
    REQUIRE(quanta::thermo::round<kilo8>(delta<int8_t>(-100)).count() == 0);
    REQUIRE(quanta::thermo::round<delta<int16_t, std::ratio<100>>>(delta<int16_t>(32749)).count() == 327);
    STATIC_REQUIRE(quanta::thermo::round<kilo8>(delta<int8_t>(0)).count() == 0);
}

TEST_CASE("delta floor and ceil with a narrow rep compare exactly", "[thermo][delta][rounding]") {
    // 16384 thirds is 10922.67 halves. Compared in their common type, sixths
    // of an int16_t, 16384 thirds overflowed.
    using thirds16 = delta<int16_t, std::ratio<1, 3>>;
    using halves16 = delta<int16_t, std::ratio<1, 2>>;
    REQUIRE(quanta::thermo::floor<halves16>(thirds16(16384)).count() == 10922);
    REQUIRE(quanta::thermo::ceil<halves16>(thirds16(16384)).count() == 10923);
    REQUIRE(quanta::thermo::round<halves16>(thirds16(16384)).count() == 10923);
    REQUIRE(quanta::thermo::floor<halves16>(thirds16(-16384)).count() == -10923);
    REQUIRE(quanta::thermo::ceil<halves16>(thirds16(-16384)).count() == -10922);
    REQUIRE(quanta::thermo::round<halves16>(thirds16(-16384)).count() == -10923);

    // 64 thirds is 42.67 halves
    using thirds8 = delta<int8_t, std::ratio<1, 3>>;
    using halves8 = delta<int8_t, std::ratio<1, 2>>;
    REQUIRE(quanta::thermo::floor<halves8>(thirds8(64)).count() == 42);
    REQUIRE(quanta::thermo::ceil<halves8>(thirds8(64)).count() == 43);
    REQUIRE(quanta::thermo::round<halves8>(thirds8(64)).count() == 43);
    STATIC_REQUIRE(quanta::thermo::round<halves8>(thirds8(64)).count() == 43);
}

TEST_CASE("delta ceil of a negative count into an unsigned rep", "[thermo][delta][rounding]") {
    // -1/3 is -0.67 halves, whose ceiling, 0, an unsigned target holds; the
    // comparison with the cast keeps the count's sign, with or without a
    // 128-bit intermediate.
    REQUIRE(quanta::thermo::ceil<delta<uint64_t, std::ratio<1, 2>>>(delta<int64_t, std::ratio<1, 3>>(-1)).count() == 0);
    REQUIRE(quanta::thermo::ceil<delta<uint64_t>>(delta<int64_t, std::milli>(-400)).count() == 0);
    REQUIRE(quanta::thermo::ceil<delta<uint64_t, std::kilo>>(delta<int64_t, std::ratio<1, 3>>(-64)).count() == 0);
}

TEST_CASE("delta round is exact where the rounded value fits", "[thermo][delta][rounding]") {
    // -0.4 rounds to 0, which an unsigned target holds although -1, the
    // value rounded down, does not.
    REQUIRE(quanta::thermo::round<delta<uint32_t>>(delta<int32_t, std::milli>(-400)).count() == 0);
    REQUIRE(quanta::thermo::round<delta<uint32_t, std::kilo>>(delta<int8_t>(-1)).count() == 0);
    REQUIRE(quanta::thermo::round<delta<uint64_t>>(delta<int64_t, std::milli>(-400)).count() == 0);
    REQUIRE(quanta::thermo::round<delta<uint8_t>>(delta<int64_t, std::milli>(-500)).count() == 0);
    STATIC_REQUIRE(quanta::thermo::round<delta<uint32_t>>(delta<int32_t, std::milli>(-400)).count() == 0);

    // Counts near the rep's limits round exactly, with or without a 128-bit
    // intermediate: INT64_MAX thirds is 6148914691236517204.67 halves.
    using thirds = delta<int64_t, std::ratio<1, 3>>;
    using halves = delta<int64_t, std::ratio<1, 2>>;
    REQUIRE(quanta::thermo::round<halves>(thirds(9'223'372'036'854'775'807)).count() == 6'148'914'691'236'517'205);
    REQUIRE(quanta::thermo::round<halves>(thirds(-9'223'372'036'854'775'807)).count() == -6'148'914'691'236'517'205);
}

// A conversion temperature_cast cannot compute is not constructible, and
// asking is not an error: an overflowing precision ratio (on the same scale
// or across scales) or an overflowing common denominator of the precision
// ratio and the offset.
static_assert(!std::is_constructible_v<
              temperature<celsius_scale, delta<int64_t, std::kilo>>,
              temperature<celsius_scale, delta<int64_t, std::atto>>>);
static_assert(!std::is_convertible_v<
              temperature<celsius_scale, delta<double, std::atto>>,
              temperature<celsius_scale, delta<double, std::exa>>>);
static_assert(!std::is_constructible_v<
              temperature<kelvin_scale, delta<double, std::exa>>,
              temperature<celsius_scale, delta<double, std::atto>>>);
static_assert(!std::is_constructible_v<
              temperature<kelvin_scale, delta<int64_t, std::ratio<7>>>,
              temperature<fahrenheit_scale, delta<int64_t, std::atto>>>);
// Conversions that temperature_cast can compute are unaffected
static_assert(std::is_constructible_v<
              temperature<kelvin_scale, delta<int64_t, std::ratio<7>>>,
              temperature<fahrenheit_scale, delta<int64_t, std::milli>>>);
static_assert(std::is_convertible_v<millicelsius, millikelvin>);
static_assert(std::is_constructible_v<celsius, kelvin> && !std::is_convertible_v<kelvin, celsius>);

// A custom scale whose offset difference from another overflows intmax_t:
// conversions between them are not constructible, and asking is not an
// error. A custom scale with a moderate offset converts as usual.
namespace custom_scales {
struct far_scale {
    using offset = std::ratio<4611686018427387903, 7>;
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "°X";
};
struct near_scale {
    using offset = std::ratio<100>;
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "°N";
};
static_assert(!std::is_constructible_v<temperature<far_scale>, celsius>);
static_assert(!std::is_constructible_v<celsius, temperature<far_scale>>);
static_assert(!std::is_convertible_v<millicelsius, temperature<far_scale, delta<double, std::milli>>>);
static_assert(std::is_convertible_v<kelvin, temperature<near_scale>>);
static_assert(temperature<near_scale>(kelvin(300)).count() == 200);
} // namespace custom_scales

// Every conversion temperature_cast can perform has exactly one constructor:
// a floating-point rep converts explicitly to an integral one (same scale or
// across scales), and a lossless conversion to a rep the source converts to
// only explicitly is explicit.
static_assert(std::is_constructible_v<celsius, temperature<celsius_scale, delta<double>>>);
static_assert(!std::is_convertible_v<temperature<celsius_scale, delta<double>>, celsius>);
static_assert(std::is_constructible_v<millikelvin, temperature<celsius_scale, delta<double, std::milli>>>);
static_assert(!std::is_convertible_v<temperature<celsius_scale, delta<double, std::milli>>, millikelvin>);
static_assert(std::is_convertible_v<celsius, temperature<celsius_scale, delta<double>>>);

namespace explicit_temperature_rep {
struct tick {
    long long v = 0;
    constexpr tick() = default;
    constexpr explicit tick(long long x)
        : v(x) {}
    constexpr operator long long() const { return v; }
};
using tick_celsius = temperature<celsius_scale, delta<tick>>;
static_assert(std::is_constructible_v<tick_celsius, celsius>);
static_assert(!std::is_convertible_v<celsius, tick_celsius>);
static_assert(tick_celsius(celsius(25)).count().v == 25);
} // namespace explicit_temperature_rep

TEST_CASE("temperature explicit conversion from a floating-point rep", "[thermo][temperature]") {
    REQUIRE(celsius(temperature<celsius_scale, delta<double>>(21.7)).count() == 21);
    REQUIRE(celsius(temperature<celsius_scale, delta<double>>(-21.7)).count() == -21);
    REQUIRE(millikelvin(temperature<celsius_scale, delta<double, std::milli>>(1500.0)).count() == 274650);
}
