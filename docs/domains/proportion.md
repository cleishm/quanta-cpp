# Proportion: `quanta::proportion`

Type-safe proportions (dimensionless linear ratios), modeled after `std::chrono::duration`. Include `<quanta/proportion>`.

## Features

- **Type-safe proportions** with distinct types for different precisions
- **Standard precisions**: percent, permille, parts per million, parts per billion, and plain fractions
- **Integer and floating-point representations** for exact or fractional precision
- **Automatic conversions** between precisions
- **Lossless implicit conversions** (lossy conversions require explicit casts)
- **Exact products**: a proportion times a proportion is a proportion, at the product of their precisions
- **Scaling quantities**: a proportion scales the linear quantities of the other domains, exactly (`3300_mV * 50_pct`)
- **Decibel conversions**: to and from decibel gains, as power or amplitude ratios (in `<quanta/decibel>`; not constexpr)
- **User-defined literals** for concise notation
- **`std::format` support** (when available)
- **Fully constexpr** - all operations can be evaluated at compile time

## Usage

```cpp
#include <quanta/proportion.hpp>

using namespace quanta::proportion;
using namespace quanta::proportion_literals;

// Basic proportions
percent duty_cycle{25};
permille slope{12};
ppm tolerance{50};
ppb impurity{3};
proportion<double> efficiency{0.875};  // a plain fraction

// Using literals
auto p1 = 25_pct;
auto p2 = 5_permille;
auto p3 = 50_ppm;
auto p4 = 3_ppb;

// Precision control
ppm precise{12345};             // 1.2345%
percent coarse = percent(precise);  // explicit lossy conversion: 1%
ppm back = coarse;              // implicit lossless conversion: 10000 ppm

// Arithmetic
auto sum = 1_pct + 50_ppm;      // ppm(10050), the common precision
auto scaled = 5_pct * 3;        // 15%
auto ratio = 50_pct / 25_pct;   // 2 (scalar)
auto both = 50_pct * 50_pct;    // 25%, exactly: proportion<int64_t, std::ratio<1, 10000>>(2500)

// Comparisons work across precisions
if (percent{1} == ppm{10000}) {
    // true - same proportion, different precision
}

// String conversion
std::string s = to_string(percent(25));  // "25%"

// Formatting: always in the type's own unit unless a marker asks otherwise
percent reading{25};
std::string raw  = std::format("{}", reading);      // "25%" (exact stored value)
std::string disp = std::format("{:.1f}", reading);  // "25.0%"
std::string frac = std::format("{:b}", reading);    // "0.25" (base unit: a plain fraction)
std::string sc   = std::format("{:s}", 200_ppm);    // "200ppm" (scaled)
```

## Proportion Types

| Type | Unit | Precision |
|------|------|-----------|
| `percent` | % | 1/100 |
| `permille` | ‰ | 1/1,000 |
| `ppm` | ppm | 1/1,000,000 |
| `ppb` | ppb | 1/1,000,000,000 |

All types use `int64_t` representation by default. A plain fraction (precision 1) has no alias: write
`proportion<double>{0.25}`, or `proportion<int64_t>` for whole multiples. For fractional precision, use
`proportion<double, Precision>` (see Floating-Point Proportions section).

### Literals

```cpp
using namespace quanta::proportion_literals;

25_pct       // percent(25)
5_permille   // permille(5)
50_ppm       // ppm(50)
3_ppb        // ppb(3)
```

## Multiplying Proportions

The product of two proportions is a proportion. Counts multiply and precisions multiply, so no
conversion is needed and the result is exact for integer representations:

```cpp
auto both = 50_pct * 50_pct;   // proportion<int64_t, std::ratio<1, 10000>>(2500)
both == 25_pct;                // true
auto tiny = 3_pct * 7_ppm;     // proportion<int64_t, std::ratio<1, 100000000>>(21), 210 ppb

// Cast or round back to a named precision when needed
percent p = round<percent>(33_pct * 33_pct);  // 11% (10.89% rounded)
```

A scalar on either side scales the count and keeps the precision (`25_pct * 2`, `2 * 25_pct`), while
dividing two proportions gives a scalar (`50_pct / 25_pct` is `2`). A product whose precision cannot be
represented as a `std::ratio` (such as `1_ppb * 1_ppb * 1_ppb`, 10⁻²⁷) is a compile-time error.

## Scaling Quantities

