#pragma once

/**
 * @file electro.hpp
 * @brief Electrical unit types and arithmetic, modeled after std::chrono.
 */

#include <assert.h>
#include <charconv>
#include <chrono>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <quanta/core.hpp>
#include <ratio>
#include <string>
#include <system_error>
#include <type_traits>

/**
 * @brief Electrical unit types and utilities.
 *
 * This library provides type-safe handling of the principal electrical
 * quantities - voltage, current, resistance, power, charge, energy,
 * capacitance and inductance - with support for multiple precisions
 * (e.g. millivolts, microamperes, kiloohms), following the design of
 * std::chrono::duration.
 *
 * ## Basic Usage
 *
 * The library provides standard integer-based types for each quantity:
 * @code
 * using namespace quanta::electro;
 * using namespace quanta::electro_literals;
 *
 * millivolts vbat{3300};
 * milliamperes load{150};
 * kiloohms pullup{10};
 * auto usb = 5_V;
 * auto shunt = 50_mOhm;
 * @endcode
 *
 * ## Cross-Quantity Arithmetic
 *
 * Quantities of different kinds combine according to the laws of circuit
 * theory, producing results in the correct unit with the correct precision:
 *
 * @code
 * // Ohm's law: V = I x R
 * auto v = milliamperes(100) * kiloohms(2);   // 200 V
 * auto i = millivolts(5000) / ohms(100);      // 50 mA
 * auto r = volts(12) / amperes(3);            // 4 Ω
 *
 * // Power: P = V x I
 * auto p = volts(12) * amperes(2);            // 24 W
 * auto i2 = watts(60) / volts(12);            // 5 A
 *
 * // Charge and energy via std::chrono durations
 * auto q = milliamperes(100) * std::chrono::hours(2);  // 200 mAh
 * auto e = watts(100) * std::chrono::hours(5);         // 500 Wh
 * auto runtime = ampere_hours(2) / amperes(1);         // 2 h
 *
 * // RC and L/R time constants
 * auto tau = kiloohms(47) * microfarads(10);  // 470 ms
 * auto lr = millihenries(10) / ohms(2);       // 5 ms
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
 * std::format("{}", millivolts(3300));        // "3300mV"   own unit
 * std::format("{:.1f}", millivolts(3300));    // "3300.0mV" own unit
 * std::format("{:b}", millivolts(3300));      // "3.3V"     base unit
 * std::format("{:s.2f}", ohms(4700));         // "4.70kΩ"   scaled
 * to_string(milliampere_hours(200), base_unit); // "0.2Ah"
 * to_string(ohms(4700), scaled);              // "4.7kΩ"
 * @endcode
 *
 * ## Integer vs Floating-Point
 *
 * - **Integer types** (default): Exact arithmetic, support modulo operations,
 *   preferred for embedded and digital systems. Note that cross-quantity
 *   division truncates (volts(5) / ohms(3) is 1 A); use a finer precision
 *   (millivolts(5000) / ohms(3) is 1666 mA) or a floating-point
 *   representation when the quotient may be fractional.
 * - **Floating-point types**: Fractional precision, e.g.
 *   voltage<double> or current<double, std::milli>.
 *
 * Conversions between precisions are implicit when lossless and require an
 * explicit cast when lossy, exactly as with std::chrono::duration.
 */
