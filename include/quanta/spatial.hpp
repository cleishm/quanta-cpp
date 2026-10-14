#pragma once

/**
 * @file spatial.hpp
 * @brief Spatial unit types and arithmetic, modeled after std::chrono.
 */

#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <quanta/core.hpp>
#include <ratio>
#include <string>

/**
 * @brief Spatial unit types and utilities.
 *
 * This library provides type-safe handling of spatial quantities - length,
 * area and volume - with support for multiple precisions, both metric
 * (nanometers through kilometers, square millimeters through hectares,
 * milliliters through cubic meters) and imperial/US customary (mils through
 * nautical miles, square inches through acres, cubic inches through US
 * gallons), following the design of std::chrono::duration.
 *
 * ## Basic Usage
 *
 * The library provides standard integer-based types for each precision:
 * @code
 * using namespace quanta::spatial;
 * using namespace quanta::spatial_literals;
 *
 * millimeters board_width{100};
 * kilometers commute{12};
 * feet ceiling{9};
 * auto trace = 8_mil;
 * auto run = 5_km;
 * @endcode
 *
 * ## Metric and Imperial
 *
 * Imperial and nautical units are defined by their exact metric values (the
 * international inch is exactly 25.4 mm, the nautical mile exactly 1852 m),
 * so every precision is an exact std::ratio of one meter and quantities of
 * any precision mix freely:
 *
 * @code
 * static_assert(inches(1) == micrometers(25400));
 * static_assert(miles(1) == feet(5280));
 * auto total = 1_m + 1_in;            // 5127 in units of 0.2 mm
 * auto mm = round<millimeters>(1_ft); // 305 mm
 * @endcode
 *
 * ## Area and Volume
 *
 * Multiplying lengths yields areas and volumes, and dividing them yields
 * lengths and areas again, with the result precision derived exactly from
 * the operand precisions:
 *
 * @code
 * static_assert(1_ft * 1_ft == square_inches(144));
 * static_assert(acres(1) == square_feet(43560));
 * static_assert(us_gallons(1) == cubic_inches(231));
 * auto floor_area = 12_ft * 10_ft;      // 120 ft²
 * auto tank = 2_m * 1_m * 50_cm;        // 100 in units of 0.01 m³
 * auto depth = 1000_cm3 / 100_cm2;      // 10 cm
 * @endcode
 *
 * ## Formatting
 *
 * to_string() and std::format show a quantity in its own unit by default,
 * whatever the format spec: `{}` prints the exact count and `{:.1f}` the count
 * as a double, each followed by the precision's suffix. A single leading
 * marker letter in the spec, or a tag argument to to_string(), opts into a
 * different unit:
 *
 * @code
 * std::format("{}", milliliters(1500));      // "1500mL"   own unit
 * std::format("{:p}", milliliters(1500));    // "1500cm³"  power form
 * std::format("{:b}", milliliters(1500));    // "0.0015m³" base unit
 * std::format("{:s}", milliliters(1500));    // "1.5L"     scaled
 * std::format("{:s.2f}", millimeters(1500)); // "1.50m"
 * to_string(milliliters(1500), scaled);      // "1.5L"
 * @endcode
 *
 * ## Integer vs Floating-Point
 *
 * - **Integer types** (default): Exact arithmetic, support modulo operations,
 *   preferred for embedded and digital systems.
 * - **Floating-point types**: Fractional precision, e.g. length<double> or
 *   length<double, std::milli>.
 *
 * Conversions between precisions are implicit when lossless and require an
 * explicit cast when lossy, exactly as with std::chrono::duration. For
 * example, inches convert implicitly to micrometers (1 in = 25400 µm), but
 * millimeters to inches requires a cast.
 */
namespace quanta::spatial {

// The shared trait, re-exported for convenience; specialize quanta::treat_as_inexact.
using quanta::treat_as_inexact;
using quanta::treat_as_inexact_v;

template<typename Unit, typename Rep, typename Precision = std::ratio<1>>
class quantity;

/**
 * @brief Trait to detect quantity specializations.
 * @tparam T Type to check.
 */
template<typename T>
struct is_quantity : std::false_type {};

template<typename Unit, typename Rep, typename Precision>
struct is_quantity<quantity<Unit, Rep, Precision>> : std::true_type {};

template<typename T>
inline constexpr bool is_quantity_v = is_quantity<T>::value;

/** @cond INTERNAL */
template<typename Rep>
concept not_quantity = !is_quantity_v<Rep>;
/** @endcond */

/**
 * @brief Provides special values for quantity representations.
 * @tparam Rep The representation type.
 */
template<typename Rep>
struct quantity_values {
    /** @brief Returns a zero quantity. */
    static constexpr Rep zero() noexcept { return Rep(0); }

    /** @brief Returns the maximum representable quantity. */
    static constexpr Rep max() noexcept { return std::numeric_limits<Rep>::max(); }

    /** @brief Returns the minimum representable quantity. */
    static constexpr Rep min() noexcept { return std::numeric_limits<Rep>::lowest(); }
};

// ============================================================================
// Units
// ============================================================================

/**
 * @defgroup SpatialUnits Spatial Units
 * @{
 *
 * Tag types identifying the kind of spatial quantity: length, area and
 * volume. Each carries the SI unit symbol of its base unit. The quantity
 * template is unit-generic; the tags differ only in which cross-unit
 * operators relate them, in whether the quantity exposes a `period` alias
 * (lengths only) and in their display suffixes.
 */

/** @brief Length, measured in meters. */
struct meter_unit {
    static constexpr const char* symbol = "m";
};

/** @brief Area, measured in square meters. */
struct square_meter_unit {
    static constexpr const char* symbol = "m²";
};

/** @brief Volume, measured in cubic meters. */
struct cubic_meter_unit {
    static constexpr const char* symbol = "m³";
};

/** @} */ // end of SpatialUnits group

/** @cond INTERNAL */
// Base of quantity that supplies the nested `period` alias for lengths only.
// Generic code that recognises duration-like or distance-like types by their
// `period` member must not accept areas or volumes, so other units get an
// empty base. Non-ratio precisions also get the empty base, leaving the
// diagnostic to quantity's own static_assert.
template<typename Unit, typename Precision>
struct _period_base {};

template<typename Precision>
    requires _is_ratio<Precision>::value
struct _period_base<meter_unit, Precision> {
    using period = typename Precision::type;
};
/** @endcond */

/**
 * @brief A quantity of a spatial unit with a representation and precision.
 *
 * Similar to std::chrono::duration, quantity represents an amount of some
 * spatial unit (meters, square meters or cubic meters). The precision is
 * expressed as a std::ratio of one base unit, so millimeters have precision
 * std::milli, inches have precision std::ratio<127, 5000> (exactly 0.0254 m)
 * and milliliters have precision std::micro (of one cubic meter).
 *
 * ## Examples
 *
 * @code
 * // Integer quantities (default)
 * millimeters width{100};
 * feet height{9};
 * auto run = 5_km;
 *
 * // Arithmetic within a unit
 * auto sum = 100_mm + 5_cm;      // 150 mm
 * auto scaled = 12_in * 3;       // 36 in
 * auto ratio = 1_mi / 1_ft;      // 5280 (scalar)
 *
 * // Arithmetic across units
 * auto patch = 3_ft * 4_ft;      // 12 ft²
 * auto box = patch * 2_ft;       // 24 ft³
 * @endcode
 *
 * Lengths (and only lengths) have a nested `period` alias, equal to
 * `precision`, matching std::chrono::duration::period. It gives a length the
 * same shape as a duration, so it can be passed to generic code that expects
 * a duration-like or distance-like type, such as
 * `quanta::frequency::frequency::wavelength<quanta::spatial::millimeters>()`.
 * Areas and volumes deliberately lack it, so such code rejects them.
 *
 * @tparam Unit      The unit tag type (meter_unit, square_meter_unit or
 *                   cubic_meter_unit).
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1).
 */
template<typename Unit, typename Rep, typename Precision>
class quantity : public _period_base<Unit, Precision> {
    static_assert(!is_quantity_v<Rep>, "rep cannot be a quanta::spatial::quantity");
    static_assert(_is_ratio<Precision>::value, "precision must be a specialization of std::ratio");
    static_assert(Precision::num > 0, "precision must be positive");

public:
    /** @brief The unit tag type. */
    using unit = Unit;
    /** @brief The representation type. */
    using rep = Rep;
    /** @brief The precision as a std::ratio. */
    using precision = typename Precision::type;

