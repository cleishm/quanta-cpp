#pragma once

/**
 * @file proportion.hpp
 * @brief Proportion (dimensionless ratio) types and arithmetic, modeled after std::chrono.
 */

#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <quanta/core.hpp>
#include <ratio>
#include <string>
#include <type_traits>

/**
 * @brief Proportion types and utilities.
 *
 * This library provides type-safe handling of proportions, dimensionless
 * linear ratios such as a duty cycle, a tolerance or a concentration, with
 * support for multiple precisions (1, %, ‰, ppm, ppb), following the design
 * of std::chrono::duration. Percent, permille, ppm and ppb are precisions of
 * the one type, as milliseconds and seconds are of std::chrono::duration.
 *
 * ## Basic Usage
 *
 * The library provides standard integer-based proportion types:
 * @code
 * using namespace quanta::proportion;
 * using namespace quanta::proportion_literals;
 *
 * percent duty{25};
 * ppm tolerance{50};
 * auto drift = 20_ppm;
 * auto half = proportion<double>{0.5};  // a plain fraction
 * @endcode
 *
 * ## Mixing Precisions
 *
 * Proportions of different precisions mix and convert exactly as durations
 * do: lossless conversions are implicit, lossy ones explicit, and arithmetic
 * between them is in their common precision.
 *
 * @code
 * ppm p = 1_pct;                     // 10000 ppm (lossless)
 * percent q = percent(12345_ppm);    // 1% (lossy, truncates)
 * auto sum = 1_pct + 50_ppm;         // ppm(10050)
 * @endcode
 *
 * Multiplying two proportions gives a proportion whose precision is the
 * product of theirs, so the result is exact for integer representations:
 *
 * @code
 * auto both = 50_pct * 50_pct;  // proportion<int64_t, std::ratio<1, 10000>>(2500), 25%
 * @endcode
 *
 * ## Integer vs Floating-Point
 *
 * - **Integer types** (default): Exact arithmetic, support modulo operations
 * - **Floating-point types**: Fractional precision, natural for calculations
 *   involving division, but no modulo operations
 *
 * Conversions between integer and floating-point follow the same rules as
 * precision conversions (implicit when lossless, explicit when lossy).
 *
 * ## Formatting
 *
 * to_string() and std::format show a proportion in its own unit by default,
 * whatever the format spec: `{}` prints the exact count and `{:.1f}` the count
 * as a double, each followed by the precision's symbol (`%`, `‰`, `ppm`,
 * `ppb`, or none for a plain fraction). A single leading marker letter in the
 * spec, or a tag argument to to_string(), opts into a different unit:
 *
 * @code
 * std::format("{}", percent(25));       // "25%"     own unit
 * std::format("{:.1f}", percent(25));   // "25.0%"   own unit
 * std::format("{:b}", percent(25));     // "0.25"    base unit (a plain fraction)
 * std::format("{:s}", ppm(200));        // "200ppm"  scaled
 * std::format("{:s}", permille(4));     // "0.4%"
 * to_string(ppb(3), scaled);            // "3ppb"
 * @endcode
 *
 * A precision with no symbol (e.g. `std::ratio<1, 10000>`) has no unit of its
 * own: `{}` and to_string(p) do not compile for it, and a format spec without
 * a marker renders the value as a plain fraction.
 */
namespace quanta::proportion {

// The shared trait, re-exported for convenience; specialize quanta::treat_as_inexact.
using quanta::treat_as_inexact;
using quanta::treat_as_inexact_v;

template<typename Rep, typename Precision = std::ratio<1>>
class proportion;

/**
 * @brief Trait to detect proportion specializations.
 * @tparam T Type to check.
 */
template<typename T>
struct is_proportion : std::false_type {};

template<typename Rep, typename Precision>
struct is_proportion<proportion<Rep, Precision>> : std::true_type {};

template<typename T>
inline constexpr bool is_proportion_v = is_proportion<T>::value;

/** @cond INTERNAL */
template<typename Rep>
concept not_proportion = !_is_specialization_of_v<Rep, proportion>;

/**
 * @brief Provides special values for proportion representations.
 * @tparam Rep The representation type.
 */
template<typename Rep>
struct proportion_values {
    /** @brief Returns a zero proportion. */
    static constexpr Rep zero() noexcept { return Rep(0); }

