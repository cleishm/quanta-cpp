#pragma once

/**
 * @file data.hpp
 * @brief Data size and data rate types and arithmetic, modeled after std::chrono.
 */

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
#include <type_traits>

/**
 * @brief Data size and data rate types and utilities.
 *
 * This library provides type-safe handling of data quantities - sizes and
 * rates - with support for multiple precisions, in bits (bit through Tbit),
 * SI-prefixed bytes (B through TB) and IEC-prefixed bytes (KiB through TiB),
 * following the design of std::chrono::duration.
 *
 * ## Basic Usage
 *
 * The library provides standard integer-based types for each precision:
 * @code
 * using namespace quanta::data;
 * using namespace quanta::data_literals;
 *
 * kibibytes page{4};
 * megabytes image{12};
 * auto packet = 1500_B;
 * auto link = 100_Mbit_per_s;
 * @endcode
 *
 * ## Bits, SI and IEC Units
 *
 * Every unit is a precision of one byte: a bit is std::ratio<1, 8>, a kilobit
 * std::ratio<125>, a kilobyte std::ratio<1000> and a kibibyte
 * std::ratio<1024>. Quantities of any precision therefore mix freely, and
 * conversions are implicit exactly when they are lossless:
 *
 * @code
 * static_assert(bytes(1) == bits(8));
 * static_assert(kibibytes(1) == bytes(1024));
 * static_assert(kilobytes(1) == kilobits(8));
 * bits b = 2_B;                          // implicit: 16 bit
 * bytes n = 3_KiB;                       // implicit: 3072 B
 * auto k = kilobytes(1_KiB);             // explicit: 1 kB (truncated from 1.024)
 * auto total = 1_kB + 1_KiB;             // 253 in units of 8 B
 * @endcode
 *
 * ## Data Rates and std::chrono
 *
 * Rates are quantities of bytes per second, with the same precisions. Sizes
 * and rates combine with std::chrono durations, with the result precision
 * derived exactly from the operand precisions and the duration's period:
 *
 * @code
 * auto throughput = 1500_kB / std::chrono::milliseconds(500);  // 3 MB/s
 * auto sent = 100_kbit_per_s * std::chrono::milliseconds(250); // 25000 bit
 * auto eta = 1000_MB / 100_MB_per_s;                           // 10 s
 * @endcode
 *
 * ## Formatting
 *
 * to_string() and std::format show a quantity in its own unit by default,
 * whatever the format spec: `{}` prints the exact count and `{:.1f}` the count
 * as a double, each followed by the precision's symbol. A single leading
 * marker letter in the spec, or a tag argument to to_string(), opts into a
 * different unit:
 *
 * @code
 * std::format("{}", kilobytes(1500));     // "1500kB"   own unit
 * std::format("{:b}", kilobits(3));       // "375B"     base unit
 * std::format("{:s}", kilobytes(1500));   // "1.5MB"    scaled
 * std::format("{:s}", kibibytes(1536));   // "1.5MiB"
 * std::format("{:s}", kilobits_per_second(1500)); // "1.5Mbit/s"
 * to_string(bytes(2048), scaled);         // "2.048kB"
 * @endcode
 *
 * ## Integer vs Floating-Point
 *
 * - **Integer types** (default): Exact arithmetic, support modulo operations,
 *   preferred for embedded and digital systems. Note that dividing by a
 *   duration or a rate truncates (megabytes(1) / std::chrono::seconds(2) is
 *   0 MB/s); use a finer precision (kilobytes(1000) / std::chrono::seconds(2)
 *   is 500 kB/s) or a floating-point representation when the quotient may be
 *   fractional.
 * - **Floating-point types**: Fractional precision, e.g. size<double> or
 *   rate<double, std::mega>.
 *
 * Conversions between precisions are implicit when lossless and require an
 * explicit cast when lossy, exactly as with std::chrono::duration. For
 * example, bytes convert implicitly to bits (1 B = 8 bit) and kibibytes to
 * bytes, but bytes to kibibytes, or kibibytes to kilobytes, require a cast.
 */
