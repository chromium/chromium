# bitbybit: Bit fields and bit enums

This crate provides macros that create bit fields and bit enums, which are useful in bit packing code (e.g. in drivers
or networking code).

Some highlights:

- Highly efficient and 100% safe code that is just as good and hand-written shifts and masks,
- Full compatibility with const contexts,
- Useable in no-std environments,
- Strong compile time guarantees (for example, taking 5 bits out of a bitfield and putting them into another won't even
  need to compile a bounds check),
- Automatic creation of bitenums, which allow converting enums to/from numbers,
- Array support within bitfields to represent repeating bit patterns.

## Basic declaration

A bit field is created similar to a regular Rust struct. Annotations define the layout of the structure. As an example,
consider the following definition, which specifies a bit field:

```rs
#[bitfield(u32)]
struct GICD_TYPER {
    #[bits(11..=15, r)]
    lspi: u5,

    #[bit(10, r)]
    security_extn: bool,

    #[bits(5..=7, r)]
    cpu_number: u3,

    #[bits(0..=4, r)]
    itlines_number: u5,
}
```

How this works:

- #[bitfield(u32)] specifies that this is a bitfield in which u32 is the underlying data type. This means that all the
  bits inside of the bitfield
  have to fit within 32 bits. Built-in Rust types (u8, u16, u32, u64, u128) as well as arbitrary-ints (u17, u48 etc) are
  supported.
- Each field is annotated with the range of bits that are used by the field. The data type must match the number of
  bits: A range of 0..=8 with u8 would cause a compile error, as u9 is the data type that matches 0..=8.