    /** @brief Returns the maximum representable proportion. */
    static constexpr Rep max() noexcept { return std::numeric_limits<Rep>::max(); }

    /** @brief Returns the minimum representable proportion. */
    static constexpr Rep min() noexcept { return std::numeric_limits<Rep>::lowest(); }
};

/** @endcond */

/**
 * @brief A proportion value with a representation and precision.
 *
 * Similar to std::chrono::duration, proportion represents a dimensionless
 * linear ratio. The precision is expressed as a std::ratio of the whole (1):
 * std::ratio<1, 100> counts percent, std::ratio<1, 1000000> parts per
 * million.
 *
 * ## Basic Examples
 *
 * @code
 * // Integer proportions (default)
 * percent duty{25};
 * ppm tolerance{50};
 * auto drift = 20_ppm;
 *
 * // Arithmetic
 * auto sum = 25_pct + 50_permille;  // permille(300)
 * auto scaled = 5_pct * 3;          // 15%
 * auto ratio = 50_pct / 25_pct;     // 2 (scalar)
 * auto both = 50_pct * 50_pct;      // 25%, at precision 1/10000
 * @endcode
 *
 * ## Floating-Point Examples
 *
 * @code
 * // A plain fraction has precision 1
 * proportion<double> efficiency{0.875};
 * proportion<double, std::ratio<1, 100>> share{12.5};  // 12.5%
 *
 * // Implicit lossless conversion to a floating-point rep
 * proportion<double> fraction = 25_pct;  // 0.25
 * @endcode
 *
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1).
 */
template<typename Rep, typename Precision>
class proportion {
    static_assert(!is_proportion<Rep>::value, "rep cannot be a quanta::proportion::proportion");
    static_assert(_is_ratio<Precision>::value, "precision must be a specialization of std::ratio");
    static_assert(Precision::num > 0, "precision must be positive");

public:
    /** @brief The representation type. */
    using rep = Rep;
    /** @brief The precision as a std::ratio. */
    using precision = typename Precision::type;

    /** @brief Constructs a zero proportion. */
    constexpr proportion() = default;
    proportion(const proportion&) = default;

