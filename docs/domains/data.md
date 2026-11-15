# Data sizes and rates: `quanta::data`

Type-safe data sizes and data rates, modeled after `std::chrono::duration`. Include `<quanta/data>`.

## Features

- **Type-safe sizes and rates** with distinct types for different precisions
- **Bits, SI and IEC units**: bit, kbit, Mbit, Gbit, Tbit; B, kB, MB, GB, TB; KiB, MiB, GiB, TiB, all defined as exact ratios of the byte (1 bit = 1/8 B, 1 kB = 1000 B, 1 KiB = 1024 B), so they mix without drift
- **`std::chrono` interop**: `size / duration = rate`, `rate × duration = size` and `size / rate = duration`, with exact result precisions (`100_kbit_per_s * 250ms` is exactly `bits(25000)`)
- **Integer and floating-point representations** for exact or fractional precision
- **Lossless implicit conversions** (lossy conversions require explicit casts)
- **User-defined literals** for concise notation (`1500_B`, `4_KiB`, `100_Mbit_per_s`, `20_MiB_per_s`)
- **`std::format` support** (when available), with opt-in base-unit (`1.5B`) and scaled (`1.5MiB`) display
- **Fully constexpr** - all operations can be evaluated at compile time

## Usage

```cpp
#include <quanta/data>  // or <quanta/data.hpp>

using namespace quanta::data;
using namespace quanta::data_literals;
using namespace std::chrono_literals;

// Basic sizes and rates
kibibytes page{4};
megabytes image{12};
megabits_per_second link{100};

// Using literals
auto packet = 1500_B;
auto buffer = 64_KiB;
auto uplink = 20_Mbit_per_s;

// Precision control
bytes precise{1536};
kibibytes coarse = kibibytes(precise);  // explicit lossy conversion (truncates to 1 KiB)
bytes back = coarse;                    // implicit lossless conversion (1024 B)

// Arithmetic
auto total = 1_kB + 500_B;              // 1500 B
auto scaled = 4_KiB * 3;                // 12 KiB
auto ratio = 1_MiB / 1_KiB;             // 1024 (scalar)

// Comparisons work across precisions and families
if (bytes{1} == bits{8} && kilobits{8} == kilobytes{1}) {
    // true - same size, different precision
}

// Rates and durations
auto throughput = 1500_kB / 500ms;      // megabytes_per_second(3)
auto sent = 100_kbit_per_s * 250ms;     // bits(25000)
auto eta = 1000_MB / 100_MB_per_s;      // std::chrono::seconds(10)

// String conversion
std::string s = to_string(kibibytes(4));  // "4KiB"

// Formatting: the default renders the exact stored value; a format spec
// changes only how the number is rendered, never the unit.
kilobytes reading{1500};
std::string raw  = std::format("{}", reading);      // "1500kB" (exact stored value)
std::string disp = std::format("{:.1f}", reading);  // "1500.0kB"
std::format("{}", 100_Mbit_per_s);                  // "100Mbit/s"
std::format("{:s}", reading);                       // "1.5MB" (scaled)
```

### Bits, SI and IEC

Every precision is an exact `std::ratio` of one byte, so bits, SI-prefixed
bytes and IEC-prefixed bytes compare, add and convert exactly. Where a
conversion is lossless it is implicit; otherwise it needs a cast, and `floor`,
`ceil`, `trunc` and `round` give explicit rounding control:

```cpp
bits b = 2_B;                               // implicit: 16 bit
bits k = 1_KiB;                             // implicit: 8192 bit
bytes n = 3_KiB;                            // implicit: 3072 B
kilobits kb = 1_kB;                         // implicit: 8 kbit
kilobytes mb = 1_Mbit;                      // implicit: 125 kB

bytes whole = bytes(12_bit);                // explicit: 1 B (truncated from 1.5)
kilobytes si = kilobytes(1_KiB);            // explicit: 1 kB (truncated from 1.024)
auto r = round<kibibytes>(1_MB);            // 977 KiB (from 976.5625)
auto c = ceil<bytes>(12_bit);               // 2 B

auto sum = 1_kB + 1_KiB;                    // exact, in units of 8 B (253)
size<double, std::kilo> exact = 1_KiB;      // 1.024 kB
```

Mixing SI and IEC precisions gives a common type with the greatest precision
that divides both (8 B for kB and KiB, 64 B for MB and MiB); like any
precision without a symbol, it has no `{}` or `to_string(q)` form until
converted to a named one, but the base-unit and scaled forms show it (see
Formatting below).

### std::chrono Interop