    /** @brief Constructs a zero quantity. */
    constexpr quantity() = default;
    quantity(const quantity&) = default;

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
    constexpr explicit quantity(const Rep2& r)
        : _r(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from another quantity of the same unit with different
     * representation or precision.
     *
     * Implicit when the conversion is lossless (the target rep is
     * floating-point, or the target precision evenly divides the source
     * precision and the source rep is not floating-point) and the source rep
     * converts implicitly to the target rep. Any other conversion that the
     * cast can perform is explicit (see below).
     *
     * @tparam Rep2       Source representation type.
     * @tparam Precision2 Source precision.
     * @param q           The source quantity.
     */
    template<typename Rep2, typename Precision2>
        requires std::convertible_to<const Rep2&, rep> &&
        detail::_lossless_conversion<Rep2, Precision2, rep, precision> &&
        detail::_quantity_castable<quantity<Unit, Rep2, Precision2>, quantity>
    constexpr quantity(const quantity<Unit, Rep2, Precision2>& q)
        : _r(quantity_cast<quantity>(q).count()) {}

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
     * @param q           The source quantity.
     */
    template<typename Rep2, typename Precision2>
        requires(!detail::_lossless_conversion<Rep2, Precision2, rep, precision> ||
                 !std::convertible_to<const Rep2&, rep>) &&
        detail::_quantity_castable<quantity<Unit, Rep2, Precision2>, quantity>
    constexpr explicit quantity(const quantity<Unit, Rep2, Precision2>& q)
        : _r(quantity_cast<quantity>(q).count()) {}

    ~quantity() = default;
    quantity& operator=(const quantity&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _r; }

    /** @brief Returns a copy of this quantity. */
    constexpr quantity<Unit, typename std::common_type<rep>::type, precision> operator+() const {
        return quantity<Unit, typename std::common_type<rep>::type, precision>(_r);
    }

    /** @brief Returns the negation of this quantity. */
    constexpr quantity<Unit, typename std::common_type<rep>::type, precision> operator-() const {
        return quantity<Unit, typename std::common_type<rep>::type, precision>(-_r);
    }

    constexpr quantity& operator++() {
        ++_r;
        return *this;
    }

    constexpr quantity operator++(int) { return quantity(_r++); }

    constexpr quantity& operator--() {
        --_r;
        return *this;
    }

    constexpr quantity operator--(int) { return quantity(_r--); }

    constexpr quantity& operator+=(const quantity& q) {
        _r += q.count();
        return *this;
    }

    constexpr quantity& operator-=(const quantity& q) {
        _r -= q.count();
        return *this;
    }

    constexpr quantity& operator*=(const rep& r) {
        _r *= r;
        return *this;
    }

    constexpr quantity& operator/=(const rep& r) {
        _r /= r;
        return *this;
    }

    constexpr quantity& operator%=(const rep& r)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= r;
        return *this;
    }

    constexpr quantity& operator%=(const quantity& q)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= q.count();
        return *this;
    }

    /** @brief Returns a zero quantity. */
    static constexpr quantity zero() noexcept { return quantity(quantity_values<rep>::zero()); }

    /** @brief Returns the minimum representable quantity. */
    static constexpr quantity min() noexcept { return quantity(quantity_values<rep>::min()); }

    /** @brief Returns the maximum representable quantity. */
    static constexpr quantity max() noexcept { return quantity(quantity_values<rep>::max()); }

private:
    rep _r{};
};

/**
 * @brief Converts a quantity to a different precision or representation.
 *
 * For integer-to-integer conversions, this function uses wider intermediate
 * types (128-bit when available) to minimize overflow risk during ratio
 * arithmetic. Integer results truncate toward zero; use floor, ceil or round
 * for other rounding modes.
 *
 * @tparam ToQuantity The target quantity type (must have the same unit).
 * @tparam Unit       The unit tag type.
 * @tparam Rep        Source representation type.
 * @tparam Precision  Source precision.
 * @param q           The quantity to convert.
 *
 * @return The converted quantity.
 *
 * @code
 * using namespace quanta::spatial;
 * auto in = quantity_cast<inches>(millimeters(100));  // 3 in (3.937 truncated)
 * @endcode
 */
template<typename ToQuantity, typename Unit, typename Rep, typename Precision>
    requires(!std::is_same_v<typename ToQuantity::unit, Unit>) ||
    detail::_quantity_cast_requirements<quantity<Unit, Rep, Precision>, ToQuantity>
constexpr ToQuantity quantity_cast(const quantity<Unit, Rep, Precision>& q) {
    // The constraint admits a cast between units, so that this reports it.
    static_assert(
        std::is_same_v<typename ToQuantity::unit, Unit>, "quantity_cast cannot convert between different units"
    );
    return detail::_quantity_cast<ToQuantity>(q);
}

/**
 * @brief Converts a quantity to the target type, rounding toward negative infinity.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToQuantity The target quantity type.
 * @tparam Unit       The unit tag type.
 * @tparam Rep        Source representation type.
 * @tparam Precision  Source precision.
 * @param q           The quantity to convert.
 *
 * @return The converted quantity, rounded toward negative infinity.
 *
 * @code
 * using namespace quanta::spatial;
 * auto m = floor<meters>(millimeters(1500));  // 1 m (rounded down from 1.5)
 * auto in = floor<inches>(centimeters(10));   // 3 in (rounded down from 3.94)
 * @endcode
 */
template<typename ToQuantity, typename Unit, typename Rep, typename Precision>
constexpr ToQuantity floor(const quantity<Unit, Rep, Precision>& q) {
    return detail::_floor_cast(quantity_cast<ToQuantity>(q), q);
}