namespace quanta::electro {

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

template<typename T>
struct _is_chrono_duration : std::false_type {};

template<typename Rep, typename Period>
struct _is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

/**
 * @brief Concept for the time operands of charge and energy arithmetic.
 *
 * Satisfied only by specializations of std::chrono::duration. Other types
 * with a chrono-like shape (count(), rep and period), such as the length
 * quantities of sibling unit libraries, are deliberately rejected so that
 * e.g. a current times a length is not mistaken for a charge.
 */
template<typename T>
concept duration_like = _is_chrono_duration<std::remove_cvref_t<T>>::value;
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
 * @defgroup ElectroUnits Electrical Units
 * @{
 *
 * Tag types identifying the kind of electrical quantity. Each carries the
 * SI unit symbol used for display.
 */

/** @brief Voltage, measured in volts. */
struct volt_unit {
    static constexpr const char* symbol = "V";
};

/** @brief Electric current, measured in amperes. */
struct ampere_unit {
    static constexpr const char* symbol = "A";
};

/** @brief Electrical resistance, measured in ohms. */
struct ohm_unit {
    static constexpr const char* symbol = "Ω";
};

/** @brief Power, measured in watts. */
struct watt_unit {
    static constexpr const char* symbol = "W";
};

/** @brief Electric charge, measured in coulombs. */
struct coulomb_unit {
    static constexpr const char* symbol = "C";
};

/** @brief Energy, measured in joules. */
struct joule_unit {
    static constexpr const char* symbol = "J";
};

/** @brief Capacitance, measured in farads. */
struct farad_unit {
    static constexpr const char* symbol = "F";
};

/** @brief Inductance, measured in henries. */
struct henry_unit {
    static constexpr const char* symbol = "H";
};

/** @} */ // end of ElectroUnits group

/**
 * @brief A quantity of an electrical unit with a representation and precision.
 *
 * Similar to std::chrono::duration, quantity represents an amount of some
 * electrical unit (volts, amperes, ohms, ...). The precision is expressed
 * as a std::ratio of one base unit.
 *
 * ## Examples
 *
 * @code
 * // Integer quantities (default)
 * millivolts vbat{3300};
 * kiloohms r1{10};
 * auto usb = 5_V;
 *
 * // Arithmetic within a unit
 * auto sum = 1000_mV + 500_mV;    // 1500 mV
 * auto scaled = 100_mA * 3;       // 300 mA
 * auto ratio = 10_kOhm / 5_kOhm;  // 2 (scalar)
 *
 * // Cross-unit arithmetic
 * auto v = 100_mA * 2_kOhm;       // 200 V
 * auto p = 12_V * 2_A;            // 24 W
 * @endcode
 *
 * @tparam Unit      The unit tag type (volt_unit, ampere_unit, ...).
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1).
 */
template<typename Unit, typename Rep, typename Precision>
class quantity {
    static_assert(!is_quantity_v<Rep>, "rep cannot be an quanta::electro::quantity");
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

    constexpr quantity<Unit, typename std::common_type<rep>::type, precision> operator+() const {
        return quantity<Unit, typename std::common_type<rep>::type, precision>(_r);
    }

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
 * arithmetic.
 *
 * @tparam ToQuantity The target quantity type (must have the same unit).
 * @tparam Unit       The unit tag type.
 * @tparam Rep        Source representation type.
 * @tparam Precision  Source precision.
 * @param q           The quantity to convert.
 *
 * @return The converted quantity.
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
 * using namespace quanta::electro;
 * millivolts mv{1500};
 * auto v = floor<volts>(mv);  // 1 V (rounded down from 1.5)
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
 * using namespace quanta::electro;
 * millivolts mv{1500};
 * auto v = ceil<volts>(mv);  // 2 V (rounded up from 1.5)
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
 * using namespace quanta::electro;
 * auto v = trunc<volts>(millivolts(-1500));  // -1 V (rounded toward zero from -1.5)
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
 * using namespace quanta::electro;
 * millivolts mv{1500};
 * auto v = round<volts>(mv);  // 2 V (rounded to even)
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

// Product of a quantity and a duration, in the given result unit.
template<typename ToUnit, typename U1, typename R1, typename P1, duration_like D>
constexpr auto _duration_product(const quantity<U1, R1, P1>& lhs, const D& rhs) {
    using cr = std::common_type_t<R1, typename D::rep>;
    using result_t = quantity<ToUnit, cr, std::ratio_multiply<P1, typename D::period>>;
    return result_t(static_cast<cr>(lhs.count()) * static_cast<cr>(rhs.count()));
}

// Quotient of a quantity and a duration, in the given result unit.
template<typename ToUnit, typename U1, typename R1, typename P1, duration_like D>
constexpr auto _duration_quotient(const quantity<U1, R1, P1>& lhs, const D& rhs) {
    using cr = std::common_type_t<R1, typename D::rep>;
    using result_t = quantity<ToUnit, cr, std::ratio_divide<P1, typename D::period>>;
    return result_t(static_cast<cr>(lhs.count()) / static_cast<cr>(rhs.count()));
}

// Quotient of two quantities yielding a std::chrono::duration.
template<typename U1, typename R1, typename P1, typename U2, typename R2, typename P2>
constexpr auto _quotient_duration(const quantity<U1, R1, P1>& lhs, const quantity<U2, R2, P2>& rhs) {
    using cr = std::common_type_t<R1, R2>;
    using result_t = std::chrono::duration<cr, std::ratio_divide<P1, P2>>;
    return result_t(static_cast<cr>(lhs.count()) / static_cast<cr>(rhs.count()));
}

// Product of two quantities yielding a std::chrono::duration.
template<typename U1, typename R1, typename P1, typename U2, typename R2, typename P2>
constexpr auto _product_duration(const quantity<U1, R1, P1>& lhs, const quantity<U2, R2, P2>& rhs) {
    using cr = std::common_type_t<R1, R2>;
    using result_t = std::chrono::duration<cr, std::ratio_multiply<P1, P2>>;
    return result_t(static_cast<cr>(lhs.count()) * static_cast<cr>(rhs.count()));
}
/** @endcond */

/**
 * @defgroup ElectroCrossUnit Electrical Cross-Unit Arithmetic
 * @{
 *
 * Operators combining quantities of different kinds according to circuit
 * theory. The result precision is derived from the operand precisions, so
 * results are exact for products (e.g. milliamperes x kiloohms yields volts)
 * while quotients truncate for integer representations (use finer precisions
 * or floating-point representations for fractional results).
 */

/** @brief Ohm's law: current x resistance = voltage. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<ampere_unit, R1, P1>& i, const quantity<ohm_unit, R2, P2>& r) {
    return _product<volt_unit>(i, r);
}

/** @brief Ohm's law: resistance x current = voltage. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<ohm_unit, R1, P1>& r, const quantity<ampere_unit, R2, P2>& i) {
    return _product<volt_unit>(r, i);
}

/** @brief Ohm's law: voltage / resistance = current. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<volt_unit, R1, P1>& v, const quantity<ohm_unit, R2, P2>& r) {
    return _quotient<ampere_unit>(v, r);
}

/** @brief Ohm's law: voltage / current = resistance. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<volt_unit, R1, P1>& v, const quantity<ampere_unit, R2, P2>& i) {
    return _quotient<ohm_unit>(v, i);
}

/** @brief Power law: voltage x current = power. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<volt_unit, R1, P1>& v, const quantity<ampere_unit, R2, P2>& i) {
    return _product<watt_unit>(v, i);
}

/** @brief Power law: current x voltage = power. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<ampere_unit, R1, P1>& i, const quantity<volt_unit, R2, P2>& v) {
    return _product<watt_unit>(i, v);
}

/** @brief Power law: power / voltage = current. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<watt_unit, R1, P1>& p, const quantity<volt_unit, R2, P2>& v) {
    return _quotient<ampere_unit>(p, v);
}

/** @brief Power law: power / current = voltage. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<watt_unit, R1, P1>& p, const quantity<ampere_unit, R2, P2>& i) {
    return _quotient<volt_unit>(p, i);
}

/**
 * @brief Charge: current x time = charge.
 *
 * @code
 * auto q = milliamperes(100) * std::chrono::hours(2);  // 200 mAh
 * @endcode
 */
template<typename R1, typename P1, duration_like D>
constexpr auto operator*(const quantity<ampere_unit, R1, P1>& i, const D& t) {
    return _duration_product<coulomb_unit>(i, t);
}

/** @brief Charge: time x current = charge. */
template<duration_like D, typename R1, typename P1>
constexpr auto operator*(const D& t, const quantity<ampere_unit, R1, P1>& i) {
    return _duration_product<coulomb_unit>(i, t);
}

/** @brief Charge: charge / time = current. */
template<typename R1, typename P1, duration_like D>
constexpr auto operator/(const quantity<coulomb_unit, R1, P1>& q, const D& t) {
    return _duration_quotient<ampere_unit>(q, t);
}

/**
 * @brief Charge: charge / current = time.
 *
 * @code
 * auto runtime = milliampere_hours(2000) / milliamperes(100);  // 20 h
 * @endcode
 */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<coulomb_unit, R1, P1>& q, const quantity<ampere_unit, R2, P2>& i) {
    return _quotient_duration(q, i);
}

/**
 * @brief Energy: power x time = energy.
 *
 * @code
 * auto e = watts(100) * std::chrono::hours(5);  // 500 Wh
 * @endcode
 */
template<typename R1, typename P1, duration_like D>
constexpr auto operator*(const quantity<watt_unit, R1, P1>& p, const D& t) {
    return _duration_product<joule_unit>(p, t);
}

/** @brief Energy: time x power = energy. */
template<duration_like D, typename R1, typename P1>
constexpr auto operator*(const D& t, const quantity<watt_unit, R1, P1>& p) {
    return _duration_product<joule_unit>(p, t);
}

/** @brief Energy: energy / time = power. */
template<typename R1, typename P1, duration_like D>
constexpr auto operator/(const quantity<joule_unit, R1, P1>& e, const D& t) {
    return _duration_quotient<watt_unit>(e, t);
}

/** @brief Energy: energy / power = time. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<joule_unit, R1, P1>& e, const quantity<watt_unit, R2, P2>& p) {
    return _quotient_duration(e, p);
}

/**
 * @brief Energy: energy / voltage = charge.
 *
 * @code
 * auto capacity = watt_hours(100) / volts(5);  // 20 Ah
 * @endcode
 */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<joule_unit, R1, P1>& e, const quantity<volt_unit, R2, P2>& v) {
    return _quotient<coulomb_unit>(e, v);
}

/** @brief Energy: energy / charge = voltage. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<joule_unit, R1, P1>& e, const quantity<coulomb_unit, R2, P2>& q) {
    return _quotient<volt_unit>(e, q);
}

/** @brief Capacitance: capacitance x voltage = charge. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<farad_unit, R1, P1>& c, const quantity<volt_unit, R2, P2>& v) {
    return _product<coulomb_unit>(c, v);
}

/** @brief Capacitance: voltage x capacitance = charge. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<volt_unit, R1, P1>& v, const quantity<farad_unit, R2, P2>& c) {
    return _product<coulomb_unit>(v, c);
}

/** @brief Capacitance: charge / voltage = capacitance. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<coulomb_unit, R1, P1>& q, const quantity<volt_unit, R2, P2>& v) {
    return _quotient<farad_unit>(q, v);
}

/** @brief Capacitance: charge / capacitance = voltage. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<coulomb_unit, R1, P1>& q, const quantity<farad_unit, R2, P2>& c) {
    return _quotient<volt_unit>(q, c);
}

/**
 * @brief RC time constant: resistance x capacitance = time.
 *
 * @code
 * auto tau = kiloohms(47) * microfarads(10);  // 470 ms
 * @endcode
 */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<ohm_unit, R1, P1>& r, const quantity<farad_unit, R2, P2>& c) {
    return _product_duration(r, c);
}

