#pragma once

/**
 * @file frequency.hpp
 * @brief Frequency types and arithmetic, modeled after std::chrono.
 */

#include <assert.h>
#include <charconv>
#include <chrono>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <quanta/core.hpp>
#include <ratio>
#include <string>
#include <system_error>
#include <type_traits>

/**
 * @brief Frequency types and utilities.
 *
 * This library provides type-safe frequency handling with support for
 * multiple precisions (mHz, Hz, kHz, MHz, GHz, THz), following the design
 * of std::chrono::duration.
 *
 * ## Basic Usage
 *
 * The library provides standard integer-based frequency types:
 * @code
 * using namespace quanta::frequency;
 * using namespace quanta::frequency_literals;
 *
 * hertz audio_rate{44100};
 * kilohertz cpu_clock{3200};
 * auto wifi = 2400_MHz;
 * auto optical = 193_THz;  // Near-infrared light
 * @endcode
 *
 * ## Floating-Point Frequencies
 *
 * For fractional precision (e.g., musical tuning, scientific measurements),
 * use floating-point representations:
 *
 * @code
 * // Define floating-point frequency types
 * using hertz_d = frequency<double, std::ratio<1>>;
 * using kilohertz_d = frequency<double, std::kilo>;
 * using megahertz_d = frequency<double, std::mega>;
 *
 * // Musical tuning - concert pitch
 * hertz_d concert_a{440.0};
 * hertz_d c_sharp = concert_a.semitone_shift(4);  // 554.37 Hz
 *
 * // Scientific measurements
 * megahertz_d fm_station{88.5};  // 88.5 MHz
 * kilohertz_d middle_c{261.626}; // Middle C in Hz
 *
 * // Fractional arithmetic
 * hertz_d f1{1000.5};
 * hertz_d f2{500.25};
 * hertz_d sum = f1 + f2;  // 1500.75 Hz
 *
 * // Octave operations with fractional values
 * hertz_d a4{440.0};
 * hertz_d tritone = a4.octave_shift(0.5);  // 622.25 Hz (half octave)
 * hertz_d perfect_fifth = a4 * 1.5;        // 660.0 Hz (3:2 ratio)
 * @endcode
 *
 * ## Integer vs Floating-Point
 *
 * - **Integer types** (default): Exact arithmetic, support modulo operations,
 *   preferred for digital systems and counting applications
 * - **Floating-point types**: Fractional precision, natural for calculations
 *   involving division and musical intervals, but no modulo operations
 *
 * Conversions between integer and floating-point follow the same rules as
 * precision conversions (implicit when lossless, explicit when lossy).
 *
 * ## Formatting
 *
 * to_string() and std::format show a frequency in its own unit by default,
 * whatever the format spec: `{}` prints the exact count and `{:.1f}` the count
 * as a double, each followed by the precision's SI-prefixed unit. A single
 * leading marker letter in the spec, or a tag argument to to_string(), opts
 * into a different unit:
 *
 * @code
 * std::format("{}", millihertz(1500));      // "1500mHz"   own unit
 * std::format("{:.1f}", millihertz(1500));  // "1500.0mHz" own unit
 * std::format("{:b}", millihertz(1500));    // "1.5Hz"     base unit (hertz)
 * std::format("{:s}", megahertz(2400));     // "2.4GHz"    scaled
 * std::format("{:s.2f}", hertz(1500));      // "1.50kHz"
 * to_string(megahertz(2400), scaled);       // "2.4GHz"
 * @endcode
 *
 * A precision with no SI prefix (e.g. `std::deci`) has no unit of its own:
 * `{}` and to_string(f) do not compile for it, and a format spec without a
 * marker renders the value in hertz.
 */
namespace quanta::frequency {

// The shared trait, re-exported for convenience; specialize quanta::treat_as_inexact.
using quanta::treat_as_inexact;
using quanta::treat_as_inexact_v;

template<typename Rep, typename Precision = std::ratio<1>>
class frequency;

/**
 * @brief Trait to detect frequency specializations.
 * @tparam T Type to check.
 */
template<typename T>
struct is_frequency : std::false_type {};

template<typename Rep, typename Precision>
struct is_frequency<frequency<Rep, Precision>> : std::true_type {};

template<typename T>
inline constexpr bool is_frequency_v = is_frequency<T>::value;

/** @cond INTERNAL */
template<typename Rep>
concept not_frequency = !_is_specialization_of_v<Rep, frequency>;

/**
 * @brief Concept for duration types.
 *
 * A type satisfies duration_like if it is a specialization of
 * std::chrono::duration. Other types with the same shape (a count() method
 * and nested period and rep types), such as length types, are not durations
 * and are rejected.
 */
template<typename T>
concept duration_like = _is_specialization_of_v<std::remove_cvref_t<T>, std::chrono::duration>;

/**
 * @brief Concept for distance-like types.
 *
 * A type satisfies distance_like if it has a chrono-shaped interface but is
 * not a std::chrono::duration (e.g. the lengths of
 * quanta::spatial, such as quanta::spatial::millimeters):
 * - A count() method returning the tick count
 * - A nested period type (a std::ratio representing units relative to meters)
 * - A nested rep type (the representation type)
 */
template<typename T>
concept distance_like = !duration_like<T> && requires(T t) {
    { t.count() };
    typename T::period;
    typename T::rep;
};

/**
 * @brief Provides special values for frequency representations.
 * @tparam Rep The representation type.
 */
template<typename Rep>
struct frequency_values {
    /** @brief Returns a zero frequency. */
    static constexpr Rep zero() noexcept { return Rep(0); }

    /** @brief Returns the maximum representable frequency. */
    static constexpr Rep max() noexcept { return std::numeric_limits<Rep>::max(); }