/**
 * @brief Converts a quantity to the target type, rounding toward positive infinity.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToQuantity The target quantity type.
 * @tparam Unit       The unit tag type.
 * @tparam Rep        Source representation type.
 * @tparam Precision  Source precision.
 * @param q           The quantity to convert.
 *
 * @return The converted quantity, rounded toward positive infinity.
 *
 * @code
 * using namespace quanta::spatial;
 * auto m = ceil<meters>(millimeters(1500));  // 2 m (rounded up from 1.5)
 * auto ft = ceil<feet>(meters(1));           // 4 ft (rounded up from 3.28)
 * @endcode
 */
template<typename ToQuantity, typename Unit, typename Rep, typename Precision>
constexpr ToQuantity ceil(const quantity<Unit, Rep, Precision>& q) {
    return detail::_ceil_cast(quantity_cast<ToQuantity>(q), q);
}

/**
 * @brief Converts a quantity to the target type, rounding toward zero.
 *
 * The same as quantity_cast<ToQuantity>(q).
 *
 * @tparam ToQuantity The target quantity type.
 * @tparam Unit       The unit tag type.
 * @tparam Rep        Source representation type.
 * @tparam Precision  Source precision.
 * @param q           The quantity to convert.
 *
 * @return The converted quantity, rounded toward zero.
 *
 * @code
 * using namespace quanta::spatial;
 * auto m = trunc<meters>(millimeters(-1500));  // -1 m (rounded toward zero from -1.5)
 * @endcode
 */
template<typename ToQuantity, typename Unit, typename Rep, typename Precision>
constexpr ToQuantity trunc(const quantity<Unit, Rep, Precision>& q) {
    return quantity_cast<ToQuantity>(q);
}

/**
 * @brief Converts a quantity to the target type, rounding to nearest (ties to even).
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToQuantity The target quantity type.
 * @tparam Unit       The unit tag type.
 * @tparam Rep        Source representation type.
 * @tparam Precision  Source precision.
 * @param q           The quantity to convert.
 *
 * @return The converted quantity, rounded to nearest.
 *
 * @code
 * using namespace quanta::spatial;
 * auto m = round<meters>(millimeters(1500));  // 2 m (rounded to even)
 * auto mm = round<millimeters>(feet(1));      // 305 mm (from 304.8)
 * @endcode
 */
template<typename ToQuantity, typename Unit, typename Rep, typename Precision>
constexpr ToQuantity round(const quantity<Unit, Rep, Precision>& q) {
    return detail::_round_cast<ToQuantity>(q, [&q] { return floor<ToQuantity>(q); });
}

/**
 * @brief Returns the absolute value of a quantity.
 *
 * @tparam Unit      The unit tag type.
 * @tparam Rep       Representation type.
 * @tparam Precision Precision type.
 * @param q          The quantity.
 *
 * @return The absolute value of the quantity.
 */
template<typename Unit, typename Rep, typename Precision>
constexpr quantity<Unit, Rep, Precision> abs(const quantity<Unit, Rep, Precision>& q) {
    return detail::_abs(q);
}

// ============================================================================
// Same-unit arithmetic
// ============================================================================

/** @brief Returns the sum of two quantities of the same unit. */
template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator+(const quantity<Unit, Rep1, Precision1>& lhs, const quantity<Unit, Rep2, Precision2>& rhs)
    -> std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>> {
    using cq = std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>>;
    return cq(cq(lhs).count() + cq(rhs).count());
}

/** @brief Returns the difference of two quantities of the same unit. */
template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator-(const quantity<Unit, Rep1, Precision1>& lhs, const quantity<Unit, Rep2, Precision2>& rhs)
    -> std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>> {
    using cq = std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>>;
    return cq(cq(lhs).count() - cq(rhs).count());
}

/** @brief Multiplies a quantity by a scalar. */
template<typename Unit, typename Rep1, typename Precision, typename Rep2>
    requires not_quantity<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const quantity<Unit, Rep1, Precision>& q, const Rep2& r)
    -> quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision> {
    using cq = quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision>;
    return cq(cq(q).count() * r);
}

/** @brief Multiplies a scalar by a quantity. */
template<typename Unit, typename Rep1, typename Rep2, typename Precision>
    requires not_quantity<Rep1> && std::convertible_to<const Rep1&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const Rep1& r, const quantity<Unit, Rep2, Precision>& q)
    -> quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision> {
    return q * r;
}

/** @brief Divides a quantity by a scalar. */
template<typename Unit, typename Rep1, typename Precision, typename Rep2>
    requires not_quantity<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator/(const quantity<Unit, Rep1, Precision>& q, const Rep2& s)
    -> quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision> {
    using cq = quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision>;
    return cq(cq(q).count() / s);
}

/** @brief Divides two quantities of the same unit, returning a scalar. */
template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator/(const quantity<Unit, Rep1, Precision1>& lhs, const quantity<Unit, Rep2, Precision2>& rhs)
    -> std::common_type_t<Rep1, Rep2> {
    using cq = std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>>;
    return cq(lhs).count() / cq(rhs).count();
}

/** @brief Returns the remainder of dividing a quantity by a scalar. */
template<typename Unit, typename Rep1, typename Precision, typename Rep2>
    requires not_quantity<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>> &&
    (!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const quantity<Unit, Rep1, Precision>& q, const Rep2& s)
    -> quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision> {
    using cq = quantity<Unit, std::common_type_t<Rep1, Rep2>, Precision>;
    return cq(cq(q).count() % s);
}

/** @brief Returns the remainder of dividing two quantities of the same unit. */
template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires(!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const quantity<Unit, Rep1, Precision1>& lhs, const quantity<Unit, Rep2, Precision2>& rhs)
    -> std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>> {
    using cq = std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>>;
    return cq(cq(lhs).count() % cq(rhs).count());
}

template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr bool operator==(const quantity<Unit, Rep1, Precision1>& lhs, const quantity<Unit, Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>>;
    return ct(lhs).count() == ct(rhs).count();
}

template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires std::three_way_comparable<std::common_type_t<Rep1, Rep2>>
constexpr auto operator<=>(const quantity<Unit, Rep1, Precision1>& lhs, const quantity<Unit, Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<quantity<Unit, Rep1, Precision1>, quantity<Unit, Rep2, Precision2>>;
    return ct(lhs).count() <=> ct(rhs).count();
}

// ============================================================================
// Cross-unit arithmetic
// ============================================================================

/** @cond INTERNAL */
// Product of two quantities, in the given result unit. Counts multiply and
// precisions multiply, so no conversion factor is needed and the result is
// exact for integer representations.
template<typename ToUnit, typename U1, typename R1, typename P1, typename U2, typename R2, typename P2>
constexpr auto _product(const quantity<U1, R1, P1>& lhs, const quantity<U2, R2, P2>& rhs) {
    using cr = std::common_type_t<R1, R2>;
    using result_t = quantity<ToUnit, cr, std::ratio_multiply<P1, P2>>;
    return result_t(static_cast<cr>(lhs.count()) * static_cast<cr>(rhs.count()));
}

