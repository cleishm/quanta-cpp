#pragma once

/**
 * @file thermo.hpp
 * @brief Temperature types and arithmetic, modeled after std::chrono.
 */

#include <assert.h>
#include <charconv>
#include <compare>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <quanta/core.hpp>
#include <ratio>
#include <string>
#include <system_error>

/**
 * @brief Temperature types and utilities.
 *
 * This library provides type-safe temperature handling with support for
 * multiple scales (Celsius, Kelvin, Fahrenheit) and precisions, following
 * the design of std::chrono. It distinguishes between absolute temperatures
 * and temperature differences (deltas).
 *
 * ## Formatting
 *
 * to_string() and std::format show a temperature or delta in its own unit by
 * default, whatever the format spec: `{}` prints the exact count and `{:.1f}`
 * the count as a double, each followed by the precision-qualified unit. A
 * single leading marker letter in the spec, or a tag argument to to_string(),
 * opts into a different unit:
 *
 * @code
 * std::format("{}", millicelsius(22534));        // "22534m°C"  own unit
 * std::format("{:.1f}", millicelsius(22534));    // "22534.0m°C"
 * std::format("{:b}", millicelsius(22534));      // "22.534°C"  base unit
 * std::format("{:b.1f}", millicelsius(22534));   // "22.5°C"
 * std::format("{:b.1f}", delta_millicelsius(1500)); // "1.5Δ°C"
 * to_string(millicelsius(22534), base_unit);     // "22.534°C"
 * @endcode
 *
 * The base unit is the unprefixed degree of the same scale (m°C -> °C,
 * mK -> K, m°F -> °F); temperatures are never converted to another scale.
 * The scaled form (`s`, see scaled) keeps the type's own unit: temperatures
 * are not rescaled to suit their magnitude.
 */
namespace quanta::thermo {

// The shared trait, re-exported for convenience; specialize quanta::treat_as_inexact.
using quanta::treat_as_inexact;
using quanta::treat_as_inexact_v;

template<typename Rep, typename Precision = std::ratio<1>>
class delta;

/**
 * @brief Trait to detect delta specializations.
 * @tparam _T Type to check.
 */
template<typename _T>
struct is_delta : std::false_type {};

template<typename Rep, typename Precision>
struct is_delta<delta<Rep, Precision>> : std::true_type {};

/** @cond INTERNAL */
template<typename Rep>
concept not_delta = !_is_specialization_of_v<Rep, delta>;

template<typename Rep>
struct delta_values {
    /** @brief Returns a zero-length delta. */
    static constexpr Rep zero() noexcept { return Rep(0); }

    /** @brief Returns the maximum representable delta. */
    static constexpr Rep max() noexcept { return std::numeric_limits<Rep>::max(); }

    /** @brief Returns the minimum (most negative) representable delta. */
    static constexpr Rep min() noexcept { return std::numeric_limits<Rep>::lowest(); }
};

/** @endcond */

/**
 * @brief A temperature difference with a representation and precision.
 *
 * Similar to std::chrono::duration, delta represents the difference between
 * two temperatures. The precision is expressed as a std::ratio of one base
 * unit (Kelvin or Celsius degree).
 *
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1).
 */
template<typename Rep, typename Precision>
class delta {
    static_assert(!is_delta<Rep>::value, "rep cannot be a quanta::thermo::delta");
    static_assert(_is_ratio<Precision>::value, "precision must be a specialization of std::ratio");
    static_assert(Precision::num > 0, "precision must be positive");

public:
    /** @brief The representation type. */
    using rep = Rep;
    /** @brief The precision as a std::ratio. */
    using precision = typename Precision::type;

    /** @brief Constructs a zero delta. */
    constexpr delta() = default;
    delta(const delta&) = default;

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
    constexpr explicit delta(const Rep2& r)
        : _r(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from another delta with different representation or precision.
     *
     * Implicit when the conversion is lossless (the target rep is
     * floating-point, or the target precision evenly divides the source
     * precision and the source rep is not floating-point) and the source rep
     * converts implicitly to the target rep. Any other conversion that the
     * cast can perform is explicit (see below).
     *
     * @tparam Rep2       Source representation type.
     * @tparam Precision2 Source precision.
     * @param temp        The source delta.
     */
    template<typename Rep2, typename Precision2>
        requires std::convertible_to<const Rep2&, rep> &&
        detail::_lossless_conversion<Rep2, Precision2, rep, precision> &&
        detail::_quantity_castable<delta<Rep2, Precision2>, delta>
    constexpr delta(const delta<Rep2, Precision2>& temp)
        : _r(delta_cast<delta>(temp).count()) {}

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
     * @param d           The source delta.
     */
    template<typename Rep2, typename Precision2>
        requires(!detail::_lossless_conversion<Rep2, Precision2, rep, precision> ||
                 !std::convertible_to<const Rep2&, rep>) &&
        detail::_quantity_castable<delta<Rep2, Precision2>, delta>
    constexpr explicit delta(const delta<Rep2, Precision2>& d)
        : _r(delta_cast<delta>(d).count()) {}

    ~delta() = default;
    delta& operator=(const delta&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _r; }

    constexpr delta<typename std::common_type<rep>::type, precision> operator+() const {
        return delta<typename std::common_type<rep>::type, precision>(_r);
    }

    constexpr delta<typename std::common_type<rep>::type, precision> operator-() const {
        return delta<typename std::common_type<rep>::type, precision>(-_r);
    }

    constexpr delta& operator++() {
        ++_r;
        return *this;
    }

    constexpr delta operator++(int) { return delta(_r++); }

    constexpr delta& operator--() {
        --_r;
        return *this;
    }

    constexpr delta operator--(int) { return delta(_r--); }

    constexpr delta& operator+=(const delta& d) {
        _r += d.count();
        return *this;
    }

    constexpr delta& operator-=(const delta& d) {
        _r -= d.count();
        return *this;
    }

    constexpr delta& operator*=(const rep& r) {
        _r *= r;
        return *this;
    }

    constexpr delta& operator/=(const rep& r) {
        _r /= r;
        return *this;
    }

    constexpr delta& operator%=(const rep& r)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= r;
        return *this;
    }

    constexpr delta& operator%=(const delta& d)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= d.count();
        return *this;
    }

    /** @brief Returns a zero-length delta. */
    static constexpr delta zero() noexcept { return delta(delta_values<rep>::zero()); }

    /** @brief Returns the minimum (most negative) representable delta. */
    static constexpr delta min() noexcept { return delta(delta_values<rep>::min()); }

    /** @brief Returns the maximum representable delta. */
    static constexpr delta max() noexcept { return delta(delta_values<rep>::max()); }

private:
    rep _r{};
};