    /**
     * @brief Constructs from a tick count.
     *
     * Implicit for inexact types, explicit otherwise to prevent accidental
     * precision loss.
     *
     * @tparam Rep2 The source representation type.
     * @param r     The tick count.
     */
    template<typename Rep2>
        requires std::convertible_to<const Rep2&, rep> && (treat_as_inexact_v<rep> || !treat_as_inexact_v<Rep2>)
    constexpr explicit proportion(const Rep2& r)
        : _r(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from another proportion with different representation or precision.
     *
     * Implicit when the conversion is lossless (the target rep is
     * floating-point, or the target precision evenly divides the source
     * precision and the source rep is not floating-point) and the source rep
     * converts implicitly to the target rep. Any other conversion that the
     * cast can perform is explicit (see below).
     *
     * @tparam Rep2       Source representation type.
     * @tparam Precision2 Source precision.
     * @param p           The source proportion.
     */
    template<typename Rep2, typename Precision2>
        requires std::convertible_to<const Rep2&, rep> &&
        detail::_lossless_conversion<Rep2, Precision2, rep, precision> &&
        detail::_quantity_castable<proportion<Rep2, Precision2>, proportion>
    constexpr proportion(const proportion<Rep2, Precision2>& p)
        : _r(proportion_cast<proportion>(p).count()) {}

    /**
     * @brief Explicit constructor for lossy conversions.
     *
     * Every conversion that the cast can perform and that is not implicit
     * (see above) is explicit: a lossy one (to a coarser precision, or from a
     * floating-point rep to an integral one), or one to a rep that the source
     * rep converts to only explicitly. The value is that of the domain's cast,
     * truncated toward zero. The cast can perform the conversion when the
     * conversion factor between the precisions fits in a std::ratio and the
     * reps are integral or have a common type with intmax_t through which the
     * count converts.
     *
     * @tparam Rep2       Source representation type.
     * @tparam Precision2 Source precision.
     * @param p           The source proportion.
     */
    template<typename Rep2, typename Precision2>
        requires(!detail::_lossless_conversion<Rep2, Precision2, rep, precision> ||
                 !std::convertible_to<const Rep2&, rep>) &&
        detail::_quantity_castable<proportion<Rep2, Precision2>, proportion>
    constexpr explicit proportion(const proportion<Rep2, Precision2>& p)
        : _r(proportion_cast<proportion>(p).count()) {}

    ~proportion() = default;
    proportion& operator=(const proportion&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _r; }

    constexpr proportion<typename std::common_type<rep>::type, precision> operator+() const {
        return proportion<typename std::common_type<rep>::type, precision>(_r);
    }

    constexpr proportion<typename std::common_type<rep>::type, precision> operator-() const {
        return proportion<typename std::common_type<rep>::type, precision>(-_r);
    }

    constexpr proportion& operator++() {
        ++_r;
        return *this;
    }

    constexpr proportion operator++(int) { return proportion(_r++); }

    constexpr proportion& operator--() {
        --_r;
        return *this;
    }

    constexpr proportion operator--(int) { return proportion(_r--); }

    constexpr proportion& operator+=(const proportion& p) {
        _r += p.count();
        return *this;
    }

    constexpr proportion& operator-=(const proportion& p) {
        _r -= p.count();
        return *this;
    }

    constexpr proportion& operator*=(const rep& r) {
        _r *= r;
        return *this;
    }

    constexpr proportion& operator/=(const rep& r) {
        _r /= r;
        return *this;
    }

    constexpr proportion& operator%=(const rep& r)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= r;
        return *this;
    }

    constexpr proportion& operator%=(const proportion& p)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= p.count();
        return *this;
    }

    /** @brief Returns a zero proportion. */
    static constexpr proportion zero() noexcept { return proportion(proportion_values<rep>::zero()); }

    /** @brief Returns the minimum representable proportion. */
    static constexpr proportion min() noexcept { return proportion(proportion_values<rep>::min()); }

    /** @brief Returns the maximum representable proportion. */
    static constexpr proportion max() noexcept { return proportion(proportion_values<rep>::max()); }

private:
    rep _r{};
};

/**
 * @brief Converts a proportion to a different precision or representation.
 *
 * For integer-to-integer conversions, this function uses wider intermediate types
 * (128-bit when available) to minimize overflow risk during ratio arithmetic.
 *
 * @tparam ToProportion The target proportion type.
 * @tparam Rep          Source representation type.
 * @tparam Precision    Source precision.
 * @param p             The proportion to convert.
 *
 * @return The converted proportion.
 *
 * @code
 * using namespace quanta::proportion;
 * auto pct = proportion_cast<percent>(ppm(12345));  // 1% (truncated from 1.2345)
 * @endcode
 */
template<typename ToProportion, typename Rep, typename Precision>
    requires detail::_quantity_cast_requirements<proportion<Rep, Precision>, ToProportion>
constexpr ToProportion proportion_cast(const proportion<Rep, Precision>& p) {
    return detail::_quantity_cast<ToProportion>(p);
}

/**
 * @brief Converts a proportion to the target type, rounding toward negative infinity.
 *
 * This function performs a proportion conversion with floor rounding semantics.
 * When converting to a coarser precision, values are rounded down.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToProportion The target proportion type.
 * @tparam Rep          Source representation type.
 * @tparam Precision    Source precision.
 * @param p             The proportion to convert.
 *
 * @return The converted proportion, rounded toward negative infinity.
 *
 * @code
 * using namespace quanta::proportion;
 * permille pm{15};
 * auto pct = floor<percent>(pm);  // 1% (rounded down from 1.5)
 * @endcode
 */
