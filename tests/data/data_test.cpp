// Unit tests for data library
// Uses Catch2 test framework with compile-time static_asserts

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <limits>
#include <quanta/data>

using namespace quanta::data;
using namespace quanta::data_literals;
using namespace std::chrono_literals;

// =============================================================================
// Compile-time tests (static_assert)
// These verify correctness at compile time - if this file compiles, they pass.
// =============================================================================

// Construction and count
static_assert(bits(12).count() == 12);
static_assert(kilobits(64).count() == 64);
static_assert(megabits(8).count() == 8);
static_assert(gigabits(2).count() == 2);
static_assert(bytes(1500).count() == 1500);
static_assert(kilobytes(64).count() == 64);
static_assert(megabytes(12).count() == 12);
static_assert(gigabytes(4).count() == 4);
static_assert(kibibytes(4).count() == 4);
static_assert(mebibytes(16).count() == 16);
static_assert(gibibytes(8).count() == 8);
static_assert(bits_per_second(9600).count() == 9600);
static_assert(gibibytes_per_second(1).count() == 1);

// Precisions are exact ratios of one byte
static_assert(std::is_same_v<bits::precision, std::ratio<1, 8>>);
static_assert(std::is_same_v<kilobits::precision, std::ratio<125>>);
static_assert(std::is_same_v<megabits::precision, std::ratio<125000>>);
static_assert(std::is_same_v<gigabits::precision, std::ratio<125000000>>);
static_assert(std::is_same_v<bytes::precision, std::ratio<1>>);
static_assert(std::is_same_v<kilobytes::precision, std::kilo>);
static_assert(std::is_same_v<megabytes::precision, std::mega>);
static_assert(std::is_same_v<gigabytes::precision, std::giga>);
static_assert(std::is_same_v<kibibytes::precision, std::ratio<1024>>);
static_assert(std::is_same_v<mebibytes::precision, std::ratio<1048576>>);
static_assert(std::is_same_v<gibibytes::precision, std::ratio<1073741824>>);
static_assert(std::is_same_v<bytes::unit, byte_unit>);
static_assert(std::is_same_v<bytes::rep, int64_t>);
static_assert(std::is_same_v<size<double>, quantity<byte_unit, double, std::ratio<1>>>);

// Rates have the same precisions, of one byte per second
static_assert(std::is_same_v<bits_per_second::precision, bits::precision>);
static_assert(std::is_same_v<kilobits_per_second::precision, kilobits::precision>);
static_assert(std::is_same_v<megabits_per_second::precision, megabits::precision>);
static_assert(std::is_same_v<gigabits_per_second::precision, gigabits::precision>);
static_assert(std::is_same_v<bytes_per_second::precision, bytes::precision>);
static_assert(std::is_same_v<kilobytes_per_second::precision, kilobytes::precision>);
static_assert(std::is_same_v<megabytes_per_second::precision, megabytes::precision>);
static_assert(std::is_same_v<gigabytes_per_second::precision, gigabytes::precision>);
static_assert(std::is_same_v<kibibytes_per_second::precision, kibibytes::precision>);
static_assert(std::is_same_v<mebibytes_per_second::precision, mebibytes::precision>);
static_assert(std::is_same_v<gibibytes_per_second::precision, gibibytes::precision>);
static_assert(std::is_same_v<bytes_per_second::unit, byte_per_second_unit>);
static_assert(std::is_same_v<rate<double>, quantity<byte_per_second_unit, double, std::ratio<1>>>);

// Same-unit arithmetic
static_assert(bytes(5) + bytes(3) == bytes(8));
static_assert(bytes(5) - bytes(3) == bytes(2));
static_assert(bytes(5) * 2 == bytes(10));
static_assert(2 * bytes(5) == bytes(10));
static_assert(bytes(10) / 2 == bytes(5));
static_assert(bytes(10) / bytes(2) == 5);
static_assert(bytes(10) % 3 == bytes(1));
static_assert(bytes(10) % bytes(3) == bytes(1));
static_assert(-bytes(5) == bytes(-5));
static_assert(+bytes(5) == bytes(5));
static_assert(mebibytes(1) / kibibytes(1) == 1024);
static_assert(kilobytes(1) / kibibytes(1) == 0);
static_assert(kibibytes(1) % bytes(1000) == bytes(24));
static_assert(megabits_per_second(100) / megabits_per_second(25) == 4);
static_assert(kilobytes_per_second(3) * 2 == kilobytes_per_second(6));

// Same-precision results keep their type
static_assert(std::is_same_v<decltype(kibibytes(1) + kibibytes(2)), kibibytes>);
static_assert(std::is_same_v<decltype(bits(1) * 2), bits>);
static_assert(std::is_same_v<decltype(-gigabytes(1)), gigabytes>);
static_assert(std::is_same_v<decltype(bytes(10) / bytes(2)), int64_t>);
static_assert(std::is_same_v<decltype(kilobits_per_second(1) + kilobits_per_second(1)), kilobits_per_second>);

// Mixed-precision arithmetic yields the common type
static_assert(kilobytes(1) + bytes(500) == bytes(1500));
static_assert(kibibytes(1) + bytes(1) == bytes(1025));
static_assert(bytes(1) + bits(1) == bits(9));
static_assert(kilobytes(1) + kilobits(1) == kilobits(9));
static_assert(kilobytes(1) + kibibytes(1) == bytes(2024));
static_assert((kilobytes(1) + kibibytes(1)).count() == 253); // in units of 8 B
static_assert(mebibytes(1) - kibibytes(1) == kibibytes(1023));
static_assert(megabytes_per_second(1) + kilobytes_per_second(1) == kilobytes_per_second(1001));

// Bits, SI and IEC precision equalities
static_assert(bytes(1) == bits(8));
static_assert(kilobits(1) == bits(1000));
static_assert(kilobits(8) == bytes(1000));
static_assert(kilobits(1) == bytes(125));
static_assert(megabits(1) == kilobits(1000));
static_assert(gigabits(1) == megabits(1000));
static_assert(gigabits(1) == megabytes(125));
static_assert(kilobytes(1) == bytes(1000));
static_assert(megabytes(1) == kilobytes(1000));
static_assert(gigabytes(1) == megabytes(1000));
static_assert(kibibytes(1) == bytes(1024));
static_assert(kibibytes(1) == bits(8192));
static_assert(mebibytes(1) == kibibytes(1024));
static_assert(gibibytes(1) == mebibytes(1024));
static_assert(gibibytes(1) == bytes(1073741824));
static_assert(kibibytes(125) == kilobytes(128));
static_assert(kilobytes(1) != kibibytes(1));
static_assert(bytes_per_second(1) == bits_per_second(8));
static_assert(kibibytes_per_second(1) == bytes_per_second(1024));

// Implicit lossless conversions
constexpr bytes b_from_kib = kibibytes(2);
static_assert(b_from_kib.count() == 2048);
constexpr bits bit_from_b = bytes(3);
static_assert(bit_from_b.count() == 24);
constexpr bits bit_from_kbit = kilobits(2);
static_assert(bit_from_kbit.count() == 2000);
constexpr kilobits kbit_from_kb = kilobytes(1);
static_assert(kbit_from_kb.count() == 8);
constexpr bits bit_from_kib = kibibytes(1);
static_assert(bit_from_kib.count() == 8192);
constexpr kibibytes kib_from_mib = mebibytes(3);
static_assert(kib_from_mib.count() == 3072);
constexpr kilobytes kb_from_mbit = megabits(1);
static_assert(kb_from_mbit.count() == 125);
constexpr bytes b_from_kbit = kilobits(1);
static_assert(b_from_kbit.count() == 125);
constexpr megabytes mb_from_gbit = gigabits(8);
static_assert(mb_from_gbit.count() == 1000);
constexpr bits_per_second bps_from_kbps = kilobytes_per_second(1);
static_assert(bps_from_kbps.count() == 8000);