/**
 * @brief Converts a delta to a different precision or representation.
 *
 * For integer-to-integer conversions, this function uses wider intermediate
 * types (128-bit when available) to minimize overflow risk during ratio
 * arithmetic.
 *
 * @tparam ToDelta The target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to convert.
 *
 * @return The converted delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
    requires detail::_quantity_cast_requirements<delta<Rep, Precision>, ToDelta>
constexpr ToDelta delta_cast(const delta<Rep, Precision>& d) {
    return detail::_quantity_cast<ToDelta>(d);
}

/**
 * @brief Rounds a delta up to the nearest representable value in the target precision.
 *
 * Returns the smallest value of type ToDelta that is greater than or equal to d.
 * When converting to the same or finer precision, returns the exact conversion.
 * A target whose rep is not integral (such as a floating-point one) is not
 * rounded: the result is delta_cast<ToDelta>(d).
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta ceil(const delta<Rep, Precision>& d) {
    return detail::_ceil_cast(delta_cast<ToDelta>(d), d);
}

/**
 * @brief Rounds a delta down to the nearest representable value in the target precision.
 *
 * Returns the largest value of type ToDelta that is less than or equal to d.
 * When converting to the same or finer precision, returns the exact conversion.
 * A target whose rep is not integral (such as a floating-point one) is not
 * rounded: the result is delta_cast<ToDelta>(d).
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta floor(const delta<Rep, Precision>& d) {
    return detail::_floor_cast(delta_cast<ToDelta>(d), d);
}

/**
 * @brief Rounds a delta toward zero to the nearest representable value in the target precision.
 *
 * Returns the value of type ToDelta that is closest to zero.
 * When converting to the same or finer precision, returns the exact conversion.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta trunc(const delta<Rep, Precision>& d) {
    return delta_cast<ToDelta>(d);
}

/**
 * @brief Rounds a delta to the nearest representable value in the target precision.
 *
 * Returns the value of type ToDelta that is nearest to d. When exactly halfway
 * between two values, rounds to the even one, as std::chrono::round does.
 * When converting to the same or finer precision, returns the exact conversion.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToDelta Target delta type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param d        The delta to round.
 * @return The rounded delta.
 */
template<typename ToDelta, typename Rep, typename Precision>
constexpr ToDelta round(const delta<Rep, Precision>& d) {
    return detail::_round_cast<ToDelta>(d, [&d] { return floor<ToDelta>(d); });
}

/**
 * @brief Returns the absolute value of a delta.
 *
 * @tparam Rep       Representation type.
 * @tparam Precision Precision type.
 * @param d          The delta.
 *
 * @return The absolute value of the delta.
 *
 * @code
 * using namespace quanta::thermo;
 * auto a = abs(delta_millicelsius(-1500));  // delta_millicelsius(1500)
 * @endcode
 */
template<typename Rep, typename Precision>
constexpr delta<Rep, Precision> abs(const delta<Rep, Precision>& d) {
    return detail::_abs(d);
}

/** @brief Returns the sum of two deltas. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator+(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(cd(lhs).count() + cd(rhs).count());
}

/** @brief Returns the difference of two deltas. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator-(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(cd(lhs).count() - cd(rhs).count());
}

/** @brief Multiplies a delta by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_delta<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const delta<Rep1, Precision>& d, const Rep2& r)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    using cd = delta<std::common_type_t<Rep1, Rep2>, Precision>;
    return cd(cd(d).count() * r);
}

/** @brief Multiplies a scalar by a delta. */
template<typename Rep1, typename Rep2, typename Precision>
    requires not_delta<Rep1> && std::convertible_to<const Rep1&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const Rep1& r, const delta<Rep2, Precision>& d)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    return d * r;
}

/** @brief Divides a delta by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_delta<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator/(const delta<Rep1, Precision>& d, const Rep2& s)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    using cd = delta<std::common_type_t<Rep1, Rep2>, Precision>;
    return cd(cd(d).count() / s);
}

/** @brief Divides two deltas, returning a scalar. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator/(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<Rep1, Rep2> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(lhs).count() / cd(rhs).count();
}

/** @brief Returns the remainder of dividing a delta by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_delta<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>> &&
    (!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const delta<Rep1, Precision>& d, const Rep2& s)
    -> delta<std::common_type_t<Rep1, Rep2>, Precision> {
    using cd = delta<std::common_type_t<Rep1, Rep2>, Precision>;
    return cd(cd(d).count() % s);
}

/** @brief Returns the remainder of dividing two deltas. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires(!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs)
    -> std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>> {
    using cd = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return cd(cd(lhs).count() % cd(rhs).count());
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr bool operator==(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return ct(lhs).count() == ct(rhs).count();
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires std::three_way_comparable<std::common_type_t<Rep1, Rep2>>
constexpr auto operator<=>(const delta<Rep1, Precision1>& lhs, const delta<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<delta<Rep1, Precision1>, delta<Rep2, Precision2>>;
    return ct(lhs).count() <=> ct(rhs).count();
}

/** @brief Delta with 1 degree precision (Celsius/Kelvin). */
using delta_celsius = delta<int64_t>;
/** @brief Delta with 0.1 degree precision (Celsius/Kelvin). */
using delta_decicelsius = delta<int64_t, std::deci>;
/** @brief Delta with 0.001 degree precision (Celsius/Kelvin). */
using delta_millicelsius = delta<int64_t, std::milli>;
/** @brief Delta with 1 degree precision (Celsius/Kelvin). */
using delta_kelvin = delta<int64_t>;
/** @brief Delta with 0.1 degree precision (Celsius/Kelvin). */
using delta_decikelvin = delta<int64_t, std::deci>;
/** @brief Delta with 0.001 degree precision (Celsius/Kelvin). */
using delta_millikelvin = delta<int64_t, std::milli>;
/** @brief Delta with 1°F precision. */
using delta_fahrenheit = delta<int64_t, std::ratio<5, 9>>;
/** @brief Delta with 0.1°F precision. */
using delta_decifahrenheit = delta<int64_t, std::ratio<5, 90>>;
/** @brief Delta with 0.001°F precision. */
using delta_millifahrenheit = delta<int64_t, std::ratio<5, 9000>>;

/** @cond INTERNAL */
// SI prefix for a degrees-per-count ratio; nullptr when unmapped.
template<typename Ratio>
struct _si_prefix {
    static constexpr const char* value = nullptr;
};
template<>
struct _si_prefix<std::ratio<1>> {
    static constexpr const char* value = "";
};
template<>
struct _si_prefix<std::ratio<1, 10>> {
    static constexpr const char* value = "d";
};
template<>
struct _si_prefix<std::ratio<1, 1000>> {
    static constexpr const char* value = "m";
};
template<typename Ratio>
inline constexpr const char* _si_prefix_v = _si_prefix<typename Ratio::type>::value;