    /** @brief Returns the minimum representable frequency. */
    static constexpr Rep min() noexcept { return std::numeric_limits<Rep>::lowest(); }
};

/** @endcond */

/**
 * @brief A frequency value with a representation and precision.
 *
 * Similar to std::chrono::duration, frequency represents a quantity of
 * cycles per second. The precision is expressed as a std::ratio of one
 * Hertz.
 *
 * ## Basic Examples
 *
 * @code
 * // Integer frequencies (default)
 * hertz audio{44100};
 * kilohertz cpu{3200};
 * auto wifi = 2400_MHz;
 *
 * // Arithmetic
 * auto sum = 1000_Hz + 500_Hz;  // 1500 Hz
 * auto scaled = 100_Hz * 3;     // 300 Hz
 * auto ratio = 1000_Hz / 100_Hz; // 10 (scalar)
 * @endcode
 *
 * ## Floating-Point Examples
 *
 * @code
 * // Define floating-point types
 * using hertz_d = frequency<double>;
 * using kilohertz_d = frequency<double, std::kilo>;
 *
 * // Fractional values
 * hertz_d concert_pitch{440.0};
 * hertz_d a_flat = concert_pitch.semitone_shift(-1);  // 415.30 Hz
 *
 * // Exact ratios for just intonation
 * hertz_d fundamental{100.0};
 * hertz_d perfect_fifth = fundamental * 1.5;  // 3:2 ratio
 * hertz_d major_third = fundamental * 1.25;   // 5:4 ratio
 *
 * // Scientific measurements
 * kilohertz_d am_radio{540.5};  // 540.5 kHz
 * @endcode
 *
 * @tparam Rep       Arithmetic type for the tick count.
 * @tparam Precision A std::ratio representing the precision (default: 1 Hz).
 */
template<typename Rep, typename Precision>
class frequency {
    static_assert(!is_frequency<Rep>::value, "rep cannot be a quanta::frequency::frequency");
    static_assert(_is_ratio<Precision>::value, "precision must be a specialization of std::ratio");
    static_assert(Precision::num > 0, "precision must be positive");

public:
    /** @brief The representation type. */
    using rep = Rep;
    /** @brief The precision as a std::ratio. */
    using precision = typename Precision::type;

    /** @brief Constructs a zero frequency. */
    constexpr frequency() = default;
    frequency(const frequency&) = default;

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
    constexpr explicit frequency(const Rep2& r)
        : _r(static_cast<rep>(r)) {}

    /**
     * @brief Constructs from another frequency with different representation or precision.
     *
     * Implicit when the conversion is lossless (the target rep is
     * floating-point, or the target precision evenly divides the source
     * precision and the source rep is not floating-point) and the source rep
     * converts implicitly to the target rep. Any other conversion that the
     * cast can perform is explicit (see below).
     *
     * @tparam Rep2       Source representation type.
     * @tparam Precision2 Source precision.
     * @param f           The source frequency.
     */
    template<typename Rep2, typename Precision2>
        requires std::convertible_to<const Rep2&, rep> &&
        detail::_lossless_conversion<Rep2, Precision2, rep, precision> &&
        detail::_quantity_castable<frequency<Rep2, Precision2>, frequency>
    constexpr frequency(const frequency<Rep2, Precision2>& f)
        : _r(frequency_cast<frequency>(f).count()) {}

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
     * @param f           The source frequency.
     */
    template<typename Rep2, typename Precision2>
        requires(!detail::_lossless_conversion<Rep2, Precision2, rep, precision> ||
                 !std::convertible_to<const Rep2&, rep>) &&
        detail::_quantity_castable<frequency<Rep2, Precision2>, frequency>
    constexpr explicit frequency(const frequency<Rep2, Precision2>& f)
        : _r(frequency_cast<frequency>(f).count()) {}

    ~frequency() = default;
    frequency& operator=(const frequency&) = default;

    /** @brief Returns the tick count. */
    constexpr rep count() const { return _r; }

    constexpr frequency<typename std::common_type<rep>::type, precision> operator+() const {
        return frequency<typename std::common_type<rep>::type, precision>(_r);
    }

    constexpr frequency<typename std::common_type<rep>::type, precision> operator-() const {
        return frequency<typename std::common_type<rep>::type, precision>(-_r);
    }

    constexpr frequency& operator++() {
        ++_r;
        return *this;
    }

    constexpr frequency operator++(int) { return frequency(_r++); }

    constexpr frequency& operator--() {
        --_r;
        return *this;
    }

    constexpr frequency operator--(int) { return frequency(_r--); }

    constexpr frequency& operator+=(const frequency& f) {
        _r += f.count();
        return *this;
    }

    constexpr frequency& operator-=(const frequency& f) {
        _r -= f.count();
        return *this;
    }

    constexpr frequency& operator*=(const rep& r) {
        _r *= r;
        return *this;
    }

    constexpr frequency& operator/=(const rep& r) {
        _r /= r;
        return *this;
    }

    constexpr frequency& operator%=(const rep& r)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= r;
        return *this;
    }

    constexpr frequency& operator%=(const frequency& f)
        requires(!treat_as_inexact_v<rep>)
    {
        _r %= f.count();
        return *this;
    }

    /** @brief Returns a zero frequency. */
    static constexpr frequency zero() noexcept { return frequency(frequency_values<rep>::zero()); }

    /** @brief Returns the minimum representable frequency. */
    static constexpr frequency min() noexcept { return frequency(frequency_values<rep>::min()); }

    /** @brief Returns the maximum representable frequency. */
    static constexpr frequency max() noexcept { return frequency(frequency_values<rep>::max()); }