template<typename ToProportion, typename Rep, typename Precision>
constexpr ToProportion floor(const proportion<Rep, Precision>& p) {
    return detail::_floor_cast(proportion_cast<ToProportion>(p), p);
}

/**
 * @brief Converts a proportion to the target type, rounding toward positive infinity.
 *
 * This function performs a proportion conversion with ceiling rounding semantics.
 * When converting to a coarser precision, values are rounded up.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToProportion The target proportion type.
 * @tparam Rep          Source representation type.
 * @tparam Precision    Source precision.
 * @param p             The proportion to convert.
 *
 * @return The converted proportion, rounded toward positive infinity.
 *
 * @code
 * using namespace quanta::proportion;
 * permille pm{15};
 * auto pct = ceil<percent>(pm);  // 2% (rounded up from 1.5)
 * @endcode
 */
template<typename ToProportion, typename Rep, typename Precision>
constexpr ToProportion ceil(const proportion<Rep, Precision>& p) {
    return detail::_ceil_cast(proportion_cast<ToProportion>(p), p);
}

/**
 * @brief Converts a proportion to the target type, rounding toward zero.
 *
 * The same as proportion_cast<ToProportion>(p).
 *
 * @tparam ToProportion The target proportion type.
 * @tparam Rep          Source representation type.
 * @tparam Precision    Source precision.
 * @param p             The proportion to convert.
 *
 * @return The converted proportion, rounded toward zero.
 *
 * @code
 * using namespace quanta::proportion;
 * auto pct = trunc<percent>(permille(-15));  // -1% (rounded toward zero from -1.5)
 * @endcode
 */
template<typename ToProportion, typename Rep, typename Precision>
constexpr ToProportion trunc(const proportion<Rep, Precision>& p) {
    return proportion_cast<ToProportion>(p);
}

/**
 * @brief Converts a proportion to the target type, rounding to nearest (ties to even).
 *
 * This function performs a proportion conversion with round-to-nearest rounding semantics.
 * When converting to a coarser precision, values are rounded to the nearest representable
 * value, with ties rounded to even.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToProportion The target proportion type.
 * @tparam Rep          Source representation type.
 * @tparam Precision    Source precision.
 * @param p             The proportion to convert.
 *
 * @return The converted proportion, rounded to nearest.
 *
 * @code
 * using namespace quanta::proportion;
 * auto a = round<percent>(permille(15));  // 2% (rounded to even)
 * auto b = round<percent>(permille(25));  // 2% (rounded to even)
 * @endcode
 */
template<typename ToProportion, typename Rep, typename Precision>
constexpr ToProportion round(const proportion<Rep, Precision>& p) {
    return detail::_round_cast<ToProportion>(p, [&p] { return floor<ToProportion>(p); });
}

/**
 * @brief Returns the absolute value of a proportion.
 *
 * @tparam Rep       Representation type.
 * @tparam Precision Precision type.
 * @param p          The proportion.
 *
 * @return The absolute value of the proportion.
 *
 * @code
 * using namespace quanta::proportion;
 * percent change{-5};
 * auto magnitude = abs(change);  // 5%
 * @endcode
 */
template<typename Rep, typename Precision>
constexpr proportion<Rep, Precision> abs(const proportion<Rep, Precision>& p) {
    return detail::_abs(p);
}

/** @brief Returns the sum of two proportions. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator+(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs)
    -> std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>> {
    using cp = std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>>;
    return cp(cp(lhs).count() + cp(rhs).count());
}

/** @brief Returns the difference of two proportions. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator-(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs)
    -> std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>> {
    using cp = std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>>;
    return cp(cp(lhs).count() - cp(rhs).count());
}

/** @brief Multiplies a proportion by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_proportion<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const proportion<Rep1, Precision>& p, const Rep2& r)
    -> proportion<std::common_type_t<Rep1, Rep2>, Precision> {
    using cp = proportion<std::common_type_t<Rep1, Rep2>, Precision>;
    return cp(cp(p).count() * r);
}

/** @brief Multiplies a scalar by a proportion. */
template<typename Rep1, typename Rep2, typename Precision>
    requires not_proportion<Rep1> && std::convertible_to<const Rep1&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const Rep1& r, const proportion<Rep2, Precision>& p)
    -> proportion<std::common_type_t<Rep1, Rep2>, Precision> {
    return p * r;
}