// Display family (degree size + unit suffix) for a delta precision, keyed on
// NORMALIZED ratios. Undefined primary template: unmapped precisions have no
// determinable unit and are not formattable.
template<typename Precision>
struct _delta_unit;
struct _celsius_delta_unit {
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "°C";
};
struct _fahrenheit_delta_unit {
    using degree = std::ratio<5, 9>;
    static constexpr const char* suffix = "°F";
};
template<>
struct _delta_unit<std::ratio<1>> : _celsius_delta_unit {};
template<>
struct _delta_unit<std::ratio<1, 10>> : _celsius_delta_unit {};
template<>
struct _delta_unit<std::ratio<1, 1000>> : _celsius_delta_unit {};
template<>
struct _delta_unit<std::ratio<5, 9>> : _fahrenheit_delta_unit {};
template<>
struct _delta_unit<std::ratio<1, 18>> : _fahrenheit_delta_unit {}; // 5/90 normalized
template<>
struct _delta_unit<std::ratio<1, 1800>> : _fahrenheit_delta_unit {}; // 5/9000 normalized

// How a delta or temperature type is displayed: the delta marker ("Δ" or
// ""), the unit suffix of its scale, the precision in degrees of that unit
// (dpc), and the SI prefix for that precision (nullptr when it has none).
template<typename T>
struct _display_traits;

template<typename Rep, typename Precision>
struct _display_traits<delta<Rep, Precision>> {
private:
    using _precision = typename delta<Rep, Precision>::precision;
    using _unit = _delta_unit<_precision>; // hard error if unmapped

public:
    using dpc = typename std::ratio_divide<_precision, typename _unit::degree>::type;
    static constexpr const char* mark = "Δ";
    static constexpr const char* unit = _unit::suffix;
    static constexpr const char* prefix = _si_prefix_v<dpc>;
};

// A value and SI prefix to display, followed by the type's mark and unit.
struct _display {
    double value;
    const char* prefix;
};

// The base-unit form: the value in unprefixed degrees of the same scale.
template<typename Traits, typename Rep>
constexpr _display _base_unit_display(const Rep& count) {
    return {detail::_ratio_value<typename Traits::dpc>(count), ""};
}

// The scaled form. Temperatures are not rescaled, so this is the type's own
// unit: the count (as a double) with the precision's prefix. A precision with
// no SI prefix has no own unit label and is shown in unprefixed degrees.
template<typename Traits, typename Rep>
constexpr _display _scaled_display(const Rep& count) {
    if constexpr (Traits::prefix != nullptr) {
        return {static_cast<double>(count), Traits::prefix};
    } else {
        return _base_unit_display<Traits>(count);
    }
}

// Renders a display value with no format spec, followed by its unit.
template<typename Traits>
std::string _display_string(const _display& d) {
    return _fixed_string(d.value) + Traits::mark + d.prefix + Traits::unit;
}

// Renders an exact count with the type's own precision-qualified unit.
template<typename Traits, typename Rep>
std::string _own_unit_string(const Rep& count) {
    static_assert(Traits::prefix != nullptr, "thermo: precision has no SI prefix");
    return _count_string(count) + Traits::mark + Traits::prefix + Traits::unit;
}

#if CONFIG_QUANTA_STD_FORMAT
// How std::format renders a delta or temperature with display traits Traits;
// see detail::_unit_formatter. A precision with no SI prefix has no own unit.
template<typename Traits>
struct _format_traits {
    static constexpr const char* markers = "bs";
    static constexpr bool has_own_unit = Traits::prefix != nullptr;

    // Reports "{}" for a precision with no own unit. The parameter only gives
    // the function a path that is a constant expression, as a constexpr
    // function needs; the formatter always calls it with true.
    static constexpr void no_own_unit_error(bool fail = true) {
        if (fail) {
            _format_error("thermo: precision has no SI prefix; use an explicit format spec");
        }
    }

    // The own unit is written from constants; there is nothing to obtain.
    static constexpr bool own_unit(bool power) { return power; }

    template<typename OutputIt>
    static OutputIt append_own_unit(OutputIt out, bool) {
        out = _format_append(out, Traits::mark);
        out = _format_append(out, Traits::prefix);
        return _format_append(out, Traits::unit);
    }

    template<typename T>
    static _display base_display(const T& t) {
        return _base_unit_display<Traits>(t.count());
    }

    template<typename T>
    static _display scaled_display(const T& t) {
        return _scaled_display<Traits>(t.count());
    }

    template<typename OutputIt>
    static OutputIt append_label(OutputIt out, const _display& d) {
        out = _format_append(out, Traits::mark);
        out = _format_append(out, d.prefix);
        return _format_append(out, Traits::unit);
    }
};

// The std::formatter implementation shared by deltas and temperatures; see
// the formatter specializations for the spec grammar.
template<typename Traits>
using _formatter = detail::_unit_formatter<_format_traits<Traits>>;
#endif
/** @endcond */

/**
 * @brief Selects the base-unit form in to_string().
 *
 * The value is shown in the unprefixed degree of its own scale (°C, K or °F,
 * with deltas keeping their Δ marker): the count times the precision, rendered
 * in the shortest fixed notation that round-trips (never scientific), exactly
 * as by the `b` format marker. Temperatures are never converted to another
 * scale.
 *
 * @code
 * to_string(millicelsius(22534), base_unit);       // "22.534°C"
 * to_string(decikelvin(2955), base_unit);          // "295.5K"
 * to_string(millifahrenheit(-40250), base_unit);   // "-40.25°F"
 * to_string(delta_millicelsius(1500), base_unit);  // "1.5Δ°C"
 * @endcode
 */
using quanta::base_unit;
using quanta::base_unit_t;

/**
 * @brief Selects the scaled form in to_string().
 *
 * Provided for parity with the sibling unit libraries. Temperatures are not
 * rescaled to suit their magnitude, so the scaled form is the type's own unit:
 * the count (as a double) in the shortest fixed notation that round-trips,
 * with the precision-qualified unit, exactly as by the `s` format marker. A
 * precision with no SI prefix (and so no unit label of its own) is shown in
 * the base unit.
 *
 * @code
 * to_string(millicelsius(22534), scaled);       // "22534m°C"
 * to_string(delta_millicelsius(1500), scaled);  // "1500Δm°C"
 * @endcode
 */
using quanta::scaled;
using quanta::scaled_t;

/**
 * @brief Renders a delta as its exact count with a precision-qualified unit,
 * e.g. `to_string(delta_millicelsius(1500)) == "1500Δm°C"`.
 *
 * Floating-point counts are rendered in the shortest fixed notation that
 * round-trips, as by `{}` in std::format.
 */
template<typename Rep, typename Precision>
std::string to_string(const delta<Rep, Precision>& d) {
    return _own_unit_string<_display_traits<delta<Rep, Precision>>>(d.count());
}

/**
 * @brief Renders a delta in its base unit (Δ°C or Δ°F), see base_unit,
 * e.g. `to_string(delta_millicelsius(1500), base_unit) == "1.5Δ°C"`.
 */