/** @brief RC time constant: capacitance x resistance = time. */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator*(const quantity<farad_unit, R1, P1>& c, const quantity<ohm_unit, R2, P2>& r) {
    return _product_duration(c, r);
}

/**
 * @brief L/R time constant: inductance / resistance = time.
 *
 * @code
 * auto tau = millihenries(10) / ohms(2);  // 5 ms
 * @endcode
 */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<henry_unit, R1, P1>& l, const quantity<ohm_unit, R2, P2>& r) {
    return _quotient_duration(l, r);
}

/** @} */ // end of ElectroCrossUnit group

// ============================================================================
// Type aliases
// ============================================================================

/**
 * @defgroup ElectroQuantityTypes Standard Electrical Quantity Type Aliases
 * @{
 *
 * Standard integer-based types using int64_t representation. For
 * floating-point representations, use the alias templates, e.g.
 * voltage<double> or current<double, std::milli>.
 */

/** @brief Voltage quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using voltage = quantity<volt_unit, Rep, Precision>;

/** @brief Current quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using current = quantity<ampere_unit, Rep, Precision>;

/** @brief Resistance quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using resistance = quantity<ohm_unit, Rep, Precision>;

/** @brief Power quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using power = quantity<watt_unit, Rep, Precision>;

/** @brief Charge quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using charge = quantity<coulomb_unit, Rep, Precision>;

/** @brief Energy quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using energy = quantity<joule_unit, Rep, Precision>;

/** @brief Capacitance quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using capacitance = quantity<farad_unit, Rep, Precision>;

/** @brief Inductance quantity with configurable representation and precision. */
template<typename Rep, typename Precision = std::ratio<1>>
using inductance = quantity<henry_unit, Rep, Precision>;