    /**
     * @brief Returns the period of this frequency as a duration.
     *
     * The period is the inverse of the frequency. For example, a frequency of
     * 1000 Hz has a period of 1 millisecond.
     *
     * For integer-to-integer conversions, this method uses exact integer arithmetic
     * with wider intermediate types (128-bit when available) to minimize overflow risk.
     * Common conversions like hertz{1000} to milliseconds will produce exact results.
     *
     * When either the frequency or duration uses floating-point representation,
     * the calculation falls back to floating-point arithmetic.
     *
     * @tparam Duration The target duration type, a std::chrono::duration (see duration_like).
     * @return The period as a duration.
     *
     * @note For integer duration types, be aware that the result may truncate if the
     *       exact period is not representable (e.g., gigahertz{2}.period<nanoseconds>()
     *       truncates to 0 since the true period is 0.5 ns).
     *
     * @code
     * // Integer-to-integer conversions (exact)
     * hertz{1000}.period<std::chrono::milliseconds>()   // exactly 1 ms
     * kilohertz{1}.period<std::chrono::milliseconds>()  // exactly 1 ms
     * hertz{100}.period<std::chrono::microseconds>()    // exactly 10 µs
     *
     * // Floating-point for fractional precision
     * using hertz_d = frequency<double>;
     * hertz_d{440.0}.period<std::chrono::duration<double, std::milli>>() // ~2.27 ms
     * @endcode
     */
    template<duration_like Duration>
    constexpr Duration period() const {
        // Period ratio is the inverse of frequency precision
        // For frequency<Rep, ratio<N,D>>, the period of 1 tick is ratio<D,N> seconds
        using period_ratio = std::ratio_divide<std::ratio<1>, precision>;
        using duration_period = typename Duration::period;
        using cf = std::ratio_divide<period_ratio, duration_period>;
        using duration_rep = typename Duration::rep;

        if (_r == rep(0)) {
            return Duration::max();
        }

        // Integer-only path when both types are integral
        if constexpr (std::is_integral_v<rep> && std::is_integral_v<duration_rep>) {
            using wider_t = detail::_conversion_integer<rep, duration_rep>;

            wider_t count = static_cast<wider_t>(_r);

            if constexpr (cf::den == 1 && cf::num == 1) {
                // period_ticks = 1 / freq_count (in the target duration units)
                return Duration(static_cast<duration_rep>(1 / count));
            } else if constexpr (cf::den == 1) {
                // period_ticks = num / freq_count
                // Use GCD: g = gcd(num, count), then (num/g) / (count/g)
                wider_t num = static_cast<wider_t>(cf::num);
                wider_t g = _runtime_gcd(num, count);
                wider_t reduced_num = num / g;
                wider_t reduced_count = count / g;
                return Duration(static_cast<duration_rep>(reduced_num / reduced_count));
            } else if constexpr (cf::num == 1) {
                // period_ticks = 1 / (freq_count * den)
                wider_t denom = count * static_cast<wider_t>(cf::den);
                return Duration(static_cast<duration_rep>(1 / denom));
            } else {
                // period_ticks = num / (freq_count * den)
                // Use GCD: g = gcd(num, count), then (num/g) / ((count/g) * den)
                wider_t num = static_cast<wider_t>(cf::num);
                wider_t g = _runtime_gcd(num, count);
                wider_t reduced_num = num / g;
                wider_t reduced_count = count / g;
                wider_t denom = reduced_count * static_cast<wider_t>(cf::den);
                return Duration(static_cast<duration_rep>(reduced_num / denom));
            }
        } else {
            // Floating-point path
            using cr = std::common_type_t<duration_rep, double>;

            if constexpr (cf::den == 1 && cf::num == 1) {
                return Duration(static_cast<duration_rep>(1.0 / static_cast<double>(_r)));
            } else if constexpr (cf::den == 1) {
                return Duration(static_cast<duration_rep>(static_cast<cr>(cf::num) / static_cast<double>(_r)));
            } else if constexpr (cf::num == 1) {
                return Duration(static_cast<duration_rep>(1.0 / (static_cast<double>(_r) * static_cast<cr>(cf::den))));
            } else {
                return Duration(
                    static_cast<duration_rep>(
                        static_cast<cr>(cf::num) / (static_cast<double>(_r) * static_cast<cr>(cf::den))
                    )
                );
            }
        }
    }

    /**
     * @brief Returns the nth harmonic of this frequency.
     *
     * The nth harmonic is n times the fundamental frequency.
     * For example, the 2nd harmonic of 440 Hz is 880 Hz.
     *
     * This method provides a semantically clear way to express harmonic
     * relationships, equivalent to `freq * n`.
     *
     * @param n The harmonic number (must be positive).
     * @return A frequency representing the nth harmonic.
     */
    constexpr frequency harmonic(unsigned int n) const { return *this * n; }

    /**
     * @brief Returns the nth subharmonic of this frequency.
     *
     * The nth subharmonic is the fundamental frequency divided by n.
     * For example, the 2nd subharmonic of 880 Hz is 440 Hz.
     *
     * This method provides a semantically clear way to express subharmonic
     * relationships, equivalent to `freq / n`.
     *
     * @param n The subharmonic divisor (must be positive).
     * @return A frequency representing the nth subharmonic.
     */
    constexpr frequency subharmonic(unsigned int n) const {
        assert(n > 0 && "subharmonic divisor must be positive");
        return *this / n;
    }

    /**
     * @brief Returns this frequency shifted by a number of octaves.
     *
     * Shifts the frequency up or down by the specified number of octaves.
     * Each octave represents a doubling (positive) or halving (negative) of frequency.
     * For example, shifting 440 Hz up by 1 octave yields 880 Hz.
     *
     * Fractional octaves are supported and particularly useful with floating-point types:
     * @code
     * // Integer types round to nearest
     * hertz a4{440};
     * auto tritone = a4.octave_shift(0.5);  // 622 Hz (rounded)
     *
     * // Floating-point types preserve fractional values
     * using hertz_d = frequency<double>;
     * hertz_d a4_d{440.0};
     * auto tritone_d = a4_d.octave_shift(0.5);  // 622.254 Hz (exact)
     * @endcode
     *
     * @tparam T The type of the octave value (default: double).
     * @param octaves The number of octaves to shift (positive = up, negative = down).
     * @return A frequency representing this frequency shifted by the specified octaves.
     */
    template<typename T = double>
    frequency octave_shift(T octaves) const {
        double multiplier = std::pow(2.0, static_cast<double>(octaves));
        if constexpr (std::is_integral_v<rep>) {
            return frequency(static_cast<rep>(std::round(static_cast<double>(_r) * multiplier)));
        } else {
            return frequency(static_cast<rep>(static_cast<double>(_r) * multiplier));
        }
    }