template<typename Rep, typename Precision>
std::string to_string(const delta<Rep, Precision>& d, base_unit_t) {
    using traits = _display_traits<delta<Rep, Precision>>;
    return _display_string<traits>(_base_unit_display<traits>(d.count()));
}

/**
 * @brief Renders a delta in its scaled form (its own unit), see scaled,
 * e.g. `to_string(delta_millicelsius(1500), scaled) == "1500Δm°C"`.
 */
template<typename Rep, typename Precision>
std::string to_string(const delta<Rep, Precision>& d, scaled_t) {
    using traits = _display_traits<delta<Rep, Precision>>;
    return _display_string<traits>(_scaled_display<traits>(d.count()));
}

// ============================================================================
// Temperature Scales and Absolute Temperatures
// ============================================================================

/** @cond INTERNAL */
/**
 * @brief Celsius temperature scale.
 *
 * 0°C = 273.15 K (offset from absolute zero).
 */
struct celsius_scale {
    using offset = std::ratio<27315, 100>;
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "°C";
};

/**
 * @brief Kelvin temperature scale.
 *
 * 0 K = absolute zero.
 */
struct kelvin_scale {
    using offset = std::ratio<0>;
    using degree = std::ratio<1>;
    static constexpr const char* suffix = "K";
};

/**
 * @brief Fahrenheit temperature scale.
 *
 * 0°F = 255.37 K (approximately).
 */
struct fahrenheit_scale {
    using offset = std::ratio<45967, 180>;
    using degree = std::ratio<5, 9>;
    static constexpr const char* suffix = "°F";
};

/**
 * @brief True when a scale's zero point is not absolute zero.
 *
 * Negating an absolute temperature only makes sense on a "relative" scale
 * (e.g. Celsius, Fahrenheit), where the zero point sits above absolute zero.
 * On an absolute scale (Kelvin, offset == 0) a negated temperature always
 * falls below absolute zero, so unary minus is disabled there.
 */
template<typename Scale>
inline constexpr bool _is_relative_scale = std::ratio_not_equal_v<typename Scale::offset, std::ratio<0>>;
/** @endcond */

template<typename Scale, typename Delta = delta<int64_t>>
class temperature;

/** @cond INTERNAL */
// The ratios temperature_cast uses to convert across scales, from (Scale1,
// Delta1) to (Scale2, Delta2): the precision ratio and the offset in target
// units, computed without std::ratio_divide so that an overflow makes fits
// false rather than failing to compile, and the least common denominator of
// the two (which the cast's integer path computes as an intmax_t).
template<typename Scale1, typename Delta1, typename Scale2, typename Delta2>
struct _temperature_ratios {
    static constexpr _ratio_quotient offset_diff =
        _safe_ratio_subtract<typename Scale1::offset, typename Scale2::offset>;

    static constexpr _ratio_quotient prec = _safe_ratio_divide<typename Delta1::precision, typename Delta2::precision>;
    static constexpr _ratio_quotient offset =
        _safe_ratio_divide<std::ratio<offset_diff.num, offset_diff.den>, typename Delta2::precision>;
    static constexpr bool fits = offset_diff.fits && prec.fits && offset.fits &&
        _product_fits(prec.den / _gcd(prec.den, offset.den), offset.den);
};

template<typename Scale1, typename Delta1, typename Scale2, typename Delta2>
struct _lossless_temperature_conversion_impl {
    using ratios = _temperature_ratios<Scale1, Delta1, Scale2, Delta2>;

    static constexpr bool value = ratios::offset_diff.fits && ratios::prec.fits && ratios::offset.fits &&
        ratios::prec.den == 1 && ratios::offset.den == 1;
};

// The operations temperature_cast performs across scales to convert a
// FromRep count to a ToRep count, with Common the common type of FromRep,
// ToRep and intmax_t: in floating point through Common (an inexact Common),
// or else with integer arithmetic in Wide on the count converted to Source.
// Integral reps convert directly to a signed wide integer (128-bit where
// available, else intmax_t), whatever their signedness, since the offset
// between scales may be negative and an unsigned intermediate would wrap;
// other reps compute in Common.
template<typename Source, typename ToRep, typename Wide>
concept _temperature_integer_steps = requires(Source& from_val, Wide& w, const intmax_t& k) {
    {
        static_cast<Wide>(from_val) * static_cast<Wide>(k) * static_cast<Wide>(k) +
            static_cast<Wide>(k) * static_cast<Wide>(k)
    } -> std::convertible_to<Wide>;
    { w / static_cast<Wide>(k) } -> std::convertible_to<Wide>;
    static_cast<ToRep>(w);
};

template<typename FromRep, typename ToRep>
inline constexpr bool _integral_temperature_reps = std::is_integral_v<FromRep> && std::is_integral_v<ToRep>;

template<typename FromRep, typename ToRep, typename Common>
using _temperature_source = std::conditional_t<_integral_temperature_reps<FromRep, ToRep>, FromRep, Common>;

template<typename FromRep, typename ToRep, typename Common>
using _temperature_wide =
    std::conditional_t<_integral_temperature_reps<FromRep, ToRep>, detail::_wide_integer<intmax_t>, Common>;

template<typename FromRep, typename ToRep, typename Common>
concept _temperature_cast_steps =
    (treat_as_inexact_v<Common> && requires(FromRep (&count)()) { static_cast<Common>(count()); } &&
     requires(Common& from_val, const double& k, Common& result) {
         { from_val * k + k } -> std::convertible_to<Common>;
         static_cast<ToRep>(result);
     }) ||
    (!treat_as_inexact_v<Common> &&
     requires(FromRep (&count)()) { static_cast<_temperature_source<FromRep, ToRep, Common>>(count()); } &&
     _temperature_integer_steps<
         _temperature_source<FromRep, ToRep, Common>,
         ToRep,
         _temperature_wide<FromRep, ToRep, Common>>);

// Whether a conversion from (Scale1, Delta1) to (Scale2, Delta2) is implicit,
// as for the other domains' quantities: the source rep converts implicitly to
// the target rep, and the conversion loses nothing. On one scale, that is the
// quantities' rule (a floating-point target rep, or a precision that evenly
// divides the source's from a rep that is not floating-point); across scales,
// both the precision ratio and the offset must be whole numbers of target
// ticks, and a floating-point source rep needs a floating-point target rep.
template<typename Scale1, typename Delta1, typename Scale2, typename Delta2>
concept _implicit_temperature_conversion = std::convertible_to<const typename Delta1::rep&, typename Delta2::rep> &&
    ((std::is_same_v<Scale1, Scale2> &&
      detail::_lossless_conversion<
          typename Delta1::rep,
          typename Delta1::precision,
          typename Delta2::rep,
          typename Delta2::precision>) ||
     (!std::is_same_v<Scale1, Scale2> && _lossless_temperature_conversion_impl<Scale1, Delta1, Scale2, Delta2>::value &&
      (treat_as_inexact_v<typename Delta2::rep> || !treat_as_inexact_v<typename Delta1::rep>)));

