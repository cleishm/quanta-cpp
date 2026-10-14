#pragma once

/**
 * @file core.hpp
 * @brief Configuration, traits and internals shared by every quanta domain.
 *
 * Included by each domain header; there is no need to include it directly.
 */

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <ratio>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <version> // pulls in __cpp_lib_format so the detection below works

/**
 * @brief 1 if std::format support (std::formatter specializations) is enabled.
 *
 * Defaults to 1 when `<format>` is available, and may be defined to 0 before
 * including any quanta header to disable it. When quanta is used as an
 * ESP-IDF component, the `QUANTA_STD_FORMAT_SUPPORT` Kconfig option controls
 * it: enabled, std::format is supported where the toolchain provides
 * `<format>`; disabled, the component's build defines this to 0.
 */
#ifndef CONFIG_QUANTA_STD_FORMAT
#if __has_include(<format>) && defined(__cpp_lib_format)
#define CONFIG_QUANTA_STD_FORMAT 1
#else
#define CONFIG_QUANTA_STD_FORMAT 0
#endif
#endif

// The per-domain macros of the pre-quanta libraries are gone. Only a value of 0
// (or an empty definition) is diagnosed: under ESP-IDF a stale component left
// installed defines them to 1 through sdkconfig.h, which is harmless.
#if defined(CONFIG_THERMO_STD_FORMAT) && (CONFIG_THERMO_STD_FORMAT + 0) == 0
#error "CONFIG_THERMO_STD_FORMAT has been replaced by CONFIG_QUANTA_STD_FORMAT"
#endif
#if defined(CONFIG_FREQUENCY_STD_FORMAT) && (CONFIG_FREQUENCY_STD_FORMAT + 0) == 0
#error "CONFIG_FREQUENCY_STD_FORMAT has been replaced by CONFIG_QUANTA_STD_FORMAT"
#endif
#if defined(CONFIG_ELECTRO_STD_FORMAT) && (CONFIG_ELECTRO_STD_FORMAT + 0) == 0
#error "CONFIG_ELECTRO_STD_FORMAT has been replaced by CONFIG_QUANTA_STD_FORMAT"
#endif
#if defined(CONFIG_SPATIAL_STD_FORMAT) && (CONFIG_SPATIAL_STD_FORMAT + 0) == 0
#error "CONFIG_SPATIAL_STD_FORMAT has been replaced by CONFIG_QUANTA_STD_FORMAT"
#endif

#if CONFIG_QUANTA_STD_FORMAT
#include <format>
#endif

/**
 * @brief 1 if the compiler accepts non-ASCII literal suffixes (e.g. `_Ω`).
 *
 * GCC before 14 (including the ESP-IDF toolchains before GCC 14) cannot lex
 * extended characters in user-defined literal suffixes, so those literals are
 * omitted there. Each has an ASCII alternative that is always available:
 *
 * - thermo: `_Δc`, `_Δmc`, `_Δk`, `_Δmk`, `_Δf`, `_Δmf` → `_delta_c`,
 *   `_delta_mc`, `_delta_k`, `_delta_mk`, `_delta_f`, `_delta_mf`
 * - electro: `_mΩ`, `_Ω`, `_kΩ`, `_MΩ`, `_dBµV` → `_mOhm`, `_Ohm`, `_kOhm`,
 *   `_MOhm`, `_dBuV`
 * - spatial: `_µm`, `_µL` → `_um`, `_uL`
 */
#if !defined(__GNUC__) || defined(__clang__) || __GNUC__ >= 14
#define QUANTA_HAS_UTF8_LITERALS 1
#else
#define QUANTA_HAS_UTF8_LITERALS 0
#endif

/**
 * @brief Type-safe units modeled after std::chrono.
 *
 * Each domain lives in its own namespace: quanta::thermo (temperature),
 * quanta::frequency (frequency), quanta::electro (electrical units and
 * decibels) and quanta::spatial (length, area and volume). As with
 * std::chrono, the literals of every domain are available through `using
 * namespace quanta::literals;`, and each domain's through `using namespace
 * quanta::<domain>_literals;` (e.g. quanta::frequency_literals) or `using
 * namespace quanta::<domain>;`.
 */