    /**
     * @brief Returns this frequency shifted by a number of semitones.
     *
     * Shifts the frequency up or down by the specified number of semitones.
     * There are 12 semitones per octave in equal temperament tuning.
     * For example, shifting 440 Hz up by 12 semitones yields 880 Hz (one octave).
     *
     * Fractional semitones enable precise microtonal adjustments:
     * @code
     * using hertz_d = frequency<double>;
     * hertz_d a4{440.0};
     *
     * // Equal temperament intervals
     * auto major_third = a4.semitone_shift(4);   // 554.37 Hz (C#)
     * auto perfect_fifth = a4.semitone_shift(7); // 659.26 Hz (E)
     *
     * // Microtonal adjustment (50 cents = 0.5 semitones)
     * auto slightly_sharp = a4.semitone_shift(0.5);  // 452.89 Hz
     * @endcode
     *
     * @tparam T The type of the semitone value (default: double).
     * @param semitones The number of semitones to shift (positive = up, negative = down).
     * @return A frequency representing this frequency shifted by the specified semitones.
     */
    template<typename T = double>
    frequency semitone_shift(T semitones) const {
        double multiplier = std::pow(2.0, static_cast<double>(semitones) / 12.0);
        if constexpr (std::is_integral_v<rep>) {
            return frequency(static_cast<rep>(std::round(static_cast<double>(_r) * multiplier)));
        } else {
            return frequency(static_cast<rep>(static_cast<double>(_r) * multiplier));
        }
    }

    /**
     * @brief Calculates the interval in octaves between this frequency and another.
     *
     * Returns the number of octaves between this frequency and the other frequency.
     * Positive values indicate this frequency is higher, negative values indicate lower.
     * For example, octaves_from(440 Hz) called on 880 Hz returns 1.0 (one octave higher).
     *
     * @tparam T The return type for the octave count (default: double).
     * @param other The reference frequency to compare against.
     * @return The number of octaves between the two frequencies.
     */
    template<typename T = double>
    T octaves_from(const frequency& other) const {
        return static_cast<T>(std::log2(static_cast<double>(_r) / static_cast<double>(other._r)));
    }

    /**
     * @brief Calculates the interval in semitones between this frequency and another.
     *
     * Returns the number of semitones between this frequency and the other frequency.
     * There are 12 semitones per octave in equal temperament tuning.
     * Positive values indicate this frequency is higher, negative values indicate lower.
     *
     * @tparam T The return type for the semitone count (default: double).
     * @param other The reference frequency to compare against.
     * @return The number of semitones between the two frequencies.
     */
    template<typename T = double>
    T semitones_from(const frequency& other) const {
        return static_cast<T>(12.0 * std::log2(static_cast<double>(_r) / static_cast<double>(other._r)));
    }

    /**
     * @brief Calculates the wavelength for this frequency.
     *
     * Returns the wavelength for this frequency given the time it takes for a wave to
     * travel one unit of distance. The relationship is: wavelength = period / time_per_unit_distance
     *
     * For electromagnetic waves in vacuum, use std::chrono::nanoseconds(3) for approximately
     * 1 nanosecond per 0.3 meters (since light travels ~0.3 meters per nanosecond).
     *
     * @tparam Distance The target distance type, e.g. quanta::spatial::millimeters. Must satisfy the
     *                  distance_like concept (so it cannot be a std::chrono::duration).
     * @tparam Duration The duration type representing time per unit distance, a std::chrono::duration.
     * @param time_per_unit_distance The time it takes to travel one unit of the target distance.
     * @return The wavelength as a distance-like value.
     *
     * @note This method uses the period() method internally and performs ratio arithmetic
     *       to convert between time and distance domains.
     */
    template<distance_like Distance, duration_like Duration>
    constexpr Distance wavelength(const Duration& time_per_unit_distance) const {
        // Get the period of this frequency
        using period_rep = double;
        using period_ratio = std::ratio_divide<std::ratio<1>, precision>;

        if (_r == rep(0)) {
            return Distance::max();
        }

        // Calculate period in our internal representation (1/frequency)
        period_rep wave_period = 1.0 / static_cast<double>(_r);

        // Convert to common time representation
        using duration_period = typename Duration::period;
        using time_cf = std::ratio_divide<period_ratio, duration_period>;
        using time_cr = std::common_type_t<period_rep, typename Duration::rep, double>;

        // Calculate period in time_per_unit_distance units
        time_cr period_in_duration_units;
        if constexpr (time_cf::den == 1 && time_cf::num == 1) {
            period_in_duration_units = static_cast<time_cr>(wave_period);
        } else if constexpr (time_cf::den == 1) {
            period_in_duration_units = static_cast<time_cr>(wave_period) * static_cast<time_cr>(time_cf::num);
        } else if constexpr (time_cf::num == 1) {
            period_in_duration_units = static_cast<time_cr>(wave_period) / static_cast<time_cr>(time_cf::den);
        } else {
            period_in_duration_units = static_cast<time_cr>(wave_period) * static_cast<time_cr>(time_cf::num) /
                static_cast<time_cr>(time_cf::den);
        }

        // Wavelength = period / time_per_unit_distance
        double wavelength_count = period_in_duration_units / static_cast<double>(time_per_unit_distance.count());

        return Distance(static_cast<typename Distance::rep>(wavelength_count));
    }