// Lossless directions are implicit; lossy ones are explicit only
static_assert(std::is_convertible_v<kibibytes, bytes>);
static_assert(std::is_convertible_v<kilobits, bits>);
static_assert(std::is_convertible_v<bytes, bits>); // x8
static_assert(std::is_convertible_v<kilobytes, kilobits>);
static_assert(std::is_convertible_v<kibibytes, bits>);
static_assert(std::is_convertible_v<gibibytes, mebibytes>);
static_assert(std::is_convertible_v<gigabytes, bytes>);
static_assert(std::is_convertible_v<megabits, kilobytes>);
static_assert(std::is_convertible_v<kilobits, bytes>);
static_assert(std::is_convertible_v<kibibytes_per_second, bytes_per_second>);
static_assert(std::is_convertible_v<bytes_per_second, bits_per_second>);
static_assert(!std::is_convertible_v<bytes, kibibytes>);
static_assert(!std::is_convertible_v<bits, bytes>);
static_assert(!std::is_convertible_v<kibibytes, kilobytes>); // 1.024 kB
static_assert(!std::is_convertible_v<kilobytes, kibibytes>);
static_assert(!std::is_convertible_v<kibibytes, kilobits>); // 8.192 kbit
static_assert(!std::is_convertible_v<megabytes, kibibytes>);
static_assert(!std::is_convertible_v<bits, kilobits>);
static_assert(!std::is_convertible_v<bytes_per_second, kibibytes_per_second>);
static_assert(std::is_constructible_v<kibibytes, bytes>);
static_assert(std::is_constructible_v<bytes, bits>);
static_assert(std::is_constructible_v<kilobytes, kibibytes>);
static_assert(std::is_constructible_v<kibibytes, kilobytes>);
static_assert(std::is_constructible_v<kilobits, kibibytes>);

// Implicit to floating-point, from any precision
static_assert(std::is_convertible_v<kilobytes, size<double, std::ratio<1024>>>);
static_assert(std::is_convertible_v<bits, size<double>>);
static_assert(!std::is_convertible_v<size<double>, bytes>);

// Sizes and rates never convert into each other
static_assert(!std::is_constructible_v<bytes, bytes_per_second>);
static_assert(!std::is_constructible_v<bytes_per_second, bytes>);
static_assert(!std::is_constructible_v<size<double>, rate<double>>);

// Explicit lossy conversions (truncate toward zero)
static_assert(bytes(bits(15)).count() == 1);
static_assert(kibibytes(bytes(2047)).count() == 1);
static_assert(kilobytes(kibibytes(1)).count() == 1); // 1.024 kB
static_assert(kibibytes(kilobytes(3)).count() == 2); // 2.93 KiB
static_assert(kilobits(kibibytes(1)).count() == 8);  // 8.192 kbit
static_assert(megabits(megabytes(1)).count() == 8);  // exact
static_assert(gigabytes(gibibytes(1)).count() == 1); // 1.074 GB
static_assert(gibibytes(gigabytes(1)).count() == 0); // 0.93 GiB
static_assert(bytes(bits(-15)).count() == -1);
static_assert(kibibytes_per_second(kilobytes_per_second(3)).count() == 2);

// quantity_cast
static_assert(quantity_cast<bits>(kibibytes(1)).count() == 8192);
static_assert(quantity_cast<kibibytes>(bytes(5000)).count() == 4);
static_assert(quantity_cast<kilobytes>(kibibytes(3)).count() == 3); // 3.072 kB
static_assert(quantity_cast<mebibytes>(gigabytes(1)).count() == 953);
static_assert(quantity_cast<megabits_per_second>(megabytes_per_second(1)).count() == 8);
static_assert(std::is_same_v<decltype(quantity_cast<kibibytes>(bytes(1))), kibibytes>);

// Rounding conversions
static_assert(floor<bytes>(bits(12)) == bytes(1));
static_assert(floor<bytes>(bits(-12)) == bytes(-2));
static_assert(ceil<bytes>(bits(12)) == bytes(2));
static_assert(ceil<bytes>(bits(-12)) == bytes(-1));
static_assert(round<bytes>(bits(11)) == bytes(1));
static_assert(round<bytes>(bits(13)) == bytes(2));
static_assert(round<bytes>(bits(12)) == bytes(2));             // tie to even
static_assert(round<bytes>(bits(20)) == bytes(2));             // tie to even
static_assert(floor<kibibytes>(kilobytes(3)) == kibibytes(2)); // 2.93
static_assert(ceil<kibibytes>(kilobytes(3)) == kibibytes(3));
static_assert(round<kibibytes>(kilobytes(3)) == kibibytes(3));
static_assert(round<kilobytes>(kibibytes(3)) == kilobytes(3));   // 3.072
static_assert(round<kibibytes>(megabytes(1)) == kibibytes(977)); // 976.5625
static_assert(round<mebibytes>(gigabytes(1)) == mebibytes(954)); // 953.67
static_assert(floor<kibibytes>(kilobytes(-3)) == kibibytes(-3));
static_assert(ceil<kibibytes>(kilobytes(-3)) == kibibytes(-2));
static_assert(round<megabytes_per_second>(kibibytes_per_second(1000)) == megabytes_per_second(1)); // 1.024
static_assert(abs(bytes(-5)) == bytes(5));
static_assert(abs(bytes(5)) == bytes(5));
static_assert(abs(kibibytes_per_second(-3)) == kibibytes_per_second(3));

// zero/min/max
static_assert(bytes::zero().count() == 0);
static_assert(bytes::max().count() == std::numeric_limits<int64_t>::max());
static_assert(bytes::min().count() == std::numeric_limits<int64_t>::lowest());
static_assert(kibibytes::zero() == bits(0));
static_assert(size<double>::zero().count() == 0.0);
static_assert(megabits_per_second::max().count() == std::numeric_limits<int64_t>::max());

// Comparison operators
static_assert(bytes(5) == bytes(5));
static_assert(bytes(5) != bytes(6));
static_assert(bytes(5) < bytes(6));
static_assert(bytes(6) > bytes(5));
static_assert(bytes(5) <= bytes(5));
static_assert(bytes(5) >= bytes(5));
static_assert(bytes(999) < kilobytes(1));
static_assert(kilobytes(1) < kibibytes(1));
static_assert(megabytes(1) < mebibytes(1));
static_assert(gigabytes(1) < gibibytes(1));
static_assert(bits(7) < bytes(1));
static_assert(bits(9) > bytes(1));
static_assert(kilobits(8) == kilobytes(1));
static_assert(megabits(9) > megabytes(1));
static_assert((kibibytes(1) <=> kilobytes(1)) == std::strong_ordering::greater);
static_assert((bytes(1) <=> bits(8)) == std::strong_ordering::equal);
static_assert((size<double>(1.0) <=> size<double>(2.0)) == std::partial_ordering::less);
static_assert(megabits_per_second(100) > megabytes_per_second(12));
static_assert(megabits_per_second(100) < megabytes_per_second(13));

// =============================================================================
// Floating-point representations
// =============================================================================

static_assert(size<double>(2.5).count() == 2.5);
static_assert(size<double>(bytes(5)).count() == 5.0); // implicit int -> double
static_assert(size<double>(bits(4)).count() == 0.5);
static_assert(size<double, std::kilo>(kibibytes(1)).count() == 1.024);
static_assert(size<double, std::ratio<1024>>(bytes(512)).count() == 0.5);
static_assert(size<double, std::ratio<1, 8>>(kilobytes(1)).count() == 8000.0);
static_assert(size<double>(1.0) + size<double>(0.5) == size<double>(1.5));
static_assert(rate<double, std::mega>(megabits_per_second(100)).count() == 12.5);

// =============================================================================
// common_type
// =============================================================================

static_assert(std::is_same_v<std::common_type_t<kilobytes, bytes>, bytes>);
static_assert(std::is_same_v<std::common_type_t<bytes, bytes>, bytes>);
static_assert(std::is_same_v<std::common_type_t<gigabytes, megabytes>, megabytes>);
static_assert(std::is_same_v<std::common_type_t<mebibytes, kibibytes>, kibibytes>);
static_assert(std::is_same_v<std::common_type_t<kibibytes, bytes>, bytes>);
static_assert(std::is_same_v<std::common_type_t<bytes, bits>, bits>);
static_assert(std::is_same_v<std::common_type_t<kibibytes, bits>, bits>);
static_assert(std::is_same_v<std::common_type_t<kilobytes, kilobits>, kilobits>);
static_assert(std::is_same_v<std::common_type_t<megabits, kilobytes>, kilobytes>);
static_assert(std::is_same_v<std::common_type_t<gigabits, megabytes>, megabytes>);
// Mixed SI/IEC lands on the finest exact common precision
static_assert(std::is_same_v<std::common_type_t<kilobytes, kibibytes>, size<int64_t, std::ratio<8>>>);
static_assert(std::is_same_v<std::common_type_t<megabytes, mebibytes>, size<int64_t, std::ratio<64>>>);
static_assert(std::is_same_v<std::common_type_t<kilobits, kibibytes>, size<int64_t, std::ratio<1>>>);
static_assert(std::is_same_v<std::common_type_t<bytes, size<double>>, size<double>>);
static_assert(std::is_same_v<std::common_type_t<kibibytes, size<double, std::kilo>>, size<double, std::ratio<8>>>);
static_assert(
    std::is_same_v<std::common_type_t<kilobytes_per_second, kibibytes_per_second>, rate<int64_t, std::ratio<8>>>
);
static_assert(std::is_same_v<std::common_type_t<bytes_per_second, bits_per_second>, bits_per_second>);