namespace quanta::data {

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
 * @brief Concept for the time operands of data rate arithmetic.
 *
 * Satisfied only by specializations of std::chrono::duration. Other types
 * with a chrono-like shape (count(), rep and period), such as the length
 * quantities of sibling unit libraries, are deliberately rejected so that
 * e.g. a size divided by a length is not mistaken for a rate.
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
 * @defgroup DataUnits Data Units
 * @{
 *
 * Tag types identifying the kind of data quantity: size and rate. Each
 * carries the symbol of its base unit. The quantity template is
 * unit-generic; the tags differ only in which std::chrono operators relate
 * them and in their display suffixes.
 */

/** @brief Data size, measured in bytes. */
struct byte_unit {
    static constexpr const char* symbol = "B";
};

/** @brief Data rate, measured in bytes per second. */
struct byte_per_second_unit {
    static constexpr const char* symbol = "B/s";
};

/** @} */ // end of DataUnits group

/**
 * @brief A quantity of a data unit with a representation and precision.
 *
 * Similar to std::chrono::duration, quantity represents an amount of some
 * data unit (bytes or bytes per second). The precision is expressed as a
 * std::ratio of one base unit, so bits have precision std::ratio<1, 8>,
 * kilobytes std::kilo and kibibytes std::ratio<1024>.
 *
 * ## Examples
 *
 * @code
 * // Integer quantities (default)
 * kibibytes page{4};
 * megabits_per_second link{100};
 * auto packet = 1500_B;
 *
 * // Arithmetic within a unit
 * auto sum = 1_kB + 500_B;       // 1500 B
 * auto scaled = 4_KiB * 3;       // 12 KiB
 * auto ratio = 1_MiB / 1_KiB;    // 1024 (scalar)
 *
 * // Arithmetic with std::chrono durations
 * auto throughput = 1500_kB / std::chrono::milliseconds(500); // 3 MB/s
 * auto sent = throughput * std::chrono::seconds(2);            // 6 MB
 * @endcode
 *
 * Unlike a spatial length, a data quantity has no nested `period` alias, so
 * generic code that recognises duration-like or distance-like types by their
 * `period` member rejects it.
 *
 * @tparam Unit      The unit tag type (byte_unit or byte_per_second_unit).
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1).
 */
template<typename Unit, typename Rep, typename Precision>
class quantity {
    static_assert(!is_quantity_v<Rep>, "rep cannot be a quanta::data::quantity");
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
     * Explicit, as in std::chrono::duration. A floating-point (inexact) count
     * is accepted only when rep is inexact too, to prevent accidental
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
 * using namespace quanta::data;
 * auto kb = quantity_cast<kilobytes>(kibibytes(3));  // 3 kB (3.072 truncated)
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
 * using namespace quanta::data;
 * auto b = floor<bytes>(bits(12));          // 1 B (rounded down from 1.5)
 * auto kib = floor<kibibytes>(kilobytes(3)); // 2 KiB (rounded down from 2.93)
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
 * using namespace quanta::data;
 * auto b = ceil<bytes>(bits(12));            // 2 B (rounded up from 1.5)
 * auto kib = ceil<kibibytes>(kilobytes(3));  // 3 KiB (rounded up from 2.93)
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
 * using namespace quanta::data;
 * auto b = trunc<bytes>(bits(-12));  // -1 B (rounded toward zero from -1.5)
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
 * using namespace quanta::data;
 * auto b = round<bytes>(bits(12));            // 2 B (rounded to even)
 * auto kb = round<kilobytes>(kibibytes(3));   // 3 kB (from 3.072)
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
// Arithmetic with std::chrono durations
// ============================================================================

/** @cond INTERNAL */
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
/** @endcond */

/**
 * @defgroup DataChrono Data Arithmetic with std::chrono
 * @{
 *
 * Operators relating sizes, rates and std::chrono durations. Only these
 * combinations are defined; any other product or quotient of a size, a rate
 * and a duration (such as size x duration or rate / duration) is ill-formed,
 * as is adding or comparing a size and a rate.
 *
 * The result precision is the product (or quotient) of the operand
 * precision and the duration's period (or the other operand's precision),
 * and the representation is their common type, so products are exact for
 * integer representations: kilobits_per_second(100) *
 * std::chrono::milliseconds(250) is exactly bits(25000). Quotients divide
 * the counts, so for integer representations they truncate in the result
 * precision, which may be coarse when the operand precisions differ
 * (megabytes(1) / std::chrono::seconds(2) has MB/s precision and yields
 * 0 MB/s). Convert the operands to suitable precisions first, or use
 * floating-point representations, for fractional results.
 *
 * Only specializations of std::chrono::duration take part (see
 * duration_like).
 */

/**
 * @brief size / time = rate.
 *
 * @code
 * auto r = kilobytes(1500) / std::chrono::milliseconds(500);  // 3 MB/s
 * @endcode
 */
template<typename R1, typename P1, duration_like D>
constexpr auto operator/(const quantity<byte_unit, R1, P1>& s, const D& t) {
    return _duration_quotient<byte_per_second_unit>(s, t);
}

/**
 * @brief rate x time = size.
 *
 * @code
 * auto s = kilobits_per_second(100) * std::chrono::milliseconds(250);  // 25000 bit
 * @endcode
 */
template<typename R1, typename P1, duration_like D>
constexpr auto operator*(const quantity<byte_per_second_unit, R1, P1>& r, const D& t) {
    return _duration_product<byte_unit>(r, t);
}

/** @brief time x rate = size. */
template<duration_like D, typename R1, typename P1>
constexpr auto operator*(const D& t, const quantity<byte_per_second_unit, R1, P1>& r) {
    return _duration_product<byte_unit>(r, t);
}

/**
 * @brief size / rate = time.
 *
 * @code
 * auto eta = megabytes(1000) / megabytes_per_second(100);  // 10 s
 * @endcode
 */
template<typename R1, typename P1, typename R2, typename P2>
constexpr auto operator/(const quantity<byte_unit, R1, P1>& s, const quantity<byte_per_second_unit, R2, P2>& r) {
    return _quotient_duration(s, r);
}

/** @} */ // end of DataChrono group

// ============================================================================
// Type aliases
// ============================================================================

/**
 * @defgroup DataQuantityTypes Standard Data Quantity Type Aliases
 * @{
 *
 * Standard integer-based types using int64_t representation. For
 * floating-point representations, use the alias templates, e.g.
 * size<double>, size<double, std::mega> or rate<double, std::ratio<125>>.
 *
 * Every precision is an exact ratio of one byte: 1 bit = 1/8 B, SI prefixes
 * are powers of 1000 (1 kbit = 1000 bit = 125 B, 1 kB = 1000 B) and IEC
 * prefixes powers of 1024 (1 KiB = 1024 B). Rates have the same precisions,
 * as ratios of one byte per second.
 */

/** @brief Data size with configurable representation and precision (a ratio of one byte). */
template<typename Rep, typename Precision = std::ratio<1>>
using size = quantity<byte_unit, Rep, Precision>;

/** @brief Data size with 1 bit (1/8 B) precision. */
using bits = size<int64_t, std::ratio<1, 8>>;
/** @brief Data size with 1 kbit (1000 bit = 125 B) precision. */
using kilobits = size<int64_t, std::ratio<125>>;
/** @brief Data size with 1 Mbit (10⁶ bit = 125,000 B) precision. */
using megabits = size<int64_t, std::ratio<125'000>>;
/** @brief Data size with 1 Gbit (10⁹ bit = 125,000,000 B) precision. */
using gigabits = size<int64_t, std::ratio<125'000'000>>;
/** @brief Data size with 1 Tbit (10¹² bit = 125,000,000,000 B) precision. */
using terabits = size<int64_t, std::ratio<125'000'000'000>>;

/** @brief Data size with 1 B precision. */
using bytes = size<int64_t>;
/** @brief Data size with 1 kB (1000 B) precision. */
using kilobytes = size<int64_t, std::kilo>;
/** @brief Data size with 1 MB (10⁶ B) precision. */
using megabytes = size<int64_t, std::mega>;
/** @brief Data size with 1 GB (10⁹ B) precision. */
using gigabytes = size<int64_t, std::giga>;
/** @brief Data size with 1 TB (10¹² B) precision. */
using terabytes = size<int64_t, std::tera>;

/** @brief Data size with 1 KiB (1024 B) precision. */
using kibibytes = size<int64_t, std::ratio<1024>>;
/** @brief Data size with 1 MiB (1024² B) precision. */
using mebibytes = size<int64_t, std::ratio<1024 * 1024>>;
/** @brief Data size with 1 GiB (1024³ B) precision. */
using gibibytes = size<int64_t, std::ratio<1024 * 1024 * 1024>>;
/** @brief Data size with 1 TiB (1024⁴ B) precision. */
using tebibytes = size<int64_t, std::ratio<1024LL * 1024 * 1024 * 1024>>;

/** @brief Data rate with configurable representation and precision (a ratio of one B/s). */
template<typename Rep, typename Precision = std::ratio<1>>
using rate = quantity<byte_per_second_unit, Rep, Precision>;

/** @brief Data rate with 1 bit/s precision. */
using bits_per_second = rate<int64_t, bits::precision>;
/** @brief Data rate with 1 kbit/s precision. */
using kilobits_per_second = rate<int64_t, kilobits::precision>;
/** @brief Data rate with 1 Mbit/s precision. */
using megabits_per_second = rate<int64_t, megabits::precision>;
/** @brief Data rate with 1 Gbit/s precision. */
using gigabits_per_second = rate<int64_t, gigabits::precision>;
/** @brief Data rate with 1 Tbit/s precision. */
using terabits_per_second = rate<int64_t, terabits::precision>;

/** @brief Data rate with 1 B/s precision. */
using bytes_per_second = rate<int64_t>;
/** @brief Data rate with 1 kB/s precision. */
using kilobytes_per_second = rate<int64_t, kilobytes::precision>;
/** @brief Data rate with 1 MB/s precision. */
using megabytes_per_second = rate<int64_t, megabytes::precision>;
/** @brief Data rate with 1 GB/s precision. */
using gigabytes_per_second = rate<int64_t, gigabytes::precision>;
/** @brief Data rate with 1 TB/s precision. */
using terabytes_per_second = rate<int64_t, terabytes::precision>;

/** @brief Data rate with 1 KiB/s precision. */
using kibibytes_per_second = rate<int64_t, kibibytes::precision>;
/** @brief Data rate with 1 MiB/s precision. */
using mebibytes_per_second = rate<int64_t, mebibytes::precision>;
/** @brief Data rate with 1 GiB/s precision. */
using gibibytes_per_second = rate<int64_t, gibibytes::precision>;
/** @brief Data rate with 1 TiB/s precision. */
using tebibytes_per_second = rate<int64_t, tebibytes::precision>;

/** @} */ // end of DataQuantityTypes group

// ============================================================================
// String conversion and formatting
// ============================================================================

/**
 * @brief Trait giving the display symbol for a data precision.
 *
 * Specializations exist for the precisions of the standard aliases: "bit",
 * "kbit", "Mbit", "Gbit", "Tbit", "B", "kB", "MB", "GB", "TB", "KiB", "MiB",
 * "GiB" and "TiB".
 * @tparam Precision The (reduced) precision ratio.
 */
template<typename Precision>
struct precision_symbol;

/** @cond INTERNAL */
template<>
struct precision_symbol<bits::precision> {
    static constexpr const char* symbol = "bit";
};

template<>
struct precision_symbol<kilobits::precision> {
    static constexpr const char* symbol = "kbit";
};

template<>
struct precision_symbol<megabits::precision> {
    static constexpr const char* symbol = "Mbit";
};

template<>
struct precision_symbol<gigabits::precision> {
    static constexpr const char* symbol = "Gbit";
};

template<>
struct precision_symbol<terabits::precision> {
    static constexpr const char* symbol = "Tbit";
};

template<>
struct precision_symbol<bytes::precision> {
    static constexpr const char* symbol = "B";
};

template<>
struct precision_symbol<kilobytes::precision> {
    static constexpr const char* symbol = "kB";
};

template<>
struct precision_symbol<megabytes::precision> {
    static constexpr const char* symbol = "MB";
};

template<>
struct precision_symbol<gigabytes::precision> {
    static constexpr const char* symbol = "GB";
};

template<>
struct precision_symbol<terabytes::precision> {
    static constexpr const char* symbol = "TB";
};

template<>
struct precision_symbol<kibibytes::precision> {
    static constexpr const char* symbol = "KiB";
};

template<>
struct precision_symbol<mebibytes::precision> {
    static constexpr const char* symbol = "MiB";
};

template<>
struct precision_symbol<gibibytes::precision> {
    static constexpr const char* symbol = "GiB";
};

template<>
struct precision_symbol<tebibytes::precision> {
    static constexpr const char* symbol = "TiB";
};
/** @endcond */

/**
 * @brief Trait giving the display suffix for a (unit, precision) pair.
 *
 * A specialization provides `static std::string value()`, the suffix. A
 * (unit, precision) pair without `value()` has no suffix: quantities of it
 * have no to_string(q) or `{}` form, and are shown only in the base-unit and
 * scaled forms, or with a format spec (which renders in the base unit).
 *
 * For sizes the suffix is the precision's symbol (see precision_symbol), and
 * for rates the symbol followed by "/s" ("kbit/s", "KiB/s"), so only the
 * precisions of the standard aliases have suffixes. Specialize
 * precision_symbol to give another precision a symbol for both units, or
 * this trait to give it one for a single unit.
 *
 * The base-unit and scaled forms (see base_unit and scaled) do not use these
 * suffixes for the precisions of the standard aliases: they compute the value
 * in the base unit or in a unit chosen by magnitude. The scaled form shows
 * other precisions with value(), or, where they have none, in SI bytes.
 *
 * @tparam Unit      The unit tag type.
 * @tparam Precision The (reduced) precision ratio.
 */
template<typename Unit, typename Precision>
struct quantity_suffix {};

/** @cond INTERNAL */
template<typename Precision>
struct quantity_suffix<byte_unit, Precision> {
    static std::string value()
        requires requires { precision_symbol<Precision>::symbol; }
    {
        return precision_symbol<Precision>::symbol;
    }
};

template<typename Precision>
struct quantity_suffix<byte_per_second_unit, Precision> {
    static std::string value()
        requires requires { precision_symbol<Precision>::symbol; }
    {
        return std::string(precision_symbol<Precision>::symbol) + "/s";
    }
};

template<typename Unit, typename Precision>
concept _has_suffix = requires { quantity_suffix<Unit, Precision>::value(); };

// A value and suffix to display, for the base-unit and scaled forms.
struct _display {
    double value;
    std::string suffix;
};

// The units the scaled form chooses between for a precision: the bit units
// (bit, kbit, Mbit, Gbit, Tbit), the SI byte units (B, kB, MB, GB, TB) or the
// IEC byte units (B, KiB, MiB, GiB, TiB). Precisions outside these families
// render in their own unit, or, where they have none, in SI bytes.
enum class _scale_family { own, bit, si, iec };

template<typename Precision>
inline constexpr _scale_family _family_of = _scale_family::own;

template<>
inline constexpr _scale_family _family_of<bits::precision> = _scale_family::bit;
template<>
inline constexpr _scale_family _family_of<kilobits::precision> = _scale_family::bit;
template<>
inline constexpr _scale_family _family_of<megabits::precision> = _scale_family::bit;
template<>
inline constexpr _scale_family _family_of<gigabits::precision> = _scale_family::bit;
template<>
inline constexpr _scale_family _family_of<terabits::precision> = _scale_family::bit;
template<>
inline constexpr _scale_family _family_of<bytes::precision> = _scale_family::si;
template<>
inline constexpr _scale_family _family_of<kilobytes::precision> = _scale_family::si;
template<>
inline constexpr _scale_family _family_of<megabytes::precision> = _scale_family::si;
template<>
inline constexpr _scale_family _family_of<gigabytes::precision> = _scale_family::si;
template<>
inline constexpr _scale_family _family_of<terabytes::precision> = _scale_family::si;
template<>
inline constexpr _scale_family _family_of<kibibytes::precision> = _scale_family::iec;
template<>
inline constexpr _scale_family _family_of<mebibytes::precision> = _scale_family::iec;
template<>
inline constexpr _scale_family _family_of<gibibytes::precision> = _scale_family::iec;
template<>
inline constexpr _scale_family _family_of<tebibytes::precision> = _scale_family::iec;

// The units of each family, in ascending order: for the bit and SI families
// by power-of-ten exponent of one bit and one byte respectively, for the IEC
// family by power-of-1024 exponent of one byte.
template<typename Unit>
struct _scale_units {};

template<>
struct _scale_units<byte_unit> {
    static constexpr detail::_scale_entry bit[] = {{0, "bit"}, {3, "kbit"}, {6, "Mbit"}, {9, "Gbit"}, {12, "Tbit"}};
    static constexpr detail::_scale_entry si[] = {{0, "B"}, {3, "kB"}, {6, "MB"}, {9, "GB"}, {12, "TB"}};
    static constexpr detail::_scale_entry iec[] = {{0, "B"}, {1, "KiB"}, {2, "MiB"}, {3, "GiB"}, {4, "TiB"}};
};

template<>
struct _scale_units<byte_per_second_unit> {
    static constexpr detail::_scale_entry bit[] =
        {{0, "bit/s"}, {3, "kbit/s"}, {6, "Mbit/s"}, {9, "Gbit/s"}, {12, "Tbit/s"}};
    static constexpr detail::_scale_entry si[] = {{0, "B/s"}, {3, "kB/s"}, {6, "MB/s"}, {9, "GB/s"}, {12, "TB/s"}};
    static constexpr detail::_scale_entry iec[] = {{0, "B/s"}, {1, "KiB/s"}, {2, "MiB/s"}, {3, "GiB/s"}, {4, "TiB/s"}};
};

consteval int _log1024(intmax_t n) {
    int e = 0;
    while (n >= 1024) {
        n /= 1024;
        ++e;
    }
    return e;
}

// v x 1024^k, for detail::_pick_scale over the IEC family. Scaling by a power
// of two is exact, so values that land exactly on a unit are exactly 1 in it.
inline double _scale_pow1024(double v, int k) {
    return std::ldexp(v, 10 * k);
}

// The quantity in the unit's base unit (B or B/s), as a double.
template<typename Unit, typename Rep, typename Precision>
_display _base_unit_display(const quantity<Unit, Rep, Precision>& q) {
    using p = typename Precision::type;
    return {detail::_ratio_value<p>(q.count()), Unit::symbol};
}

// The quantity in the largest unit of its precision's family in which its
// magnitude is at least 1, else the smallest; zero (keeping its sign) and
// non-finite values are shown in the family's smallest unit (bit or bit/s for
// the bit family, else the base unit B or B/s). Precisions outside the
// families are shown in their own unit, or, where they have none, in the SI
// byte units. Each candidate's value is computed from the count with a single
// scaling by a power of ten (or of two, for the IEC family), so values that
// land exactly on a unit (1000 kB) are exactly 1 in it; a precision with no
// symbol that is not a power of ten is scaled from its value in the base unit.
template<typename Unit, typename Rep, typename Precision>
_display _scaled_display(const quantity<Unit, Rep, Precision>& q) {
    using p = typename Precision::type;
    constexpr _scale_family family =
        _family_of<p> == _scale_family::own && !_has_suffix<Unit, p> ? _scale_family::si : _family_of<p>;
    const double count = static_cast<double>(q.count());
    if constexpr (family == _scale_family::own) {
        return {count, quantity_suffix<Unit, p>::value()};
    } else if constexpr (family == _scale_family::si && !detail::_power_of_ten_precision<p>) {
        const double x = detail::_ratio_value<p>(q.count());
        if (x == 0 || !std::isfinite(x)) {
            return {x, Unit::symbol};
        }
        const detail::_scaled s = detail::_pick_scale(x, 0, _scale_units<Unit>::si);
        return {s.value, s.label};
    } else {
        if (count == 0 || !std::isfinite(count)) {
            if constexpr (family == _scale_family::bit) {
                return {count, _scale_units<Unit>::bit[0].label};
            } else {
                return {count, Unit::symbol};
            }
        }
        if constexpr (family == _scale_family::bit) {
            // A bit precision is a power of ten of one bit.
            constexpr int exponent = _precision_exponent<std::ratio_multiply<p, std::ratio<8>>>;
            const detail::_scaled s = detail::_pick_scale(count, exponent, _scale_units<Unit>::bit);
            return {s.value, s.label};
        } else if constexpr (family == _scale_family::si) {
            const detail::_scaled s = detail::_pick_scale(count, _precision_exponent<p>, _scale_units<Unit>::si);
            return {s.value, s.label};
        } else {
            const detail::_scaled s =
                detail::_pick_scale(count, _log1024(p::num), _scale_units<Unit>::iec, _scale_pow1024);
            return {s.value, s.label};
        }
    }
}

#if CONFIG_QUANTA_STD_FORMAT
// How std::format renders a quantity of the given unit and (reduced)
// precision; see detail::_unit_formatter. A precision with no symbol has no
// own unit.
template<typename Unit, typename Precision>
struct _format_traits {
    static constexpr const char* markers = "bs";
    static constexpr bool has_own_unit = _has_suffix<Unit, Precision>;

    // The format error message for "{}" on a precision with no own unit; see
    // detail::_no_own_unit_error.
    static constexpr const char* no_own_unit_message =
        "data: precision has no unit symbol; use an explicit format spec";

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
 * The quantity is shown in its unit's base unit (B or B/s), with the value
 * (count x precision) rendered in the shortest fixed notation that
 * round-trips (never scientific), exactly as by the `b` format marker.
 *
 * @code
 * to_string(kilobytes(1500), base_unit);          // "1500000B"
 * to_string(bits(12), base_unit);                 // "1.5B"
 * to_string(kibibytes(2), base_unit);             // "2048B"
 * to_string(kilobits_per_second(8), base_unit);   // "1000B/s"
 * @endcode
 */
using quanta::base_unit;
using quanta::base_unit_t;

/**
 * @brief Selects the scaled form in to_string().
 *
 * The quantity is shown in the largest unit of its precision's family in
 * which the magnitude is at least 1, or the smallest of the family if there
 * is none; zero and non-finite values are shown in the family's smallest unit
 * (bit or bit/s for the bit family, otherwise B or B/s). The family is that
 * of the precision: bit, kbit, Mbit, Gbit and Tbit for the bit precisions; B,
 * kB, MB, GB and TB for bytes and the SI-prefixed precisions; and B, KiB, MiB,
 * GiB and TiB for the IEC-prefixed precisions. Rates use the same units per
 * second. Other precisions are shown in their own unit, or, where they have no
 * symbol (such as the 8 B common type of kilobytes and kibibytes), in B, kB,
 * MB, GB and TB. The value is rendered in the shortest fixed notation that
 * round-trips (never scientific), exactly as by the `s` format marker.
 *
 * @code
 * to_string(kilobytes(1500), scaled);            // "1.5MB"
 * to_string(kibibytes(1536), scaled);            // "1.5MiB"
 * to_string(bytes(2048), scaled);                // "2.048kB"
 * to_string(kilobits_per_second(1500), scaled);  // "1.5Mbit/s"
 * @endcode
 */
using quanta::scaled;
using quanta::scaled_t;

/**
 * @brief Converts a quantity to a string with its unit suffix.
 *
 * Available for quantities whose precision has a suffix (see
 * quantity_suffix): those of the standard aliases. Integer counts are shown
 * exactly; floating-point counts in the shortest fixed notation that
 * round-trips (never scientific), matching std::format("{}").
 *
 * @code
 * to_string(kilobytes(64));             // "64kB"
 * to_string(bits(12));                  // "12bit"
 * to_string(mebibytes(4));              // "4MiB"
 * to_string(megabits_per_second(100));  // "100Mbit/s"
 * to_string(size<double>(2.5));         // "2.5B"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires _has_suffix<Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q) {
    return _count_string(q.count()) + quantity_suffix<Unit, typename Precision::type>::value();
}

/**
 * @brief Converts a quantity to a string in its base unit (B or B/s).
 *
 * Available for the same quantities as to_string(q), and for precisions with
 * no symbol of their own; see base_unit.
 *
 * @code
 * to_string(kilobits(3), base_unit);     // "375B"
 * to_string(kibibytes(2), base_unit);    // "2048B"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires detail::_formattable<quantity_suffix, Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, base_unit_t) {
    const _display d = _base_unit_display(q);
    return _fixed_string(d.value) + d.suffix;
}

/**
 * @brief Converts a quantity to a string in a unit scaled to its magnitude.
 *
 * Available for the same quantities as to_string(q), and for precisions with
 * no symbol of their own; see scaled.
 *
 * @code
 * to_string(kilobytes(1500), scaled);   // "1.5MB"
 * to_string(kibibytes(2048), scaled);   // "2MiB"
 * @endcode
 */
template<typename Unit, typename Rep, typename Precision>
    requires detail::_formattable<quantity_suffix, Unit, typename Precision::type>
inline std::string to_string(const quantity<Unit, Rep, Precision>& q, scaled_t) {
    const _display d = _scaled_display(q);
    return _fixed_string(d.value) + d.suffix;
}

} // namespace quanta::data

namespace std {

template<typename Unit, typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<quanta::data::quantity<Unit, Rep1, Precision1>, quanta::data::quantity<Unit, Rep2, Precision2>> {
private:
    using common_precision = quanta::detail::_common_precision<Precision1, Precision2>;

public:
    using type = quanta::data::quantity<Unit, std::common_type_t<Rep1, Rep2>, common_precision>;
};

#if CONFIG_QUANTA_STD_FORMAT
// "{}" prints the exact stored count with a precision-qualified unit
// (kilobytes(64) -> "64kB"; floating-point counts in the shortest fixed
// notation that round-trips, as to_string does: size<double>(1e6) ->
// "1000000B", never "1e+06B"). A non-empty spec controls only how the number
// is rendered (as double), never the unit, so both forms use the same suffix:
// std::format("{:.1f}", kilobytes(1500)) == "1500.0kB" and
// std::format("{:.1f}", kibibytes(4)) == "4.0KiB".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then handled as above:
// - 'b' (base unit, see quanta::base_unit) shows the value in B or B/s:
//   std::format("{:b.1f}", bits(12)) == "1.5B".
// - 's' (scaled, see quanta::scaled) shows the value in a unit of its
//   precision's family suited to its magnitude:
//   std::format("{:s}", kilobytes(1500)) == "1.5MB".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(q, quanta::base_unit) or to_string(q, quanta::scaled):
// std::format("{:b}", bits(1)) == "0.125B".
//
// A spec starting with 'b' or 's' is always a marker, so these letters cannot
// be the fill character of a spec that starts with a fill ("{:s>10.1f}" is
// the marker followed by ">10.1f").
//
// A precision with no symbol (e.g. the 8 B common type of kilobytes and
// kibibytes) has no unit of its own: "{}" is a format error (a compile error
// for a constant format string), a spec without a marker renders the value in
// B or B/s, as 'b' does, and 's' scales it in SI bytes:
// std::format("{:.2f}", 1_kB + 1_KiB) == "2024.00B" and
// std::format("{:s}", 1_kB + 1_KiB) == "2.024kB".
template<typename Unit, typename Rep, typename Precision>
    requires quanta::detail::_formattable<quanta::data::quantity_suffix, Unit, typename Precision::type>
struct formatter<quanta::data::quantity<Unit, Rep, Precision>, char>
    : private quanta::detail::_unit_formatter<quanta::data::_format_traits<Unit, typename Precision::type>> {
    using quanta::detail::_unit_formatter<quanta::data::_format_traits<Unit, typename Precision::type>>::parse;

    template<typename FormatContext>
    auto format(const quanta::data::quantity<Unit, Rep, Precision>& q, FormatContext& ctx) const {
        return this->format_quantity(q, ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for data quantity types.
 */
namespace quanta {
inline namespace literals {
inline namespace data_literals {
/** @brief Literal for bits (e.g., 12_bit). */
template<char... Digits>
constexpr quanta::data::bits operator""_bit() {
    return detail::check_overflow<quanta::data::bits, Digits...>();
}

/** @brief Literal for kilobits (e.g., 64_kbit). */
template<char... Digits>
constexpr quanta::data::kilobits operator""_kbit() {
    return detail::check_overflow<quanta::data::kilobits, Digits...>();
}

/** @brief Literal for megabits (e.g., 8_Mbit). */
template<char... Digits>
constexpr quanta::data::megabits operator""_Mbit() {
    return detail::check_overflow<quanta::data::megabits, Digits...>();
}

/** @brief Literal for gigabits (e.g., 2_Gbit). */
template<char... Digits>
constexpr quanta::data::gigabits operator""_Gbit() {
    return detail::check_overflow<quanta::data::gigabits, Digits...>();
}

/** @brief Literal for terabits (e.g., 3_Tbit). */
template<char... Digits>
constexpr quanta::data::terabits operator""_Tbit() {
    return detail::check_overflow<quanta::data::terabits, Digits...>();
}

/** @brief Literal for bytes (e.g., 1500_B). */
template<char... Digits>
constexpr quanta::data::bytes operator""_B() {
    return detail::check_overflow<quanta::data::bytes, Digits...>();
}

/** @brief Literal for kilobytes (e.g., 64_kB). */
template<char... Digits>
constexpr quanta::data::kilobytes operator""_kB() {
    return detail::check_overflow<quanta::data::kilobytes, Digits...>();
}

/** @brief Literal for megabytes (e.g., 12_MB). */
template<char... Digits>
constexpr quanta::data::megabytes operator""_MB() {
    return detail::check_overflow<quanta::data::megabytes, Digits...>();
}

/** @brief Literal for gigabytes (e.g., 4_GB). */
template<char... Digits>
constexpr quanta::data::gigabytes operator""_GB() {
    return detail::check_overflow<quanta::data::gigabytes, Digits...>();
}

/** @brief Literal for terabytes (e.g., 2_TB). */
template<char... Digits>
constexpr quanta::data::terabytes operator""_TB() {
    return detail::check_overflow<quanta::data::terabytes, Digits...>();
}

/** @brief Literal for kibibytes (e.g., 4_KiB). */
template<char... Digits>
constexpr quanta::data::kibibytes operator""_KiB() {
    return detail::check_overflow<quanta::data::kibibytes, Digits...>();
}

/** @brief Literal for mebibytes (e.g., 16_MiB). */
template<char... Digits>
constexpr quanta::data::mebibytes operator""_MiB() {
    return detail::check_overflow<quanta::data::mebibytes, Digits...>();
}

/** @brief Literal for gibibytes (e.g., 8_GiB). */
template<char... Digits>
constexpr quanta::data::gibibytes operator""_GiB() {
    return detail::check_overflow<quanta::data::gibibytes, Digits...>();
}

/** @brief Literal for tebibytes (e.g., 1_TiB). */
template<char... Digits>
constexpr quanta::data::tebibytes operator""_TiB() {
    return detail::check_overflow<quanta::data::tebibytes, Digits...>();
}

/** @brief Literal for bits per second (e.g., 9600_bit_per_s). */
template<char... Digits>
constexpr quanta::data::bits_per_second operator""_bit_per_s() {
    return detail::check_overflow<quanta::data::bits_per_second, Digits...>();
}

/** @brief Literal for kilobits per second (e.g., 128_kbit_per_s). */
template<char... Digits>
constexpr quanta::data::kilobits_per_second operator""_kbit_per_s() {
    return detail::check_overflow<quanta::data::kilobits_per_second, Digits...>();
}

/** @brief Literal for megabits per second (e.g., 100_Mbit_per_s). */
template<char... Digits>
constexpr quanta::data::megabits_per_second operator""_Mbit_per_s() {
    return detail::check_overflow<quanta::data::megabits_per_second, Digits...>();
}

/** @brief Literal for gigabits per second (e.g., 10_Gbit_per_s). */
template<char... Digits>
constexpr quanta::data::gigabits_per_second operator""_Gbit_per_s() {
    return detail::check_overflow<quanta::data::gigabits_per_second, Digits...>();
}

/** @brief Literal for terabits per second (e.g., 1_Tbit_per_s). */
template<char... Digits>
constexpr quanta::data::terabits_per_second operator""_Tbit_per_s() {
    return detail::check_overflow<quanta::data::terabits_per_second, Digits...>();
}

/** @brief Literal for bytes per second (e.g., 512_B_per_s). */
template<char... Digits>
constexpr quanta::data::bytes_per_second operator""_B_per_s() {
    return detail::check_overflow<quanta::data::bytes_per_second, Digits...>();
}

/** @brief Literal for kilobytes per second (e.g., 250_kB_per_s). */
template<char... Digits>
constexpr quanta::data::kilobytes_per_second operator""_kB_per_s() {
    return detail::check_overflow<quanta::data::kilobytes_per_second, Digits...>();
}

/** @brief Literal for megabytes per second (e.g., 100_MB_per_s). */
template<char... Digits>
constexpr quanta::data::megabytes_per_second operator""_MB_per_s() {
    return detail::check_overflow<quanta::data::megabytes_per_second, Digits...>();
}

/** @brief Literal for gigabytes per second (e.g., 2_GB_per_s). */
template<char... Digits>
constexpr quanta::data::gigabytes_per_second operator""_GB_per_s() {
    return detail::check_overflow<quanta::data::gigabytes_per_second, Digits...>();
}

/** @brief Literal for terabytes per second (e.g., 1_TB_per_s). */
template<char... Digits>
constexpr quanta::data::terabytes_per_second operator""_TB_per_s() {
    return detail::check_overflow<quanta::data::terabytes_per_second, Digits...>();
}

/** @brief Literal for kibibytes per second (e.g., 64_KiB_per_s). */
template<char... Digits>
constexpr quanta::data::kibibytes_per_second operator""_KiB_per_s() {
    return detail::check_overflow<quanta::data::kibibytes_per_second, Digits...>();
}

/** @brief Literal for mebibytes per second (e.g., 20_MiB_per_s). */
template<char... Digits>
constexpr quanta::data::mebibytes_per_second operator""_MiB_per_s() {
    return detail::check_overflow<quanta::data::mebibytes_per_second, Digits...>();
}

/** @brief Literal for gibibytes per second (e.g., 1_GiB_per_s). */
template<char... Digits>
constexpr quanta::data::gibibytes_per_second operator""_GiB_per_s() {
    return detail::check_overflow<quanta::data::gibibytes_per_second, Digits...>();
}

/** @brief Literal for tebibytes per second (e.g., 1_TiB_per_s). */
template<char... Digits>
constexpr quanta::data::tebibytes_per_second operator""_TiB_per_s() {
    return detail::check_overflow<quanta::data::tebibytes_per_second, Digits...>();
}

} // namespace data_literals
} // namespace literals

namespace data {
using namespace literals::data_literals;
} // namespace data
} // namespace quanta