    /**
     * @brief Calculates the wavelength for this frequency given a propagation velocity.
     *
     * Returns the wavelength for this frequency given the propagation velocity.
     * The relationship is: wavelength = velocity / frequency
     *
     * @tparam Distance The target distance type, e.g. quanta::spatial::millimeters. Must satisfy the
     *                  distance_like concept (so it cannot be a std::chrono::duration).
     * @param velocity The propagation velocity in meters per second.
     *                 Defaults to the speed of light (299,792,458 m/s).
     * @return The wavelength as a distance-like value.
     *
     * @note For electromagnetic waves in vacuum, use the default speed of light.
     *       For other media or wave types, specify the appropriate propagation velocity.
     *       For sound in air at 20°C, use approximately 343 m/s.
     *
     * @code
     * // With quanta::spatial
     * auto lambda = (2400_MHz).wavelength<quanta::spatial::millimeters>();  // 124 mm
     * @endcode
     */
    template<distance_like Distance>
    constexpr Distance wavelength(double velocity = 299792458.0) const {
        if (_r == rep(0)) {
            return Distance::max();
        }

        // Convert frequency to Hz
        double freq_hz = detail::_ratio_value<precision>(_r);

        // Calculate wavelength in meters: wavelength = velocity / frequency
        double wavelength_meters = velocity / freq_hz;

        // Convert from meters to the target distance type's units
        // Distance::period represents the ratio of the distance unit to meters
        using distance_period = typename Distance::period;
        double wavelength_in_units =
            wavelength_meters * static_cast<double>(distance_period::den) / static_cast<double>(distance_period::num);

        return Distance(static_cast<typename Distance::rep>(wavelength_in_units));
    }

private:
    rep _r{};
};

/**
 * @brief Converts a frequency to a different precision or representation.
 *
 * For integer-to-integer conversions, this function uses wider intermediate types
 * (128-bit when available) to minimize overflow risk during ratio arithmetic.
 *
 * @tparam ToFreq The target frequency type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param f        The frequency to convert.
 *
 * @return The converted frequency.
 */
template<typename ToFreq, typename Rep, typename Precision>
    requires detail::_quantity_cast_requirements<frequency<Rep, Precision>, ToFreq>
constexpr ToFreq frequency_cast(const frequency<Rep, Precision>& f) {
    return detail::_quantity_cast<ToFreq>(f);
}

/**
 * @brief Converts a frequency to the target type, rounding toward negative infinity.
 *
 * This function performs a frequency conversion with floor rounding semantics.
 * When converting to a coarser precision, values are rounded down.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToFreq The target frequency type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param f        The frequency to convert.
 *
 * @return The converted frequency, rounded toward negative infinity.
 *
 * @code
 * using namespace quanta::frequency;
 * hertz h{1500};
 * auto kh = floor<kilohertz>(h);  // 1 kHz (rounded down from 1.5)
 * @endcode
 */
template<typename ToFreq, typename Rep, typename Precision>
constexpr ToFreq floor(const frequency<Rep, Precision>& f) {
    return detail::_floor_cast(frequency_cast<ToFreq>(f), f);
}

/**
 * @brief Converts a frequency to the target type, rounding toward positive infinity.
 *
 * This function performs a frequency conversion with ceiling rounding semantics.
 * When converting to a coarser precision, values are rounded up.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToFreq The target frequency type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param f        The frequency to convert.
 *
 * @return The converted frequency, rounded toward positive infinity.
 *
 * @code
 * using namespace quanta::frequency;
 * hertz h{1500};
 * auto kh = ceil<kilohertz>(h);  // 2 kHz (rounded up from 1.5)
 * @endcode
 */
template<typename ToFreq, typename Rep, typename Precision>
constexpr ToFreq ceil(const frequency<Rep, Precision>& f) {
    return detail::_ceil_cast(frequency_cast<ToFreq>(f), f);
}

/**
 * @brief Converts a frequency to the target type, rounding toward zero.
 *
 * The same as frequency_cast<ToFreq>(f).
 *
 * @tparam ToFreq The target frequency type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param f        The frequency to convert.
 *
 * @return The converted frequency, rounded toward zero.
 *
 * @code
 * using namespace quanta::frequency;
 * auto kh = trunc<kilohertz>(hertz(-1500));  // -1 kHz (rounded toward zero from -1.5)
 * @endcode
 */
template<typename ToFreq, typename Rep, typename Precision>
constexpr ToFreq trunc(const frequency<Rep, Precision>& f) {
    return frequency_cast<ToFreq>(f);
}

/**
 * @brief Converts a frequency to the target type, rounding to nearest (ties to even).
 *
 * This function performs a frequency conversion with round-to-nearest rounding semantics.
 * When converting to a coarser precision, values are rounded to the nearest representable
 * value, with ties rounded to even.
 *
 * As with the cast, the result is undefined if it does not fit the target rep.
 *
 * @tparam ToFreq The target frequency type.
 * @tparam Rep     Source representation type.
 * @tparam Precision Source precision.
 * @param f        The frequency to convert.
 *
 * @return The converted frequency, rounded to nearest.
 *
 * @code
 * using namespace quanta::frequency;
 * hertz h1{1500};
 * hertz h2{2500};
 * auto kh1 = round<kilohertz>(h1);  // 2 kHz (rounded to even)
 * auto kh2 = round<kilohertz>(h2);  // 2 kHz (rounded to even)
 * @endcode
 */
template<typename ToFreq, typename Rep, typename Precision>
constexpr ToFreq round(const frequency<Rep, Precision>& f) {
    return detail::_round_cast<ToFreq>(f, [&f] { return floor<ToFreq>(f); });
}

/**
 * @brief Calculates the beat frequency between two frequencies.
 *
 * In acoustics and signal processing, the beat frequency is the absolute
 * difference between two frequencies. When two sound waves of slightly
 * different frequencies interfere, they produce a beating pattern at this
 * frequency.
 *
 * This is a convenience function equivalent to abs(f1 - f2).
 *
 * @tparam Rep1 First frequency representation type.
 * @tparam Precision1 First frequency precision.
 * @tparam Rep2 Second frequency representation type.
 * @tparam Precision2 Second frequency precision.
 * @param f1 The first frequency.
 * @param f2 The second frequency.
 * @return The beat frequency as the absolute difference.
 *
 * @code
 * using namespace quanta::frequency;
 * using namespace quanta::frequency_literals;
 *
 * // Acoustic beats between two tuning forks
 * auto f1 = 440_Hz;  // A4 concert pitch
 * auto f2 = 442_Hz;  // Slightly sharp A4
 * auto beat = beat(f1, f2);  // 2 Hz beat
 *
 * // Mixed precisions handled automatically
 * auto carrier = 1000_Hz;
 * auto modulator = 1_kHz + 10_Hz;
 * auto beat2 = beat(carrier, modulator);  // 10 Hz
 * @endcode
 *
 * @see abs(), operator-()
 */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto beat(const frequency<Rep1, Precision1>& f1, const frequency<Rep2, Precision2>& f2)
    -> std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>> {
    using cf = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return abs(cf(f1) - cf(f2));
}