// The requirements of _temperature_cast, read off its code and checked by its
// requires-clause: a plain copy, a delta_cast on the same scale, or the
// cross-scale arithmetic above with ratios that fit. Only _temperature_cast
// and temperature_cast, which calls it, are constrained on this: whatever
// else needs to know whether a conversion can be performed asks whether the
// cast can be called (_temperature_castable), so that the answer is the
// cast's own.
template<typename Scale1, typename Delta1, typename Scale2, typename Delta2>
concept _temperature_cast_requirements = (std::is_same_v<Scale1, Scale2> && std::is_same_v<Delta1, Delta2>) ||
    (std::is_same_v<Scale1, Scale2> && detail::_quantity_cast_requirements<Delta1, Delta2>) ||
    (!std::is_same_v<Scale1, Scale2> && _temperature_ratios<Scale1, Delta1, Scale2, Delta2>::fits &&
     requires { typename std::common_type<typename Delta1::rep, typename Delta2::rep, intmax_t>::type; } &&
     _temperature_cast_steps<
         typename Delta1::rep,
         typename Delta2::rep,
         std::common_type_t<typename Delta1::rep, typename Delta2::rep, intmax_t>>);

// The conversion behind temperature_cast, constrained on exactly what it
// does (see _temperature_cast_requirements).
template<typename ToTemp, typename Scale, typename Delta>
    requires _temperature_cast_requirements<Scale, Delta, typename ToTemp::scale, typename ToTemp::delta_type>
constexpr ToTemp _temperature_cast(const temperature<Scale, Delta>& t) {
    using ToScale = typename ToTemp::scale;
    using ToDelta = typename ToTemp::delta_type;
    using to_rep = typename ToDelta::rep;

    if constexpr (std::is_same_v<Scale, ToScale> && std::is_same_v<Delta, ToDelta>) {
        return t;
    } else if constexpr (std::is_same_v<Scale, ToScale>) {
        return ToTemp(delta_cast<ToDelta>(Delta(t.count())));
    } else {
        using from_prec = typename Delta::precision;
        using from_off = typename Scale::offset;
        using to_prec = typename ToDelta::precision;
        using to_off = typename ToScale::offset;

        using offset_diff = std::ratio_subtract<from_off, to_off>;
        using common_rep = std::common_type_t<typename Delta::rep, to_rep, intmax_t>;

        using prec_ratio = std::ratio_divide<from_prec, to_prec>;
        using offset_ratio = std::ratio_divide<offset_diff, to_prec>;

        if constexpr (treat_as_inexact_v<common_rep>) {
            common_rep from_val = static_cast<common_rep>(t.count());
            constexpr double pr = static_cast<double>(prec_ratio::num) / prec_ratio::den;
            constexpr double or_ = static_cast<double>(offset_ratio::num) / offset_ratio::den;
            common_rep result = from_val * pr + or_;
            return ToTemp(ToDelta(static_cast<to_rep>(result)));
        } else {
            // Integral reps compute in a signed wide integer (see
            // _temperature_integer_steps), any others in their common type.
            using source_t = _temperature_source<typename Delta::rep, to_rep, common_rep>;
            using wider_t = _temperature_wide<typename Delta::rep, to_rep, common_rep>;
            source_t from_val = static_cast<source_t>(t.count());
            // Put both ratios over their least common denominator:
            // (count * pn * (lcd / pd) + on * (lcd / od)) / lcd
            constexpr intmax_t lcd = prec_ratio::den / _gcd(prec_ratio::den, offset_ratio::den) * offset_ratio::den;
            wider_t numerator = static_cast<wider_t>(from_val) * static_cast<wider_t>(prec_ratio::num) *
                    static_cast<wider_t>(lcd / prec_ratio::den) +
                static_cast<wider_t>(offset_ratio::num) * static_cast<wider_t>(lcd / offset_ratio::den);
            wider_t result = numerator / static_cast<wider_t>(lcd);
            return ToTemp(ToDelta(static_cast<to_rep>(result)));
        }
    }
}

// Whether _temperature_cast can convert a From to a To. temperature's
// converting constructors are constrained on this, so that a conversion is
// offered exactly when the cast can perform it.
template<typename From, typename To>
concept _temperature_castable = requires(const From& t) { _temperature_cast<To>(t); };

/**
 * @brief Trait to detect temperature specializations.
 * @tparam T Type to check.
 */
template<typename T>
struct is_temperature : std::false_type {};

template<typename Scale, typename Delta>
struct is_temperature<temperature<Scale, Delta>> : std::true_type {};

template<typename T>
inline constexpr bool is_temperature_v = is_temperature<T>::value;
/** @endcond */

/**
 * @brief Converts a temperature to a different scale or precision.
 *
 * For integer-to-integer conversions, this function uses wider intermediate
 * types (128-bit when available) to minimize overflow risk during ratio
 * arithmetic. Across scales the intermediate is signed even for unsigned
 * reps, so a negative offset between the scales does not wrap; as with any
 * cast, a result outside the target rep's range is the caller's concern.
 * Without 128-bit integer support (e.g. MSVC, 32-bit targets such as the
 * ESP32), cross-scale integral conversions of counts near the 64-bit limits
 * can overflow the intermediate.
 *
 * @tparam ToTemp The target temperature type.
 * @tparam Scale  Source scale.
 * @tparam Delta  Source delta type.
 * @param t       The temperature to convert.
 *
 * @return The converted temperature.
 */
template<typename ToTemp, typename Scale, typename Delta>
    requires _temperature_cast_requirements<Scale, Delta, typename ToTemp::scale, typename ToTemp::delta_type>
constexpr ToTemp temperature_cast(const temperature<Scale, Delta>& t) {
    return _temperature_cast<ToTemp>(t);
}

/**
 * @brief An absolute temperature on a given scale.
 *
 * Similar to std::chrono::time_point, temperature represents an absolute
 * point on a temperature scale. The scale defines the zero point (offset
 * from absolute zero), and the delta type defines the precision.
 *
 * As with time_point, a temperature plus or minus a delta is a temperature,
 * and the difference of two temperatures is a delta. Adding two temperatures
 * is ill-formed.
 *
 * @code
 * celsius room{20};
 * auto warmer = room + delta_celsius(5);  // celsius(25)
 * auto rise = warmer - room;              // delta_celsius(5)
 * @endcode
 *
 * @tparam Scale The temperature scale (celsius_scale, kelvin_scale, fahrenheit_scale).
 * @tparam Delta The delta type for precision (default: delta<int64_t>).
 */
template<typename Scale, typename Delta>
class temperature {
    static_assert(is_delta<Delta>::value, "Delta must be a quanta::thermo::delta type");

public:
    /** @brief The temperature scale. */
    using scale = Scale;
    /** @brief The delta type. */
    using delta_type = Delta;
    /** @brief The representation type. */
    using rep = typename Delta::rep;