Sizes and rates combine with `std::chrono::duration` (and only with it: types
that merely look like a duration are rejected):

| Operation | Result |
|-----------|--------|
| size ÷ duration | rate |
| rate × duration, duration × rate | size |
| size ÷ rate | `std::chrono::duration` |

Dividing two quantities of the same unit still yields a scalar
(`1_MiB / 1_KiB == 1024`). Any other combination, such as size × duration,
rate ÷ duration, size × rate, rate ÷ size or size + rate, does not compile.

The result precision is the product (or quotient) of the operand precision and
the duration's period (or the other operand's precision), and the
representation is their common type, so products are exact:

```cpp
static_assert(std::is_same_v<decltype(1500_kB / 500ms), megabytes_per_second>);
static_assert(1500_kB / 500ms == 3_MB_per_s);    // kilo / milli = mega
static_assert(100_kbit_per_s * 250ms == 25_kbit); // in bits: 125 B x 1/1000 = 1/8 B
static_assert(5_Mbit_per_s * 2h == 4500_MB);
static_assert(1_GiB / 1_MiB_per_s == 1024s);

auto t = 700_MiB / 100_Mbit_per_s;                        // 7 in units of 8.388608 s
auto secs = std::chrono::duration_cast<std::chrono::seconds>(t);  // 58 s
```

Quotients divide the counts, so with integer representations they truncate in
the result precision, which may be coarse when the operand precisions differ:
`1_MB / 2s` has MB/s precision and yields `0 MB/s`, and `1_GB / 100_MB_per_s`
has a precision of 1000 s and yields `0`. Convert the operands first
(`kilobytes(1_MB) / 2s` is `500 kB/s`), or use floating-point representations
(`size<double, std::mega>(1) / 2s` is `0.5 MB/s`), for fractional results.

## Size Types

All standard aliases use `int64_t` representation. The alias template
`size<Rep, Precision>` covers custom representations and precisions, e.g.
`size<double>` or `size<float, std::ratio<1024>>`.

| Type | Unit | Precision |
|------|------|-----------|
| `bits` | bit | 1/8 B |
| `kilobits` | kbit | 1000 bit = 125 B |
| `megabits` | Mbit | 10⁶ bit = 125,000 B |
| `gigabits` | Gbit | 10⁹ bit = 125,000,000 B |
| `terabits` | Tbit | 10¹² bit = 125,000,000,000 B |
| `bytes` | B | 1 B |
| `kilobytes` | kB | 1000 B |
| `megabytes` | MB | 10⁶ B |
| `gigabytes` | GB | 10⁹ B |
| `terabytes` | TB | 10¹² B |
| `kibibytes` | KiB | 1024 B |
| `mebibytes` | MiB | 1024² B = 1,048,576 B |
| `gibibytes` | GiB | 1024³ B = 1,073,741,824 B |
| `tebibytes` | TiB | 1024⁴ B = 1,099,511,627,776 B |

## Rate Types

Rates are quantities of `byte_per_second_unit`, with the alias template
`rate<Rep, Precision>` whose precision is a `std::ratio` of one byte per
second, the same ratios as the sizes. They support the same arithmetic,
conversions, rounding and comparisons as sizes, within their own unit.

| Type | Unit | Precision |
|------|------|-----------|
| `bits_per_second` | bit/s | 1/8 B/s |
| `kilobits_per_second` | kbit/s | 125 B/s |
| `megabits_per_second` | Mbit/s | 125,000 B/s |
| `gigabits_per_second` | Gbit/s | 125,000,000 B/s |
| `terabits_per_second` | Tbit/s | 125,000,000,000 B/s |
| `bytes_per_second` | B/s | 1 B/s |
| `kilobytes_per_second` | kB/s | 1000 B/s |
| `megabytes_per_second` | MB/s | 10⁶ B/s |
| `gigabytes_per_second` | GB/s | 10⁹ B/s |
| `terabytes_per_second` | TB/s | 10¹² B/s |
| `kibibytes_per_second` | KiB/s | 1024 B/s |
| `mebibytes_per_second` | MiB/s | 1024² B/s |
| `gibibytes_per_second` | GiB/s | 1024³ B/s |
| `tebibytes_per_second` | TiB/s | 1024⁴ B/s |

## Literals

```cpp
using namespace quanta::data_literals;

12_bit     // bits(12)
64_kbit    // kilobits(64)
8_Mbit     // megabits(8)
2_Gbit     // gigabits(2)
3_Tbit     // terabits(3)
1500_B     // bytes(1500)
64_kB      // kilobytes(64)
12_MB      // megabytes(12)
4_GB       // gigabytes(4)
2_TB       // terabytes(2)
4_KiB      // kibibytes(4)
16_MiB     // mebibytes(16)
8_GiB      // gibibytes(8)
1_TiB      // tebibytes(1)
```