// =============================================================================
// Arithmetic with std::chrono durations
// =============================================================================

// size / duration = rate
static_assert(bytes(1000) / 2s == bytes_per_second(500));
static_assert(kilobytes(1500) / 500ms == megabytes_per_second(3)); // kilo / milli = mega
static_assert(megabytes(1000) / 500ms == gigabytes_per_second(2));
static_assert(kibibytes(64) / 1s == kibibytes_per_second(64));
static_assert(bits(9600) / 1s == bits_per_second(9600));
static_assert(megabits(100) / 1s == megabits_per_second(100));
static_assert(mebibytes(30) / 1min == rate<int64_t, std::ratio<1048576, 60>>(30));
static_assert(mebibytes(30) / 1min == kibibytes_per_second(512));

// Quotients truncate the counts in the result precision
static_assert(megabytes(1) / 2s == megabytes_per_second(0));
static_assert(kilobytes(megabytes(1)) / 2s == kilobytes_per_second(500));
static_assert(size<double, std::mega>(1) / 2s == rate<double, std::mega>(0.5));

// rate x duration = size, and duration x rate = size
static_assert(bytes_per_second(500) * 2s == bytes(1000));
static_assert(2s * bytes_per_second(500) == bytes(1000));
static_assert(kilobits_per_second(100) * 250ms == bits(25000)); // 125 B x 1/1000 = 1/8 B
static_assert(250ms * kilobits_per_second(100) == kilobits(25));
static_assert(megabytes_per_second(3) * 2s == megabytes(6));
static_assert(gigabits_per_second(10) * 1ns == bits(10));
static_assert(megabits_per_second(5) * 2h == megabytes(4500));
static_assert(kibibytes_per_second(4) * 1min == kibibytes(240));
static_assert(rate<double>(1.5) * std::chrono::duration<double>(2.0) == size<double>(3.0));

// size / rate = duration
static_assert(megabytes(1000) / megabytes_per_second(100) == 10s);
static_assert(gibibytes(1) / mebibytes_per_second(1) == 1024s);
static_assert(megabits(8) / megabytes_per_second(1) == 1s);                         // 1/8 s x 8
static_assert(kilobytes(8) / kilobits_per_second(8) == 8s);                         // 8 s x 1
static_assert(bytes(1500) / kilobytes_per_second(1) == 1500ms);                     // 1/1000 s x 1500
static_assert(gigabytes(1) / megabytes_per_second(100) == std::chrono::seconds(0)); // 1/100 ks, truncated
static_assert(size<double, std::giga>(1) / rate<double, std::mega>(100) == std::chrono::duration<double>(10.0));

// Result types: the precision is the product or quotient of the operand
// precision and the duration's period, the rep their common type
static_assert(std::is_same_v<decltype(kilobytes(1500) / 500ms), megabytes_per_second>);
static_assert(std::is_same_v<decltype(bytes(1) / 1s), bytes_per_second>);
static_assert(std::is_same_v<decltype(kilobits_per_second(100) * 250ms), bits>);
static_assert(std::is_same_v<decltype(250ms * kilobits_per_second(100)), bits>);
static_assert(std::is_same_v<decltype(megabytes(1) / megabytes_per_second(1)), std::chrono::seconds>);
static_assert(
    std::is_same_v<decltype(gibibytes(1) / mebibytes_per_second(1)), std::chrono::duration<int64_t, std::ratio<1024>>>
);
static_assert(std::is_same_v<decltype(megabits_per_second(5) * 2h), size<int64_t, std::ratio<450000000>>>);
static_assert(std::is_same_v<decltype(size<double>(1) / std::chrono::duration<float>(1)), rate<double>>);
static_assert(std::is_same_v<decltype(bytes(1) / std::chrono::duration<int, std::milli>(1)), rate<int64_t, std::kilo>>);

// Only these combinations are defined: a size never mixes with a rate except
// through a duration
template<typename A, typename B>
concept multipliable = requires(const A& a, const B& b) { a * b; };
template<typename A, typename B>
concept dividable = requires(const A& a, const B& b) { a / b; };
template<typename A, typename B>
concept addable = requires(const A& a, const B& b) { a + b; };
template<typename A, typename B>
concept subtractable = requires(const A& a, const B& b) { a - b; };
template<typename A, typename B>
concept comparable = requires(const A& a, const B& b) { a == b; };
template<typename A, typename B>
concept ordered = requires(const A& a, const B& b) { a < b; };
template<typename A, typename B>
concept modulable = requires(const A& a, const B& b) { a % b; };

static_assert(dividable<bytes, std::chrono::seconds>);
static_assert(dividable<kibibytes, std::chrono::minutes>);
static_assert(dividable<size<double>, std::chrono::duration<float>>);
static_assert(multipliable<bytes_per_second, std::chrono::seconds>);
static_assert(multipliable<std::chrono::hours, kilobits_per_second>);
static_assert(multipliable<rate<double>, std::chrono::duration<double, std::milli>>);
static_assert(dividable<bytes, bytes_per_second>);
static_assert(dividable<kibibytes, megabits_per_second>);
static_assert(dividable<bytes, bytes>);                       // scalar
static_assert(dividable<bytes_per_second, bytes_per_second>); // scalar
static_assert(multipliable<bytes, int>);
static_assert(multipliable<int, bytes_per_second>);

static_assert(!addable<bytes, bytes_per_second>);
static_assert(!addable<bytes_per_second, bytes>);
static_assert(!subtractable<kilobytes, kilobytes_per_second>);
static_assert(!subtractable<kilobytes_per_second, kilobytes>);
static_assert(!comparable<bytes, bytes_per_second>);
static_assert(!comparable<kilobits_per_second, kilobits>);
static_assert(!ordered<bytes, bytes_per_second>);
static_assert(!modulable<bytes, bytes_per_second>);
static_assert(!multipliable<bytes, bytes_per_second>);
static_assert(!multipliable<bytes_per_second, bytes>);
static_assert(!dividable<bytes_per_second, bytes>);
static_assert(!multipliable<bytes, bytes>);
static_assert(!multipliable<bytes_per_second, bytes_per_second>);
static_assert(!multipliable<bytes, std::chrono::seconds>); // size x duration
static_assert(!multipliable<std::chrono::seconds, bytes>);
static_assert(!dividable<bytes_per_second, std::chrono::seconds>); // rate / duration
static_assert(!dividable<std::chrono::seconds, bytes>);
static_assert(!dividable<std::chrono::seconds, bytes_per_second>);
static_assert(!addable<bytes, std::chrono::seconds>);
static_assert(!addable<bytes_per_second, std::chrono::seconds>);

namespace {

// A type with the shape of a std::chrono::duration (count(), rep, period) that
// is not one, such as a length quantity from a sibling unit library.
struct chrono_shaped {
    using rep = int64_t;
    using period = std::ratio<1>;
    constexpr rep count() const { return 3; }
};

} // namespace

static_assert(!dividable<bytes, chrono_shaped>);
static_assert(!multipliable<bytes_per_second, chrono_shaped>);
static_assert(!multipliable<chrono_shaped, bytes_per_second>);

// Data quantities have no period, so they are not mistaken for durations or
// distances by generic code
template<typename T>
concept has_period = requires { typename T::period; };
static_assert(!has_period<bytes>);
static_assert(!has_period<kibibytes>);
static_assert(!has_period<bytes_per_second>);
static_assert(!has_period<size<double>>);

