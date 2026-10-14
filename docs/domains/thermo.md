# Temperature: `quanta::thermo`

Type-safe temperature handling, modeled after `std::chrono`. Include `<quanta/thermo>`.

## Features

- **Type-safe temperatures** with distinct types for Celsius, Kelvin, and Fahrenheit
- **Configurable precision** (degree, decidegree, millidegree) plus real-valued (`double`) types for display and computation
- **Automatic conversions** between scales and precisions
- **Lossless implicit conversions** (lossy conversions require explicit casts)
- **Temperature deltas** distinct from absolute temperatures
- **User-defined literals** for concise notation
- **`std::format` support** (when available)
- **Fully constexpr** - all operations can be evaluated at compile time

## Usage

```cpp
#include <quanta/thermo>  // or <quanta/thermo.hpp>

using namespace quanta::thermo;
using namespace quanta::thermo_literals;

// Absolute temperatures
celsius room_temp{20};
millicelsius precise_room = room_temp;  // implicit lossless conversion to higher precision
fahrenheit f = fahrenheit(room_temp);  // explicit cross-scale conversion

// Using literals
auto boiling = 100_c;
auto freezing = 32_f;
auto absolute_zero = 0_k;

// Precision control
millicelsius precise{25500};  // 25.5°C
celsius coarse = celsius(precise);  // explicit lossy conversion (truncates to 25°C)
millicelsius back = coarse;  // implicit lossless conversion (25000 m°C)

// Temperature deltas (differences)
auto rise = 5_delta_c;  // 5 degree change (or 5_Δc, except on GCC < 14)
auto new_temp = room_temp + rise;  // 25°C

// The difference of two temperatures is a delta, as with std::chrono::time_point
delta_celsius diff = boiling - room_temp;  // 80°C difference, same as difference(boiling, room_temp)
// boiling + room_temp;                    // ill-formed: adding two temperatures is meaningless

// Comparisons work across precisions
if (millicelsius{20000} == celsius{20}) {
    // true - same temperature, different precision
}

// String conversion
std::string s = to_string(room_temp);  // "20°C"

// Formatting: always in the type's own unit, unless a marker opts out.
millicelsius reading{22534};
std::string raw  = std::format("{}", reading);      // "22534m°C" (exact stored value)
std::string spec = std::format("{:.1f}", reading);  // "22534.0m°C" (same unit)
std::string disp = std::format("{:b.1f}", reading); // "22.5°C" (base unit)
```

## Temperature Types

### Absolute Temperatures

| Type | Scale | Precision |
|------|-------|-----------|
| `celsius` | Celsius | 1°C |
| `decicelsius` | Celsius | 0.1°C |
| `millicelsius` | Celsius | 0.001°C |
| `kelvin` | Kelvin | 1 K |
| `decikelvin` | Kelvin | 0.1 K |
| `millikelvin` | Kelvin | 0.001 K |
| `fahrenheit` | Fahrenheit | 1°F |
| `decifahrenheit` | Fahrenheit | 0.1°F |
| `millifahrenheit` | Fahrenheit | 0.001°F |

See Formatting below for how these render as strings.

### Temperature Deltas

| Type | Precision |
|------|-----------|
| `delta_celsius` / `delta_kelvin` | 1° |
| `delta_decicelsius` / `delta_decikelvin` | 0.1° |
| `delta_millicelsius` / `delta_millikelvin` | 0.001° |
| `delta_fahrenheit` | 1°F (= 5/9°C) |
| `delta_decifahrenheit` | 0.1°F |
| `delta_millifahrenheit` | 0.001°F |

### Literals

```cpp
using namespace quanta::thermo_literals;

// Absolute temperatures
20_c      // celsius(20)
200_dc    // decicelsius(200) = 20.0°C
20000_mc  // millicelsius(20000) = 20.000°C
293_k     // kelvin(293)
68_f      // fahrenheit(68)

// Negative absolute temperatures (unary minus on the literal)
-123_dc   // decicelsius(-123) = -12.3°C
-40_f     // fahrenheit(-40)
// -25_k  // ill-formed: negating a Kelvin temperature falls below absolute zero

// Temperature deltas (not on GCC < 14; see the ASCII spellings below)
5_Δc      // delta_celsius(5)
5000_Δmc  // delta_millicelsius(5000)
9_Δf      // delta_fahrenheit(9) = delta_celsius(5)

// ASCII spellings of the delta literals (always available, including on GCC before 14)
5_delta_c      // delta_celsius(5), same as 5_Δc
5000_delta_mc  // delta_millicelsius(5000), same as 5000_Δmc
5_delta_k      // delta_kelvin(5), same as 5_Δk
5000_delta_mk  // delta_millikelvin(5000), same as 5000_Δmk
9_delta_f      // delta_fahrenheit(9), same as 9_Δf
9000_delta_mf  // delta_millifahrenheit(9000), same as 9000_Δmf
```

The `_Δ…` literals are unavailable on GCC before 14, which cannot lex non-ASCII
literal suffixes (`QUANTA_HAS_UTF8_LITERALS` is 0 there). The `_delta_…` forms
are their ASCII alternatives and are always available.