/**
 * @brief Returns the absolute value of a frequency.
 *
 * @tparam Rep       Representation type.
 * @tparam Precision Precision type.
 * @param f          The frequency.
 *
 * @return The absolute value of the frequency.
 *
 * @code
 * using namespace quanta::frequency;
 * hertz h{-440};
 * auto abs_h = abs(h);  // 440 Hz
 * @endcode
 */
template<typename Rep, typename Precision>
constexpr frequency<Rep, Precision> abs(const frequency<Rep, Precision>& f) {
    return detail::_abs(f);
}

/** @brief Returns the sum of two frequencies. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator+(const frequency<Rep1, Precision1>& lhs, const frequency<Rep2, Precision2>& rhs)
    -> std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>> {
    using cf = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return cf(cf(lhs).count() + cf(rhs).count());
}

/** @brief Returns the difference of two frequencies. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator-(const frequency<Rep1, Precision1>& lhs, const frequency<Rep2, Precision2>& rhs)
    -> std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>> {
    using cf = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return cf(cf(lhs).count() - cf(rhs).count());
}

/** @brief Multiplies a frequency by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_frequency<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const frequency<Rep1, Precision>& f, const Rep2& r)
    -> frequency<std::common_type_t<Rep1, Rep2>, Precision> {
    using cf = frequency<std::common_type_t<Rep1, Rep2>, Precision>;
    return cf(cf(f).count() * r);
}

/** @brief Multiplies a scalar by a frequency. */
template<typename Rep1, typename Rep2, typename Precision>
    requires not_frequency<Rep1> && std::convertible_to<const Rep1&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator*(const Rep1& r, const frequency<Rep2, Precision>& f)
    -> frequency<std::common_type_t<Rep1, Rep2>, Precision> {
    return f * r;
}

/** @brief Divides a frequency by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_frequency<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>>
constexpr auto operator/(const frequency<Rep1, Precision>& f, const Rep2& s)
    -> frequency<std::common_type_t<Rep1, Rep2>, Precision> {
    using cf = frequency<std::common_type_t<Rep1, Rep2>, Precision>;
    return cf(cf(f).count() / s);
}

/** @brief Divides two frequencies, returning a scalar. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr auto operator/(const frequency<Rep1, Precision1>& lhs, const frequency<Rep2, Precision2>& rhs)
    -> std::common_type_t<Rep1, Rep2> {
    using cf = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return cf(lhs).count() / cf(rhs).count();
}

/** @brief Returns the remainder of dividing a frequency by a scalar. */
template<typename Rep1, typename Precision, typename Rep2>
    requires not_frequency<Rep2> && std::convertible_to<const Rep2&, std::common_type_t<Rep1, Rep2>> &&
    (!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const frequency<Rep1, Precision>& f, const Rep2& s)
    -> frequency<std::common_type_t<Rep1, Rep2>, Precision> {
    using cf = frequency<std::common_type_t<Rep1, Rep2>, Precision>;
    return cf(cf(f).count() % s);
}