// ... but everything else of the interface is common to both units
template<typename T>
concept quantity_interface = requires(T t) {
    { t.count() } -> std::same_as<typename T::rep>;
    typename T::precision;
    typename T::unit;
    T::zero();
    T::min();
    T::max();
};
static_assert(quantity_interface<bytes>);
static_assert(quantity_interface<gibibytes>);
static_assert(quantity_interface<megabits_per_second>);

// =============================================================================
// User-defined literals
// =============================================================================

static_assert(12_bit == bits(12));
static_assert(64_kbit == kilobits(64));
static_assert(8_Mbit == megabits(8));
static_assert(2_Gbit == gigabits(2));
static_assert(1500_B == bytes(1500));
static_assert(64_kB == kilobytes(64));
static_assert(12_MB == megabytes(12));
static_assert(4_GB == gigabytes(4));
static_assert(4_KiB == kibibytes(4));
static_assert(16_MiB == mebibytes(16));
static_assert(8_GiB == gibibytes(8));
static_assert(std::is_same_v<decltype(12_bit), bits>);
static_assert(std::is_same_v<decltype(64_kbit), kilobits>);
static_assert(std::is_same_v<decltype(8_Mbit), megabits>);
static_assert(std::is_same_v<decltype(2_Gbit), gigabits>);
static_assert(std::is_same_v<decltype(1500_B), bytes>);
static_assert(std::is_same_v<decltype(64_kB), kilobytes>);
static_assert(std::is_same_v<decltype(12_MB), megabytes>);
static_assert(std::is_same_v<decltype(4_GB), gigabytes>);
static_assert(std::is_same_v<decltype(4_KiB), kibibytes>);
static_assert(std::is_same_v<decltype(16_MiB), mebibytes>);
static_assert(std::is_same_v<decltype(8_GiB), gibibytes>);
static_assert(9223372036854775807_B == bytes::max());
static_assert(0x7fff'ffff'ffff'ffff_GiB == gibibytes::max());

// Rate literals
static_assert(9600_bit_per_s == bits_per_second(9600));
static_assert(128_kbit_per_s == kilobits_per_second(128));
static_assert(100_Mbit_per_s == megabits_per_second(100));
static_assert(10_Gbit_per_s == gigabits_per_second(10));
static_assert(512_B_per_s == bytes_per_second(512));
static_assert(250_kB_per_s == kilobytes_per_second(250));
static_assert(100_MB_per_s == megabytes_per_second(100));
static_assert(2_GB_per_s == gigabytes_per_second(2));
static_assert(64_KiB_per_s == kibibytes_per_second(64));
static_assert(20_MiB_per_s == mebibytes_per_second(20));
static_assert(1_GiB_per_s == gibibytes_per_second(1));
static_assert(std::is_same_v<decltype(9600_bit_per_s), bits_per_second>);
static_assert(std::is_same_v<decltype(128_kbit_per_s), kilobits_per_second>);
static_assert(std::is_same_v<decltype(100_Mbit_per_s), megabits_per_second>);
static_assert(std::is_same_v<decltype(10_Gbit_per_s), gigabits_per_second>);
static_assert(std::is_same_v<decltype(512_B_per_s), bytes_per_second>);
static_assert(std::is_same_v<decltype(250_kB_per_s), kilobytes_per_second>);
static_assert(std::is_same_v<decltype(100_MB_per_s), megabytes_per_second>);
static_assert(std::is_same_v<decltype(2_GB_per_s), gigabytes_per_second>);
static_assert(std::is_same_v<decltype(64_KiB_per_s), kibibytes_per_second>);
static_assert(std::is_same_v<decltype(20_MiB_per_s), mebibytes_per_second>);
static_assert(std::is_same_v<decltype(1_GiB_per_s), gibibytes_per_second>);
static_assert(9223372036854775807_bit_per_s == bits_per_second::max());

// Integer literal forms
static_assert(1'500_B == bytes(1500));
static_assert(0x10_KiB == kibibytes(16));
static_assert(0b101_Mbit == megabits(5));
static_assert(017_kB == kilobytes(15)); // octal, as std::chrono's 017h

// Literals in expressions
static_assert(1_kB + 500_B == 1500_B);
static_assert(1_B == 8_bit);
static_assert(1_KiB == 1024_B);
static_assert(1_kB == 8_kbit);
static_assert(1_Gbit == 125_MB);
static_assert(-3_KiB == kibibytes(-3));
static_assert(1500_kB / 500ms == 3_MB_per_s);
static_assert(100_kbit_per_s * 250ms == 25_kbit);
static_assert(1000_MB / 100_MB_per_s == 10s);
static_assert(700_MiB / 100_Mbit_per_s == std::chrono::duration<int64_t, std::ratio<131072, 15625>>(7));

// =============================================================================
// Traits
// =============================================================================

static_assert(is_quantity_v<bytes>);
static_assert(is_quantity_v<gibibytes>);
static_assert(is_quantity_v<size<double>>);
static_assert(is_quantity_v<megabits_per_second>);
static_assert(!is_quantity_v<int>);
static_assert(!is_quantity_v<std::chrono::seconds>);
static_assert(treat_as_inexact_v<double>);
static_assert(treat_as_inexact_v<float>);
static_assert(!treat_as_inexact_v<int64_t>);
static_assert(std::is_same_v<treat_as_inexact<double>, quanta::treat_as_inexact<double>>);

// =============================================================================
// Runtime tests
// =============================================================================

TEST_CASE("trunc", "[quantity][rounding]") {
    REQUIRE(trunc<bytes>(bits(12)) == bytes(1));
    REQUIRE(trunc<bytes>(bits(-12)) == bytes(-1));
    REQUIRE(trunc<bytes>(bits(15)) == bytes(1));
    REQUIRE(trunc<bytes>(bits(0)) == bytes(0));
    REQUIRE(trunc<bytes>(size<double>(1.7)) == bytes(1));
    REQUIRE(trunc<bytes>(size<double>(-1.7)) == bytes(-1));
    REQUIRE(trunc<kibibytes>(kilobytes(-3)) == kibibytes(-2));
    REQUIRE(trunc<kilobytes_per_second>(bytes_per_second(2500)) == kilobytes_per_second(2));
    REQUIRE(trunc<size<double>>(bits(12)).count() == 1.5);
    static_assert(trunc<bytes>(bits(-15)) == bytes(-1));
}

TEST_CASE("rounding from a floating-point rep", "[quantity][rounding]") {
    REQUIRE(floor<bytes>(size<double>(1.5)) == bytes(1));
    REQUIRE(floor<bytes>(size<double>(-1.5)) == bytes(-2));
    REQUIRE(ceil<bytes>(size<double>(1.2)) == bytes(2));
    REQUIRE(ceil<bytes>(size<double>(-1.2)) == bytes(-1));
    REQUIRE(round<bytes>(size<double>(1.7)) == bytes(2));
    REQUIRE(round<bytes>(size<double>(-1.7)) == bytes(-2));
    REQUIRE(round<bytes>(size<double>(1.2)) == bytes(1));
    REQUIRE(round<bytes>(size<double>(1.5)) == bytes(2));   // tie to even
    REQUIRE(round<bytes>(size<double>(2.5)) == bytes(2));   // tie to even
    REQUIRE(round<bytes>(size<double>(-2.5)) == bytes(-2)); // tie to even

    // Across precisions
    REQUIRE(floor<kibibytes>(size<double, std::kilo>(2.5)) == kibibytes(2)); // 2.44 KiB
    REQUIRE(ceil<kibibytes>(size<double, std::kilo>(2.5)) == kibibytes(3));
    REQUIRE(round<kibibytes>(size<double, std::kilo>(2.5)) == kibibytes(2));
    REQUIRE(floor<bytes>(size<double, std::ratio<1, 8>>(-1.0)) == bytes(-1));
    REQUIRE(ceil<bytes>(size<double, std::ratio<1, 8>>(-1.0)) == bytes(0));
    REQUIRE(round<megabits_per_second>(rate<double, std::mega>(1.3)) == megabits_per_second(10)); // 10.4

    // A floating-point target is not rounded
    REQUIRE(floor<size<double>>(bits(12)).count() == 1.5);
    REQUIRE(ceil<size<double>>(bits(12)).count() == 1.5);
    REQUIRE(round<size<double>>(bits(12)).count() == 1.5);
}

TEST_CASE("compound assignment operators", "[quantity]") {
    bytes b{1000};
    b += bytes{500};
    REQUIRE(b.count() == 1500);
    b -= bytes{300};
    REQUIRE(b.count() == 1200);
    b *= 2;
    REQUIRE(b.count() == 2400);
    b /= 4;
    REQUIRE(b.count() == 600);
    b %= 7;
    REQUIRE(b.count() == 5);
    b %= bytes{3};
    REQUIRE(b.count() == 2);

    // Lossless implicit conversion on the right-hand side
    bits total{0};
    total += kibibytes{1};
    total += kilobits{1};
    total += bytes{1};
    REQUIRE(total == 9200_bit);

    bytes_per_second r{100};
    r += kibibytes_per_second{1};
    REQUIRE(r == 1124_B_per_s);
}

TEST_CASE("increment and decrement operators", "[quantity]") {
    kibibytes k{5};
    REQUIRE((++k).count() == 6);
    REQUIRE((k++).count() == 6);
    REQUIRE(k.count() == 7);
    REQUIRE((--k).count() == 6);
    REQUIRE((k--).count() == 6);
    REQUIRE(k.count() == 5);
}

TEST_CASE("bit, SI and IEC conversions", "[conversion]") {
    // IEC -> SI is lossy, so explicit
    kilobytes a = kilobytes(kibibytes(10)); // 10.24 kB, truncated
    REQUIRE(a.count() == 10);
    bytes b = kibibytes(10); // implicit: exact in bytes
    REQUIRE(b.count() == 10240);
    REQUIRE(round<kilobytes>(b) == kilobytes(10));

    // Bytes -> bits is lossless; bits -> bytes truncates
    bits c = megabytes(1);
    REQUIRE(c.count() == 8000000);
    REQUIRE(bytes(bits(1001)).count() == 125);
    REQUIRE(ceil<bytes>(bits(1001)) == bytes(126));

    // Floating-point holds the exact value
    size<double, std::kilo> d = kibibytes(10);
    REQUIRE(d.count() == Catch::Approx(10.24));
    rate<double, std::ratio<125'000>> e = megabytes_per_second(1);
    REQUIRE(e.count() == Catch::Approx(8.0));
}

TEST_CASE("floating-point arithmetic", "[quantity]") {
    size<double> a{2.5};
    size<double> b{1.0};
    REQUIRE((a + b).count() == Catch::Approx(3.5));
    REQUIRE((a - b).count() == Catch::Approx(1.5));
    REQUIRE((a * 2.0).count() == Catch::Approx(5.0));
    REQUIRE(a / b == Catch::Approx(2.5));
}

TEST_CASE("arithmetic with std::chrono durations", "[chrono]") {
    // Throughput of a transfer
    auto transferred = 1500_kB;
    auto elapsed = 500ms;
    auto throughput = transferred / elapsed;
    STATIC_REQUIRE(std::is_same_v<decltype(throughput), megabytes_per_second>);
    REQUIRE(throughput == 3_MB_per_s);
    REQUIRE(megabits_per_second(throughput) == 24_Mbit_per_s);

    // Bytes sent at a rate for a time, in either order
    auto link = 100_kbit_per_s;
    REQUIRE(link * 250ms == 25000_bit);
    REQUIRE(250ms * link == 25000_bit);
    REQUIRE(link * 2s == 25_kB);
    REQUIRE(link * std::chrono::duration<int, std::milli>(250) == 25_kbit);

    // Time to transfer a size at a rate
    auto eta = 1000_MB / 100_MB_per_s;
    STATIC_REQUIRE(std::is_same_v<decltype(eta), std::chrono::seconds>);
    REQUIRE(eta == 10s);
    REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(1_MiB / 1_kB_per_s) == 1048576ms);

    // Integer quotients truncate; a finer precision or a floating-point rep
    // keeps the fraction
    REQUIRE(1_MB / 2s == 0_MB_per_s);
    REQUIRE(kilobytes(1_MB) / 2s == 500_kB_per_s);
    REQUIRE((size<double, std::mega>(1) / 2s).count() == 0.5);
    REQUIRE((1_GB / 100_MB_per_s).count() == 0); // 0.01 ks
    REQUIRE(gigabytes(1) / rate<double, std::mega>(100) == std::chrono::duration<double, std::kilo>(0.01));

    // Floating-point durations
    REQUIRE((rate<double>(1.5) * std::chrono::duration<double>(2.0)).count() == 3.0);
    REQUIRE((bytes(3) / std::chrono::duration<double>(2.0)).count() == 1.5);
}

// Every size and rate suffix, with {} and to_string.
template<typename Q>
static void check_suffix(const Q& q, const char* suffix) {
    const std::string count = std::to_string(q.count());
    CHECK(to_string(q) == count + suffix);
#if CONFIG_QUANTA_STD_FORMAT
    CHECK(std::format("{}", q) == count + suffix);
    CHECK(std::format("{:.1f}", q) == count + ".0" + suffix);
#endif
}

TEST_CASE("size suffixes", "[format]") {
    check_suffix(bits(12), "bit");
    check_suffix(kilobits(64), "kbit");
    check_suffix(megabits(8), "Mbit");
    check_suffix(gigabits(2), "Gbit");
    check_suffix(bytes(1500), "B");
    check_suffix(kilobytes(64), "kB");
    check_suffix(megabytes(12), "MB");
    check_suffix(gigabytes(4), "GB");
    check_suffix(kibibytes(4), "KiB");
    check_suffix(mebibytes(16), "MiB");
    check_suffix(gibibytes(8), "GiB");
    check_suffix(bytes(-3), "B");
}

TEST_CASE("rate suffixes", "[format]") {
    check_suffix(bits_per_second(9600), "bit/s");
    check_suffix(kilobits_per_second(128), "kbit/s");
    check_suffix(megabits_per_second(100), "Mbit/s");
    check_suffix(gigabits_per_second(10), "Gbit/s");
    check_suffix(bytes_per_second(512), "B/s");
    check_suffix(kilobytes_per_second(250), "kB/s");
    check_suffix(megabytes_per_second(100), "MB/s");
    check_suffix(gigabytes_per_second(2), "GB/s");
    check_suffix(kibibytes_per_second(64), "KiB/s");
    check_suffix(mebibytes_per_second(20), "MiB/s");
    check_suffix(gibibytes_per_second(1), "GiB/s");
}

TEST_CASE("to_string of computed quantities", "[format]") {
    // Common types that land on a named precision keep its symbol
    REQUIRE(to_string(1_kB + 500_B) == "1500B");
    REQUIRE(to_string(1_B + 1_bit) == "9bit");
    REQUIRE(to_string(1_kB + 1_kbit) == "9kbit");
    REQUIRE(to_string(1500_kB / 500ms) == "3MB/s");
    REQUIRE(to_string(100_kbit_per_s * 250ms) == "25000bit");
}

// Precisions with no symbol have no to_string(q), rather than failing at
// runtime, but have the base-unit and scaled forms.
template<typename Q>
concept has_to_string = requires(Q q) { to_string(q); };
template<typename Q>
concept has_base_to_string = requires(Q q) { to_string(q, base_unit); };
template<typename Q>
concept has_scaled_to_string = requires(Q q) { to_string(q, scaled); };
using eight_bytes = std::common_type_t<kilobytes, kibibytes>;
static_assert(has_to_string<bytes>);
static_assert(has_to_string<gibibytes>);
static_assert(has_to_string<megabits_per_second>);
static_assert(has_to_string<size<double, std::kilo>>);
static_assert(!has_to_string<eight_bytes>);
static_assert(!has_to_string<std::common_type_t<megabytes, mebibytes>>);
static_assert(!has_to_string<std::common_type_t<kilobytes_per_second, kibibytes_per_second>>);
static_assert(!has_to_string<size<int64_t, std::milli>>); // no SI-prefix rule: "mB" is not a unit
static_assert(!has_to_string<size<int64_t, std::centi>>);
static_assert(!has_to_string<size<int64_t, std::ratio<1, 1000>>>);
static_assert(!has_to_string<decltype(megabits_per_second(5) * 2h)>);
static_assert(!has_to_string<rate<int64_t, std::ratio<1048576, 60>>>);
static_assert(has_base_to_string<kibibytes>);
static_assert(has_scaled_to_string<bits_per_second>);
static_assert(has_base_to_string<eight_bytes>);
static_assert(has_scaled_to_string<eight_bytes>);

// to_string(q, base_unit) and to_string(q, scaled), and the equivalent {:b}
// and {:s} formatting, which must agree exactly.
template<typename Q>
static void check_forms(const Q& q, const char* base_expected, const char* scaled_expected) {
    CAPTURE(to_string(q));
    CHECK(to_string(q, base_unit) == base_expected);
    CHECK(to_string(q, scaled) == scaled_expected);
#if CONFIG_QUANTA_STD_FORMAT
    CHECK(std::format("{:b}", q) == base_expected);
    CHECK(std::format("{:s}", q) == scaled_expected);
#endif
}

// Floating-point counts with no spec: shortest round-trip fixed notation, never
// scientific, identical between to_string and std::format("{}").
template<typename Q>
static void check_plain(const Q& q, const char* expected) {
    CHECK(to_string(q) == expected);
#if CONFIG_QUANTA_STD_FORMAT
    CHECK(std::format("{}", q) == expected);
#endif
}

TEST_CASE("floating-point counts without a spec", "[format]") {
    check_plain(size<double>(1000000.0), "1000000B");
    check_plain(size<double>(2.5), "2.5B");
    check_plain(size<double>(-2.5), "-2.5B");
    check_plain(size<double>(0.0), "0B");
    check_plain(size<double>(-0.0), "-0B");
    check_plain(size<double>(1e22), "10000000000000000000000B");
    check_plain(size<double, std::kilo>(1.5), "1.5kB");
    check_plain(size<double, std::ratio<1024>>(1.0 / 3.0), "0.3333333333333333KiB");
    check_plain(size<double, std::ratio<1, 8>>(0.000001), "0.000001bit");
    check_plain(size<float>(0.1f), "0.1B");
    check_plain(rate<double, std::mega>(2.5), "2.5MB/s");
    check_plain(rate<double, std::ratio<125'000>>(1e7), "10000000Mbit/s");
    // Integer counts are unchanged.
    check_plain(bytes(1000000), "1000000B");
    check_plain(kibibytes_per_second(-24), "-24KiB/s");
#if CONFIG_QUANTA_STD_FORMAT
    // Explicit specs are unchanged.
    REQUIRE(std::format("{:.3e}", size<double>(1e6)) == "1.000e+06B");
    REQUIRE(std::format("{:.2f}", size<double, std::kilo>(1.5)) == "1.50kB");
    REQUIRE(std::format("{:b.3f}", bits(1)) == "0.125B");
    REQUIRE(std::format("{:b.3e}", gibibytes(1)) == "1.074e+09B");
#endif
}

TEST_CASE("non-finite values are shown in the base unit or the bit family's smallest", "[format]") {
    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    check_forms(size<double>(inf), "infB", "infB");
    check_forms(size<double>(-inf), "-infB", "-infB");
    check_forms(size<double>(nan), "nanB", "nanB");
    check_forms(size<double, std::kilo>(inf), "infB", "infB");
    check_forms(size<double, std::ratio<1, 8>>(-inf), "-infB", "-infbit");
    check_forms(size<double, std::ratio<1024>>(nan), "nanB", "nanB");
    check_forms(rate<double, std::ratio<125>>(inf), "infB/s", "infbit/s");
#if CONFIG_QUANTA_STD_FORMAT
    REQUIRE(std::format("{:s.1f}", size<double, std::mega>(inf)) == "infB");
#endif
}

TEST_CASE("base-unit and scaled forms", "[format]") {
    // SI bytes: B, kB, MB, GB
    check_forms(kilobytes(1500), "1500000B", "1.5MB");
    check_forms(bytes(2048), "2048B", "2.048kB"); // B is SI
    check_forms(bytes(1500), "1500B", "1.5kB");
    check_forms(bytes(-1500), "-1500B", "-1.5kB");
    check_forms(megabytes(1500), "1500000000B", "1.5GB");
    check_forms(gigabytes(5000), "5000000000000B", "5000GB");
    check_forms(size<double>(0.5), "0.5B", "0.5B"); // smallest candidate
    check_forms(size<double, std::kilo>(0.25), "250B", "250B");

    // Candidate boundaries
    check_forms(bytes(999), "999B", "999B");
    check_forms(bytes(1000), "1000B", "1kB");
    check_forms(bytes(-999), "-999B", "-999B");
    check_forms(bytes(-1000), "-1000B", "-1kB");
    check_forms(kilobytes(999), "999000B", "999kB");
    check_forms(kilobytes(1000), "1000000B", "1MB");
    check_forms(megabytes(999), "999000000B", "999MB");
    check_forms(megabytes(1000), "1000000000B", "1GB");

    // Bits: bit, kbit, Mbit, Gbit
    check_forms(bits(12), "1.5B", "12bit");
    check_forms(bits(1), "0.125B", "1bit");
    check_forms(bits(-8), "-1B", "-8bit");
    check_forms(bits(1500), "187.5B", "1.5kbit");
    check_forms(kilobits(1500), "187500B", "1.5Mbit");
    check_forms(kilobits(999), "124875B", "999kbit");
    check_forms(kilobits(1000), "125000B", "1Mbit");
    check_forms(megabits(1000), "125000000B", "1Gbit");
    check_forms(gigabits(3), "375000000B", "3Gbit");
    check_forms(gigabits(2000), "250000000000B", "2000Gbit");
    check_forms(size<double, std::ratio<1, 8>>(0.5), "0.0625B", "0.5bit");

    // IEC bytes: B, KiB, MiB, GiB
    check_forms(kibibytes(1536), "1572864B", "1.5MiB");
    check_forms(kibibytes(-1536), "-1572864B", "-1.5MiB");
    check_forms(kibibytes(1), "1024B", "1KiB");
    check_forms(kibibytes(1023), "1047552B", "1023KiB");
    check_forms(kibibytes(1024), "1048576B", "1MiB");
    check_forms(mebibytes(1536), "1610612736B", "1.5GiB");
    check_forms(mebibytes(1023), "1072693248B", "1023MiB");
    check_forms(gibibytes(2048), "2199023255552B", "2048GiB");
    check_forms(size<double, std::ratio<1024>>(0.5), "512B", "512B"); // the IEC family's B
    check_forms(size<double, std::ratio<1024>>(0.000244140625), "0.25B", "0.25B");

    // Zero (keeping its sign) is shown in the family's smallest unit
    check_forms(bytes(0), "0B", "0B");
    check_forms(kilobytes(0), "0B", "0B");
    check_forms(bits(0), "0B", "0bit");
    check_forms(kilobits(0), "0B", "0bit");
    check_forms(kibibytes(0), "0B", "0B");
    check_forms(size<double>(-0.0), "-0B", "-0B");
    check_forms(size<double, std::ratio<1024>>(-0.0), "-0B", "-0B");

    // Rates use the same units per second
    check_forms(kilobits_per_second(1500), "187500B/s", "1.5Mbit/s");
    check_forms(gigabits_per_second(10), "1250000000B/s", "10Gbit/s");
    check_forms(bits_per_second(9600), "1200B/s", "9.6kbit/s");
    check_forms(bytes_per_second(2048), "2048B/s", "2.048kB/s");
    check_forms(megabytes_per_second(1500), "1500000000B/s", "1.5GB/s");
    check_forms(kibibytes_per_second(1536), "1572864B/s", "1.5MiB/s");
    check_forms(mebibytes_per_second(-20), "-20971520B/s", "-20MiB/s");
    check_forms(bits_per_second(0), "0B/s", "0bit/s");
    check_forms(kibibytes_per_second(0), "0B/s", "0B/s");

    // Floating-point representations
    check_forms(size<double, std::kilo>(1500.5), "1500500B", "1.5005MB");
    check_forms(rate<double, std::ratio<125>>(2.5), "312.5B/s", "2.5kbit/s");
    check_forms(size<double, std::ratio<1048576>>(0.5), "524288B", "512KiB");
}

// A user symbol for another precision: shown in its own unit by every form
// but the base unit, since it is in none of the scaled form's families
template<>
struct quanta::data::precision_symbol<std::tera> {
    static constexpr const char* symbol = "TB";
};

TEST_CASE("a user precision symbol is shown in its own unit when scaled", "[format]") {
    using terabytes = size<int64_t, std::tera>;
    using terabytes_per_second = rate<int64_t, std::tera>;
    REQUIRE(to_string(terabytes(3)) == "3TB");
    REQUIRE(to_string(terabytes_per_second(3)) == "3TB/s");
    check_forms(terabytes(3), "3000000000000B", "3TB");
    check_forms(terabytes(0), "0B", "0TB");
    check_forms(terabytes_per_second(2), "2000000000000B/s", "2TB/s");
}

// Precisions with no symbol of their own still have a base unit, and are
// scaled in SI bytes (B is SI), whatever the precisions they came from; with
// std::format, a spec without a marker renders in the base unit too.
TEST_CASE("precisions with no symbol render in the base unit or scaled in SI bytes", "[format]") {
    using kilobytes_kibibytes_per_second = std::common_type_t<kilobytes_per_second, kibibytes_per_second>;
    using bytes_e5 = size<int64_t, std::ratio<1, 100000>>;
    using ten_bytes = size<int64_t, std::ratio<10>>;
    REQUIRE(to_string(1_kB + 1_KiB, base_unit) == "2024B");
    REQUIRE(to_string(1_kB + 1_KiB, scaled) == "2.024kB");
    REQUIRE(to_string(1_MB + 1_MiB, scaled) == "2.048576MB");
    REQUIRE(to_string(5_Mbit_per_s * 2h, base_unit) == "4500000000B");
    REQUIRE(to_string(5_Mbit_per_s * 2h, scaled) == "4.5GB");
    REQUIRE(to_string(kilobytes_kibibytes_per_second(1000), base_unit) == "8000B/s");
    REQUIRE(to_string(kilobytes_kibibytes_per_second(1000), scaled) == "8kB/s");
    REQUIRE(to_string(eight_bytes(0), scaled) == "0B");
    REQUIRE(to_string(ten_bytes(75000), base_unit) == "750000B");
    REQUIRE(to_string(ten_bytes(75000), scaled) == "750kB");
    REQUIRE(to_string(bytes_e5(5), base_unit) == "0.00005B");
    REQUIRE(to_string(bytes_e5(5), scaled) == "0.00005B"); // nothing below B
    REQUIRE(to_string(size<int64_t, std::milli>(2500), scaled) == "2.5B");
#if CONFIG_QUANTA_STD_FORMAT
    REQUIRE(std::format("{:b}", 1_kB + 1_KiB) == "2024B");
    REQUIRE(std::format("{:s}", 1_kB + 1_KiB) == "2.024kB");
    REQUIRE(std::format("{:s.2f}", 1_kB + 1_KiB) == "2.02kB");
    REQUIRE(std::format("{:.2f}", 1_kB + 1_KiB) == "2024.00B");
    REQUIRE(std::format("{:8.1f}", 1_kB + 1_KiB) == "  2024.0B");
    REQUIRE(std::format("{:s}", 5_Mbit_per_s * 2h) == "4.5GB");
    REQUIRE(std::format("{:b}", ten_bytes(75000)) == "750000B");
    REQUIRE(std::format("{:s}", ten_bytes(75000)) == "750kB");
    REQUIRE(std::format("{:.2f}", bytes_e5(5)) == "0.00B");
    REQUIRE(std::format("{:s}", kilobytes_kibibytes_per_second(1000)) == "8kB/s");
    for (const auto q : {eight_bytes(253), eight_bytes(-1), eight_bytes(0)}) {
        REQUIRE(std::format("{:b}", q) == to_string(q, base_unit));
        REQUIRE(std::format("{:s}", q) == to_string(q, scaled));
    }
    REQUIRE(std::format("{:b}", bytes_e5(5)) == to_string(bytes_e5(5), base_unit));
    REQUIRE(std::format("{:s}", bytes_e5(5)) == to_string(bytes_e5(5), scaled));
    // "{}" needs a unit of its own (a compile error for a constant format string)
    eight_bytes b(1);
    REQUIRE_THROWS_AS(std::vformat("{}", std::make_format_args(b)), std::format_error);
#endif
}

#if CONFIG_QUANTA_STD_FORMAT
static_assert(std::is_default_constructible_v<std::formatter<bytes, char>>);
static_assert(std::is_default_constructible_v<std::formatter<gibibytes, char>>);
static_assert(std::is_default_constructible_v<std::formatter<bits_per_second, char>>);
static_assert(std::is_default_constructible_v<std::formatter<eight_bytes, char>>);
static_assert(std::is_default_constructible_v<std::formatter<size<int64_t, std::milli>, char>>);

TEST_CASE("std::format support", "[format]") {
    REQUIRE(std::format("{}", bits(12)) == "12bit");
    REQUIRE(std::format("{}", kilobits(64)) == "64kbit");
    REQUIRE(std::format("{}", bytes(1500)) == "1500B");
    REQUIRE(std::format("{}", kilobytes(64)) == "64kB");
    REQUIRE(std::format("{}", kibibytes(4)) == "4KiB");
    REQUIRE(std::format("{}", gibibytes(8)) == "8GiB");
    REQUIRE(std::format("{}", megabits_per_second(100)) == "100Mbit/s");
    REQUIRE(std::format("{}", kibibytes_per_second(64)) == "64KiB/s");
    REQUIRE(std::format("{}", bytes_per_second(-3)) == "-3B/s");
    REQUIRE(std::format("sent {} at {}", 4_MiB, 100_Mbit_per_s) == "sent 4MiB at 100Mbit/s");
}

TEST_CASE("std::format with spec keeps the type's own unit", "[format]") {
    // A spec changes only the number formatting, never the unit.
    REQUIRE(std::format("{:.1f}", kilobytes(1500)) == "1500.0kB");
    REQUIRE(std::format("{:.2f}", kibibytes(4)) == "4.00KiB");
    REQUIRE(std::format("{:.0f}", gigabits(2)) == "2Gbit");
    REQUIRE(std::format("{:.1f}", megabits_per_second(100)) == "100.0Mbit/s");

    // Other numeric presentations apply to the same count.
    REQUIRE(std::format("{:e}", bytes(1500)) == "1.500000e+03B");
    REQUIRE(std::format("{:g}", kibibytes(-3)) == "-3KiB");

    // Width and alignment pass through to the numeric part.
    REQUIRE(std::format("{:8.1f}", kilobytes(1500)) == "  1500.0kB");
    REQUIRE(std::format("{:<6.1f}", mebibytes(6)) == "6.0   MiB");
    REQUIRE(std::format("{:*>6}", bits_per_second(96)) == "****96bit/s");

    // Floating-point representations format the same way.
    REQUIRE(std::format("{}", size<double, std::kilo>(2.5)) == "2.5kB");
    REQUIRE(std::format("{:.2f}", rate<double, std::ratio<1024>>(1.5)) == "1.50KiB/s");
}

TEST_CASE("std::format base-unit and scaled markers", "[format]") {
    // Without a marker the unit is the type's own, whatever the spec.
    REQUIRE(std::format("{}", kilobytes(1500)) == "1500kB");
    REQUIRE(std::format("{:.1f}", kilobytes(1500)) == "1500.0kB");

    // With a trailing spec, the spec applies to the converted value.
    REQUIRE(std::format("{:b.1f}", bits(12)) == "1.5B");
    REQUIRE(std::format("{:b.0f}", kibibytes(2)) == "2048B");
    REQUIRE(std::format("{:b.3e}", gigabytes(2)) == "2.000e+09B");
    REQUIRE(std::format("{:b.1f}", kilobits_per_second(3)) == "375.0B/s");
    REQUIRE(std::format("{:s.2f}", kilobytes(1500)) == "1.50MB");
    REQUIRE(std::format("{:s.1f}", kibibytes(1536)) == "1.5MiB");
    REQUIRE(std::format("{:s.3f}", bytes(2048)) == "2.048kB");
    REQUIRE(std::format("{:s.1f}", kilobits_per_second(1500)) == "1.5Mbit/s");
    REQUIRE(std::format("{:s.0f}", bytes(0)) == "0B");

    // Width, alignment and fill after the marker apply to the numeric part.
    REQUIRE(std::format("{:b*>8}", bits(12)) == "*****1.5B");
    REQUIRE(std::format("{:b>8.2f}", bits(12)) == "    1.50B");
    REQUIRE(std::format("{:s<6.1f}", kibibytes(1536)) == "1.5   MiB");
    REQUIRE(std::format("{:s_^7}", kilobits_per_second(2500)) == "__2.5__Mbit/s");

    // A leading 'b' or 's' is always the marker, never a fill character.
    REQUIRE(std::format("{:s>6}", kilobytes(2000)) == "     2MB");
    REQUIRE(std::format("{:b>6}", bytes(2)) == "     2B");

    // Only one marker: a second one reaches the numeric spec and is an error.
    // There is no power form, so 'p' is not a marker either.
    kilobytes kb{1500};
    REQUIRE_THROWS_AS(std::vformat("{:sb}", std::make_format_args(kb)), std::format_error);
    REQUIRE_THROWS_AS(std::vformat("{:bs}", std::make_format_args(kb)), std::format_error);
    REQUIRE_THROWS_AS(std::vformat("{:p}", std::make_format_args(kb)), std::format_error);

    // The tagged to_string overloads agree exactly with the markers.
    REQUIRE(std::format("{:b}", gibibytes(7)) == to_string(gibibytes(7), base_unit));
    REQUIRE(std::format("{:s}", megabits(3)) == to_string(megabits(3), scaled));
    REQUIRE(std::format("{:s}", size<double>(1.0 / 3.0)) == to_string(size<double>(1.0 / 3.0), scaled));
    REQUIRE(std::format("{:b}", rate<double, std::kilo>(0.1)) == to_string(rate<double, std::kilo>(0.1), base_unit));
}
#endif

TEST_CASE("download time example", "[integration]") {
    // A 700 MiB image over a 100 Mbit/s link
    auto image = 700_MiB;
    auto link = 100_Mbit_per_s;
    auto t = image / link; // in units of 1 MiB / 1 Mbit = 8.388608 s
    REQUIRE(t.count() == 7);
    REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(t) == 58s);
    REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(t) == 58720256us);

    // What remains after 20 s
    auto sent = link * 20s;
    REQUIRE(sent == 250_MB);
    auto remaining = image - sent;
    REQUIRE(round<mebibytes>(remaining) == 462_MiB); // 461.58 MiB
#if CONFIG_QUANTA_STD_FORMAT
    REQUIRE(std::format("{:s.1f}", round<kibibytes>(remaining)) == "461.6MiB");
#endif
}