    /** @brief Constructs a temperature at the scale's zero point. */
    constexpr temperature() = default;
    temperature(const temperature&) = default;

    /**
     * @brief Constructs from a tick count.
     * @tparam Rep2 The source representation type.
     * @param r     The tick count in scale units.
     */
    template<typename Rep2>
        requires std::convertible_to<const Rep2&, rep> && (treat_as_inexact_v<rep> || !treat_as_inexact_v<Rep2>)
    constexpr explicit temperature(const Rep2& r)
        : _d(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from a delta.
     * @param d The delta from the scale's zero point.
     */
    constexpr explicit temperature(const Delta& d)
        : _d(d) {}

    // Every conversion temperature_cast can perform has exactly one of the
    // constructors below: implicit when lossless and the reps convert
    // implicitly (see _implicit_temperature_conversion), explicit otherwise.

    // Same scale, different delta - implicit
    template<typename Delta2>
        requires(!std::is_same_v<Delta, Delta2>) && _temperature_castable<temperature<Scale, Delta2>, temperature> &&
        _implicit_temperature_conversion<Scale, Delta2, Scale, Delta>
    constexpr temperature(const temperature<Scale, Delta2>& t)
        : _d(delta_cast<Delta>(Delta2(t.count()))) {}

    // Same scale, different delta - explicit
    template<typename Delta2>
        requires(!std::is_same_v<Delta, Delta2>) && _temperature_castable<temperature<Scale, Delta2>, temperature> &&
        (!_implicit_temperature_conversion<Scale, Delta2, Scale, Delta>)
    constexpr explicit temperature(const temperature<Scale, Delta2>& t)
        : _d(delta_cast<Delta>(Delta2(t.count()))) {}

    // Across scales - implicit
    template<typename Scale2, typename Delta2>
        requires(!std::is_same_v<temperature, temperature<Scale2, Delta2>>) &&
        _temperature_castable<temperature<Scale2, Delta2>, temperature> &&
        _implicit_temperature_conversion<Scale2, Delta2, Scale, Delta>
    constexpr temperature(const temperature<Scale2, Delta2>& t)
        : temperature(temperature_cast<temperature>(t)) {}

    // Across scales - explicit
    template<typename Scale2, typename Delta2>
        requires(!std::is_same_v<temperature, temperature<Scale2, Delta2>>) &&
        _temperature_castable<temperature<Scale2, Delta2>, temperature> &&
        (!_implicit_temperature_conversion<Scale2, Delta2, Scale, Delta>)
    constexpr explicit temperature(const temperature<Scale2, Delta2>& t)
        : temperature(temperature_cast<temperature>(t)) {}

    ~temperature() = default;
    temperature& operator=(const temperature&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _d.count(); }

    constexpr temperature& operator++() {
        ++_d;
        return *this;
    }

    constexpr temperature operator++(int) { return temperature(_d++); }

    constexpr temperature& operator--() {
        --_d;
        return *this;
    }

    constexpr temperature operator--(int) { return temperature(_d--); }

    /**
     * @brief Negates the temperature (e.g. to write @c -123_dc as @c -(123_dc)).
     *
     * Only available on relative scales such as Celsius and Fahrenheit; on an
     * absolute scale (Kelvin) negation would fall below absolute zero and is a
     * compile error.
     */
    constexpr temperature operator-() const
        requires _is_relative_scale<Scale>
    {
        return temperature(-_d);
    }

    template<typename Rep2, typename Precision2>
    constexpr temperature& operator+=(const delta<Rep2, Precision2>& d) {
        _d += d;
        return *this;
    }

    template<typename Rep2, typename Precision2>
    constexpr temperature& operator-=(const delta<Rep2, Precision2>& d) {
        _d -= d;
        return *this;
    }

    /** @brief Returns the minimum representable temperature. */
    static constexpr temperature min() noexcept { return temperature(Delta::min()); }

    /** @brief Returns the maximum representable temperature. */
    static constexpr temperature max() noexcept { return temperature(Delta::max()); }

private:
    Delta _d{};
};

/**
 * @brief Returns the difference between two temperatures as a delta.
 *
 * The same as `lhs - rhs`.
 *
 * @param lhs First temperature.
 * @param rhs Second temperature.
 *
 * @return lhs - rhs as a delta.
 */
template<typename Scale, typename Delta1, typename Delta2>
constexpr auto difference(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs)
    -> std::common_type_t<Delta1, Delta2> {
    return Delta1(lhs.count()) - Delta2(rhs.count());
}

/**
 * @brief Subtracts two temperatures on the same scale, giving the delta
 * between them.
 *
 * As with std::chrono::time_point, the difference of two absolute points is
 * a delta, not a temperature: `celsius(25) - celsius(20) == delta_celsius(5)`.
 * Equivalent to difference(lhs, rhs).
 */
template<typename Scale, typename Delta1, typename Delta2>
constexpr auto operator-(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs)
    -> std::common_type_t<Delta1, Delta2> {
    return difference(lhs, rhs);
}

/**
 * @brief Adding two absolute temperatures is meaningless, and ill-formed.
 *
 * Add a delta to a temperature instead (`celsius(20) + delta_celsius(5)`).
 */
template<typename Scale, typename Delta1, typename Delta2>
void operator+(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs) = delete;

/** @brief Adds a delta to a temperature. */
template<typename Scale, typename Delta1, typename Rep2, typename Precision2>
constexpr auto operator+(const temperature<Scale, Delta1>& t, const delta<Rep2, Precision2>& d)
    -> temperature<Scale, std::common_type_t<Delta1, delta<Rep2, Precision2>>> {
    using result_delta = std::common_type_t<Delta1, delta<Rep2, Precision2>>;
    return temperature<Scale, result_delta>(result_delta(Delta1(t.count())) + result_delta(d));
}

/** @brief Adds a temperature to a delta. */
template<typename Rep1, typename Precision1, typename Scale, typename Delta2>
constexpr auto operator+(const delta<Rep1, Precision1>& d, const temperature<Scale, Delta2>& t)
    -> temperature<Scale, std::common_type_t<delta<Rep1, Precision1>, Delta2>> {
    return t + d;
}

/** @brief Subtracts a delta from a temperature. */
template<typename Scale, typename Delta1, typename Rep2, typename Precision2>
constexpr auto operator-(const temperature<Scale, Delta1>& t, const delta<Rep2, Precision2>& d)
    -> temperature<Scale, std::common_type_t<Delta1, delta<Rep2, Precision2>>> {
    using result_delta = std::common_type_t<Delta1, delta<Rep2, Precision2>>;
    return temperature<Scale, result_delta>(result_delta(Delta1(t.count())) - result_delta(d));
}

template<typename Scale, typename Delta1, typename Delta2>
constexpr bool operator==(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs) {
    return Delta1(lhs.count()) == Delta2(rhs.count());
}

template<typename Scale, typename Delta1, typename Delta2>
    requires std::three_way_comparable<std::common_type_t<typename Delta1::rep, typename Delta2::rep>>
constexpr auto operator<=>(const temperature<Scale, Delta1>& lhs, const temperature<Scale, Delta2>& rhs) {
    return Delta1(lhs.count()) <=> Delta2(rhs.count());
}

/** @brief Celsius with 1 degree precision. */
using celsius = temperature<celsius_scale>;
/** @brief Celsius with 0.1 degree precision. */
using decicelsius = temperature<celsius_scale, delta<int64_t, std::deci>>;
/** @brief Celsius with 0.001 degree precision. */
using millicelsius = temperature<celsius_scale, delta<int64_t, std::milli>>;
/** @brief Kelvin with 1 degree precision. */
using kelvin = temperature<kelvin_scale>;
/** @brief Kelvin with 0.1 degree precision. */
using decikelvin = temperature<kelvin_scale, delta<int64_t, std::deci>>;
/** @brief Kelvin with 0.001 degree precision. */
using millikelvin = temperature<kelvin_scale, delta<int64_t, std::milli>>;
/** @brief Fahrenheit with 1 degree precision. */
using fahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 9>>>;
/** @brief Fahrenheit with 0.1 degree precision. */
using decifahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 90>>>;
/** @brief Fahrenheit with 0.001 degree precision. */
using millifahrenheit = temperature<fahrenheit_scale, delta<int64_t, std::ratio<5, 9000>>>;

/** @cond INTERNAL */
template<typename Scale, typename Rep, typename Precision>
struct _display_traits<temperature<Scale, delta<Rep, Precision>>> {
    using dpc = typename std::ratio_divide<typename delta<Rep, Precision>::precision, typename Scale::degree>::type;
    static constexpr const char* mark = "";
    static constexpr const char* unit = Scale::suffix;
    static constexpr const char* prefix = _si_prefix_v<dpc>;
};
/** @endcond */

/**
 * @brief Renders a temperature as its exact count with a precision-qualified
 * unit, e.g. `to_string(millicelsius(22500)) == "22500m°C"`.
 *
 * Floating-point counts are rendered in the shortest fixed notation that
 * round-trips, as by `{}` in std::format.
 */
template<typename Scale, typename Rep, typename Precision>
std::string to_string(const temperature<Scale, delta<Rep, Precision>>& t) {
    return _own_unit_string<_display_traits<temperature<Scale, delta<Rep, Precision>>>>(t.count());
}

/**
 * @brief Renders a temperature in the unprefixed degree of its own scale
 * (°C, K or °F), see base_unit,
 * e.g. `to_string(millicelsius(22534), base_unit) == "22.534°C"`.
 */
template<typename Scale, typename Rep, typename Precision>
std::string to_string(const temperature<Scale, delta<Rep, Precision>>& t, base_unit_t) {
    using traits = _display_traits<temperature<Scale, delta<Rep, Precision>>>;
    return _display_string<traits>(_base_unit_display<traits>(t.count()));
}

/**
 * @brief Renders a temperature in its scaled form (its own unit), see scaled,
 * e.g. `to_string(millicelsius(22534), scaled) == "22534m°C"`.
 */
template<typename Scale, typename Rep, typename Precision>
std::string to_string(const temperature<Scale, delta<Rep, Precision>>& t, scaled_t) {
    using traits = _display_traits<temperature<Scale, delta<Rep, Precision>>>;
    return _display_string<traits>(_scaled_display<traits>(t.count()));
}

} // namespace quanta::thermo