- Single bit fields are declared as "bit", all other fields as "bits"
- Valid data types for fields are the basic types u8, u16, u32, u64, u128, bool as well as enums (see below) or types
  like u1, u2, u3 from [arbitrary-int](https://crates.io/crates/arbitrary-int)
- Bit numbering is LSB0, which means that bits are counted from the bottom: bit(0) has a value of 0x1, bit(1) is 0x2,
  bit(2) is 0x4, bit(15) is 0x8000 and so on. An MSB0 mode (to match documentation of some big endian devices) can be
  added later, if there is demand.
- Fields are declared as "r" for read-only, "w" for write-only or "rw" as read/write. In the example above, all fields
  are read-only as this specific register is only used to read values.

## Enumerations

Very often, fields aren't just numbers but really enums. This is supported by first defining a bitenum and then using
that inside of a bitfield:

```rs
#[bitenum(u2, exhaustive = false)]
enum NonExhaustiveEnum {
    Zero = 0b00,
    One = 0b01,
    Two = 0b10,
}

#[bitenum(u2, exhaustive = true)]
enum ExhaustiveEnum {
    Zero = 0b00,
    One = 0b01,
    Two = 0b10,
    Three = 0b11,
}

#[bitfield(u64, default = 0)]
struct BitfieldWithEnum {
    #[bits(2..=3, rw)]
    e2: Option<NonExhaustiveEnum>,

    #[bits(0..=1, rw)]
    e1: ExhaustiveEnum,
}
```

- The bitenum macro turns an enum into an enum that can be used within bitfields. The first argument is the base data
  type, which specifies, how many bits the enum occupies. This can be any value from u1 up to u64
- When an enum is used within a bitfield, the number of bits has to match - if it doesn't, there will be a compiler
  error
- The exhaustive argument specifies whether every possible bit combination is contained within the enum. The example
  above has both an exhaustive and a non-exhaustive enum. Notice how the non-exhaustive enum has to be wrapped in an
  Option to account for the case of e2 not being one of the defined enum values.

## Arrays

Sometimes, bits inside bitfields are repeated. To support this, this crate allows specifying bitwise arrays. For
example, the following struct gives read/write access to each individual nibble (hex character) of a u64:

```rs
#[bitfield(u64, default = 0)]
struct Nibble64 {
     #[bits(0..=3, rw)]
     nibble: [u4; 16],
}
```

Arrays can also have a stride. This is useful in the case of multiple smaller values repeating. For example, the
following definition provides access to each bit of each nibble:

```rs
#[bitfield(u64, default = 0)]
struct NibbleBits64 {
    #[bit(0, rw, stride = 4)]
    nibble_bit0: [bool; 16],

    #[bit(1, rw, stride = 4)]
    nibble_bit1: [bool; 16],

    #[bit(2, rw, stride = 4)]
    nibble_bit2: [bool; 16],

    #[bit(3, rw, stride = 4)]
    nibble_bit3: [bool; 16],
}
```

## Default

Bitfields can always be created through new_with_raw_value():

```rs
let a = NibbleBits64::new_with_raw_value(0x43218765);
```

However, pretty often a specific value can be considered the default (for example 0). This can be specified like this:

```rs
#[bitfield(u32, default = 0x1234)]
struct Bitfield1 {
  #[bits(0..=3, rw)]
  nibble: [u4; 4],
}
```

If a default value is specified, the bitfield can easily be created with this specific value:

```rs
let a = Bitfield1::Default;
const A: Bitfield1 = Bitfield1::DEFAULT;
```

Default values are used as-is, even if they affect bits that aren't defined within the bitfield.

## Setting all fields at once using the builder syntax

It is possible to set all fields at once, like this:

```rs
const T: Test = Test::builder()
    .with_baudrate(0x12)
    .with_some_other_bits(u4::new(0x2))
    .with_array([1, 2, 3, 4])
    .build();
```

Using `builder()` it is impossible to forget setting any fields. This is checked at compile time: If any field is not
set, `build()` can not be called.

At the moment, it is required to set all fields in the same order as they are specified. As Rust's const generics become
more powerful, this restriction might be lifted.

For the `builder()` to be available, the following has to be true:

- The bitfield has to be completely filled with writable fields (no gaps) OR there has to be a default value specified,
- No writable fields overlap.

## Non-contiguous bitranges

Occasionally it can be useful for bitranges to not be contiguous. For example, RISC-V defines some
immediates in a way that they have to be reassembled. This can be achieved like this:

```rs
  #[bitfield(u32)]
  struct SBFormat {
      #[bits([8..=11, 25..=30, 7, 31], rw)]
      imm_half: u12,
  }
```

## Debug

The `bitfield` macro can generate a `Debug` implementation for you which prints
the `Debug` implementation of the inner fields. You can do this using the `debug`
specifier:

```rs
#[bitfield(u32, debug)]
struct GICD_TYPER {
    #[bits(11..=15, r)]
    lspi: u5,

    #[bit(10, r)]
    security_extn: bool,

    #[bits(5..=7, r)]
    cpu_number: u3,

    #[bits(0..=4, r)]
    itlines_number: u5,
}
```

## Introspection

If information like bit offsets or mask values are needed at runtime, use the `introspect` specifier:

```rs
    #[bitfield(u32, introspect)]
    struct Bitfield {
        #[bits(0..=7)]
        a: u8,
        #[bits(8..=15)]
        b: [u8; 2],
        #[bits([24..=25, 30..=31])]
        c: u4,
        #[bit(29)]
        d: bool,
    }

    // <NAME>_BITS exposes the "bits" value(s):
    assert_eq!(Bitfield::A_BITS, 0..=7);
    assert_eq!(Bitfield::B_BITS, 8..=15);
    assert_eq!(Bitfield::C_BITS, [24..=25, 30..=31]);
    assert_eq!(Bitfield::D_BITS, 29..=29);

    // Arrays also have <NAME>_COUNT and <NAME>_STRIDE
    assert_eq!(Bitfield::B_COUNT, 2);
    assert_eq!(Bitfield::B_STRIDE, 8);

    // <name>_mask() returns a mask for the field
    assert_eq!(Bitfield::a_mask(), 0x000000FF);
    assert_eq!(Bitfield::b_mask(0), 0x0000FF00);
    assert_eq!(Bitfield::b_mask(1), 0x00FF0000);
    assert_eq!(Bitfield::c_mask(), 0xC3000000);
    assert_eq!(Bitfield::d_mask(), 0x20000000);
```

This functionality can be enabled for all bitfields by enabling the `introspect` feature.

## `defmt` support

The `bitfield` macro can generate a `defmt::Format` implementation for you.
There are two variants:

- Bitfield variant which used the [bitfield support](https://defmt.ferrous-systems.com/bitfields)
  provided by `defmt` to print the bits of the raw value. Can be activated using the
  `defmt_bitfields` attribute.
- Field variant which simply forwards to the `defmt` implementation of the field getter functions.
  This might not be as bandwidth-efficient as the bitfield variant but might be preferred for
  something like inner `bitenum` fields.

Additionally, both specifiers allow to feature gate the generated code, which is commonly done
in libaries. For example, to put the `defmt::Format` implementation behind a `defmt` feature gate,
you can use `defmt_bitfields(feature = "defmt")` or `defmt_fields(feature = "defmt")`.

Example:

```rs
#[bitfield(u32, defmt_bitfields(feature = "defmt"))]
struct GICD_TYPER {
    #[bits(11..=15, r)]
    lspi: u5,

    #[bit(10, r)]
    security_extn: bool,

    #[bits(5..=7, r)]
    cpu_number: u3,

    #[bits(0..=4, r)]
    itlines_number: u5,
}
```

## Dependencies

Arbitrary bit widths like u5 or u67 do not exist in Rust at the moment. Therefore, the following dependency is required:

```toml
arbitrary-int = "1.3.0"
```

## Usage

Even though bitfields feel somewhat like structs, they are internally implemented as simple data types like u32.
Therefore, they provide an immutable interface: Instead of changing the value of a field, any change operation will
return a new bitfield with that field modified.

```rs
let a = NibbleBits64::new_with_raw_value(0x12345678_ABCDEFFF);
// Read a value
assert_eq!(u4::new(0xE), a.nibble(3));
// Change a value
let b = a.with_nibble(0, u4::new(0x3))
assert_eq!(0x12345678_ABCDEFF3, b.raw_value());
```