TEST_CASE("streaming budget example", "[integration]") {
    // A 5 Mbit/s stream for two hours
    auto stream = 5_Mbit_per_s * 2h;
    REQUIRE(stream == 4500_MB);
    REQUIRE(round<gigabytes>(stream) == 4_GB); // 4.5, ties to even
    REQUIRE(ceil<gibibytes>(stream) == 5_GiB); // 4.19 GiB
    REQUIRE(floor<gibibytes>(stream) == 4_GiB);
    REQUIRE(to_string(megabytes(stream), scaled) == "4.5GB");
}

// =============================================================================
// Result types (pinned: they depend on the exact std::ratio spelling)
// =============================================================================

namespace pin {
using U = quantity<byte_unit, int, std::ratio<10, 20>>;
static_assert(std::is_same_v<decltype(+U(1)), quantity<byte_unit, int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(-U(1)), quantity<byte_unit, int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) * 2L), quantity<byte_unit, long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(2L * U(1)), quantity<byte_unit, long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(U(1) / 2L), quantity<byte_unit, long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(U(1) % 2L), quantity<byte_unit, long, std::ratio<10, 20>>>);
static_assert(std::is_same_v<decltype(U(1) / U(1)), int>);
static_assert(std::is_same_v<std::common_type_t<U, U>, quantity<byte_unit, int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) + U(1)), quantity<byte_unit, int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) - U(1)), quantity<byte_unit, int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<decltype(U(1) % U(1)), quantity<byte_unit, int, std::ratio<1, 2>>>);
static_assert(std::is_same_v<
              std::common_type_t<U, quantity<byte_unit, long, std::ratio<1, 3>>>,
              quantity<byte_unit, long, std::ratio<1, 6>>>);