/**
 * @brief Multiplies two proportions, returning a proportion.
 *
 * Counts multiply and precisions multiply, so no conversion factor is needed
 * and the result is exact for integer representations: the product of two
 * percentages has a precision of 1/10000.
 *
 * A product whose precision cannot be represented as a std::ratio (such as
 * ppb x ppb x ppb, 10⁻²⁷) is a compile-time error.
 *
 * @code
 * auto both = 50_pct * 50_pct;  // proportion<int64_t, std::ratio<1, 10000>>(2500), 25%
 * percent p = percent(both);    // 25%
 * @endcode
 */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator*(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs) {
    using cr = std::common_type_t<Rep1, Rep2>;
    using result_t = proportion<cr, std::ratio_multiply<Precision1, Precision2>>;
    return result_t(static_cast<cr>(lhs.count()) * static_cast<cr>(rhs.count()));
}

/** @brief Divides a proportion by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_proportion<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator/(const proportion<Rep1, Precision>& p, const Rep2& s)
    -> proportion<std::common_type_t<Rep1, Rep2>, Precision> {
    using cp = proportion<std::common_type_t<Rep1, Rep2>, Precision>;
    return cp(cp(p).count() / s);
}

/** @brief Divides two proportions, returning a scalar. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator/(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs)
    -> std::common_type_t<Rep1, Rep2> {
    using cp = std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>>;
    return cp(lhs).count() / cp(rhs).count();
}

/** @brief Returns the remainder of dividing a proportion by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_proportion<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>> &&
    (!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const proportion<Rep1, Precision>& p, const Rep2& s)
    -> proportion<std::common_type_t<Rep1, Rep2>, Precision> {
    using cp = proportion<std::common_type_t<Rep1, Rep2>, Precision>;
    return cp(cp(p).count() % s);
}

/** @brief Returns the remainder of dividing two proportions. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires(!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs)
    -> std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>> {
    using cp = std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>>;
    return cp(cp(lhs).count() % cp(rhs).count());
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr bool operator==(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>>;
    return ct(lhs).count() == ct(rhs).count();
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires std::three_way_comparable<std::common_type_t<Rep1, Rep2>>
constexpr auto operator<=>(const proportion<Rep1, Precision1>& lhs, const proportion<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<proportion<Rep1, Precision1>, proportion<Rep2, Precision2>>;
    return ct(lhs).count() <=> ct(rhs).count();
}

/**
 * @defgroup ProportionTypes Standard Proportion Type Aliases
 * @{
 *
 * Standard integer-based proportion types using int64_t representation.
 * A plain fraction (precision 1) has no alias: write proportion<double>.
 * For floating-point representations, use proportion<double, Precision>.
 *
 * @code
 * // Integer types (default)
 * percent duty{25};
 * permille slope{12};
 * ppm tolerance{50};
 * ppb impurity{3};
 *
 * // Floating-point types (custom)
 * using percent_d = proportion<double, std::ratio<1, 100>>;
 *
 * proportion<double> fraction{0.25};
 * percent_d share{12.5};
 * @endcode
 */

/** @brief Proportion with 1/100 (percent) precision. */
using percent = proportion<int64_t, std::ratio<1, 100>>;
/** @brief Proportion with 1/1000 (permille) precision. */
using permille = proportion<int64_t, std::ratio<1, 1000>>;
/** @brief Proportion with 1/1,000,000 (parts per million) precision. */
using ppm = proportion<int64_t, std::ratio<1, 1'000'000>>;
/** @brief Proportion with 1/1,000,000,000 (parts per billion) precision. */
using ppb = proportion<int64_t, std::ratio<1, 1'000'000'000>>;