// Quotient of two quantities, in the given result unit. Counts divide and
// precisions divide; for integer representations the count division truncates.
template<typename ToUnit, typename U1, typename R1, typename P1, typename U2, typename R2, typename P2>
constexpr auto _quotient(const quantity<U1, R1, P1>& lhs, const quantity<U2, R2, P2>& rhs) {
    using cr = std::common_type_t<R1, R2>;
    using result_t = quantity<ToUnit, cr, std::ratio_divide<P1, P2>>;
    return result_t(static_cast<cr>(lhs.count()) / static_cast<cr>(rhs.count()));
}
/** @endcond */

/**
 * @defgroup SpatialCrossUnit Spatial Cross-Unit Arithmetic
 * @{
 *
 * Operators relating length, area and volume. Only these combinations are
 * defined; any other product or quotient of quantities of different units
 * (such as area x area or length / area) is ill-formed, as is adding or
 * comparing quantities of different units.
 *
 * The result precision is the product (or quotient) of the operand
 * precisions and the representation is their common type, so products are
 * exact for integer representations: feet(3) * feet(4) is exactly
 * square_feet(12). Quotients divide the counts, so for integer
 * representations they truncate in the result precision, which may be coarse
 * when the operand precisions differ (square_meters(10) / millimeters(3) has
 * kilometer precision and yields 3 km). Convert the operands to suitable
 * precisions first, or use floating-point representations, for fractional
 * results.
 *
 * A product whose precision cannot be represented as a std::ratio (such as
 * nanometers x nanometers x nanometers, 10⁻²⁷ m³) is a compile-time error.
 */

/** @brief length x length = area. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<meter_unit, R1, P1>& lhs, const quantity<meter_unit, R2, P2>& rhs) {
    return _product<square_meter_unit>(lhs, rhs);
}

/** @brief length x area = volume. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<meter_unit, R1, P1>& lhs, const quantity<square_meter_unit, R2, P2>& rhs) {
    return _product<cubic_meter_unit>(lhs, rhs);
}

/** @brief area x length = volume. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<square_meter_unit, R1, P1>& lhs, const quantity<meter_unit, R2, P2>& rhs) {
    return _product<cubic_meter_unit>(lhs, rhs);
}

/** @brief area / length = length. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<square_meter_unit, R1, P1>& lhs, const quantity<meter_unit, R2, P2>& rhs) {
    return _quotient<meter_unit>(lhs, rhs);
}

/** @brief volume / length = area. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<cubic_meter_unit, R1, P1>& lhs, const quantity<meter_unit, R2, P2>& rhs) {
    return _quotient<square_meter_unit>(lhs, rhs);
}

/** @brief volume / area = length. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto
operator/(const quantity<cubic_meter_unit, R1, P1>& lhs, const quantity<square_meter_unit, R2, P2>& rhs) {
    return _quotient<meter_unit>(lhs, rhs);
}

/** @} */ // end of SpatialCrossUnit group

// ============================================================================
// Type aliases
// ============================================================================

/**
 * @defgroup SpatialQuantityTypes Standard Spatial Quantity Type Aliases
 * @{
 *
 * Standard integer-based types using int64_t representation. For
 * floating-point representations, use the alias templates, e.g.
 * length<double>, length<double, std::milli> or volume<double, std::milli>.
 *
 * Imperial and nautical precisions are exact ratios of one meter, from the
 * international definitions: 1 in = 25.4 mm, 1 ft = 12 in, 1 yd = 3 ft,
 * 1 mi = 5280 ft, 1 mil = 1/1000 in and 1 nmi = 1852 m. Area and volume
 * precisions are exact ratios of one square or cubic meter: the squares and
 * cubes of the length precisions, plus 1 ha = 1 hm², 1 acre = 43560 ft²,
 * 1 L = 1 dm³ and 1 US gallon = 231 in³.
 */

/** @brief Length quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using length = quantity<meter_unit, Rep, Precision>;

/** @brief Length with 1 nm precision. */
using nanometers = length<int64_t, std::nano>;
/** @brief Length with 1 µm precision. */
using micrometers = length<int64_t, std::micro>;
/** @brief Length with 1 mm precision. */
using millimeters = length<int64_t, std::milli>;
/** @brief Length with 1 cm precision. */
using centimeters = length<int64_t, std::centi>;
/** @brief Length with 1 m precision. */
using meters = length<int64_t>;
/** @brief Length with 1 km precision. */
using kilometers = length<int64_t, std::kilo>;

/** @brief Length with 1 mil (thou, 1/1000 in = 25.4 µm) precision. */
using mils = length<int64_t, std::ratio<127, 5000000>>;
/** @brief Length with 1 in (25.4 mm) precision. */
using inches = length<int64_t, std::ratio<127, 5000>>;
/** @brief Length with 1 ft (12 in = 0.3048 m) precision. */
using feet = length<int64_t, std::ratio<381, 1250>>;
/** @brief Length with 1 yd (3 ft = 0.9144 m) precision. */
using yards = length<int64_t, std::ratio<1143, 1250>>;
/** @brief Length with 1 mi (5280 ft = 1609.344 m) precision. */
using miles = length<int64_t, std::ratio<201168, 125>>;
/** @brief Length with 1 nmi (1852 m) precision. */
using nautical_miles = length<int64_t, std::ratio<1852>>;

/** @brief Area quantity with configurable representation and precision (a ratio of one m²). */
template<typename Rep, typename Precision = std::ratio<1>>
using area = quantity<square_meter_unit, Rep, Precision>;

/** @brief Area with 1 mm² precision. */
using square_millimeters = area<int64_t, std::micro>;
/** @brief Area with 1 cm² precision. */
using square_centimeters = area<int64_t, std::ratio<1, 10000>>;
/** @brief Area with 1 m² precision. */
using square_meters = area<int64_t>;
/** @brief Area with 1 ha (1 hm² = 10,000 m²) precision. */
using hectares = area<int64_t, std::ratio<10000>>;
/** @brief Area with 1 km² precision. */
using square_kilometers = area<int64_t, std::mega>;

/** @brief Area with 1 in² (645.16 mm²) precision. */
using square_inches = area<int64_t, std::ratio<16129, 25000000>>;
/** @brief Area with 1 ft² (144 in² = 0.09290304 m²) precision. */
using square_feet = area<int64_t, std::ratio<145161, 1562500>>;
/** @brief Area with 1 yd² (9 ft² = 0.83612736 m²) precision. */
using square_yards = area<int64_t, std::ratio<1306449, 1562500>>;
/** @brief Area with 1 acre (43,560 ft² = 4046.8564224 m²) precision. */
using acres = area<int64_t, std::ratio<316160658, 78125>>;
/** @brief Area with 1 mi² (640 acres = 2,589,988.110336 m²) precision. */
using square_miles = area<int64_t, std::ratio<40468564224, 15625>>;