static_assert(std::is_same_v<
              std::common_type_t<quantity<byte_unit, int, std::milli>, quantity<byte_unit, long long, std::ratio<1>>>,
              quantity<byte_unit, long long, std::ratio<1, 1000>>>);
static_assert(std::is_same_v<decltype(abs(U(1))), U>);
static_assert(std::is_same_v<decltype(U::zero()), U>);
static_assert(std::is_same_v<decltype(U::min()), U>);
static_assert(std::is_same_v<decltype(++std::declval<U&>()), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>()++), U>);
static_assert(std::is_same_v<decltype(std::declval<U&>() += U(1)), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() *= 2), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() %= 2), U&>);
static_assert(std::is_same_v<decltype(std::declval<U&>() %= U(1)), U&>);
static_assert(std::is_same_v<
              decltype(U(1) / std::chrono::duration<int, std::ratio<1, 4>>(1)),
              quantity<byte_per_second_unit, int, std::ratio<2>>>);
static_assert(sizeof(bytes) == sizeof(int64_t));
static_assert(sizeof(bytes_per_second) == sizeof(int64_t));
static_assert(sizeof(quantity<byte_unit, char>) == sizeof(char));
static_assert(std::is_trivially_copyable_v<bytes>);
static_assert(std::is_trivially_copyable_v<bytes_per_second>);
} // namespace pin