/** @} */ // end of ProportionTypes group

/** @cond INTERNAL */
// Unit symbol for a proportion-per-count ratio: none ("") for a plain
// fraction; nullptr when unmapped.
template<typename Ratio>
struct _unit_symbol {
    static constexpr const char* value = nullptr;
};
template<>
struct _unit_symbol<std::ratio<1>> {
    static constexpr const char* value = "";
};
template<>
struct _unit_symbol<std::ratio<1, 100>> {
    static constexpr const char* value = "%";
};
template<>
struct _unit_symbol<std::ratio<1, 1000>> {
    static constexpr const char* value = "‰";
};
template<>
struct _unit_symbol<std::ratio<1, 1'000'000>> {
    static constexpr const char* value = "ppm";
};
template<>
struct _unit_symbol<std::ratio<1, 1'000'000'000>> {
    static constexpr const char* value = "ppb";
};
template<typename Ratio>
inline constexpr const char* _unit_symbol_v = _unit_symbol<typename Ratio::type>::value;

// A value and unit symbol to display, for the base-unit and scaled forms.
struct _display {
    double value;
    const char* symbol;
};

// Precisions that are exactly a power of ten of the whole, whether or not
// they have a symbol (std::ratio<1, 10000> does not).
template<typename Precision>
concept _decimal_precision = detail::_power_of_ten_precision<Precision>;

// The proportion as a plain fraction, as a double.
template<typename Rep, typename Precision>
_display _base_unit_display(const proportion<Rep, Precision>& p) {
    using pr = typename proportion<Rep, Precision>::precision;
    return {detail::_ratio_value<pr>(p.count()), _unit_symbol_v<std::ratio<1>>};
}

// The units of the scaled form below percent, which _pick_scale chooses
// between: the largest in which the magnitude is at least 1, else ppb.
inline constexpr detail::_scale_entry _scale_below_percent[] = {{-9, "ppb"}, {-6, "ppm"}};

// The proportion in percent if its magnitude is at least 0.001 (0.1%), else in
// ppm if it is at least 1e-6, else in ppb; permille is never chosen. Zero
// (keeping its sign) and non-finite values are shown as a plain fraction, as
// the base-unit form would show them. For a power-of-ten precision each
// candidate's value is computed from the count with a single scaling by a
// power of ten, so values that land exactly on a unit (1000 ppb) are exactly 1
// in it; other precisions are scaled from their plain-fraction value.
//
// Percent is chosen a decade below the magnitude at which it reaches 1, which
// _pick_scale cannot express, so it is tested first; only the units below it
// go through _pick_scale.
template<typename Rep, typename Precision>
_display _scaled_display(const proportion<Rep, Precision>& p) {
    using pr = typename proportion<Rep, Precision>::precision;
    double x;
    int exponent;
    if constexpr (_decimal_precision<pr>) {
        x = static_cast<double>(p.count());
        exponent = _precision_exponent<pr>;
    } else {
        x = _base_unit_display(p).value;
        exponent = 0;
    }
    if (x == 0 || !std::isfinite(x)) {
        return {x, _unit_symbol_v<std::ratio<1>>};
    }
    const double tenths_of_percent = _scale_pow10(x, exponent + 3);
    if (tenths_of_percent >= 1.0 || tenths_of_percent <= -1.0) {
        return {_scale_pow10(x, exponent + 2), _unit_symbol_v<std::ratio<1, 100>>};
    }
    const detail::_scaled s = detail::_pick_scale(x, exponent, _scale_below_percent);
    return {s.value, s.label};
}

#if CONFIG_QUANTA_STD_FORMAT
// How std::format renders a proportion of the given (reduced) precision; see
// detail::_unit_formatter. A precision with no symbol has no own unit.
template<typename Precision>
struct _format_traits {
    static constexpr const char* markers = "bs";
    static constexpr bool has_own_unit = _unit_symbol_v<Precision> != nullptr;