/** @brief Voltage with 1 µV precision. */
using microvolts = voltage<int64_t, std::micro>;
/** @brief Voltage with 1 mV precision. */
using millivolts = voltage<int64_t, std::milli>;
/** @brief Voltage with 1 V precision. */
using volts = voltage<int64_t>;
/** @brief Voltage with 1 kV precision. */
using kilovolts = voltage<int64_t, std::kilo>;

/** @brief Current with 1 µA precision. */
using microamperes = current<int64_t, std::micro>;
/** @brief Current with 1 mA precision. */
using milliamperes = current<int64_t, std::milli>;
/** @brief Current with 1 A precision. */
using amperes = current<int64_t>;
/** @brief Shorthand for microamperes. */
using microamps = microamperes;
/** @brief Shorthand for milliamperes. */
using milliamps = milliamperes;
/** @brief Shorthand for amperes. */
using amps = amperes;

/** @brief Resistance with 1 mΩ precision. */
using milliohms = resistance<int64_t, std::milli>;
/** @brief Resistance with 1 Ω precision. */
using ohms = resistance<int64_t>;
/** @brief Resistance with 1 kΩ precision. */
using kiloohms = resistance<int64_t, std::kilo>;
/** @brief Resistance with 1 MΩ precision. */
using megaohms = resistance<int64_t, std::mega>;
/** @brief Resistance with 1 GΩ precision. */
using gigaohms = resistance<int64_t, std::giga>;

/** @brief Power with 1 µW precision. */
using microwatts = power<int64_t, std::micro>;
/** @brief Power with 1 mW precision. */
using milliwatts = power<int64_t, std::milli>;
/** @brief Power with 1 W precision. */
using watts = power<int64_t>;
/** @brief Power with 1 kW precision. */
using kilowatts = power<int64_t, std::kilo>;
/** @brief Power with 1 MW precision. */
using megawatts = power<int64_t, std::mega>;
/** @brief Power with 1 GW precision. */
using gigawatts = power<int64_t, std::giga>;

/** @brief Charge with 1 µC precision. */
using microcoulombs = charge<int64_t, std::micro>;
/** @brief Charge with 1 mC precision. */
using millicoulombs = charge<int64_t, std::milli>;
/** @brief Charge with 1 C precision. */
using coulombs = charge<int64_t>;
/** @brief Charge with 1 mAh (3.6 C) precision. */
using milliampere_hours = charge<int64_t, std::ratio<18, 5>>;
/** @brief Charge with 1 Ah (3600 C) precision. */
using ampere_hours = charge<int64_t, std::ratio<3600>>;

/** @brief Energy with 1 mJ precision. */
using millijoules = energy<int64_t, std::milli>;
/** @brief Energy with 1 J precision. */
using joules = energy<int64_t>;
/** @brief Energy with 1 kJ precision. */
using kilojoules = energy<int64_t, std::kilo>;
/** @brief Energy with 1 MJ precision. */
using megajoules = energy<int64_t, std::mega>;
/** @brief Energy with 1 Wh (3600 J) precision. */
using watt_hours = energy<int64_t, std::ratio<3600>>;
/** @brief Energy with 1 kWh (3,600,000 J) precision. */
using kilowatt_hours = energy<int64_t, std::ratio<3600000>>;