// Every conversion between two specializations is either implicit (lossless)
// or explicit (lossy), never ill-formed
static_assert(std::is_constructible_v<bytes, size<double>>);
static_assert(!std::is_convertible_v<size<double>, bytes>);
static_assert(std::is_constructible_v<bytes, size<double, std::ratio<1024>>>);
static_assert(!std::is_convertible_v<size<double, std::ratio<1024>>, bytes>);
static_assert(std::is_constructible_v<kibibytes, bytes>);
static_assert(!std::is_convertible_v<bytes, kibibytes>);
static_assert(std::is_convertible_v<bits, size<double>>);

TEST_CASE("explicit conversion covers every lossy case", "[quantity][conversion]") {
    // Floating-point rep to integral rep, same precision: truncates
    REQUIRE(bytes(size<double>(1.5)).count() == 1);
    REQUIRE(bytes(size<double>(-1.5)).count() == -1);
    // Floating-point rep to integral rep, finer precision: truncates
    REQUIRE(bytes(size<double, std::ratio<1024>>(1.5)).count() == 1536);
    REQUIRE(bits(size<double>(0.3)).count() == 2);
    // Integral rep to a coarser precision: truncates
    REQUIRE(kibibytes(bytes(1500)).count() == 1);
    REQUIRE(kibibytes(bytes(-1500)).count() == -1);
    REQUIRE(kilobytes(size<double, std::ratio<1024>>(2.0)).count() == 2);
    // Different units are still not convertible
    STATIC_REQUIRE(!std::is_constructible_v<bytes, rate<double>>);
    STATIC_REQUIRE(!std::is_constructible_v<bytes_per_second, bytes>);
}

TEST_CASE("abs of an unreduced precision", "[quantity]") {
    // A precision not in lowest terms (unary minus gives the reduced one)
    using QI = quantity<byte_unit, int64_t, std::ratio<4, 6>>;
    using QD = quantity<byte_unit, double, std::ratio<4, 6>>;
    STATIC_REQUIRE(std::is_same_v<decltype(abs(QI(-3))), QI>);
    REQUIRE(abs(QI(-3)).count() == 3);
    REQUIRE(abs(QI(3)).count() == 3);
    REQUIRE(abs(QD(-1.5)).count() == 1.5);
    STATIC_REQUIRE(abs(QI(-7)) == QI(7));
}

// Precisions whose conversion factor overflows intmax_t do not convert, and
// asking is not an error
static_assert(!std::is_convertible_v<size<double, std::atto>, size<double, std::exa>>);
static_assert(!std::is_constructible_v<size<double, std::atto>, size<double, std::exa>>);
static_assert(!std::is_constructible_v<size<double, std::exa>, size<double, std::atto>>);
static_assert(!std::is_convertible_v<size<int64_t, std::atto>, size<int64_t, std::exa>>);
static_assert(!std::is_constructible_v<size<int64_t, std::exa>, size<int64_t, std::atto>>);