    // Reports "{}" for a precision with no own unit. The parameter only gives
    // the function a path that is a constant expression, as a constexpr
    // function needs; the formatter always calls it with true.
    static constexpr void no_own_unit_error(bool fail = true) {
        if (fail) {
            _format_error("proportion: precision has no unit symbol; use an explicit format spec");
        }
    }

    // The own unit is written from constants; there is nothing to obtain.
    static constexpr bool own_unit(bool power) { return power; }

    template<typename OutputIt>
    static OutputIt append_own_unit(OutputIt out, bool) {
        return _format_append(out, _unit_symbol_v<Precision>);
    }

    template<typename P>
    static _display base_display(const P& p) {
        return _base_unit_display(p);
    }

    template<typename P>
    static _display scaled_display(const P& p) {
        return _scaled_display(p);
    }

    template<typename OutputIt>
    static OutputIt append_label(OutputIt out, const _display& d) {
        return _format_append(out, d.symbol);
    }
};
#endif

/** @endcond */

/**
 * @brief Selects the base-unit form in to_string().
 *
 * The proportion is shown as a plain fraction, with the value (count x
 * precision) rendered in the shortest fixed notation that round-trips (never
 * scientific), exactly as by the `b` format marker.
 *
 * @code
 * to_string(percent(25), base_unit);  // "0.25"
 * to_string(ppm(50), base_unit);      // "0.00005"
 * to_string(percent(150), base_unit); // "1.5"
 * @endcode
 */
using quanta::base_unit;
using quanta::base_unit_t;

/**
 * @brief Selects the scaled form in to_string().
 *
 * The proportion is shown in % if its magnitude is at least 0.001 (0.1%),
 * else in ppm if it is at least 0.000001, else in ppb; ‰ is never chosen, and
 * zero is shown as a plain fraction. This applies to every precision,
 * including those with no symbol of their own. The value is rendered in the
 * shortest fixed notation that round-trips (never scientific), exactly as by
 * the `s` format marker.
 *
 * @code
 * to_string(proportion<double>(0.5), scaled);  // "50%"
 * to_string(permille(4), scaled);              // "0.4%"
 * to_string(ppm(200), scaled);                 // "200ppm"
 * to_string(ppb(3), scaled);                   // "3ppb"
 * to_string(percent(0), scaled);               // "0"
 * @endcode
 */
using quanta::scaled;
using quanta::scaled_t;

/**
 * @brief Converts a proportion to a string with its precision's unit symbol.
 *
 * Integer counts are shown exactly; floating-point counts in the shortest
 * fixed notation that round-trips (never scientific), matching
 * std::format("{}"). A plain fraction (precision 1) has no symbol. Available
 * for precisions 1, %, ‰, ppm and ppb; for other precisions use the base_unit
 * or scaled form.
 *
 * @code
 * to_string(percent(25));                 // "25%"
 * to_string(permille(-3));                // "-3‰"
 * to_string(ppm(50));                     // "50ppm"
 * to_string(proportion<double>(0.25));    // "0.25"
 * to_string(proportion<double, std::ratio<1, 100>>(12.5)); // "12.5%"
 * @endcode
 */
template<typename Rep, typename Precision>
std::string to_string(const proportion<Rep, Precision>& p) {
    using precision = typename proportion<Rep, Precision>::precision;
    static_assert(_unit_symbol_v<precision> != nullptr, "proportion: precision has no unit symbol");
    return _count_string(p.count()) + _unit_symbol_v<precision>;
}

/**
 * @brief Converts a proportion to a string as a plain fraction; see base_unit.
 *
 * Available for every precision.
 *
 * @code
 * to_string(percent(25), base_unit);  // "0.25"
 * @endcode
 */
template<typename Rep, typename Precision>
std::string to_string(const proportion<Rep, Precision>& p, base_unit_t) {
    const _display d = _base_unit_display(p);
    return _fixed_string(d.value) + d.symbol;
}

