# quanta

Type-safe, header-only C++20 units for temperature, frequency, electrical and
spatial quantities, modeled after `std::chrono`.

📚 **[Full API Documentation](https://cleishm.github.io/quanta-cpp/)**

```cpp
#include <quanta/electro>
#include <quanta/frequency>
#include <quanta/spatial>
#include <quanta/thermo>

using namespace quanta::literals;

auto room = 22534_mc;                     // millicelsius
auto clock = 2400_MHz;
auto power = 3300_mV * 20_mA;             // V x I = P

std::format("{:b.1f}", room);             // "22.5°C"
std::format("{:s}", clock);               // "2.4GHz"
std::format("{:s}", power);               // "66mW"
std::format("{}", clock.wavelength<quanta::spatial::millimeters>()); // "124mm"
```

## Domains

Each domain has its own header and namespace, and can be used on its own.

| Domain | Header | Namespace | Guide |
|--------|--------|-----------|-------|
| Temperature (absolute and delta; °C, K, °F) | `<quanta/thermo>` | `quanta::thermo` | [thermo](docs/domains/thermo.md) |
| Frequency (mHz to THz; period, wavelength, musical intervals) | `<quanta/frequency>` | `quanta::frequency` | [frequency](docs/domains/frequency.md) |
| Electrical units (V, A, Ω, W, C, J, F, H) and decibels (dB, dBm, dBW, dBµV) | `<quanta/electro>`, `<quanta/decibel>` | `quanta::electro` | [electro](docs/domains/electro.md) |
| Length, area and volume (metric and imperial) | `<quanta/spatial>` | `quanta::spatial` | [spatial](docs/domains/spatial.md) |

Every header is also available with a `.hpp` extension (`<quanta/thermo.hpp>`).

## Features

- **Type-safe quantities**: mixing up units of different kinds is a compile error
- **Configurable precision** and representation, as with `std::chrono::duration`
- **Lossless implicit conversions** (lossy conversions require explicit casts)
- **User-defined literals** for concise notation (`25_c`, `2400_MHz`, `3300_mV`, `250_mm`)
- **`std::format` support** (when available), with markers for base-unit (`b`) and scaled (`s`) forms
- **Works without exceptions**, as on embedded targets
- **Fully constexpr** wherever the standard library allows

## Namespaces and Literals

As with `std::chrono`, each domain's literals are in an inline namespace of
`quanta::literals`, and are also brought into the domain's own namespace:

```cpp
using namespace quanta::literals;         // literals for every domain
using namespace quanta::thermo_literals;  // just the temperature literals
using namespace quanta::thermo;           // temperature types and literals
```

Literal values are integers, written as any C++ integer literal: with digit
separators (`1'000_mV`), in hexadecimal (`0x10_V`), binary (`0b101_V`) or, with
a leading `0`, octal (`017_Hz` is 15 Hz, as `std::chrono`'s `017h` is 15 h). A
value the literal's type cannot represent is a compile error.

The traits and tags shared by every domain (`quanta::treat_as_inexact`,
`quanta::base_unit`, `quanta::scaled`) are defined once in `quanta::` and
re-exported into each domain namespace, so they are never ambiguous when
several domains are in use. Specialize `quanta::treat_as_inexact` to make a
custom representation behave like a floating-point one in every domain.

## Requirements

- C++20 compiler (GCC 12+, Clang 15+, MSVC 19.30+)
- **MSVC users:** The CMake target adds `/utf-8` automatically (the headers
  contain UTF-8 unit symbols). Projects that do not use the CMake target (e.g.
  a plain include path) must add `/utf-8` themselves.
- **GCC before 14** (including ESP-IDF toolchains before GCC 14) cannot lex
  non-ASCII literal suffixes, so the literals that use them (`_Δc`, `_kΩ`,
  `_dBµV`, `_µm`, …) are unavailable there (`QUANTA_HAS_UTF8_LITERALS` is 0).
  Each has an ASCII alternative that is always available (`_delta_c`, `_kOhm`,
  `_dBuV`, `_um`, …).

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    quanta
    GIT_REPOSITORY https://github.com/cleishm/quanta-cpp.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(quanta)

target_link_libraries(your_target PRIVATE quanta::quanta)
```

### Installed package

```cmake
find_package(quanta CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE quanta::quanta)
```

### ESP-IDF

The library can be used as an ESP-IDF component. Add it to your project's
`idf_component.yml`:

```yaml
dependencies:
  cleishm/quanta: "^0.1.0"
```

`std::format` support is controlled by the `CONFIG_QUANTA_STD_FORMAT_SUPPORT`
Kconfig option in menuconfig (under "Component config" → "Quanta Library").
Enabled (the default), `std::format` is supported where the toolchain provides
`<format>` (not the GCC 12 of ESP-IDF 5.1); disabled, it is off.
Outside ESP-IDF it is enabled when `<format>` is available, and can be disabled
by defining `CONFIG_QUANTA_STD_FORMAT` to `0`.

### Manual

Copy the `include/quanta/` directory to your project.

## Migrating from thermo-cpp, frequency-cpp, electro-cpp and spatial-cpp

quanta replaces these four libraries. Compared with their final releases
(thermo-cpp 2.2, frequency-cpp 1.3, electro-cpp 0.4 and spatial-cpp 0.1), the
types, functions and literals are unchanged apart from the behaviour changes
listed below the table; only their names and locations moved:

| Before | After |
|--------|-------|
| `#include <thermo/thermo>` | `#include <quanta/thermo>` |
| `#include <frequency/frequency>` | `#include <quanta/frequency>` |
| `#include <electro/electro>`, `<electro/decibel>` | `#include <quanta/electro>`, `<quanta/decibel>` |
| `#include <spatial/spatial>` | `#include <quanta/spatial>` |
| `thermo::`, `freq::`, `electro::`, `spatial::` | `quanta::thermo::`, `quanta::frequency::`, `quanta::electro::`, `quanta::spatial::` |
| `thermo_literals`, `frequency_literals`, … | `quanta::thermo_literals`, `quanta::frequency_literals`, … (or `quanta::literals`) |
| `thermo::treat_as_inexact` (and the others) | `quanta::treat_as_inexact` |
| `CONFIG_THERMO_STD_FORMAT` macro (and the others) | `CONFIG_QUANTA_STD_FORMAT` |
| `CONFIG_THERMO_STD_FORMAT` Kconfig option (and the others) | `CONFIG_QUANTA_STD_FORMAT_SUPPORT` |
| `THERMO_HAS_UTF8_LITERALS` (and the others) | `QUANTA_HAS_UTF8_LITERALS` |
| `thermo::thermo` CMake target (and the others) | `quanta::quanta` |
| `cleishm/thermo` ESP-IDF component (and the others) | `cleishm/quanta` |

Behaviour changes:

- thermo: `round` on deltas rounds ties to even, as `std::chrono::round` and
  the other domains do; thermo-cpp rounded ties away from zero.
- thermo: `floor` and `ceil` of a delta step the cast result by one tick of
  the target precision only when the target rep is integral, as in the other
  domains; for any other target rep (floating-point or user-defined) they are
  a plain `delta_cast`. thermo-cpp stepped every target, so an inexact
  floating-point cast could move by a whole tick
  (`floor<delta<double>>(delta_millicelsius(2007))` gave 1.007).
- thermo: subtracting two temperatures gives a delta, exactly as
  `difference()` does, and adding two temperatures is ill-formed; thermo-cpp
  returned a temperature from both.
- electro: `to_level` of a zero quantity gives a level of -infinity when the
  level's rep is floating-point; electro-cpp gave `min()`, which integral
  levels still get.
- All domains: literals follow the C++ integer-literal rules, so a leading `0`
  makes a literal octal (`0123_V` is 83 V; the old libraries read it as 123 V),
  digit separators, hexadecimal and binary are accepted, and a value beyond
  `unsigned long long` is a compile error rather than wrapping silently.
- All domains: every conversion between two quantities of the same kind that
  the domain's cast can perform is available by constructor, implicitly or
  explicitly but never both. It is implicit when it is lossless (to a
  floating-point rep, or to a precision that evenly divides the source's from
  a rep that is not floating-point) and the source rep converts implicitly to
  the target rep; otherwise it is explicit. So a floating-point quantity now
  converts explicitly to an integral one at the same or a finer precision
  (`hertz(frequency<double>(1.5))` is 1 Hz, as `frequency_cast` gives), where
  the old libraries rejected the constructor. Temperatures follow the same
  rule with their own test for lossless (across scales, the offset must also
  be a whole number of target ticks). One consequence for thermo: a
  temperature whose rep the source rep converts to only explicitly now needs
  direct-initialization (`T t(celsius(3));` rather than `T t = celsius(3);`),
  as deltas already did.

Additions, so that every domain offers the same operations:

- thermo: `abs` of a delta, and explicit lossy construction of a delta from
  another (`delta_celsius(delta_millicelsius(1500))`).
- frequency, electro (including decibel gains and levels) and spatial:
  `trunc`, rounding toward zero.

thermo previously required C++23; quanta needs only C++20 throughout.

Migrating from an earlier release (thermo-cpp before 2.2, frequency-cpp before
1.3, electro-cpp before 0.4) also changes formatted output: a format spec such
as `{:.1f}` now renders the value in the type's own unit (`22534.0m°C` for
`millicelsius(22534)`) where those releases converted to the base unit
(`22.5°C`). Add the `b` marker (`{:b.1f}`) for the old output. A plain `{}` is
unchanged.

## Building and Testing

```bash
cmake -B build -DQUANTA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

With MSVC:

```bash
cmake -B build -DQUANTA_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="/EHsc"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Formatting is checked with clang-format 23.1.0, the version CI uses
(`pipx install clang-format==23.1.0`):

```bash
cmake --build build --target format-check  # or --target format to apply it
```

## License

MIT License - see [LICENSE](LICENSE) for details.