/** @brief Capacitance with 1 pF precision. */
using picofarads = capacitance<int64_t, std::pico>;
/** @brief Capacitance with 1 nF precision. */
using nanofarads = capacitance<int64_t, std::nano>;
/** @brief Capacitance with 1 µF precision. */
using microfarads = capacitance<int64_t, std::micro>;
/** @brief Capacitance with 1 mF precision. */
using millifarads = capacitance<int64_t, std::milli>;
/** @brief Capacitance with 1 F precision. */
using farads = capacitance<int64_t>;

/** @brief Inductance with 1 nH precision. */
using nanohenries = inductance<int64_t, std::nano>;
/** @brief Inductance with 1 µH precision. */
using microhenries = inductance<int64_t, std::micro>;
/** @brief Inductance with 1 mH precision. */
using millihenries = inductance<int64_t, std::milli>;
/** @brief Inductance with 1 H precision. */
using henries = inductance<int64_t>;

/** @} */ // end of ElectroQuantityTypes group

// ============================================================================
// String conversion and formatting
// ============================================================================

/**
 * @brief Trait giving the display symbol for an SI precision prefix.
 *
 * Specializations exist for the standard decade prefixes from femto to tera.
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
 * The primary template combines an SI prefix with the unit symbol (e.g.
 * "mV", "kΩ") and is only usable when the precision is a standard SI
 * prefix. Full specializations provide conventional suffixes for
 * non-decade precisions such as "mAh", "Ah", "Wh" and "kWh".
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
template<>
struct quantity_suffix<coulomb_unit, std::ratio<18, 5>> {
    static std::string value() { return "mAh"; }
};

template<>
struct quantity_suffix<coulomb_unit, std::ratio<3600>> {
    static std::string value() { return "Ah"; }
};

template<>
struct quantity_suffix<joule_unit, std::ratio<3600>> {
    static std::string value() { return "Wh"; }
};

template<>
struct quantity_suffix<joule_unit, std::ratio<3600000>> {
    static std::string value() { return "kWh"; }
};

template<typename Unit, typename Precision>
concept _has_suffix = requires { quantity_suffix<Unit, Precision>::value(); };

// The unit the base-unit form (the `b` format marker, to_string(q,
// base_unit)) renders in, as `scale`-sized units labeled `suffix`. SI-prefixed
// precisions render in the unprefixed unit ("3.3V"); the hour-based charge and
// energy precisions render in Ah and Wh ("0.2Ah", never "720C").
template<typename Unit, typename Precision>
struct _base_display_unit {
    using scale = std::ratio<1>;
    static constexpr const char* suffix = Unit::symbol;
};

struct _ampere_hour_display_unit {
    using scale = std::ratio<3600>;
    static constexpr const char* suffix = "Ah";
};

struct _watt_hour_display_unit {
    using scale = std::ratio<3600>;
    static constexpr const char* suffix = "Wh";
};

template<>
struct _base_display_unit<coulomb_unit, std::ratio<18, 5>> : _ampere_hour_display_unit {};

template<>
struct _base_display_unit<coulomb_unit, std::ratio<3600>> : _ampere_hour_display_unit {};

template<>
struct _base_display_unit<joule_unit, std::ratio<3600>> : _watt_hour_display_unit {};

template<>
struct _base_display_unit<joule_unit, std::ratio<3600000>> : _watt_hour_display_unit {};

// A value and suffix to display, for the base-unit and scaled forms.
struct _display {
    double value;
    std::string suffix;
};

// The quantity in its base unit (see _base_display_unit), as a double.
template<typename Unit, typename Rep, typename Precision>
_display _base_unit_display(const quantity<Unit, Rep, Precision>& q) {
    using display = _base_display_unit<Unit, typename Precision::type>;
    // display units per count
    using upc = typename std::ratio_divide<typename Precision::type, typename display::scale>::type;
    return {detail::_ratio_value<upc>(q.count()), display::suffix};
}

// Precisions that are SI prefixes (see si_prefix) of the unit.
template<typename Precision>
concept _si_precision = requires { si_prefix<Precision>::symbol; } && detail::_power_of_ten_precision<Precision>;

// The quantity with the largest SI prefix for which its magnitude is at least
// 1, else the smallest; zero (keeping its sign) and non-finite values are
// shown in the unprefixed unit. Each candidate's value is computed from the
// count with a single scaling by a power of ten, so values that land exactly
// on a prefix (1000 mV) are exactly 1 in it. Precisions that are not SI
// prefixes (mAh, Ah, Wh, kWh) are shown in their own unit.
template<typename Unit, typename Rep, typename Precision>
_display _scaled_display(const quantity<Unit, Rep, Precision>& q) {
    using p = typename Precision::type;
    const double count = static_cast<double>(q.count());
    if constexpr (_si_precision<p>) {
        if (count == 0 || !std::isfinite(count)) {
            return _base_unit_display(q);
        }
        // The si_prefix set, femto to tera
        const detail::_scaled s = detail::_pick_scale(count, _precision_exponent<p>, detail::_si_scale);
        return {s.value, std::string(s.label) + Unit::symbol};
    } else {
        return {count, quantity_suffix<Unit, p>::value()};
    }
}

#if CONFIG_QUANTA_STD_FORMAT
// How std::format renders a quantity of the given unit and (reduced)
// precision; see detail::_unit_formatter.
template<typename Unit, typename Precision>
struct _format_traits {
    static constexpr const char* markers = "bs";
    static constexpr bool has_own_unit = true;

    static std::string own_unit(bool) {
        const std::string suffix = quantity_suffix<Unit, Precision>::value();
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
 * @brief Selects the base-unit form in to_string().
 *
 * The quantity is shown in its unprefixed unit (V, A, Ω, W, C, J, F or H),
 * except that the hour-based charge and energy precisions are shown in Ah and
 * Wh. The value (count x precision) is rendered in the shortest fixed
 * notation that round-trips (never scientific), exactly as by the `b` format
 * marker.
 *
 * @code
 * to_string(millivolts(3300), base_unit);       // "3.3V"
 * to_string(kiloohms(47), base_unit);           // "47000Ω"
 * to_string(milliampere_hours(200), base_unit); // "0.2Ah"
 * to_string(kilowatt_hours(2), base_unit);      // "2000Wh"
 * @endcode
 */