A proportion scales the linear quantities of the other domains, in either order, giving the same kind
of quantity:

- electrical quantities (`quanta::electro`: voltage, current, resistance, power, charge, energy,
  capacitance and inductance), but not decibel gains
- lengths, areas and volumes (`quanta::spatial`)
- frequencies (`quanta::frequency`)
- temperature deltas (`quanta::thermo::delta`), but not absolute temperatures
- data sizes and rates (`quanta::data`)

No domain header needs `<quanta/proportion>` for this; include it alongside the domain's header. As in a
product of two proportions, counts multiply and precisions multiply, so the result is exact for integer
representations. Its precision is usually one with no unit symbol: the `b` and `s` format markers (and
the `base_unit` and `scaled` tags of `to_string`) show it in the base unit or scaled, and any other spec
shows it in the base unit, but `{}` and `to_string(q)` need a named precision, so convert it to one
first, explicitly where that loses precision:

```cpp
auto v = 3300_mV * 50_pct;        // quantity<volt_unit, int64_t, std::ratio<1, 100000>>(165000), 1.65 V
std::format("{:s}", v);           // "1.65V"
std::format("{:b.3f}", v);        // "1.650V"
millivolts mv(v);                 // 1650 mV (explicit: a coarser precision); {} gives "1650mV"
auto drift = 2400_MHz * 20_ppm;   // 48 kHz, as hertz
auto share = 50_pct * 10_MB;      // 5 MB, at precision 10000 B
auto rise = delta_celsius(10) * 25_pct;  // 2.5 °C, at precision 1/100 °C
auto v2 = voltage<double>(3.3) * proportion<double>(0.5);  // 1.65 V, as voltage<double>
```

Dividing a quantity by a proportion divides the counts and the precisions; for integer representations
the count division truncates, as for the cross-quantity quotients of the other domains:

```cpp
auto full = 3300_mV / 50_pct;     // quantity<volt_unit, int64_t, std::deci>(66), 6.6 V
auto part = 10_mV / 3_pct;        // 0.3 V (truncated from 0.333...)
```

A proportion divided by a quantity is not defined. Absolute temperatures and decibel levels and gains
are not linear, so a proportion does not scale them; for decibels, see below.

## Decibels

A decibel gain is a logarithmic ratio, so it is converted to and from a proportion rather than scaled
by one. `<quanta/decibel>` provides the conversions, in `quanta::electro`, with the same power/amplitude
split as `power_ratio` and `amplitude_ratio`:

```cpp
#include <quanta/decibel>

power_proportion(3_dB);       // ~2.0 (~200%), as proportion<double>
amplitude_proportion(6_dB);   // ~2.0
power_proportion(-3_dB);      // ~0.5: half the power
power_gain(50_pct);           // ~-3.01 dB, as gain<double>
amplitude_gain(50_pct);       // ~-6.02 dB
power_gain(10_ppm);           // -50 dB
```

`power_proportion` and `amplitude_proportion` give a plain fraction (`proportion<double>`, precision 1);
`power_gain` and `amplitude_gain` take a proportion of any precision as a plain fraction, which must be
positive. None of them is `constexpr`, as they need `std::pow` and `std::log10`.

## Formatting

`to_string()` and `std::format` show a proportion in its own unit: `{}` prints the exact stored count
followed by the precision's symbol (`%`, `‰`, `ppm` or `ppb`; none for a plain fraction), and a format
spec such as `{:.1f}` changes only how the number is rendered (as a `double`), never the unit.

```cpp
to_string(percent(25));                   // "25%"
std::format("{}", permille(5));           // "5‰"
std::format("{}", proportion<double>(0.25)); // "0.25"
std::format("{:.1f}", percent(25));       // "25.0%"
std::format("{:>6.1f}", percent(25));     // "  25.0%"
```

To show a different unit, start the spec with a single marker letter; the rest of the spec is the
normal `std::format` spec for a `double`:

| Marker | Unit | Example |
|--------|------|---------|
| *(none)* | the type's own unit | `std::format("{:.1f}", percent(25))` → `"25.0%"` |
| `b` | base unit (a plain fraction, no symbol) | `std::format("{:b}", percent(25))` → `"0.25"` |
| `s` | scaled: % if the magnitude is at least 0.001 (0.1%), else ppm if at least 0.000001, else ppb (‰ is never chosen); zero and non-finite values as a plain fraction | `std::format("{:s}", permille(4))` → `"0.4%"` |