/** @brief Volume quantity with configurable representation and precision (a ratio of one m³). */
template<typename Rep, typename Precision = std::ratio<1>>
using volume = quantity<cubic_meter_unit, Rep, Precision>;

/** @brief Volume with 1 µL (1 mm³) precision. */
using microliters = volume<int64_t, std::nano>;
/** @brief Volume with 1 mm³ precision (the same type as microliters). */
using cubic_millimeters = microliters;
/** @brief Volume with 1 mL (1 cm³) precision. */
using milliliters = volume<int64_t, std::micro>;
/** @brief Volume with 1 cm³ precision (the same type as milliliters). */
using cubic_centimeters = milliliters;
/** @brief Volume with 1 L (1 dm³) precision. */
using liters = volume<int64_t, std::milli>;
/** @brief Volume with 1 m³ precision. */
using cubic_meters = volume<int64_t>;

/** @brief Volume with 1 in³ (16.387064 mL) precision. */
using cubic_inches = volume<int64_t, std::ratio<2048383, 125000000000>>;
/** @brief Volume with 1 ft³ (1728 in³ = 28.316846592 L) precision. */
using cubic_feet = volume<int64_t, std::ratio<55306341, 1953125000>>;
/** @brief Volume with 1 yd³ (27 ft³ = 0.764554857984 m³) precision. */
using cubic_yards = volume<int64_t, std::ratio<1493271207, 1953125000>>;
/** @brief Volume with 1 US gallon (231 in³ = 3.785411784 L) precision. */
using us_gallons = volume<int64_t, std::ratio<473176473, 125000000000>>;

/** @} */ // end of SpatialQuantityTypes group

// ============================================================================
// String conversion and formatting
// ============================================================================

/**
 * @brief Trait giving the display symbol for an SI precision prefix.
 *
 * Specializations exist for the standard decade prefixes from femto to tera,
 * plus centi (for centimeters).
 * @tparam Precision The precision ratio.
 */
template<typename Precision>
struct si_prefix;

/** @cond INTERNAL */
template<>
struct si_prefix<std::femto> {
    static constexpr const char* symbol = "f";
};

template<>
struct si_prefix<std::pico> {
    static constexpr const char* symbol = "p";
};

template<>
struct si_prefix<std::nano> {
    static constexpr const char* symbol = "n";
};

template<>
struct si_prefix<std::micro> {
    static constexpr const char* symbol = "µ";
};

template<>
struct si_prefix<std::milli> {
    static constexpr const char* symbol = "m";
};

template<>
struct si_prefix<std::centi> {
    static constexpr const char* symbol = "c";
};

template<>
struct si_prefix<std::ratio<1>> {
    static constexpr const char* symbol = "";
};

template<>
struct si_prefix<std::kilo> {
    static constexpr const char* symbol = "k";
};

template<>
struct si_prefix<std::mega> {
    static constexpr const char* symbol = "M";
};

template<>
struct si_prefix<std::giga> {
    static constexpr const char* symbol = "G";
};

template<>
struct si_prefix<std::tera> {
    static constexpr const char* symbol = "T";
};
/** @endcond */

/**
 * @brief Trait giving the display suffix for a (unit, precision) pair.
 *
 * A specialization provides `static std::string value()`, the default
 * suffix, and may also provide `static std::string power_value()`, the
 * power-form suffix (the precision written as a power of a length unit, as
 * selected by to_string(q, power_form) or the `p` format marker). When
 * `power_value()` is absent the power form is the same as `value()`. A
 * (unit, precision) pair without `value()` has no suffix, and quantities of
 * it cannot be converted to strings or formatted.
 *
 * For lengths, the primary template combines an SI prefix with the unit
 * symbol (e.g. "mm", "km") and is only usable when the precision is a
 * standard SI prefix. Full specializations provide the conventional
 * suffixes for the imperial and nautical precisions: "mil", "in", "ft", "yd",
 * "mi" and "nmi". A length's power form is its default suffix.
 *
 * For areas and volumes the SI prefix rule does not apply, since a prefix
 * scales the length before it is squared or cubed (a millimeter squared is
 * 10⁻⁶ m², not 10⁻³ m²). Only these precisions have suffixes, shown as
 * default / power form where they differ:
 *
 * - Area: nm², µm², mm², cm², m², ha / hm², km², mil², in², ft², yd², ac,
 *   mi² and nmi².
 * - Volume: µm³, µL / mm³, mL / cm³, L / dm³, m³, km³, in³, ft³, yd³, mi³,
 *   nmi³ and gal (US gallon).
 *
 * The base-unit and scaled forms (see base_unit and scaled) do not use these
 * suffixes for metric precisions, i.e. those that are powers of ten of the
 * base unit: they compute the value in the base unit or in a unit chosen by
 * magnitude. The scaled form shows other precisions with value().
 *
 * @tparam Unit      The unit tag type.
 * @tparam Precision The (reduced) precision ratio.
 */
template<typename Unit, typename Precision>
struct quantity_suffix {
    /** @brief Returns the display suffix. */
    static std::string value()
        requires requires { si_prefix<Precision>::symbol; }
    {
        return std::string(si_prefix<Precision>::symbol) + Unit::symbol;
    }
};

/** @cond INTERNAL */
template<typename R>
using _square_ratio = std::ratio_multiply<R, R>;

template<typename R>
using _cube_ratio = std::ratio_multiply<std::ratio_multiply<R, R>, R>;

// Areas and volumes have no SI prefix rule: only the explicit specializations
// below have suffixes.
template<typename Precision>
struct quantity_suffix<square_meter_unit, Precision> {};

template<typename Precision>
struct quantity_suffix<cubic_meter_unit, Precision> {};

template<>
struct quantity_suffix<meter_unit, std::ratio<127, 5000000>> {
    static std::string value() { return "mil"; }
};

template<>
struct quantity_suffix<meter_unit, std::ratio<127, 5000>> {
    static std::string value() { return "in"; }
};

template<>
struct quantity_suffix<meter_unit, std::ratio<381, 1250>> {
    static std::string value() { return "ft"; }
};

template<>
struct quantity_suffix<meter_unit, std::ratio<1143, 1250>> {
    static std::string value() { return "yd"; }
};

template<>
struct quantity_suffix<meter_unit, std::ratio<201168, 125>> {
    static std::string value() { return "mi"; }
};

template<>
struct quantity_suffix<meter_unit, std::ratio<1852>> {
    static std::string value() { return "nmi"; }
};