/** @brief Returns the remainder of dividing two frequencies. */
template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires(!treat_as_inexact_v<Rep1> && !treat_as_inexact_v<Rep2>)
constexpr auto operator%(const frequency<Rep1, Precision1>& lhs, const frequency<Rep2, Precision2>& rhs)
    -> std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>> {
    using cf = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return cf(cf(lhs).count() % cf(rhs).count());
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
constexpr bool operator==(const frequency<Rep1, Precision1>& lhs, const frequency<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return ct(lhs).count() == ct(rhs).count();
}

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
    requires std::three_way_comparable<std::common_type_t<Rep1, Rep2>>
constexpr auto operator<=>(const frequency<Rep1, Precision1>& lhs, const frequency<Rep2, Precision2>& rhs) {
    using ct = std::common_type_t<frequency<Rep1, Precision1>, frequency<Rep2, Precision2>>;
    return ct(lhs).count() <=> ct(rhs).count();
}

/**
 * @defgroup FrequencyTypes Standard Frequency Type Aliases
 * @{
 *
 * Standard integer-based frequency types using int64_t representation.
 * For floating-point representations, use frequency<double, Precision>.
 *
 * @code
 * // Integer types (default)
 * hertz audio{44100};
 * kilohertz cpu{3200};
 * megahertz fm{88};
 * gigahertz wifi{2};
 * terahertz optical{193};  // Near-infrared light
 *
 * // Floating-point types (custom)
 * using hertz_d = frequency<double>;
 * using kilohertz_d = frequency<double, std::kilo>;
 * using megahertz_d = frequency<double, std::mega>;
 *
 * hertz_d concert_a{440.0};
 * megahertz_d fm_precise{88.5};
 * @endcode
 */

/** @brief Frequency with 0.001 Hz (millihertz) precision. */
using millihertz = frequency<int64_t, std::milli>;
/** @brief Frequency with 1 Hz precision. */
using hertz = frequency<int64_t>;
/** @brief Frequency with 1000 Hz (kilohertz) precision. */
using kilohertz = frequency<int64_t, std::kilo>;
/** @brief Frequency with 1,000,000 Hz (megahertz) precision. */
using megahertz = frequency<int64_t, std::mega>;
/** @brief Frequency with 1,000,000,000 Hz (gigahertz) precision. */
using gigahertz = frequency<int64_t, std::giga>;
/** @brief Frequency with 1,000,000,000,000 Hz (terahertz) precision. */
using terahertz = frequency<int64_t, std::tera>;

/** @} */ // end of FrequencyTypes group

/** @cond INTERNAL */
// SI prefix for a hertz-per-count ratio; nullptr when unmapped.
template<typename Ratio>
struct _si_prefix {
    static constexpr const char* value = nullptr;
};
template<>
struct _si_prefix<std::femto> {
    static constexpr const char* value = "f";
};
template<>
struct _si_prefix<std::pico> {
    static constexpr const char* value = "p";
};
template<>
struct _si_prefix<std::nano> {
    static constexpr const char* value = "n";
};
template<>
struct _si_prefix<std::micro> {
    static constexpr const char* value = "µ";
};
template<>
struct _si_prefix<std::milli> {
    static constexpr const char* value = "m";
};
template<>
struct _si_prefix<std::ratio<1>> {
    static constexpr const char* value = "";
};
template<>
struct _si_prefix<std::kilo> {
    static constexpr const char* value = "k";
};
template<>
struct _si_prefix<std::mega> {
    static constexpr const char* value = "M";
};
template<>
struct _si_prefix<std::giga> {
    static constexpr const char* value = "G";
};
template<>
struct _si_prefix<std::tera> {
    static constexpr const char* value = "T";
};
template<typename Ratio>
inline constexpr const char* _si_prefix_v = _si_prefix<typename Ratio::type>::value;

// A value and SI prefix (of hertz) to display, for the base-unit and scaled
// forms.
struct _display {
    double value;
    const char* prefix;
};

// Precisions that are exactly a power of ten of one hertz, whether or not
// they have an SI prefix (std::deci and std::deca do not).
template<typename Precision>
concept _decimal_precision = detail::_power_of_ten_precision<Precision>;

// The frequency in hertz, as a double.
template<typename Rep, typename Precision>
_display _base_unit_display(const frequency<Rep, Precision>& f) {
    using p = typename frequency<Rep, Precision>::precision;
    return {detail::_ratio_value<p>(f.count()), _si_prefix_v<std::ratio<1>>};
}

// The frequency with the largest SI prefix in which its magnitude is at least
// 1, else the smallest; zero (keeping its sign) and non-finite values are
// shown in hertz, as the base-unit form would show them. For a power-of-ten precision each candidate's
// value is computed from the count with a single scaling by a power of ten, so
// values that land exactly on a prefix (1000 Hz) are exactly 1 in it; other
// precisions are scaled from their value in hertz.
template<typename Rep, typename Precision>
_display _scaled_display(const frequency<Rep, Precision>& f) {
    using p = typename frequency<Rep, Precision>::precision;
    double x;
    int exponent;
    if constexpr (_decimal_precision<p>) {
        x = static_cast<double>(f.count());
        exponent = _precision_exponent<p>;
    } else {
        x = _base_unit_display(f).value;
        exponent = 0;
    }
    if (x == 0 || !std::isfinite(x)) {
        return {x, _si_prefix_v<std::ratio<1>>};
    }
    // The prefixes _si_prefix maps, femto to tera
    const detail::_scaled s = detail::_pick_scale(x, exponent, detail::_si_scale);
    return {s.value, s.label};
}

#if CONFIG_QUANTA_STD_FORMAT
// How std::format renders a frequency of the given (reduced) precision; see
// detail::_unit_formatter. A precision with no SI prefix has no own unit.
template<typename Precision>
struct _format_traits {
    static constexpr const char* markers = "bs";
    static constexpr bool has_own_unit = _si_prefix_v<Precision> != nullptr;

    // Reports "{}" for a precision with no own unit. The parameter only gives
    // the function a path that is a constant expression, as a constexpr
    // function needs; the formatter always calls it with true.
    static constexpr void no_own_unit_error(bool fail = true) {
        if (fail) {
            _format_error("frequency: precision has no SI prefix; use an explicit format spec");
        }
    }

    // The own unit is written from constants; there is nothing to obtain.
    static constexpr bool own_unit(bool power) { return power; }

    template<typename OutputIt>
    static OutputIt append_own_unit(OutputIt out, bool) {
        out = _format_append(out, _si_prefix_v<Precision>);
        return _format_append(out, "Hz");
    }

    template<typename F>
    static _display base_display(const F& f) {
        return _base_unit_display(f);
    }

    template<typename F>
    static _display scaled_display(const F& f) {
        return _scaled_display(f);
    }

    template<typename OutputIt>
    static OutputIt append_label(OutputIt out, const _display& d) {
        out = _format_append(out, d.prefix);
        return _format_append(out, "Hz");
    }
};
#endif

/** @endcond */

/**
 * @brief Selects the base-unit form in to_string().
 *
 * The frequency is shown in hertz, with the value (count x precision)
 * rendered in the shortest fixed notation that round-trips (never
 * scientific), exactly as by the `b` format marker.
 *
 * @code
 * to_string(millihertz(1500), base_unit);  // "1.5Hz"
 * to_string(kilohertz(433), base_unit);    // "433000Hz"
 * to_string(millihertz(24), base_unit);    // "0.024Hz"
 * @endcode
 */
using quanta::base_unit;
using quanta::base_unit_t;

/**
 * @brief Selects the scaled form in to_string().
 *
 * The frequency is shown with the largest SI prefix (fHz, pHz, nHz, µHz, mHz,
 * Hz, kHz, MHz, GHz or THz) in which its magnitude is at least 1, or in fHz
 * if there is none; zero is shown in Hz. This applies to every precision,
 * including those with no SI prefix of their own. The value is rendered in the
 * shortest fixed notation that round-trips (never scientific), exactly as by
 * the `s` format marker.
 *
 * @code
 * to_string(megahertz(2400), scaled);  // "2.4GHz"
 * to_string(hertz(-2500), scaled);     // "-2.5kHz"
 * to_string(millihertz(33), scaled);   // "33mHz"
 * to_string(kilohertz(0), scaled);     // "0Hz"
 * @endcode
 */
using quanta::scaled;
using quanta::scaled_t;

/**
 * @brief Converts a frequency to a string with its precision-qualified unit.
 *
 * Integer counts are shown exactly; floating-point counts in the shortest
 * fixed notation that round-trips (never scientific), matching
 * std::format("{}"). Available for precisions with an SI prefix (femto to
 * tera); for other precisions use the base_unit or scaled form.
 *
 * @code
 * to_string(kilohertz(433));              // "433kHz"
 * to_string(millihertz(1500));            // "1500mHz"
 * to_string(frequency<double>(1e6));      // "1000000Hz"
 * to_string(frequency<double, std::kilo>(2.5)); // "2.5kHz"
 * @endcode
 */
template<typename Rep, typename Precision>
std::string to_string(const frequency<Rep, Precision>& f) {
    using precision = typename frequency<Rep, Precision>::precision;
    static_assert(_si_prefix_v<precision> != nullptr, "frequency: precision has no SI prefix");
    return _count_string(f.count()) + _si_prefix_v<precision> + "Hz";
}

/**
 * @brief Converts a frequency to a string in hertz; see base_unit.
 *
 * Available for every precision.
 *
 * @code
 * to_string(millihertz(1500), base_unit);  // "1.5Hz"
 * @endcode
 */
template<typename Rep, typename Precision>
std::string to_string(const frequency<Rep, Precision>& f, base_unit_t) {
    const _display d = _base_unit_display(f);
    return _fixed_string(d.value) + d.prefix + "Hz";
}

/**
 * @brief Converts a frequency to a string with an SI prefix scaled to its
 * magnitude; see scaled.
 *
 * Available for every precision.
 *
 * @code
 * to_string(megahertz(2400), scaled);  // "2.4GHz"
 * @endcode
 */
template<typename Rep, typename Precision>
std::string to_string(const frequency<Rep, Precision>& f, scaled_t) {
    const _display d = _scaled_display(f);
    return _fixed_string(d.value) + d.prefix + "Hz";
}

} // namespace quanta::frequency

