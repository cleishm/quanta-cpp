# Changelog

All notable changes to **quanta** are documented in this file.

The library follows [semantic versioning](https://semver.org/).

## v0.1.0

First release. quanta merges four libraries — thermo-cpp, frequency-cpp,
electro-cpp and spatial-cpp — into one header-only C++20 library of
type-safe units modeled after `std::chrono`, sharing a single core for
casts, rounding, common precision and formatting. See the README's
"Migrating" section for the full list of renames and behaviour changes.

### Domains

- `quanta::thermo` — absolute temperatures and temperature deltas in Celsius,
  Kelvin and Fahrenheit, at configurable precision
- `quanta::frequency` — frequencies from mHz to THz, with conversions to
  period and wavelength (formerly `freq::`)
- `quanta::electro` — voltage, current, resistance, power, charge, energy,
  capacitance and inductance, with cross-unit arithmetic, plus decibel gains
  and levels (dB, dBm, dBW, dBµV) in `<quanta/decibel>`
- `quanta::spatial` — length, area and volume in metric and imperial units

### Changes from the separate libraries

- thermo: `round` on deltas rounds ties to even, as `std::chrono::round` and
  the other domains do
- thermo: subtracting two temperatures gives a delta, and adding two
  temperatures is ill-formed
- thermo: `floor` and `ceil` step by a tick only for integral target reps, as
  in the other domains
- electro: `to_level` of a zero quantity gives -infinity for floating-point
  levels
- All domains: literals follow the C++ integer-literal rules (a leading `0` is
  octal; digit separators, hexadecimal and binary are accepted; overflow is a
  compile error)
- All domains: every conversion the domain's cast can perform is available by
  constructor — implicit when lossless, explicit otherwise
- Fixes: mixed-precision temperature arithmetic, `floor`/`ceil`/`round` from
  floating-point to integral reps, widening `delta_cast`, and scaled display of
  infinity and NaN
- thermo now needs only C++20 (previously C++23)

### Additions

- thermo: `abs` of a delta, and explicit lossy construction of one delta from
  another
- frequency, electro and spatial: `trunc`, rounding toward zero

### Build and packaging

- CMake target `quanta::quanta`, header `<quanta/...>` (each also as `.hpp`)
- ESP-IDF component `cleishm/quanta`; `std::format` support is controlled by
  the `CONFIG_QUANTA_STD_FORMAT_SUPPORT` Kconfig option
- The non-ASCII literals (`_Δc`, `_mΩ`, …) are unavailable on GCC before 14;
  ASCII spellings are always available