```cpp
std::format("{:b}", ppm(50));               // "0.00005"
std::format("{:s}", proportion<double>(0.5)); // "50%"
std::format("{:s}", 1_permille);            // "0.1%"
std::format("{:s}", 999_ppm);               // "999ppm"
std::format("{:s}", 3_ppb);                 // "3ppb"
std::format("{:s}", percent(150));          // "150%"
std::format("{:s}", percent(0));            // "0"
std::format("{:s.2f}", 200_ppm);            // "200.00ppm"

// The same forms without <format>:
to_string(percent(25), base_unit);          // "0.25"
to_string(ppm(200), scaled);                // "200ppm"
```

Notes:
- A spec that starts with `b` or `s` is always a marker, so those letters cannot be used as the fill
  character at the start of a spec (`{:s>8}` is the `s` marker followed by `>8`).
- Values are converted to `double` for the markers, so they work with integer representations too.
- With no spec (`{}` on a floating-point proportion, `{:b}`, `{:s}`, and every `to_string`), values are
  rendered in the shortest fixed notation that round-trips: `proportion<double>(1e-5)` → `"0.00001"`,
  never `"1e-05"`.
- A precision with no symbol (e.g. `proportion<int64_t, std::ratio<1, 10000>>`, the type of
  `50_pct * 50_pct`) has no unit of its own: `{}` and `to_string(p)` do not compile for it, and a spec
  without a marker renders the value as a plain fraction (`{:.4f}` → `"0.2500"`). The `b` and `s`
  markers and tags work for every precision (`{:s}` → `"25%"`).
- To show a value in another unit, cast it, e.g. `std::format("{:.2f}", proportion<double, std::ratio<1, 100>>(p))`.

## Conversion Rules

Conversions follow the same philosophy as `std::chrono`:

- **Implicit conversions** are allowed when lossless (e.g., `percent` → `ppm`)
- **Explicit conversions** are required when lossy (e.g., `ppm` → `percent`)

```cpp
// Implicit (lossless) - to a finer precision
ppm p = percent{1};               // OK: 1% → 10000 ppm
permille pm = percent{1};         // OK: 1% → 10‰
proportion<double> f = 25_pct;    // OK: to a floating-point rep, 0.25

// Explicit required (lossy) - to a coarser precision, with truncation
percent pct = percent(ppm{12345});                       // 1.2345% → 1% (truncates)
percent pct2 = percent(proportion<double, std::ratio<1, 100>>{12.5}); // floating-point to integer: 12%

// Use proportion_cast for explicit conversions
auto pct3 = proportion_cast<percent>(permille{25});  // 2%
```

Mixed-precision arithmetic and comparison use the common type, which follows the `std::chrono` rules:
`std::common_type_t<percent, ppm>` is `ppm`.

`floor`, `ceil`, `trunc` and `round` give explicit rounding control:

```cpp
auto f = floor<percent>(permille{-15});  // -2%
auto t = trunc<percent>(permille{-15});  // -1%
auto r = round<percent>(permille{25});   // 2% (round half to even)
```

## Floating-Point Proportions

The library supports floating-point representations for fractional precision. This is useful for:
- Plain fractions such as efficiencies and duty cycles (`proportion<double>{0.875}`)
- Measured or computed ratios that are not whole counts of any precision
- Calculations involving division

```cpp
// Define floating-point proportion types
using fraction_d = proportion<double>;
using percent_d = proportion<double, std::ratio<1, 100>>;

fraction_d efficiency{0.875};
percent_d share{12.5};          // 12.5%

// Fractional arithmetic
fraction_d f = fraction_d(0.5) * 1.5;   // 0.75
percent_d g = percent_d(12.5) + 1_pct;  // 13.5%

// Mixing integer and floating-point types
fraction_d from_int = 25_pct;           // 0.25 (implicit)
percent back = percent(fraction_d(0.129)); // 12% (explicit, truncates)

// Note: Modulo operations are disabled for floating-point types
// share % 3;  // Compile error - modulo not defined for floating-point
```

**Important considerations:**
- Floating-point proportions allow fractional values but sacrifice exact arithmetic
- Implicit conversions between integer and floating-point proportions follow the same lossless/lossy rules as precision conversions
- Integer types are preferred when exact values, exact products and modulo operations are needed