namespace quanta {

/**
 * @brief Trait to indicate floating-point-like behavior.
 *
 * Specializations for floating-point types return true. User-defined types
 * may specialize this to enable implicit lossy conversions. Applies to every
 * domain.
 *
 * This trait affects conversion rules and operation availability:
 * - Types with treat_as_inexact_v<T> == true allow implicit lossy conversions
 * - Modulo operations (%) are disabled for inexact types
 * - Affects rounding behavior in musical operations
 *   (quanta::frequency::frequency::octave_shift and semitone_shift)
 *
 * @code
 * // Enabled for standard floating-point types
 * static_assert(treat_as_inexact_v<float>);
 * static_assert(treat_as_inexact_v<double>);
 * static_assert(!treat_as_inexact_v<int>);
 * static_assert(!treat_as_inexact_v<int64_t>);
 * @endcode
 *
 * @tparam T The type to check.
 */
template<typename T>
struct treat_as_inexact : std::bool_constant<std::floating_point<T>> {};

template<typename T>
inline constexpr bool treat_as_inexact_v = treat_as_inexact<T>::value;

/**
 * @brief Tag type selecting the base-unit form, see base_unit.
 */
struct base_unit_t {
    explicit base_unit_t() = default;
};

/**
 * @brief Selects the base-unit form in to_string().
 *
 * The value is shown in the domain's base unit (e.g. °C, Hz, V, m), rendered
 * in the shortest fixed notation that round-trips (never scientific), exactly
 * as by the `b` format marker.
 *
 * @code
 * to_string(millihertz(1500), base_unit);  // "1.5Hz"
 * to_string(millivolts(3300), base_unit);  // "3.3V"
 * @endcode
 */
inline constexpr base_unit_t base_unit{};

/**
 * @brief Tag type selecting the scaled form, see scaled.
 */
struct scaled_t {
    explicit scaled_t() = default;
};

/**
 * @brief Selects the scaled form in to_string().
 *
 * The value is shown in the unit best suited to its magnitude, rendered in
 * the shortest fixed notation that round-trips (never scientific), exactly as
 * by the `s` format marker. See each domain's to_string() for the units it
 * chooses between.
 *
 * @code
 * to_string(megahertz(2400), scaled);  // "2.4GHz"
 * @endcode
 */
inline constexpr scaled_t scaled{};

/** @cond INTERNAL */
template<typename T, template<typename...> class Template>
struct _is_specialization_of : std::false_type {};

template<template<typename...> class Template, typename... Args>
struct _is_specialization_of<Template<Args...>, Template> : std::true_type {};

template<typename T, template<typename...> class Template>
inline constexpr bool _is_specialization_of_v = _is_specialization_of<T, Template>::value;

template<typename T>
struct _is_ratio : std::false_type {};

template<std::intmax_t Num, std::intmax_t Denom>
struct _is_ratio<std::ratio<Num, Denom>> : std::true_type {};

consteval intmax_t _gcd(intmax_t m, intmax_t n) noexcept {
    while (n != 0) {
        intmax_t rem = m % n;
        m = n;
        n = rem;
    }
    return m;
}

// Runtime GCD for integer types (using Euclidean algorithm)
template<typename T>
constexpr T _runtime_gcd(T m, T n) noexcept {
    if (m < 0) {
        m = -m;
    }
    if (n < 0) {
        n = -n;
    }
    while (n != 0) {
        T rem = m % n;
        m = n;
        n = rem;
    }
    return m;
}

// R1 / R2 as a fraction in lowest terms, computed as std::ratio_divide does
// but reporting overflow of intmax_t (fits is false) rather than failing to
// compile, so that it can be used in constraints.
struct _ratio_quotient {
    intmax_t num;
    intmax_t den;
    bool fits;
};

// Whether a * b fits in intmax_t, for non-negative a and b.
constexpr bool _product_fits(intmax_t a, intmax_t b) {
    return a >= 0 && b >= 0 && (a == 0 || b <= std::numeric_limits<intmax_t>::max() / a);
}

// |n|, for any n but the most negative (which no std::ratio holds).
constexpr intmax_t _abs_ratio_term(intmax_t n) {
    return n < 0 ? -n : n;
}

template<typename R1, typename R2>
inline constexpr _ratio_quotient _safe_ratio_divide = [] {
    if constexpr (R2::num == 0) {
        return _ratio_quotient{0, 1, false};
    } else {
        // As std::ratio_divide: cancel the common factors of the numerators and
        // of the denominators, then multiply crosswise.
        constexpr intmax_t g1 = _gcd(_abs_ratio_term(R1::num), _abs_ratio_term(R2::num));
        constexpr intmax_t g2 = _gcd(R1::den, R2::den);
        constexpr intmax_t a = R1::num / g1;
        constexpr intmax_t b = R2::den / g2;
        constexpr intmax_t c = R1::den / g2;
        constexpr intmax_t d = R2::num / g1;
        if constexpr (_product_fits(_abs_ratio_term(a), b) && _product_fits(c, _abs_ratio_term(d))) {
            constexpr intmax_t num = d < 0 ? -(a * b) : a * b;
            constexpr intmax_t den = c * _abs_ratio_term(d);
            constexpr intmax_t g = _gcd(_abs_ratio_term(num), den);
            return _ratio_quotient{num / g, den / g, true};
        } else {
            return _ratio_quotient{0, 1, false};
        }
    }
}();

// Whether a - b fits in intmax_t (excluding its most negative value, as
// std::ratio does).
constexpr bool _difference_fits(intmax_t a, intmax_t b) {
    constexpr intmax_t max = std::numeric_limits<intmax_t>::max();
    return b >= 0 ? a >= -max + b : a <= max + b;
}

// a / b as a reduced fraction (b != 0), the reduction std::ratio performs.
consteval _ratio_quotient _reduced_quotient(intmax_t a, intmax_t b) {
    const intmax_t g = _gcd(_abs_ratio_term(a), _abs_ratio_term(b));
    const intmax_t sign = (a < 0) != (b < 0) ? -1 : 1;
    return {sign * (_abs_ratio_term(a) / g), _abs_ratio_term(b) / g, true};
}

// The gcd of |m| and |n|, or 1 if both are 0: the factor std::ratio's
// arithmetic cancels, which leaves a zero term as it is.
consteval intmax_t _cancelling_gcd(intmax_t m, intmax_t n) {
    const intmax_t g = _gcd(_abs_ratio_term(m), _abs_ratio_term(n));
    return g == 0 ? 1 : g;
}

// R1 - R2 as a fraction in lowest terms, computed step by step as
// std::ratio_subtract does (cancelling common factors, then a product, a
// difference and a ratio_multiply), but reporting overflow of intmax_t (fits
// is false) rather than failing to compile.
template<typename R1, typename R2>
inline constexpr _ratio_quotient _safe_ratio_subtract = [] {
    constexpr intmax_t gn = _cancelling_gcd(R1::num, R2::num);
    constexpr intmax_t gd = _gcd(R1::den, R2::den);
    constexpr intmax_t x1 = R1::num / gn;
    constexpr intmax_t y1 = R2::den / gd;
    constexpr intmax_t x2 = R2::num / gn;
    constexpr intmax_t y2 = R1::den / gd;
    if constexpr (!_product_fits(_abs_ratio_term(x1), y1) || !_product_fits(_abs_ratio_term(x2), y2)) {
        return _ratio_quotient{0, 1, false};
    } else if constexpr (!_difference_fits(x1 * y1, x2 * y2)) {
        return _ratio_quotient{0, 1, false};
    } else {
        // ratio<gn, R1::den / gd> times ratio<x1 * y1 - x2 * y2, R2::den>
        constexpr _ratio_quotient a = _reduced_quotient(gn, R1::den / gd);
        constexpr _ratio_quotient b = _reduced_quotient(x1 * y1 - x2 * y2, R2::den);
        constexpr intmax_t g1 = _cancelling_gcd(a.num, b.den);
        constexpr intmax_t g2 = _cancelling_gcd(a.den, b.num);
        constexpr intmax_t n1 = a.num / g1;
        constexpr intmax_t n2 = b.num / g2;
        constexpr intmax_t d1 = b.den / g1;
        constexpr intmax_t d2 = a.den / g2;
        if constexpr (!_product_fits(_abs_ratio_term(n1), _abs_ratio_term(n2)) || !_product_fits(d1, d2)) {
            return _ratio_quotient{0, 1, false};
        } else {
            return _reduced_quotient(n1 * n2, d1 * d2);
        }
    }
}();

// From / To is a whole number (that fits in intmax_t), so converting a count
// from precision From to precision To loses nothing.
template<typename From, typename To>
concept _harmonic_precision = _safe_ratio_divide<From, To>.fits && _safe_ratio_divide<From, To>.den == 1;

// Whether a conversion between two quantities of the same kind (whose reps
// are convertible) loses nothing, and so is implicit: to a floating-point
// rep, or to a precision that evenly divides the source's from a rep that is
// not floating-point. Every other such conversion is explicit.
namespace detail {
template<typename FromRep, typename FromPrecision, typename ToRep, typename ToPrecision>
concept _lossless_conversion =
    treat_as_inexact_v<ToRep> || (_harmonic_precision<FromPrecision, ToPrecision> && !treat_as_inexact_v<FromRep>);
} // namespace detail

consteval bool _is_power_of_ten(intmax_t n) {
    while (n >= 10 && n % 10 == 0) {
        n /= 10;
    }
    return n == 1;
}

consteval int _log10(intmax_t n) {
    int e = 0;
    while (n >= 10) {
        n /= 10;
        ++e;
    }
    return e;
}

template<typename Precision>
inline constexpr int _precision_exponent = _log10(Precision::num) - _log10(Precision::den);

// 10^k as a double, exact for |k| <= 22.
constexpr double _pow10(int k) {
    double r = 1.0;
    for (int i = 0; i < (k < 0 ? -k : k); ++i) {
        r *= 10.0;
    }
    return r;
}

// v x 10^k with a single rounding (exact power of ten for |k| <= 22).
constexpr double _scale_pow10(double v, int k) {
    return k >= 0 ? v * _pow10(k) : v / _pow10(-k);
}

// The shortest fixed-notation representation of v that round-trips: never
// scientific notation, and no trailing zeros (2.5 -> "2.5", 1e6 -> "1000000",
// 1e-15 -> "0.000000000000001"). Used wherever a floating-point value is
// rendered without a format spec, so to_string and std::format agree.
template<std::floating_point T>
std::string _fixed_string(T v) {
    std::string s(32, '\0');
    for (;;) {
        const auto result = std::to_chars(s.data(), s.data() + s.size(), v, std::chars_format::fixed);
        if (result.ec == std::errc()) {
            s.resize(static_cast<std::size_t>(result.ptr - s.data()));
            return s;
        }
        s.resize(s.size() * 2);
    }
}

// A count rendered without a format spec: integers exactly, floating-point
// counts in shortest round-trip fixed notation.
template<typename Rep>
std::string _count_string(const Rep& r) {
    if constexpr (std::floating_point<Rep>) {
        return _fixed_string(r);
    } else {
        return std::to_string(r);
    }
}

#if CONFIG_QUANTA_STD_FORMAT
// Appends a NUL-terminated string to a format output iterator.
template<typename OutputIt>
constexpr OutputIt _format_append(OutputIt out, const char* s) {
    for (; *s != '\0'; ++s) {
        *out++ = *s;
    }
    return out;
}

// Reports an unusable format spec from a formatter's parse().
//
// std::format constant-evaluates parse() to check the format string, so
// throwing there makes a bad spec a compile error rather than a runtime fault.
// Where exceptions are unavailable, calling a non-constexpr function fails that
// same constant evaluation and so reports the error at compile time too; a
// runtime parse (std::vformat with a runtime format string) has no way to
// report it and terminates.
[[noreturn]] inline void _format_error(const char* what) {
#if defined(__cpp_exceptions) && __cpp_exceptions
    throw std::format_error(what);
#else
    (void)what;
    std::abort();
#endif
}
#endif

// Internals shared by every domain: the parsing of integer literal characters,
// and the conversion, rounding and display arithmetic of quantities.
namespace detail {

#ifdef __SIZEOF_INT128__
// 128-bit integers for wide intermediates; __extension__ keeps -Wpedantic quiet.
__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;
#endif

// The integer type of the wide intermediates of an integral conversion whose
// reps have the common type Common with intmax_t: a 128-bit integer where the
// compiler has one, signed unless Common is unsigned, and Fallback elsewhere
// (e.g. MSVC and 32-bit targets). The fallback is a policy of the caller:
// Common itself keeps the intermediate as wide as the reps and of their
// signedness, while intmax_t keeps it signed.
template<typename Common, typename Fallback = Common>
using _wide_integer =
#ifdef __SIZEOF_INT128__
    std::conditional_t<std::is_signed_v<Common>, int128_t, uint128_t>;
#else
    Fallback;
#endif

// The integer type of the wide intermediates of an integral conversion between
// Rep1 and Rep2: unsigned when both reps are, signed otherwise, so that a
// negative count of a signed rep stays negative whatever the other rep is.
// 128-bit where the compiler has it, else uintmax_t or intmax_t.
template<typename Rep1, typename Rep2>
using _conversion_integer = std::conditional_t<
    std::is_unsigned_v<Rep1> && std::is_unsigned_v<Rep2>,
    _wide_integer<uintmax_t>,
    _wide_integer<intmax_t>>;

// The value of an integer literal's characters, following the C++ rules:
// digit separators ('), hexadecimal (0x, 0X), binary (0b, 0B), octal (a
// leading 0) or decimal. `valid` is false if a character is not a digit of the
// literal's base (as in a floating-point literal such as 1.5), and `overflow`
// is true if the value exceeds unsigned long long.
struct parsed_literal {
    unsigned long long value;
    bool valid;
    bool overflow;
};

template<char... Chars>
consteval parsed_literal parse_literal() {
    constexpr char chars[] = {Chars...};
    constexpr std::size_t n = sizeof...(Chars);
    unsigned base = 10;
    std::size_t i = 0;
    if (n > 2 && chars[0] == '0' && (chars[1] == 'x' || chars[1] == 'X')) {
        base = 16;
        i = 2;
    } else if (n > 2 && chars[0] == '0' && (chars[1] == 'b' || chars[1] == 'B')) {
        base = 2;
        i = 2;
    } else if (n > 1 && chars[0] == '0') {
        base = 8;
        i = 1;
    }
    parsed_literal result{0, true, false};
    for (; i < n; ++i) {
        const char c = chars[i];
        unsigned digit;
        if (c == '\'') {
            continue;
        } else if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned>(c - 'A' + 10);
        } else {
            digit = base; // not a digit in any base
        }
        if (digit >= base) {
            result.valid = false;
            return result;
        }
        if (result.value > (std::numeric_limits<unsigned long long>::max() - digit) / base) {
            result.overflow = true;
        }
        result.value = result.value * base + digit;
    }
    return result;
}

template<typename Quantity, char... Digits>
constexpr Quantity check_overflow() {
    constexpr parsed_literal parsed = parse_literal<Digits...>();
    static_assert(parsed.valid, "invalid digit");
    static_assert(!parsed.overflow, "literal value cannot be represented by unsigned long long");
    constexpr typename Quantity::rep repval = parsed.value;
    static_assert(
        repval >= 0 && static_cast<unsigned long long>(repval) == parsed.value,
        "literal value cannot be represented by the quantity type"
    );
    return Quantity(repval);
}

// count x Ratio as a double: the count, multiplied by the numerator and then
// divided by the denominator, each step skipped when it is 1. Every domain's
// base-unit form relies on this exact order of operations.
template<typename Ratio, typename Rep>
constexpr double _ratio_value(const Rep& count) {
    double v = static_cast<double>(count);
    if constexpr (Ratio::num != 1) {
        v *= static_cast<double>(Ratio::num);
    }
    if constexpr (Ratio::den != 1) {
        v /= static_cast<double>(Ratio::den);
    }
    return v;
}

// Precisions that are exactly 10^k of the base unit, for any integer k,
// whether or not they have an SI prefix (std::deci and std::deca do not).
template<typename Precision>
concept _power_of_ten_precision = _is_power_of_ten(Precision::num) && _is_power_of_ten(Precision::den);

// An entry of a scaled-form unit table: a unit 10^exponent times the base unit,
// and its label (an SI prefix, or a whole unit suffix).
struct _scale_entry {
    int exponent;
    const char* label;
};

// The SI prefixes from femto to tera, in ascending order.
inline constexpr _scale_entry _si_scale[] = {
    {-15, "f"},
    {-12, "p"},
    {-9, "n"},
    {-6, "µ"},
    {-3, "m"},
    {0, ""},
    {3, "k"},
    {6, "M"},
    {9, "G"},
    {12, "T"},
};

// A value in the unit of a scale-table entry.
struct _scaled {
    double value;
    const char* label;
};

// The value x x 10^exponent (of the base unit) in the largest unit of table
// (ascending by exponent) in which its magnitude is at least 1, else in the
// smallest. Each candidate is computed from x with a single scaling by a power
// of ten, so values that land exactly on a unit are exactly 1 in it. Zero and
// non-finite values are left to the caller.
template<std::size_t N>
_scaled _pick_scale(double x, int exponent, const _scale_entry (&table)[N]) {
    for (std::size_t i = N; i-- > 1;) {
        const double v = _scale_pow10(x, exponent - table[i].exponent);
        if (v >= 1.0 || v <= -1.0) {
            return {v, table[i].label};
        }
    }
    return {_scale_pow10(x, exponent - table[0].exponent), table[0].label};
}

// The precision of the std::common_type of two quantities of the same kind:
// the greatest ratio of which both precisions are whole multiples. The result
// is spelled as std::ratio<gcd of numerators, lcm of denominators>, from the
// reduced terms, and result types depend on that exact spelling.
template<typename Precision1, typename Precision2>
using _common_precision = std::ratio<
    _gcd(Precision1::num, Precision2::num),
    (Precision1::den / _gcd(Precision1::den, Precision2::den)) * Precision2::den>;

// The operations _quantity_cast performs, outside its integral path, to
// convert a FromRep count by the factor Factor (a std::ratio) to a ToRep
// count, with Common the common type of ToRep, FromRep and intmax_t. As in
// the cast, the count is a prvalue (count() returns it by value): converted
// to ToRep directly for a factor of 1, and through Common otherwise.
template<typename FromRep, typename ToRep, typename Factor, typename Common>
concept _cast_steps =
    (Factor::num == 1 && Factor::den == 1 && requires(FromRep (&count)()) { static_cast<ToRep>(count()); }) ||
    (Factor::num != 1 && Factor::den == 1 && requires(FromRep (&count)()) {
        static_cast<ToRep>(static_cast<Common>(count()) * static_cast<Common>(Factor::num));
    }) || (Factor::num == 1 && Factor::den != 1 && requires(FromRep (&count)()) {
        static_cast<ToRep>(static_cast<Common>(count()) / static_cast<Common>(Factor::den));
    }) || (Factor::num != 1 && Factor::den != 1 && requires(FromRep (&count)()) {
        static_cast<ToRep>(
            static_cast<Common>(count()) * static_cast<Common>(Factor::num) / static_cast<Common>(Factor::den)
        );
    });

template<typename FromRep, typename ToRep, typename Factor>
concept _cast_through_common = requires { typename std::common_type<ToRep, FromRep, intmax_t>::type; } &&
    _cast_steps<FromRep, ToRep, Factor, std::common_type_t<ToRep, FromRep, intmax_t>>;

// The requirements of _quantity_cast, read off its code and checked by its
// requires-clause: a plain copy to the same type; otherwise the conversion
// factor must fit in a std::ratio (computed without naming
// std::ratio_divide, whose overflow would be a hard error), and then integral
// reps convert through wide integers, any others through their common type
// with intmax_t. Only _quantity_cast and the public casts that call it
// (whose diagnostics then name the requirement that failed) are constrained
// on this: whatever else needs to know whether a conversion can be performed
// asks whether the cast can be called (_quantity_castable), so that the
// answer is the cast's own.
template<typename From, typename To>
concept _quantity_cast_requirements = std::is_same_v<To, From> ||
    (_safe_ratio_divide<typename From::precision, typename To::precision>.fits &&
     ((std::is_integral_v<typename From::rep> && std::is_integral_v<typename To::rep>) ||
      _cast_through_common<
          typename From::rep,
          typename To::rep,
          std::ratio<
              _safe_ratio_divide<typename From::precision, typename To::precision>.num,
              _safe_ratio_divide<typename From::precision, typename To::precision>.den>>));

// The conversion behind every domain's cast (delta_cast, frequency_cast,
// quantity_cast): From and To are quantities of the same kind, each with a
// count(), a rep and a precision, and To is constructible from a To::rep.
// Integer-to-integer conversions use wider intermediate types (128-bit when
// available) to minimize overflow risk during ratio arithmetic.
template<typename To, typename From>
    requires _quantity_cast_requirements<From, To>
constexpr To _quantity_cast(const From& q) {
    if constexpr (std::is_same_v<To, From>) {
        return q;
    } else {
        using rep = typename From::rep;
        using to_rep = typename To::rep;
        using cf = std::ratio_divide<typename From::precision, typename To::precision>;

        // Use wider intermediate type for integer-to-integer conversions
        if constexpr (std::is_integral_v<rep> && std::is_integral_v<to_rep>) {
            using wider_t = _conversion_integer<rep, to_rep>;

            if constexpr (cf::den == 1 && cf::num == 1) {
                return To(static_cast<to_rep>(q.count()));
            } else if constexpr (cf::den == 1) {
                wider_t result = static_cast<wider_t>(q.count()) * static_cast<wider_t>(cf::num);
                return To(static_cast<to_rep>(result));
            } else if constexpr (cf::num == 1) {
                wider_t result = static_cast<wider_t>(q.count()) / static_cast<wider_t>(cf::den);
                return To(static_cast<to_rep>(result));
            } else {
                // Use GCD to reduce operands: count * num / den
                // Compute g = gcd(count, den), then (count/g) * num / (den/g)
                wider_t count = static_cast<wider_t>(q.count());
                wider_t den = static_cast<wider_t>(cf::den);
                wider_t g = _runtime_gcd(count, den);
                wider_t reduced_count = count / g;
                wider_t reduced_den = den / g;
                wider_t result = reduced_count * static_cast<wider_t>(cf::num) / reduced_den;
                return To(static_cast<to_rep>(result));
            }
        } else {
            // Floating-point path
            using cr = std::common_type_t<to_rep, rep, intmax_t>;
            if constexpr (cf::den == 1 && cf::num == 1) {
                return To(static_cast<to_rep>(q.count()));
            } else if constexpr (cf::den == 1) {
                return To(static_cast<to_rep>(static_cast<cr>(q.count()) * static_cast<cr>(cf::num)));
            } else if constexpr (cf::num == 1) {
                return To(static_cast<to_rep>(static_cast<cr>(q.count()) / static_cast<cr>(cf::den)));
            } else {
                return To(
                    static_cast<to_rep>(
                        static_cast<cr>(q.count()) * static_cast<cr>(cf::num) / static_cast<cr>(cf::den)
                    )
                );
            }
        }
    }
}

// Whether _quantity_cast can convert a From to a To. The converting
// constructors are constrained on this, so that a conversion is offered
// exactly when the cast can perform it.
template<typename From, typename To>
concept _quantity_castable = requires(const From& q) { _quantity_cast<To>(q); };

// The three-way comparison of a and b, two quantities of the same kind, as
// negative, zero or positive. Integral reps are compared exactly, as counts of
// their common precision in a wide integer, so that narrow reps cannot
// overflow (the quantities' own comparison converts both to their common
// type, whose rep is the narrower common type of the reps); any others as the
// quantities compare. The wide integer is that of the conversions
// (_conversion_integer): signed unless both reps are unsigned, so that a
// negative count compares correctly with an unsigned one. When the cast
// between the two precisions is possible, each scale fits in intmax_t, so a
// 64-bit count times its scale fits in 128 bits; without a 128-bit integer,
// the comparison is exact for reps narrower than 64 bits.
template<typename Q1, typename Q2>
constexpr int _compare(const Q1& a, const Q2& b) {
    using rep1 = typename Q1::rep;
    using rep2 = typename Q2::rep;

    if constexpr (std::is_integral_v<rep1> && std::is_integral_v<rep2>) {
        // Each precision is a whole multiple of the common one.
        using cp = _common_precision<typename Q1::precision, typename Q2::precision>;
        using scale1 = std::ratio_divide<typename Q1::precision, cp>;
        using scale2 = std::ratio_divide<typename Q2::precision, cp>;
        static_assert(scale1::den == 1 && scale2::den == 1);
        using wide = _conversion_integer<rep1, rep2>;
        const wide x = static_cast<wide>(a.count()) * static_cast<wide>(scale1::num);
        const wide y = static_cast<wide>(b.count()) * static_cast<wide>(scale2::num);
        return x < y ? -1 : x > y ? 1 : 0;
    } else {
        const auto c = a <=> b;
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    }
}

// floor<To>(q), given cast, the domain's cast of q to To: the largest To not
// greater than q. A To whose rep is not integral is not rounded.
template<typename To, typename From>
constexpr To _floor_cast(const To& cast, const From& q) {
    using to_rep = typename To::rep;

    if constexpr (std::is_integral_v<to_rep>) {
        if (_compare(cast, q) > 0) {
            return To(cast.count() - to_rep(1));
        }
    }

    return cast;
}

// ceil<To>(q), given cast, the domain's cast of q to To: the smallest To not
// less than q. A To whose rep is not integral is not rounded.
template<typename To, typename From>
constexpr To _ceil_cast(const To& cast, const From& q) {
    using to_rep = typename To::rep;

    if constexpr (std::is_integral_v<to_rep>) {
        if (_compare(cast, q) < 0) {
            return To(cast.count() + to_rep(1));
        }
    }

    return cast;
}

// round<To>(q), given floor_of_q, which returns the domain's floor<To>(q),
// and is called only where it is needed: the To nearest to q, ties to even. A
// To whose rep is not integral is not rounded (floor is then a plain cast).
template<typename To, typename From, typename Floor>
constexpr To _round_cast(const From& q, Floor floor_of_q) {
    using to_rep = typename To::rep;
    using from_rep = typename From::rep;

    if constexpr (std::is_integral_v<to_rep> && std::is_integral_v<from_rep>) {
        // Round exactly in a wide integer, so that narrow reps cannot
        // overflow; only the result converts to the target rep (floor's
        // result, where q rounded down does not fit the target rep, has
        // wrapped). In the common precision, of which each precision is a
        // whole multiple, q is count * from_scale and a target tick is
        // to_scale. Splitting count by to_scale first keeps the products
        // within the result's magnitude where the operands are large.
        using cp = _common_precision<typename From::precision, typename To::precision>;
        using from_scale = std::ratio_divide<typename From::precision, cp>;
        using to_scale = std::ratio_divide<typename To::precision, cp>;
        static_assert(from_scale::den == 1 && to_scale::den == 1);
        using wide = _conversion_integer<from_rep, to_rep>;
        constexpr bool is_signed = !(std::is_unsigned_v<from_rep> && std::is_unsigned_v<to_rep>);
        const wide scale = static_cast<wide>(from_scale::num);
        const wide tick = static_cast<wide>(to_scale::num);

        // count = whole * tick + part, with part in [0, tick)
        const wide count = static_cast<wide>(q.count());
        wide whole = count / tick;
        wide part = count % tick;
        if constexpr (is_signed) {
            if (part < 0) {
                whole -= 1;
                part += tick;
            }
        }

        // q rounded down, in target ticks, and what remains, in [0, tick)
        const wide scaled_part = part * scale;
        const wide down = whole * scale + scaled_part / tick;
        const wide rem = scaled_part % tick;

        // Nearer to down or to the tick above it; a tie goes to the even one.
        const wide to_up = tick - rem;
        const bool up = rem > to_up || (rem == to_up && down % 2 != 0);
        return To(static_cast<to_rep>(up ? down + 1 : down));
    } else if constexpr (std::is_integral_v<to_rep>) {
        const To lower = floor_of_q();
        const To upper = lower + To(to_rep(1));
        const auto diff_lower = q - lower;
        const auto diff_upper = upper - q;
        if (diff_lower < diff_upper) {
            return lower;
        } else if (diff_lower > diff_upper) {
            return upper;
        } else {
            // Tie: round to even
            return (lower.count() % to_rep(2) == to_rep(0)) ? lower : upper;
        }
    } else {
        return floor_of_q();
    }
}

// The absolute value of a quantity, in its own type. Unary minus gives the
// reduced precision, so it is converted back explicitly: a precision not in
// lowest terms would otherwise make the conditional's operand types convert
// both ways, and so be ambiguous.
template<typename Q>
constexpr Q _abs(const Q& q) {
    return q >= Q::zero() ? q : Q(-q);
}

#if CONFIG_QUANTA_STD_FORMAT
// The units a format spec can select with a leading marker letter.
enum class _unit_form { own, power, base, scaled };

// Consumes a leading marker letter of a format spec, if it is one of markers
// ('p' power form, 'b' base unit, 's' scaled), and returns the unit form it
// selects (own if there is none). A spec starting with a marker letter always
// starts with the marker, so these letters cannot be the fill character of a
// spec that starts with a fill.
constexpr _unit_form _parse_unit_marker(std::format_parse_context& ctx, const char* markers) {
    auto it = ctx.begin();
    if (it == ctx.end()) {
        return _unit_form::own;
    }
    for (const char* m = markers; *m != '\0'; ++m) {
        if (*it == *m) {
            ctx.advance_to(++it);
            return *m == 'p' ? _unit_form::power : *m == 'b' ? _unit_form::base : _unit_form::scaled;
        }
    }
    return _unit_form::own;
}

// The std::formatter implementation shared by the quantities of thermo,
// frequency, electro and spatial; see each domain's formatter specializations
// for the spec grammar. With no marker, "{}" renders the exact count and any
// other spec the count as a double, followed by the type's own unit; a marker
// selects another unit. Traits describes the formatted type:
// - markers: the marker letters accepted ("bs", or "pbs" with the power form)
// - has_own_unit: whether the type has a unit label of its own. If not, "{}"
//   is a format error, raised by no_own_unit_error() (which calls
//   _format_error with the domain's message, so that the message shows at
//   the point of the diagnostic), and any other spec without a marker
//   renders in the base unit.
// - own_unit(power): the own unit (or its power form), obtained before
//   anything is written; append_own_unit(out, own_unit) appends it
// - base_display(q), scaled_display(q): the value (as .value) and label in
//   the base unit or the scaled unit; append_label(out, display) appends the
//   label
template<typename Traits>
struct _unit_formatter {
private:
    std::formatter<double> _num;
    bool _has_spec = false;
    _unit_form _form = _unit_form::own;

public:
    constexpr auto parse(std::format_parse_context& ctx) {
        _form = _parse_unit_marker(ctx, Traits::markers);
        auto it = ctx.begin();
        if (it == ctx.end() || *it == '}') {
            if constexpr (!Traits::has_own_unit) {
                if (_form == _unit_form::own) {
                    Traits::no_own_unit_error();
                }
            }
            return it;
        }
        _has_spec = true;
        return _num.parse(ctx);
    }

    template<typename Q, typename FormatContext>
    auto format_quantity(const Q& q, FormatContext& ctx) const {
        if constexpr (Traits::has_own_unit) {
            if (_form == _unit_form::own || _form == _unit_form::power) {
                const auto unit = Traits::own_unit(_form == _unit_form::power);
                auto out = ctx.out();
                if (_has_spec) {
                    out = _num.format(static_cast<double>(q.count()), ctx);
                } else if constexpr (std::floating_point<typename Q::rep>) {
                    out = _format_append(out, _fixed_string(q.count()).c_str());
                } else {
                    out = std::format_to(out, "{}", q.count());
                }
                return Traits::append_own_unit(out, unit);
            }
        }
        // Without a unit of its own, the own form renders in the base unit.
        const auto d = _form == _unit_form::scaled ? Traits::scaled_display(q) : Traits::base_display(q);
        auto out = _has_spec ? _num.format(d.value, ctx) : _format_append(ctx.out(), _fixed_string(d.value).c_str());
        return Traits::append_label(out, d);
    }
};
#endif

} // namespace detail
/** @endcond */

} // namespace quanta