namespace std {

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<quanta::thermo::delta<Rep1, Precision1>, quanta::thermo::delta<Rep2, Precision2>> {
private:
    using common_precision = quanta::detail::_common_precision<Precision1, Precision2>;

public:
    using type = quanta::thermo::delta<std::common_type_t<Rep1, Rep2>, common_precision>;
};

template<typename Scale, typename Delta1, typename Delta2>
struct common_type<quanta::thermo::temperature<Scale, Delta1>, quanta::thermo::temperature<Scale, Delta2>> {
    using type = quanta::thermo::temperature<Scale, std::common_type_t<Delta1, Delta2>>;
};

#if CONFIG_QUANTA_STD_FORMAT
// "{}" prints the exact stored count with a precision-qualified unit
// (millicelsius(22500) -> "22500m°C"; floating-point counts in the shortest
// fixed notation that round-trips, as to_string does: never scientific). A
// non-empty spec controls only how the number is rendered (as a double),
// never the unit, so both forms use the same unit:
// std::format("{:.1f}", millicelsius(22534)) == "22534.0m°C".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then the std::formatter<double> spec:
// - 'b' (base unit, see quanta::base_unit) shows the value in the unprefixed
//   degree of the same scale (°C, K or °F; never another scale):
//   std::format("{:b.1f}", millicelsius(22534)) == "22.5°C".
// - 's' (scaled, see quanta::scaled) keeps the type's own unit, since
//   temperatures are not rescaled: std::format("{:s.1f}", millicelsius(22534))
//   == "22534.0m°C".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(t, quanta::base_unit) or to_string(t, quanta::scaled):
// std::format("{:b}", millicelsius(22534)) == "22.534°C".
//
// A spec starting with 'b' or 's' is always a marker, so these letters cannot
// be the fill character of a spec that starts with a fill ("{:b>10.1f}" is the
// marker followed by ">10.1f").
//
// A precision with no SI prefix has no unit label of its own: "{}" is a
// format error, and any other spec renders in the base unit.
template<typename Scale, typename Rep, typename Precision>
struct formatter<quanta::thermo::temperature<Scale, quanta::thermo::delta<Rep, Precision>>>
    : private quanta::thermo::_formatter<
          quanta::thermo::_display_traits<quanta::thermo::temperature<Scale, quanta::thermo::delta<Rep, Precision>>>> {
    using quanta::thermo::_formatter<quanta::thermo::_display_traits<
        quanta::thermo::temperature<Scale, quanta::thermo::delta<Rep, Precision>>>>::parse;

    template<typename FormatContext>
    auto format(
        const quanta::thermo::temperature<Scale, quanta::thermo::delta<Rep, Precision>>& t,
        FormatContext& ctx
    ) const {
        return this->format_quantity(t, ctx);
    }
};

// As for temperatures, with the Δ marker before the unit:
// std::format("{}", delta_millicelsius(1500)) == "1500Δm°C",
// std::format("{:.1f}", delta_millicelsius(1500)) == "1500.0Δm°C" and
// std::format("{:b.1f}", delta_millicelsius(1500)) == "1.5Δ°C". Celsius and
// kelvin deltas share types and display as Δ°C.
template<typename Rep, typename Precision>
struct formatter<quanta::thermo::delta<Rep, Precision>>
    : private quanta::thermo::_formatter<quanta::thermo::_display_traits<quanta::thermo::delta<Rep, Precision>>> {
    using quanta::thermo::_formatter<quanta::thermo::_display_traits<quanta::thermo::delta<Rep, Precision>>>::parse;

    template<typename FormatContext>
    auto format(const quanta::thermo::delta<Rep, Precision>& d, FormatContext& ctx) const {
        return this->format_quantity(d, ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for temperature types.
 */
namespace quanta {
inline namespace literals {
inline namespace thermo_literals {
/** @brief Literal for celsius (e.g., 25_c). */
template<char... Digits>
constexpr quanta::thermo::celsius operator""_c() {
    return quanta::thermo::celsius(detail::check_overflow<quanta::thermo::delta_celsius, Digits...>());
}

/** @brief Literal for decicelsius (e.g., 250_dc). */
template<char... Digits>
constexpr quanta::thermo::decicelsius operator""_dc() {
    return quanta::thermo::decicelsius(detail::check_overflow<quanta::thermo::delta_decicelsius, Digits...>());
}

/** @brief Literal for millicelsius (e.g., 25000_mc). */
template<char... Digits>
constexpr quanta::thermo::millicelsius operator""_mc() {
    return quanta::thermo::millicelsius(detail::check_overflow<quanta::thermo::delta_millicelsius, Digits...>());
}

/** @brief Literal for kelvin (e.g., 298_k). */
template<char... Digits>
constexpr quanta::thermo::kelvin operator""_k() {
    return quanta::thermo::kelvin(detail::check_overflow<quanta::thermo::delta_kelvin, Digits...>());
}

/** @brief Literal for decikelvin (e.g., 2980_dk). */
template<char... Digits>
constexpr quanta::thermo::decikelvin operator""_dk() {
    return quanta::thermo::decikelvin(detail::check_overflow<quanta::thermo::delta_decikelvin, Digits...>());
}

/** @brief Literal for millikelvin (e.g., 298000_mk). */
template<char... Digits>
constexpr quanta::thermo::millikelvin operator""_mk() {
    return quanta::thermo::millikelvin(detail::check_overflow<quanta::thermo::delta_millikelvin, Digits...>());
}

/** @brief Literal for fahrenheit (e.g., 77_f). */
template<char... Digits>
constexpr quanta::thermo::fahrenheit operator""_f() {
    return quanta::thermo::fahrenheit(detail::check_overflow<quanta::thermo::delta_fahrenheit, Digits...>());
}

/** @brief Literal for decifahrenheit (e.g., 770_df). */
template<char... Digits>
constexpr quanta::thermo::decifahrenheit operator""_df() {
    return quanta::thermo::decifahrenheit(detail::check_overflow<quanta::thermo::delta_decifahrenheit, Digits...>());
}

/** @brief Literal for millifahrenheit (e.g., 77000_mf). */
template<char... Digits>
constexpr quanta::thermo::millifahrenheit operator""_mf() {
    return quanta::thermo::millifahrenheit(detail::check_overflow<quanta::thermo::delta_millifahrenheit, Digits...>());
}

/** @brief Literal for delta_celsius (e.g., 5_delta_c); ASCII spelling of `_Δc`. */
template<char... Digits>
constexpr quanta::thermo::delta_celsius operator""_delta_c() {
    return detail::check_overflow<quanta::thermo::delta_celsius, Digits...>();
}

/** @brief Literal for delta_millicelsius (e.g., 5000_delta_mc); ASCII spelling of `_Δmc`. */
template<char... Digits>
constexpr quanta::thermo::delta_millicelsius operator""_delta_mc() {
    return detail::check_overflow<quanta::thermo::delta_millicelsius, Digits...>();
}

/** @brief Literal for delta_kelvin (e.g., 5_delta_k); ASCII spelling of `_Δk`. */
template<char... Digits>
constexpr quanta::thermo::delta_kelvin operator""_delta_k() {
    return detail::check_overflow<quanta::thermo::delta_kelvin, Digits...>();
}

/** @brief Literal for delta_millikelvin (e.g., 5000_delta_mk); ASCII spelling of `_Δmk`. */
template<char... Digits>
constexpr quanta::thermo::delta_millikelvin operator""_delta_mk() {
    return detail::check_overflow<quanta::thermo::delta_millikelvin, Digits...>();
}

/** @brief Literal for delta_fahrenheit (e.g., 9_delta_f); ASCII spelling of `_Δf`. */
template<char... Digits>
constexpr quanta::thermo::delta_fahrenheit operator""_delta_f() {
    return detail::check_overflow<quanta::thermo::delta_fahrenheit, Digits...>();
}

/** @brief Literal for delta_millifahrenheit (e.g., 9000_delta_mf); ASCII spelling of `_Δmf`. */
template<char... Digits>
constexpr quanta::thermo::delta_millifahrenheit operator""_delta_mf() {
    return detail::check_overflow<quanta::thermo::delta_millifahrenheit, Digits...>();
}

#if QUANTA_HAS_UTF8_LITERALS
/** @brief Literal for delta_celsius (e.g., 5_Δc). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS); use
 * `_delta_c` there. */
template<char... Digits>
constexpr quanta::thermo::delta_celsius operator""_Δc() {
    return detail::check_overflow<quanta::thermo::delta_celsius, Digits...>();
}

/** @brief Literal for delta_millicelsius (e.g., 5000_Δmc). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS);
 * use `_delta_mc` there. */
template<char... Digits>
constexpr quanta::thermo::delta_millicelsius operator""_Δmc() {
    return detail::check_overflow<quanta::thermo::delta_millicelsius, Digits...>();
}

/** @brief Literal for delta_kelvin (e.g., 5_Δk). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS); use
 * `_delta_k` there. */
template<char... Digits>
constexpr quanta::thermo::delta_kelvin operator""_Δk() {
    return detail::check_overflow<quanta::thermo::delta_kelvin, Digits...>();
}

/** @brief Literal for delta_millikelvin (e.g., 5000_Δmk). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS);
 * use `_delta_mk` there. */
template<char... Digits>
constexpr quanta::thermo::delta_millikelvin operator""_Δmk() {
    return detail::check_overflow<quanta::thermo::delta_millikelvin, Digits...>();
}

/** @brief Literal for delta_fahrenheit (e.g., 9_Δf). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS); use
 * `_delta_f` there. */
template<char... Digits>
constexpr quanta::thermo::delta_fahrenheit operator""_Δf() {
    return detail::check_overflow<quanta::thermo::delta_fahrenheit, Digits...>();
}

/** @brief Literal for delta_millifahrenheit (e.g., 9000_Δmf). Unavailable on GCC before 14 (see
 * QUANTA_HAS_UTF8_LITERALS); use `_delta_mf` there. */
template<char... Digits>
constexpr quanta::thermo::delta_millifahrenheit operator""_Δmf() {
    return detail::check_overflow<quanta::thermo::delta_millifahrenheit, Digits...>();
}
#endif

} // namespace thermo_literals
} // namespace literals

namespace thermo {
using namespace literals::thermo_literals;
} // namespace thermo
} // namespace quanta