using quanta::base_unit;
using quanta::base_unit_t;

/**
 * @brief Selects the scaled (engineering) form in to_string().
 *
 * Quantities whose precision is an SI prefix are shown with the largest
 * prefix from femto to tera (see si_prefix) for which the magnitude is at
 * least 1, or with femto if there is none; zero is shown in the unprefixed
 * unit. Quantities at the hour-based precisions (mAh, Ah, Wh, kWh) are shown
 * in their own unit. The value is rendered in the shortest fixed notation
 * that round-trips (never scientific), exactly as by the `s` format marker.
 *
 * @code
 * to_string(millivolts(3300), scaled);        // "3.3V"
 * to_string(ohms(4700), scaled);              // "4.7kΩ"
 * to_string(microamperes(-2500), scaled);     // "-2.5mA"
 * to_string(picofarads(100000), scaled);      // "100nF"
 * to_string(milliampere_hours(2500), scaled); // "2500mAh"
 * @endcode
 */
using quanta::scaled;
using quanta::scaled_t;

/**
 * @brief Converts a quantity to a string with its unit suffix.
 *
 * Available for quantities whose precision has a conventional suffix
 * (standard SI prefixes plus mAh, Ah, Wh and kWh). Integer counts are shown
 * exactly; floating-point counts in the shortest fixed notation that
 * round-trips (never scientific).
 *
 * @code
 * to_string(millivolts(3300));       // "3300mV"
 * to_string(kiloohms(10));           // "10kΩ"
 * to_string(milliampere_hours(200)); // "200mAh"
 * to_string(voltage<double>(1e6));   // "1000000V"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q) {
    return _count_string(q.count()) + quantity_suffix<Unit, typename Precision::type>::value();
}

/**
 * @brief Converts a quantity to a string in its base unit.
 *
 * Available for the same quantities as to_string(q); see base_unit.
 *
 * @code
 * to_string(millivolts(3300), base_unit);       // "3.3V"
 * to_string(milliampere_hours(200), base_unit); // "0.2Ah"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, base_unit_t) {
    const _display d = _base_unit_display(q);
    return _fixed_string(d.value) + d.suffix;
}

/**
 * @brief Converts a quantity to a string with an SI prefix suited to its magnitude.
 *
 * Available for the same quantities as to_string(q); see scaled.
 *
 * @code
 * to_string(millivolts(3300), scaled);  // "3.3V"
 * to_string(ohms(4700), scaled);        // "4.7kΩ"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, scaled_t) {
    const _display d = _scaled_display(q);
    return _fixed_string(d.value) + d.suffix;
}

} // namespace quanta::electro

namespace std {

template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<
    quanta::electro::quantity<Unit, Rep1, Precision1>,
    quanta::electro::quantity<Unit, Rep2, Precision2>> {
private:
    using common_precision = quanta::detail::_common_precision<Precision1, Precision2>;

public:
    using type = quanta::electro::quantity<Unit, std::common_type_t<Rep1, Rep2>, common_precision>;
};

#if CONFIG_QUANTA_STD_FORMAT
// "{}" prints the exact stored count with a precision-qualified unit
// (millivolts(3300) -> "3300mV"; floating-point counts in the shortest fixed
// notation that round-trips, as to_string does: voltage<double>(1e6) ->
// "1000000V", never "1e+06V"). A non-empty spec controls only how the number
// is rendered (as double), never the unit, so both forms use the same suffix:
// std::format("{:.1f}", millivolts(3300)) == "3300.0mV" and
// std::format("{:.1f}", milliampere_hours(200)) == "200.0mAh".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then handled as above:
// - 'b' (base unit, see quanta::base_unit) shows the value in the unprefixed
//   unit, or in Ah/Wh for the hour-based precisions:
//   std::format("{:b.1f}", millivolts(3300)) == "3.3V".
// - 's' (scaled, see quanta::scaled) shows the value with an SI prefix
//   suited to its magnitude: std::format("{:s}", ohms(4700)) == "4.7kΩ".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(q, quanta::base_unit) or to_string(q, quanta::scaled):
// std::format("{:b}", millivolts(24)) == "0.024V".
//
// A spec starting with 'b' or 's' is always a marker, so these letters cannot
// be the fill character of a spec that starts with a fill ("{:b>8.1f}" is the
// marker followed by ">8.1f").
template<typename Unit, typename Rep, typename Precision>
    requires quanta::electro::_has_suffix<Unit, typename Precision::type>
struct formatter<quanta::electro::quantity<Unit, Rep, Precision>, char>
    : private quanta::detail::_unit_formatter<quanta::electro::_format_traits<Unit, typename Precision::type>> {
    using quanta::detail::_unit_formatter<quanta::electro::_format_traits<Unit, typename Precision::type>>::parse;

    template<typename FormatContext>
    auto format(const quanta::electro::quantity<Unit, Rep, Precision>& q, FormatContext& ctx) const {
        return this->format_quantity(q, ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for electrical quantity types.
 */