| Rate literal | Type | | Rate literal | Type |
|--------------|------|-|--------------|------|
| `9600_bit_per_s` | `bits_per_second` | | `100_MB_per_s` | `megabytes_per_second` |
| `128_kbit_per_s` | `kilobits_per_second` | | `2_GB_per_s` | `gigabytes_per_second` |
| `100_Mbit_per_s` | `megabits_per_second` | | `1_TB_per_s` | `terabytes_per_second` |
| `10_Gbit_per_s` | `gigabits_per_second` | | `64_KiB_per_s` | `kibibytes_per_second` |
| `1_Tbit_per_s` | `terabits_per_second` | | `20_MiB_per_s` | `mebibytes_per_second` |
| `512_B_per_s` | `bytes_per_second` | | `1_GiB_per_s` | `gibibytes_per_second` |
| `250_kB_per_s` | `kilobytes_per_second` | | `1_TiB_per_s` | `tebibytes_per_second` |

There are deliberately no `kbps`- or `Bps`-style literals, which are ambiguous
between bits and bytes: the unit is always spelled out (`bit` or `B`).

Literals are integral and checked for overflow at compile time; use a
floating-point type such as `size<double>` or `rate<double, std::mega>` for
fractional values.

## Conversions

Following `std::chrono` semantics:

- **Implicit** conversions are allowed when lossless (finer target precision,
  or floating-point target representation)
- **Explicit** conversions (constructor or `quantity_cast`) are required when
  the conversion may lose precision
- `floor`, `ceil`, `trunc` and `round` provide explicit rounding control

```cpp
bytes b = kibibytes(5);                           // implicit: 5120 B
bits t = bytes(3);                                // implicit: 24 bit
kibibytes k = kibibytes(bytes(5000));             // explicit: truncates to 4 KiB
bytes n = bytes(size<double>(1.5));               // explicit: truncates to 1 B
auto r = round<kilobytes>(kibibytes(3));          // 3 kB (from 3.072)
auto m = quantity_cast<mebibytes>(gigabytes(1));  // 953 MiB
```

## Formatting

By default, `to_string` and `std::format` show a quantity in its **own unit**,
whatever the format spec. `{}` renders the exact stored count with the
precision's symbol (`bit`, `kbit`, `Mbit`, `Gbit`, `Tbit`, `B`, `kB`, `MB`,
`GB`, `TB`, `KiB`, `MiB`, `GiB`, `TiB`), followed by `/s` for rates. A non-empty spec such as
`{:.1f}` changes only the number formatting: the count is rendered as a
`double` with the spec and followed by the same suffix as `{}`. A different
unit is shown only when you opt in with a unit marker (see below), or by
converting the value first.

```cpp
std::format("{}", 12_bit);                             // "12bit"
std::format("{}", 4_KiB);                              // "4KiB"
std::format("{:.1f}", 1500_kB);                        // "1500.0kB"
std::format("{:8.1f}", 1500_kB);                       // "  1500.0kB"
std::format("{}", 100_Mbit_per_s);                     // "100Mbit/s"
std::format("{:.2f}", rate<double, std::ratio<1024>>(1.5)); // "1.50KiB/s"
std::format("{}", size<double, std::kilo>(2.5));       // "2.5kB"
```

### Unit markers

A format spec may begin with one marker letter that opts into a different
unit; the rest of the spec is then handled exactly as above. The same choices
are available from `to_string` through tag arguments, which work with or
without `std::format` support.

| Marker | `to_string` tag | Shows | Examples |
|--------|-----------------|-------|----------|
| `b` | `quanta::data::base_unit` | the value in the base unit: B or B/s | `{:b}` on `12_bit` → `1.5B`; `{:b}` on `2_KiB` → `2048B`; `{:b.1f}` on `3_kbit_per_s` → `375.0B/s` |
| `s` | `quanta::data::scaled` | the value in a unit of its family suited to its magnitude | `{:s}` on `1500_kB` → `1.5MB`; `{:s}` on `1536_KiB` → `1.5MiB`; `{:s}` on `1500_kbit_per_s` → `1.5Mbit/s` |

There is no power form, so `p` is not a marker.