// Area
template<>
struct quantity_suffix<square_meter_unit, _square_ratio<std::nano>> {
    static std::string value() { return "nm²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<std::micro>> {
    static std::string value() { return "µm²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<std::milli>> {
    static std::string value() { return "mm²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<std::centi>> {
    static std::string value() { return "cm²"; }
};

template<>
struct quantity_suffix<square_meter_unit, std::ratio<1>> {
    static std::string value() { return "m²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<std::hecto>> {
    static std::string value() { return "ha"; }
    static std::string power_value() { return "hm²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<std::kilo>> {
    static std::string value() { return "km²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<mils::precision>> {
    static std::string value() { return "mil²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<inches::precision>> {
    static std::string value() { return "in²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<feet::precision>> {
    static std::string value() { return "ft²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<yards::precision>> {
    static std::string value() { return "yd²"; }
};

template<>
struct quantity_suffix<square_meter_unit, acres::precision> {
    static std::string value() { return "ac"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<miles::precision>> {
    static std::string value() { return "mi²"; }
};

template<>
struct quantity_suffix<square_meter_unit, _square_ratio<nautical_miles::precision>> {
    static std::string value() { return "nmi²"; }
};

// Volume
template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<std::micro>> {
    static std::string value() { return "µm³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<std::milli>> {
    static std::string value() { return "µL"; }
    static std::string power_value() { return "mm³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<std::centi>> {
    static std::string value() { return "mL"; }
    static std::string power_value() { return "cm³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<std::deci>> {
    static std::string value() { return "L"; }
    static std::string power_value() { return "dm³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, std::ratio<1>> {
    static std::string value() { return "m³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<std::kilo>> {
    static std::string value() { return "km³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<inches::precision>> {
    static std::string value() { return "in³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<feet::precision>> {
    static std::string value() { return "ft³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<yards::precision>> {
    static std::string value() { return "yd³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<miles::precision>> {
    static std::string value() { return "mi³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, _cube_ratio<nautical_miles::precision>> {
    static std::string value() { return "nmi³"; }
};

template<>
struct quantity_suffix<cubic_meter_unit, us_gallons::precision> {
    static std::string value() { return "gal"; }
};

template<typename Unit, typename Precision>
concept _has_suffix = requires { quantity_suffix<Unit, Precision>::value(); };

// The power-form suffix: power_value() when the specialization provides it,
// otherwise the default suffix.
template<typename Unit, typename Precision>
std::string _power_suffix() {
    if constexpr (requires { quantity_suffix<Unit, Precision>::power_value(); }) {
        return quantity_suffix<Unit, Precision>::power_value();
    } else {
        return quantity_suffix<Unit, Precision>::value();
    }
}

// A value and suffix to display, for the base-unit and scaled forms.
struct _display {
    double value;
    std::string suffix;
};

// Metric precisions are exactly the powers of ten (of the base unit); the
// imperial, nautical and US customary ones are not.
template<typename Precision>
concept _metric_precision = detail::_power_of_ten_precision<Precision>;

// The units the scaled form chooses between, by power-of-ten exponent of the
// base unit, in ascending order. Units without a list render in their own
// precision.
template<typename Unit>
struct _scale_units {};

template<>
struct _scale_units<meter_unit> {
    static constexpr detail::_scale_entry values[] = {{-9, "nm"}, {-6, "µm"}, {-3, "mm"}, {0, "m"}, {3, "km"}};
};

template<>
struct _scale_units<square_meter_unit> {
    static constexpr detail::_scale_entry values[] = {{-18, "nm²"}, {-12, "µm²"}, {-6, "mm²"}, {0, "m²"}, {6, "km²"}};
};

template<>
struct _scale_units<cubic_meter_unit> {
    static constexpr detail::_scale_entry values[] = {{-9, "µL"}, {-6, "mL"}, {-3, "L"}, {0, "m³"}};
};

// The quantity in the unit's base unit (m, m² or m³), as a double.
template<typename Unit, typename Rep, typename Precision>
_display _base_unit_display(const quantity<Unit, Rep, Precision>& q) {
    using p = typename Precision::type;
    return {detail::_ratio_value<p>(q.count()), Unit::symbol};
}

// The quantity in the largest candidate unit in which its magnitude is at
// least 1, else the smallest; zero (keeping its sign) and non-finite values
// are shown in the base unit, as the base-unit form would show them. Non-metric
// precisions are shown in their own unit. Each candidate's value is computed
// from the count with a single scaling by a power of ten, so values that
// land exactly on a unit (1000 mm) are exactly 1 in it.
template<typename Unit, typename Rep, typename Precision>
_display _scaled_display(const quantity<Unit, Rep, Precision>& q) {
    using p = typename Precision::type;
    const double count = static_cast<double>(q.count());
    if constexpr (_metric_precision<p> && requires { _scale_units<Unit>::values; }) {
        if (count == 0 || !std::isfinite(count)) {
            return {count, Unit::symbol};
        }
        const detail::_scaled s = detail::_pick_scale(count, _precision_exponent<p>, _scale_units<Unit>::values);
        return {s.value, s.label};
    } else {
        return {count, quantity_suffix<Unit, p>::value()};
    }
}

#if CONFIG_QUANTA_STD_FORMAT
// How std::format renders a quantity of the given unit and (reduced)
// precision, with the power form ('p'); see detail::_unit_formatter.
template<typename Unit, typename Precision>
struct _format_traits {
    static constexpr const char* markers = "pbs";
    static constexpr bool has_own_unit = true;

    static std::string own_unit(bool power) {
        const std::string suffix = power ? _power_suffix<Unit, Precision>() : quantity_suffix<Unit, Precision>::value();
        return suffix;
    }

    template<typename OutputIt>
    static OutputIt append_own_unit(OutputIt out, const std::string& suffix) {
        return std::format_to(out, "{}", suffix);
    }

    template<typename Q>
    static _display base_display(const Q& q) {
        return _base_unit_display(q);
    }

    template<typename Q>
    static _display scaled_display(const Q& q) {
        return _scaled_display(q);
    }

    template<typename OutputIt>
    static OutputIt append_label(OutputIt out, const _display& d) {
        return std::format_to(out, "{}", d.suffix);
    }
};
#endif

/** @endcond */

/**
 * @brief Tag type selecting the power-form suffix, see power_form.
 */
struct power_form_t {
    explicit power_form_t() = default;
};

/**
 * @brief Selects the power-form suffix in to_string().
 *
 * The power form writes the precision as a power of a length unit, so
 * milliliters display as "cm³" rather than "mL", liters as "dm³" and
 * hectares as "hm²". Suffixes that are already powers (and all length
 * suffixes) are unchanged, as are acres ("ac") and US gallons ("gal"), which
 * have no power form.
 *
 * @code
 * to_string(milliliters(24), power_form);  // "24cm³"
 * to_string(hectares(3), power_form);      // "3hm²"
 * to_string(square_feet(12), power_form);  // "12ft²"
 * @endcode
 */
inline constexpr power_form_t power_form{};

/**
 * @brief Selects the base-unit form in to_string().
 *
 * The quantity is shown in its unit's base unit (m, m² or m³), with the value
 * (count x precision) rendered in the shortest fixed notation that
 * round-trips (never scientific), exactly as by the `b` format marker.
 *
 * @code
 * to_string(millimeters(1500), base_unit);  // "1.5m"
 * to_string(feet(6), base_unit);            // "1.8288m"
 * to_string(liters(2), base_unit);          // "0.002m³"
 * to_string(hectares(3), base_unit);        // "30000m²"
 * @endcode
 */
using quanta::base_unit;
using quanta::base_unit_t;

/**
 * @brief Selects the scaled form in to_string().
 *
 * Metric quantities (those whose precision is a power of ten) are shown in
 * the largest of a fixed set of units in which the magnitude is at least 1,
 * or the smallest of the set if there is none; zero is shown in the base
 * unit. The units are nm, µm, mm, m and km for lengths; nm², µm², mm², m²
 * and km² for areas; and µL, mL, L and m³ for volumes. Other quantities
 * (mil, in, ft, acres, gallons, ...) are shown in their own unit. The value
 * is rendered in the shortest fixed notation that round-trips (never
 * scientific), exactly as by the `s` format marker.
 *
 * @code
 * to_string(millimeters(1500), scaled);           // "1.5m"
 * to_string(micrometers(-2500), scaled);          // "-2.5mm"
 * to_string(milliliters(1500), scaled);           // "1.5L"
 * to_string(square_millimeters(2500000), scaled); // "2.5m²"
 * to_string(feet(6), scaled);                     // "6ft"
 * @endcode
 */
using quanta::scaled;
using quanta::scaled_t;

/**
 * @brief Converts a quantity to a string with its unit suffix.
 *
 * Available for quantities whose precision has a conventional suffix (see
 * quantity_suffix): standard SI prefixes plus mil, in, ft, yd, mi and nmi for
 * lengths, and the listed area and volume precisions. Integer counts are
 * shown exactly; floating-point counts in the shortest fixed notation that
 * round-trips (never scientific), matching std::format("{}").
 *
 * @code
 * to_string(millimeters(250));  // "250mm"
 * to_string(micrometers(5));    // "5µm"
 * to_string(feet(6));           // "6ft"
 * to_string(square_feet(12));   // "12ft²"
 * to_string(milliliters(24));   // "24mL"
 * to_string(length<double>(2.5)); // "2.5m"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q) {
    return _count_string(q.count()) + quantity_suffix<Unit, typename Precision::type>::value();
}

/**
 * @brief Converts a quantity to a string with its power-form unit suffix.
 *
 * Available for the same quantities as to_string(q); see power_form.
 *
 * @code
 * to_string(milliliters(24), power_form);  // "24cm³"
 * to_string(liters(2), power_form);        // "2dm³"
 * to_string(millimeters(250), power_form); // "250mm"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, power_form_t) {
    return _count_string(q.count()) + _power_suffix<Unit, typename Precision::type>();
}

/**
 * @brief Converts a quantity to a string in its base unit (m, m² or m³).
 *
 * Available for the same quantities as to_string(q); see base_unit.
 *
 * @code
 * to_string(millimeters(1500), base_unit);  // "1.5m"
 * to_string(hectares(3), base_unit);        // "30000m²"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, base_unit_t) {
    const _display d = _base_unit_display(q);
    return _fixed_string(d.value) + d.suffix;
}

/**
 * @brief Converts a quantity to a string in a unit scaled to its magnitude.
 *
 * Available for the same quantities as to_string(q); see scaled.
 *
 * @code
 * to_string(millimeters(1500), scaled);  // "1.5m"
 * to_string(liters(2000), scaled);       // "2m³"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, scaled_t) {
    const _display d = _scaled_display(q);
    return _fixed_string(d.value) + d.suffix;
}

} // namespace quanta::spatial

namespace std {

template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<
    quanta::spatial::quantity<Unit, Rep1, Precision1>,
    quanta::spatial::quantity<Unit, Rep2, Precision2>> {
private:
    using common_precision = quanta::detail::_common_precision<Precision1, Precision2>;

public:
    using type = quanta::spatial::quantity<Unit, std::common_type_t<Rep1, Rep2>, common_precision>;
};

#if CONFIG_QUANTA_STD_FORMAT
// "{}" prints the exact stored count with a precision-qualified unit
// (millimeters(250) -> "250mm"; floating-point counts in the shortest fixed
// notation that round-trips, as to_string does: length<double>(1e6) ->
// "1000000m", never "1e+06m"). A non-empty spec controls only how the number
// is rendered (as double), never the unit, so both forms use the same suffix:
// std::format("{:.1f}", millimeters(1500)) == "1500.0mm" and
// std::format("{:.1f}", feet(6)) == "6.0ft".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then handled as above:
// - 'p' (power form, see quanta::spatial::power_form) keeps the count and writes the
//   suffix as a power of a length unit: std::format("{:p}", milliliters(24))
//   == "24cm³" and std::format("{:p.1f}", milliliters(24)) == "24.0cm³".
// - 'b' (base unit, see quanta::base_unit) shows the value in m, m² or m³:
//   std::format("{:b.1f}", millimeters(1500)) == "1.5m".
// - 's' (scaled, see quanta::scaled) shows a metric value in a unit suited
//   to its magnitude: std::format("{:s}", milliliters(1500)) == "1.5L".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(q, quanta::base_unit) or to_string(q, quanta::scaled):
// std::format("{:b}", milliliters(24)) == "0.000024m³".
//
// A spec starting with 'p', 'b' or 's' is always a marker, so these letters
// cannot be the fill character of a spec that starts with a fill
// ("{:p>10.1f}" is the marker followed by ">10.1f").
template<typename Unit, typename Rep, typename Precision>
    requires quanta::spatial::_has_suffix<Unit, typename Precision::type>
struct formatter<quanta::spatial::quantity<Unit, Rep, Precision>, char>
    : private quanta::detail::_unit_formatter<quanta::spatial::_format_traits<Unit, typename Precision::type>> {
    using quanta::detail::_unit_formatter<quanta::spatial::_format_traits<Unit, typename Precision::type>>::parse;

    template<typename FormatContext>
    auto format(const quanta::spatial::quantity<Unit, Rep, Precision>& q, FormatContext& ctx) const {
        return this->format_quantity(q, ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for spatial quantity types.
 */
namespace quanta {
inline namespace literals {
inline namespace spatial_literals {
/** @brief Literal for nanometers (e.g., 850_nm). */
template<char... Digits>
constexpr quanta::spatial::nanometers operator""_nm() {
    return detail::check_overflow<quanta::spatial::nanometers, Digits...>();
}

/** @brief Literal for micrometers (e.g., 50_um). */
template<char... Digits>
constexpr quanta::spatial::micrometers operator""_um() {
    return detail::check_overflow<quanta::spatial::micrometers, Digits...>();
}

#if QUANTA_HAS_UTF8_LITERALS
/** @brief Literal for micrometers (e.g., 50_µm). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS). */
template<char... Digits>
constexpr quanta::spatial::micrometers operator""_µm() {
    return detail::check_overflow<quanta::spatial::micrometers, Digits...>();
}
#endif

/** @brief Literal for millimeters (e.g., 250_mm). */
template<char... Digits>
constexpr quanta::spatial::millimeters operator""_mm() {
    return detail::check_overflow<quanta::spatial::millimeters, Digits...>();
}

/** @brief Literal for centimeters (e.g., 30_cm). */
template<char... Digits>
constexpr quanta::spatial::centimeters operator""_cm() {
    return detail::check_overflow<quanta::spatial::centimeters, Digits...>();
}

/** @brief Literal for meters (e.g., 100_m). */
template<char... Digits>
constexpr quanta::spatial::meters operator""_m() {
    return detail::check_overflow<quanta::spatial::meters, Digits...>();
}

/** @brief Literal for kilometers (e.g., 5_km). */
template<char... Digits>
constexpr quanta::spatial::kilometers operator""_km() {
    return detail::check_overflow<quanta::spatial::kilometers, Digits...>();
}

/** @brief Literal for mils (e.g., 8_mil). */
template<char... Digits>
constexpr quanta::spatial::mils operator""_mil() {
    return detail::check_overflow<quanta::spatial::mils, Digits...>();
}

/** @brief Literal for inches (e.g., 12_in). */
template<char... Digits>
constexpr quanta::spatial::inches operator""_in() {
    return detail::check_overflow<quanta::spatial::inches, Digits...>();
}

/** @brief Literal for feet (e.g., 6_ft). */
template<char... Digits>
constexpr quanta::spatial::feet operator""_ft() {
    return detail::check_overflow<quanta::spatial::feet, Digits...>();
}

/** @brief Literal for yards (e.g., 100_yd). */
template<char... Digits>
constexpr quanta::spatial::yards operator""_yd() {
    return detail::check_overflow<quanta::spatial::yards, Digits...>();
}

/** @brief Literal for miles (e.g., 26_mi). */
template<char... Digits>
constexpr quanta::spatial::miles operator""_mi() {
    return detail::check_overflow<quanta::spatial::miles, Digits...>();
}

/** @brief Literal for nautical miles (e.g., 12_nmi). */
template<char... Digits>
constexpr quanta::spatial::nautical_miles operator""_nmi() {
    return detail::check_overflow<quanta::spatial::nautical_miles, Digits...>();
}

/** @brief Literal for square millimeters (e.g., 50_mm2). */
template<char... Digits>
constexpr quanta::spatial::square_millimeters operator""_mm2() {
    return detail::check_overflow<quanta::spatial::square_millimeters, Digits...>();
}

/** @brief Literal for square centimeters (e.g., 100_cm2). */
template<char... Digits>
constexpr quanta::spatial::square_centimeters operator""_cm2() {
    return detail::check_overflow<quanta::spatial::square_centimeters, Digits...>();
}

/** @brief Literal for square meters (e.g., 12_m2). */
template<char... Digits>
constexpr quanta::spatial::square_meters operator""_m2() {
    return detail::check_overflow<quanta::spatial::square_meters, Digits...>();
}

/** @brief Literal for hectares (e.g., 3_ha). */
template<char... Digits>
constexpr quanta::spatial::hectares operator""_ha() {
    return detail::check_overflow<quanta::spatial::hectares, Digits...>();
}

/** @brief Literal for square kilometers (e.g., 5_km2). */
template<char... Digits>
constexpr quanta::spatial::square_kilometers operator""_km2() {
    return detail::check_overflow<quanta::spatial::square_kilometers, Digits...>();
}

/** @brief Literal for square inches (e.g., 144_in2). */
template<char... Digits>
constexpr quanta::spatial::square_inches operator""_in2() {
    return detail::check_overflow<quanta::spatial::square_inches, Digits...>();
}

/** @brief Literal for square feet (e.g., 120_ft2). */
template<char... Digits>
constexpr quanta::spatial::square_feet operator""_ft2() {
    return detail::check_overflow<quanta::spatial::square_feet, Digits...>();
}

/** @brief Literal for square yards (e.g., 9_yd2). */
template<char... Digits>
constexpr quanta::spatial::square_yards operator""_yd2() {
    return detail::check_overflow<quanta::spatial::square_yards, Digits...>();
}

/** @brief Literal for acres (e.g., 40_ac). */
template<char... Digits>
constexpr quanta::spatial::acres operator""_ac() {
    return detail::check_overflow<quanta::spatial::acres, Digits...>();
}

/** @brief Literal for square miles (e.g., 2_mi2). */
template<char... Digits>
constexpr quanta::spatial::square_miles operator""_mi2() {
    return detail::check_overflow<quanta::spatial::square_miles, Digits...>();
}

/** @brief Literal for microliters (e.g., 50_uL). */
template<char... Digits>
constexpr quanta::spatial::microliters operator""_uL() {
    return detail::check_overflow<quanta::spatial::microliters, Digits...>();
}

#if QUANTA_HAS_UTF8_LITERALS
/** @brief Literal for microliters (e.g., 50_µL). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS). */
template<char... Digits>
constexpr quanta::spatial::microliters operator""_µL() {
    return detail::check_overflow<quanta::spatial::microliters, Digits...>();
}
#endif

/** @brief Literal for milliliters (e.g., 250_mL). */
template<char... Digits>
constexpr quanta::spatial::milliliters operator""_mL() {
    return detail::check_overflow<quanta::spatial::milliliters, Digits...>();
}

/** @brief Literal for liters (e.g., 2_L). */
template<char... Digits>
constexpr quanta::spatial::liters operator""_L() {
    return detail::check_overflow<quanta::spatial::liters, Digits...>();
}

/** @brief Literal for cubic meters (e.g., 3_m3). */
template<char... Digits>
constexpr quanta::spatial::cubic_meters operator""_m3() {
    return detail::check_overflow<quanta::spatial::cubic_meters, Digits...>();
}

/** @brief Literal for cubic millimeters, i.e. microliters (e.g., 50_mm3). */
template<char... Digits>
constexpr quanta::spatial::cubic_millimeters operator""_mm3() {
    return detail::check_overflow<quanta::spatial::cubic_millimeters, Digits...>();
}

/** @brief Literal for cubic centimeters, i.e. milliliters (e.g., 250_cm3). */
template<char... Digits>
constexpr quanta::spatial::cubic_centimeters operator""_cm3() {
    return detail::check_overflow<quanta::spatial::cubic_centimeters, Digits...>();
}

/** @brief Literal for cubic inches (e.g., 231_in3). */
template<char... Digits>
constexpr quanta::spatial::cubic_inches operator""_in3() {
    return detail::check_overflow<quanta::spatial::cubic_inches, Digits...>();
}

/** @brief Literal for cubic feet (e.g., 10_ft3). */
template<char... Digits>
constexpr quanta::spatial::cubic_feet operator""_ft3() {
    return detail::check_overflow<quanta::spatial::cubic_feet, Digits...>();
}

/** @brief Literal for cubic yards (e.g., 3_yd3). */
template<char... Digits>
constexpr quanta::spatial::cubic_yards operator""_yd3() {
    return detail::check_overflow<quanta::spatial::cubic_yards, Digits...>();
}

/** @brief Literal for US gallons (e.g., 5_gal). */
template<char... Digits>
constexpr quanta::spatial::us_gallons operator""_gal() {
    return detail::check_overflow<quanta::spatial::us_gallons, Digits...>();
}

} // namespace spatial_literals
} // namespace literals

namespace spatial {
using namespace literals::spatial_literals;
} // namespace spatial
} // namespace quanta