namespace quanta {
inline namespace literals {
inline namespace electro_literals {
/** @brief Literal for microvolts (e.g., 500_uV). */
template<char... Digits>
constexpr quanta::electro::microvolts operator""_uV() {
    return detail::check_overflow<quanta::electro::microvolts, Digits...>();
}

/** @brief Literal for millivolts (e.g., 3300_mV). */
template<char... Digits>
constexpr quanta::electro::millivolts operator""_mV() {
    return detail::check_overflow<quanta::electro::millivolts, Digits...>();
}

/** @brief Literal for volts (e.g., 5_V). */
template<char... Digits>
constexpr quanta::electro::volts operator""_V() {
    return detail::check_overflow<quanta::electro::volts, Digits...>();
}

/** @brief Literal for kilovolts (e.g., 11_kV). */
template<char... Digits>
constexpr quanta::electro::kilovolts operator""_kV() {
    return detail::check_overflow<quanta::electro::kilovolts, Digits...>();
}

/** @brief Literal for microamperes (e.g., 250_uA). */
template<char... Digits>
constexpr quanta::electro::microamperes operator""_uA() {
    return detail::check_overflow<quanta::electro::microamperes, Digits...>();
}

/** @brief Literal for milliamperes (e.g., 150_mA). */
template<char... Digits>
constexpr quanta::electro::milliamperes operator""_mA() {
    return detail::check_overflow<quanta::electro::milliamperes, Digits...>();
}

/** @brief Literal for amperes (e.g., 2_A). */
template<char... Digits>
constexpr quanta::electro::amperes operator""_A() {
    return detail::check_overflow<quanta::electro::amperes, Digits...>();
}

/** @brief Literal for milliohms (e.g., 50_mOhm). */
template<char... Digits>
constexpr quanta::electro::milliohms operator""_mOhm() {
    return detail::check_overflow<quanta::electro::milliohms, Digits...>();
}

/** @brief Literal for ohms (e.g., 220_Ohm). */
template<char... Digits>
constexpr quanta::electro::ohms operator""_Ohm() {
    return detail::check_overflow<quanta::electro::ohms, Digits...>();
}

/** @brief Literal for kiloohms (e.g., 10_kOhm). */
template<char... Digits>
constexpr quanta::electro::kiloohms operator""_kOhm() {
    return detail::check_overflow<quanta::electro::kiloohms, Digits...>();
}

/** @brief Literal for megaohms (e.g., 1_MOhm). */
template<char... Digits>
constexpr quanta::electro::megaohms operator""_MOhm() {
    return detail::check_overflow<quanta::electro::megaohms, Digits...>();
}

#if QUANTA_HAS_UTF8_LITERALS
/** @brief Literal for milliohms (e.g., 50_mΩ). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS). */
template<char... Digits>
constexpr quanta::electro::milliohms operator""_mΩ() {
    return detail::check_overflow<quanta::electro::milliohms, Digits...>();
}

/** @brief Literal for ohms (e.g., 220_Ω). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS). */
template<char... Digits>
constexpr quanta::electro::ohms operator""_Ω() {
    return detail::check_overflow<quanta::electro::ohms, Digits...>();
}

/** @brief Literal for kiloohms (e.g., 10_kΩ). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS). */
template<char... Digits>
constexpr quanta::electro::kiloohms operator""_kΩ() {
    return detail::check_overflow<quanta::electro::kiloohms, Digits...>();
}

/** @brief Literal for megaohms (e.g., 1_MΩ). Unavailable on GCC before 14 (see QUANTA_HAS_UTF8_LITERALS). */
template<char... Digits>
constexpr quanta::electro::megaohms operator""_MΩ() {
    return detail::check_overflow<quanta::electro::megaohms, Digits...>();
}
#endif

/** @brief Literal for microwatts (e.g., 500_uW). */
template<char... Digits>
constexpr quanta::electro::microwatts operator""_uW() {
    return detail::check_overflow<quanta::electro::microwatts, Digits...>();
}

/** @brief Literal for milliwatts (e.g., 250_mW). */
template<char... Digits>
constexpr quanta::electro::milliwatts operator""_mW() {
    return detail::check_overflow<quanta::electro::milliwatts, Digits...>();
}

/** @brief Literal for watts (e.g., 60_W). */
template<char... Digits>
constexpr quanta::electro::watts operator""_W() {
    return detail::check_overflow<quanta::electro::watts, Digits...>();
}

/** @brief Literal for kilowatts (e.g., 2_kW). */
template<char... Digits>
constexpr quanta::electro::kilowatts operator""_kW() {
    return detail::check_overflow<quanta::electro::kilowatts, Digits...>();
}

/** @brief Literal for megawatts (e.g., 5_MW). */
template<char... Digits>
constexpr quanta::electro::megawatts operator""_MW() {
    return detail::check_overflow<quanta::electro::megawatts, Digits...>();
}

/** @brief Literal for coulombs (e.g., 10_C). */
template<char... Digits>
constexpr quanta::electro::coulombs operator""_C() {
    return detail::check_overflow<quanta::electro::coulombs, Digits...>();
}

/** @brief Literal for milliampere-hours (e.g., 2000_mAh). */
template<char... Digits>
constexpr quanta::electro::milliampere_hours operator""_mAh() {
    return detail::check_overflow<quanta::electro::milliampere_hours, Digits...>();
}

/** @brief Literal for ampere-hours (e.g., 2_Ah). */
template<char... Digits>
constexpr quanta::electro::ampere_hours operator""_Ah() {
    return detail::check_overflow<quanta::electro::ampere_hours, Digits...>();
}

/** @brief Literal for millijoules (e.g., 500_mJ). */
template<char... Digits>
constexpr quanta::electro::millijoules operator""_mJ() {
    return detail::check_overflow<quanta::electro::millijoules, Digits...>();
}

/** @brief Literal for joules (e.g., 100_J). */
template<char... Digits>
constexpr quanta::electro::joules operator""_J() {
    return detail::check_overflow<quanta::electro::joules, Digits...>();
}

/** @brief Literal for kilojoules (e.g., 4_kJ). */
template<char... Digits>
constexpr quanta::electro::kilojoules operator""_kJ() {
    return detail::check_overflow<quanta::electro::kilojoules, Digits...>();
}

/** @brief Literal for watt-hours (e.g., 100_Wh). */
template<char... Digits>
constexpr quanta::electro::watt_hours operator""_Wh() {
    return detail::check_overflow<quanta::electro::watt_hours, Digits...>();
}

/** @brief Literal for kilowatt-hours (e.g., 10_kWh). */
template<char... Digits>
constexpr quanta::electro::kilowatt_hours operator""_kWh() {
    return detail::check_overflow<quanta::electro::kilowatt_hours, Digits...>();
}

/** @brief Literal for picofarads (e.g., 22_pF). */
template<char... Digits>
constexpr quanta::electro::picofarads operator""_pF() {
    return detail::check_overflow<quanta::electro::picofarads, Digits...>();
}

/** @brief Literal for nanofarads (e.g., 100_nF). */
template<char... Digits>
constexpr quanta::electro::nanofarads operator""_nF() {
    return detail::check_overflow<quanta::electro::nanofarads, Digits...>();
}

/** @brief Literal for microfarads (e.g., 10_uF). */
template<char... Digits>
constexpr quanta::electro::microfarads operator""_uF() {
    return detail::check_overflow<quanta::electro::microfarads, Digits...>();
}

/** @brief Literal for farads (e.g., 1_F). */
template<char... Digits>
constexpr quanta::electro::farads operator""_F() {
    return detail::check_overflow<quanta::electro::farads, Digits...>();
}

/** @brief Literal for nanohenries (e.g., 100_nH). */
template<char... Digits>
constexpr quanta::electro::nanohenries operator""_nH() {
    return detail::check_overflow<quanta::electro::nanohenries, Digits...>();
}

/** @brief Literal for microhenries (e.g., 47_uH). */
template<char... Digits>
constexpr quanta::electro::microhenries operator""_uH() {
    return detail::check_overflow<quanta::electro::microhenries, Digits...>();
}

/** @brief Literal for millihenries (e.g., 10_mH). */
template<char... Digits>
constexpr quanta::electro::millihenries operator""_mH() {
    return detail::check_overflow<quanta::electro::millihenries, Digits...>();
}

/** @brief Literal for henries (e.g., 1_H). */
template<char... Digits>
constexpr quanta::electro::henries operator""_H() {
    return detail::check_overflow<quanta::electro::henries, Digits...>();
}

} // namespace electro_literals
} // namespace literals

namespace electro {
using namespace literals::electro_literals;
} // namespace electro
} // namespace quanta