/**
 * @brief Converts a proportion to a string in the unit (%, ppm or ppb) suited
 * to its magnitude; see scaled.
 *
 * Available for every precision.
 *
 * @code
 * to_string(ppm(200), scaled);  // "200ppm"
 * @endcode
 */
template<typename Rep, typename Precision>
std::string to_string(const proportion<Rep, Precision>& p, scaled_t) {
    const _display d = _scaled_display(p);
    return _fixed_string(d.value) + d.symbol;
}

} // namespace quanta::proportion

namespace std {

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<quanta::proportion::proportion<Rep1, Precision1>, quanta::proportion::proportion<Rep2, Precision2>> {
private:
    using common_precision = quanta::detail::_common_precision<Precision1, Precision2>;

public:
    using type = quanta::proportion::proportion<std::common_type_t<Rep1, Rep2>, common_precision>;
};

#if CONFIG_QUANTA_STD_FORMAT
// "{}" prints the exact stored count with the precision's unit symbol
// (percent(25) -> "25%"; a plain fraction has none: proportion<int64_t>(1) ->
// "1"; floating-point counts in the shortest fixed notation that round-trips,
// as to_string does: proportion<double>(1e-5) -> "0.00001", never "1e-05"). A
// non-empty spec controls only how the number is rendered (as double), never
// the unit, so both forms use the same unit:
// std::format("{:.1f}", percent(25)) == "25.0%".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then handled as above:
// - 'b' (base unit, see quanta::base_unit) shows the value as a plain
//   fraction: std::format("{:b}", percent(25)) == "0.25".
// - 's' (scaled, see quanta::scaled) shows the value in %, ppm or ppb, by its
//   magnitude: std::format("{:s}", permille(4)) == "0.4%".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(p, quanta::base_unit) or to_string(p, quanta::scaled):
// std::format("{:b}", ppm(50)) == "0.00005".
//
// A spec starting with 'b' or 's' is always a marker, so these letters cannot
// be the fill character of a spec that starts with a fill ("{:s>10.1f}" is the
// marker followed by ">10.1f").
//
// A precision with no unit symbol (e.g. std::ratio<1, 10000>) has no unit of
// its own: "{}" is a format error (a compile error for a constant format
// string), and a spec without a marker renders the value as a plain fraction,
// as 'b' does: std::format("{:.4f}", 50_pct * 50_pct) == "0.2500".
template<typename Rep, typename Precision>
struct formatter<quanta::proportion::proportion<Rep, Precision>>
    : private quanta::detail::_unit_formatter<
          quanta::proportion::_format_traits<typename quanta::proportion::proportion<Rep, Precision>::precision>> {
    using quanta::detail::_unit_formatter<
        quanta::proportion::_format_traits<typename quanta::proportion::proportion<Rep, Precision>::precision>>::parse;

    template<typename FormatContext>
    auto format(const quanta::proportion::proportion<Rep, Precision>& p, FormatContext& ctx) const {
        return this->format_quantity(p, ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for proportion types.
 */
namespace quanta {
inline namespace literals {
inline namespace proportion_literals {
/** @brief Literal for percent (e.g., 25_pct). */
template<char... Digits>
constexpr quanta::proportion::percent operator""_pct() {
    return detail::check_overflow<quanta::proportion::percent, Digits...>();
}

/** @brief Literal for permille (e.g., 5_permille). */
template<char... Digits>
constexpr quanta::proportion::permille operator""_permille() {
    return detail::check_overflow<quanta::proportion::permille, Digits...>();
}

/** @brief Literal for parts per million (e.g., 50_ppm). */
template<char... Digits>
constexpr quanta::proportion::ppm operator""_ppm() {
    return detail::check_overflow<quanta::proportion::ppm, Digits...>();
}

/** @brief Literal for parts per billion (e.g., 3_ppb). */
template<char... Digits>
constexpr quanta::proportion::ppb operator""_ppb() {
    return detail::check_overflow<quanta::proportion::ppb, Digits...>();
}

} // namespace proportion_literals
} // namespace literals

namespace proportion {
using namespace literals::proportion_literals;
} // namespace proportion
} // namespace quanta