namespace std {

template<typename Rep1, typename Precision1, typename Rep2, typename Precision2>
struct common_type<quanta::frequency::frequency<Rep1, Precision1>, quanta::frequency::frequency<Rep2, Precision2>> {
private:
    using common_precision = quanta::detail::_common_precision<Precision1, Precision2>;

public:
    using type = quanta::frequency::frequency<std::common_type_t<Rep1, Rep2>, common_precision>;
};

#if CONFIG_QUANTA_STD_FORMAT
// "{}" prints the exact stored count with a precision-qualified unit
// (kilohertz(433) -> "433kHz"; floating-point counts in the shortest fixed
// notation that round-trips, as to_string does: frequency<double>(1e6) ->
// "1000000Hz", never "1e+06Hz"). A non-empty spec controls only how the number
// is rendered (as double), never the unit, so both forms use the same unit:
// std::format("{:.1f}", millihertz(1500)) == "1500.0mHz".
//
// The unit changes only when the spec opts in with a single leading marker
// letter; the rest of the spec is then handled as above:
// - 'b' (base unit, see quanta::base_unit) shows the value in hertz:
//   std::format("{:b.1f}", millihertz(1500)) == "1.5Hz".
// - 's' (scaled, see quanta::scaled) shows the value with the SI prefix suited
//   to its magnitude: std::format("{:s}", megahertz(2400)) == "2.4GHz".
// With 'b' or 's' and nothing after the marker, the value is rendered in the
// shortest fixed notation that round-trips (never scientific), exactly as by
// to_string(f, quanta::base_unit) or to_string(f, quanta::scaled):
// std::format("{:b}", millihertz(24)) == "0.024Hz".
//
// A spec starting with 'b' or 's' is always a marker, so these letters cannot
// be the fill character of a spec that starts with a fill ("{:s>10.1f}" is the
// marker followed by ">10.1f").
//
// A precision with no SI prefix (e.g. std::deci) has no unit of its own: "{}"
// is a format error (a compile error for a constant format string), and a spec
// without a marker renders the value in hertz, as 'b' does:
// std::format("{:.1f}", frequency<int64_t, std::deci>(225)) == "22.5Hz".
template<typename Rep, typename Precision>
struct formatter<quanta::frequency::frequency<Rep, Precision>>
    : private quanta::detail::_unit_formatter<
          quanta::frequency::_format_traits<typename quanta::frequency::frequency<Rep, Precision>::precision>> {
    using quanta::detail::_unit_formatter<
        quanta::frequency::_format_traits<typename quanta::frequency::frequency<Rep, Precision>::precision>>::parse;

    template<typename FormatContext>
    auto format(const quanta::frequency::frequency<Rep, Precision>& f, FormatContext& ctx) const {
        return this->format_quantity(f, ctx);
    }
};
#endif

} // namespace std

/**
 * @brief User-defined literals for frequency types.
 */
namespace quanta {
inline namespace literals {
inline namespace frequency_literals {
/** @brief Literal for millihertz (e.g., 1000_mHz). */
template<char... Digits>
constexpr quanta::frequency::millihertz operator""_mHz() {
    return detail::check_overflow<quanta::frequency::millihertz, Digits...>();
}

/** @brief Literal for hertz (e.g., 1000_Hz). */
template<char... Digits>
constexpr quanta::frequency::hertz operator""_Hz() {
    return detail::check_overflow<quanta::frequency::hertz, Digits...>();
}

/** @brief Literal for kilohertz (e.g., 80_kHz). */
template<char... Digits>
constexpr quanta::frequency::kilohertz operator""_kHz() {
    return detail::check_overflow<quanta::frequency::kilohertz, Digits...>();
}

/** @brief Literal for megahertz (e.g., 80_MHz). */
template<char... Digits>
constexpr quanta::frequency::megahertz operator""_MHz() {
    return detail::check_overflow<quanta::frequency::megahertz, Digits...>();
}

/** @brief Literal for gigahertz (e.g., 2_GHz). */
template<char... Digits>
constexpr quanta::frequency::gigahertz operator""_GHz() {
    return detail::check_overflow<quanta::frequency::gigahertz, Digits...>();
}

/** @brief Literal for terahertz (e.g., 1_THz). */
template<char... Digits>
constexpr quanta::frequency::terahertz operator""_THz() {
    return detail::check_overflow<quanta::frequency::terahertz, Digits...>();
}

} // namespace frequency_literals
} // namespace literals

namespace frequency {
using namespace literals::frequency_literals;
} // namespace frequency
} // namespace quanta