Unary minus is available on relative scales (Celsius, Fahrenheit) so that
negative literals like `-123_dc` work — the sign is applied to the value the
literal produces, i.e. `-(123_dc)`. It is deliberately *not* available on the
absolute Kelvin scale, where a negated temperature would fall below absolute
zero; `-25_k` is a compile error. Deltas are negatable on every scale.

## Formatting

`to_string()` and `std::format` render a temperature or delta in **its own
unit** — the scale's degree with the precision's SI prefix, and a `Δ` marker
for deltas. `{}` prints the exact stored count; any other format spec changes
only how the number is rendered (as a `double`), never the unit:

```cpp
std::format("{}", millicelsius(22534));          // "22534m°C"
std::format("{:.1f}", millicelsius(22534));      // "22534.0m°C"
std::format("{:>9.1f}", decikelvin(2955));       // "   2955.0dK"
std::format("{:.1f}", delta_millicelsius(1500)); // "1500.0Δm°C"
to_string(millicelsius(22534));                  // "22534m°C"
```

A single leading **marker** letter in the spec opts into another unit; the rest
of the spec is a normal floating-point format spec:

| Marker | Unit | Example |
|--------|------|---------|
| `b` | Base unit: the unprefixed degree of the same scale (°C, K or °F; never converted to another scale) | `{:b.1f}` of `millicelsius(22534)` → `22.5°C` |
| `s` | Scaled: temperatures are not rescaled, so this is the type's own unit | `{:s.1f}` of `millicelsius(22534)` → `22534.0m°C` |

With nothing after the marker (`{:b}`, `{:s}`), the value is rendered in the
shortest fixed notation that round-trips — never scientific notation. The same
forms are available without `<format>` through tag arguments to `to_string()`:

```cpp
std::format("{:b}", millicelsius(22534));             // "22.534°C"
std::format("{:b.1f}", delta_millifahrenheit(9000));  // "9.0Δ°F"
std::format("{:b*>8.2f}", millicelsius(22534));       // "***22.53°C"
to_string(millicelsius(22534), base_unit);            // "22.534°C"
to_string(millicelsius(22534), scaled);               // "22534m°C"
```

Because a spec starting with `b` or `s` is always a marker, those letters cannot
be used as the fill character at the start of a spec (`{:b>8}` is the `b` marker
followed by `>8`).

Floating-point representations (e.g. `temperature<celsius_scale, delta<double>>`)
render with no spec in the same shortest round-trip fixed notation, so
`to_string` and `{}` agree: `22.5°C`, `1000000°C`.

Temperature precisions without an SI prefix (e.g.
`temperature<celsius_scale, delta<int64_t, std::centi>>`) have no unit label of
their own: `{}` is a format error, and any other spec or marker renders
the value in the base unit.

## Conversion Rules

Conversions follow the same philosophy as `std::chrono`:

- **Implicit conversions** are allowed when lossless (e.g., `celsius` → `millicelsius`)
- **Explicit conversions** are required otherwise: when lossy (e.g., `millicelsius` → `celsius`),
  or from a floating-point representation to an integral one
- **Cross-scale conversions** consider both precision and scale offset: they are lossless only
  when both are whole numbers of the target's ticks
- **Without 128-bit integer support** (e.g. MSVC, 32-bit targets such as the ESP32), cross-scale
  integral conversions of counts near the 64-bit limits can overflow the intermediate

Every conversion that `temperature_cast` can perform has exactly one of these
constructors, as with the other domains' quantities.

```cpp
// Implicit (lossless)
millicelsius mc = celsius{20};       // OK: 20°C → 20000 m°C
millikelvin mk = millicelsius{0};    // OK: 0 m°C → 273150 mK

// Explicit required (lossy)
celsius c = celsius(millicelsius{1500});  // 1500 m°C → 1°C (truncates)
kelvin k = kelvin(celsius{0});            // 0°C → 273 K (truncates 273.15)
celsius w = celsius(temperature<celsius_scale, delta<double>>{21.7});  // 21°C (truncates)

// Use temperature_cast for explicit conversions
auto k2 = temperature_cast<kelvin>(celsius{100});
```

Deltas convert like the other domains' quantities: implicitly when lossless,
and otherwise explicitly (by constructor or `delta_cast`, both truncating).
`floor`, `ceil`, `trunc` and `round` give explicit rounding control (a
target whose rep is not integral, such as a floating-point one, is not
rounded). `abs` gives a delta's magnitude.

```cpp
delta_millicelsius fine = delta_celsius(2);                    // implicit: 2000
delta_celsius coarse = delta_celsius(delta_millicelsius(1500)); // explicit: 1 (truncates)
delta_celsius whole = delta_celsius(delta<double>(1.5));        // explicit: 1 (truncates)
auto d = delta_cast<delta_celsius>(delta_millicelsius(1500));  // 1 (truncates)
auto r = round<delta_celsius>(delta_millicelsius(1500));       // 2 (round half to even)
auto f = floor<delta_celsius>(delta_millicelsius(-1500));      // -2
auto a = abs(delta_millicelsius(-1500));                       // 1500
```