Wherever a floating-point value is rendered without a spec (`{:b}` and `{:s}`
with nothing after the marker, `{}` on floating-point quantities, and every
`to_string` form), it is written in the shortest **fixed** notation that
round-trips: never scientific, and without padding zeros (`2.5`, `1000000`,
`0.125`). `to_string` and `std::format` therefore produce exactly the same
text: `to_string(q, quanta::data::base_unit)` matches `{:b}`,
`to_string(q, quanta::data::scaled)` matches `{:s}`, and `to_string(q)`
matches `{}`. An explicit spec such as `{:b.3e}` is applied as given.

The scaled form rescales a value **within its own family**, determined by its
precision, so a size in bytes is never shown in bits or the reverse, nor an SI
size in IEC units:

| Precision | Family | Units |
|-----------|--------|-------|
| bit, kbit, Mbit, Gbit, Tbit | bits | `bit`, `kbit`, `Mbit`, `Gbit`, `Tbit` |
| B, kB, MB, GB, TB | SI bytes | `B`, `kB`, `MB`, `GB`, `TB` |
| KiB, MiB, GiB, TiB | IEC bytes | `B`, `KiB`, `MiB`, `GiB`, `TiB` |

Rates use the same units per second. It chooses the largest unit of the family
in which the magnitude is at least 1, or the smallest if there is none, and
shows zero and non-finite values (infinity, NaN) in the family's smallest
unit (`bit` or `bit/s` for the bit family, `B` or `B/s` otherwise). Other precisions with a symbol (only those given one by
specializing `precision_symbol`, see below) are shown in their own unit, with
the count as a `double`; precisions with no symbol are scaled in SI bytes (see
below).

```cpp
std::format("{:s}", 1500_kB);                // "1.5MB"
std::format("{:s}", 2048_B);                 // "2.048kB" (B is SI)
std::format("{:s}", 1536_KiB);               // "1.5MiB"
std::format("{:s}", 1023_KiB);               // "1023KiB"
std::format("{:s}", 4096_GiB);               // "4TiB"
std::format("{:s}", 1500_bit);               // "1.5kbit"
std::format("{:s}", 1500_kbit_per_s);        // "1.5Mbit/s"
std::format("{:s}", 9600_bit_per_s);         // "9.6kbit/s"
std::format("{:s}", 0_kbit);                 // "0bit"
std::format("{:s.2f}", 1500_kB);             // "1.50MB"
std::format("{:s<6.1f}", 1536_KiB);          // "1.5   MiB"
std::format("{:b}", 1_bit);                  // "0.125B"
std::format("{:b}", 1_GiB);                  // "1073741824B"
to_string(1500_kB, quanta::data::scaled);    // "1.5MB"
to_string(12_bit, quanta::data::base_unit);  // "1.5B"
```

A spec that starts with `b` or `s` is always a marker, so these letters cannot
be used as the fill character at the start of a spec: `{:s>8.1f}` is the
marker followed by `>8.1f`, not right-alignment filled with `s`. Only one
marker is allowed; a second one is passed on as part of the numeric spec and
is a format error.

A precision with no symbol (such as the 8 B common type of `kilobytes` and
`kibibytes`, the 8 B/s common type of their rates, or the 450,000,000 B of
`5_Mbit_per_s * 2h`) has no unit of its own: `{}` and `to_string(q)` do not
compile for it, and a spec without a marker renders the value in B or B/s. The
`b` and `s` markers and tags work for it. Such a precision belongs to no
family, so `s` scales it in SI bytes (`B`, `kB`, `MB`, `GB`, `TB`, or the
`/s` forms), as for bytes, whichever precisions it came from:

```cpp
std::format("{:b}", 1_kB + 1_KiB);           // "2024B"
std::format("{:.2f}", 1_kB + 1_KiB);         // "2024.00B"
std::format("{:s}", 1_kB + 1_KiB);           // "2.024kB"
std::format("{:s}", 5_Mbit_per_s * 2h);      // "4.5GB"
std::format("{:s}", 1_kB_per_s + 1_KiB_per_s); // "2.024kB/s"
to_string(1_kB + 1_KiB, quanta::data::scaled); // "2.024kB"
```

There is no SI-prefix rule, so `size<int64_t, std::milli>` is not "mB". To give
another precision a symbol, specialize `quanta::data::precision_symbol` (it
applies to both sizes and rates):

```cpp
template<>
struct quanta::data::precision_symbol<std::peta> {
    static constexpr const char* symbol = "PB";
};

to_string(size<int64_t, std::peta>(3));          // "3PB"
to_string(rate<int64_t, std::peta>(3));          // "3PB/s"
to_string(size<int64_t, std::peta>(3), scaled);  // "3PB" (in no family)
```
