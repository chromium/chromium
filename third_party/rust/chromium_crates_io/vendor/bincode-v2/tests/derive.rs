#![cfg(feature = "derive")]

use bincode::error::DecodeError;

#[derive(bincode::Encode, PartialEq, Debug)]
pub(crate) struct Test<T> {
    a: T,
    b: u32,
    c: u8,
}

#[test]
fn test_encode() {
    let start = Test {
        a: 5i32,
        b: 10u32,
        c: 20u8,
    };
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 3);
    assert_eq!(&slice[..bytes_written], &[10, 10, 20]);
}
#[derive(PartialEq, Debug, Eq)]
pub struct Test2<T> {
    a: T,
    b: u32,
    c: u32,
}
impl<T, Context> ::bincode::Decode<Context> for Test2<T>
where
    T: ::bincode::Decode<Context>,
{
    fn decode<D: ::bincode::de::Decoder<Context = Context>>(
        decoder: &mut D,
    ) -> core::result::Result<Self, ::bincode::error::DecodeError> {
        Ok(Self {
            a: ::bincode::Decode::decode(decoder)?,
            b: ::bincode::Decode::decode(decoder)?,
            c: ::bincode::Decode::decode(decoder)?,
        })
    }
}
impl<'__de, T, Context> ::bincode::BorrowDecode<'__de, Context> for Test2<T>
where
    T: ::bincode::BorrowDecode<'__de, Context> + '__de,
{
    fn borrow_decode<D: ::bincode::de::BorrowDecoder<'__de, Context = Context>>(
        decoder: &mut D,
    ) -> core::result::Result<Self, ::bincode::error::DecodeError> {
        Ok(Self {
            a: ::bincode::BorrowDecode::borrow_decode(decoder)?,
            b: ::bincode::BorrowDecode::borrow_decode(decoder)?,
            c: ::bincode::BorrowDecode::borrow_decode(decoder)?,
        })
    }
}

#[test]
fn test_decode() {
    let start = Test2 {
        a: 5u32,
        b: 10u32,
        c: 1024u32,
    };
    let slice = [5, 10, 251, 0, 4];
    let (result, len): (Test2<u32>, usize) =
        bincode::decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 5);
}

#[derive(bincode::BorrowDecode, bincode::Encode, PartialEq, Debug, Eq)]
pub struct Test3<'a> {
    a: &'a str,
    b: u32,
    c: u32,
    d: Option<&'a [u8]>,
}

#[test]
fn test_encode_decode_str() {
    let start = Test3 {
        a: "Foo bar",
        b: 10u32,
        c: 1024u32,
        d: Some(b"Foo bar"),
    };
    let mut slice = [0u8; 100];

    let len = bincode::encode_into_slice(&start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(len, 21);
    let (end, len): (Test3, usize) =
        bincode::borrow_decode_from_slice(&slice[..len], bincode::config::standard()).unwrap();
    assert_eq!(end, start);
    assert_eq!(len, 21);
}

#[derive(bincode::Encode, bincode::Decode, PartialEq, Debug, Eq)]
pub struct TestTupleStruct(u32, u32, u32);

#[test]
fn test_encode_tuple() {
    let start = TestTupleStruct(5, 10, 1024);
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 5);
    assert_eq!(&slice[..bytes_written], &[5, 10, 251, 0, 4]);
}

#[test]
fn test_decode_tuple() {
    let start = TestTupleStruct(5, 10, 1024);
    let slice = [5, 10, 251, 0, 4];
    let (result, len): (TestTupleStruct, usize) =
        bincode::decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 5);
}

#[derive(bincode::Encode, bincode::Decode, PartialEq, Debug, Eq)]
pub enum TestEnum {
    Foo,
    Bar { name: u32 },
    Baz(u32, u32, u32),
}
#[test]
fn test_encode_enum_struct_variant() {
    let start = TestEnum::Bar { name: 5u32 };
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 2);
    assert_eq!(&slice[..bytes_written], &[1, 5]);
}

#[test]
fn test_decode_enum_struct_variant() {
    let start = TestEnum::Bar { name: 5u32 };
    let slice = [1, 5];
    let (result, len): (TestEnum, usize) =
        bincode::decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 2);
}

#[test]
fn test_decode_enum_unit_variant() {
    let start = TestEnum::Foo;
    let slice = [0];
    let (result, len): (TestEnum, usize) =
        bincode::decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 1);
}

#[test]
fn test_encode_enum_unit_variant() {
    let start = TestEnum::Foo;
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 1);
    assert_eq!(&slice[..bytes_written], &[0]);
}

#[test]
fn test_encode_enum_tuple_variant() {
    let start = TestEnum::Baz(5, 10, 1024);
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 6);
    assert_eq!(&slice[..bytes_written], &[2, 5, 10, 251, 0, 4]);
}

#[test]
fn test_decode_enum_tuple_variant() {
    let start = TestEnum::Baz(5, 10, 1024);
    let slice = [2, 5, 10, 251, 0, 4];
    let (result, len): (TestEnum, usize) =
        bincode::decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 6);
}

#[derive(bincode::Encode, bincode::BorrowDecode, PartialEq, Debug, Eq)]
pub enum TestEnum2<'a> {
    Foo,
    Bar { name: &'a str },
    Baz(u32, u32, u32),
}

#[test]
fn test_encode_borrowed_enum_struct_variant() {
    let start = TestEnum2::Bar { name: "foo" };
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 5);
    assert_eq!(&slice[..bytes_written], &[1, 3, 102, 111, 111]);
}

#[test]
fn test_decode_borrowed_enum_struct_variant() {
    let start = TestEnum2::Bar { name: "foo" };
    let slice = [1, 3, 102, 111, 111];
    let (result, len): (TestEnum2, usize) =
        bincode::borrow_decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 5);
}

#[test]
fn test_decode_borrowed_enum_unit_variant() {
    let start = TestEnum2::Foo;
    let slice = [0];
    let (result, len): (TestEnum2, usize) =
        bincode::borrow_decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 1);
}

#[test]
fn test_encode_borrowed_enum_unit_variant() {
    let start = TestEnum2::Foo;
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 1);
    assert_eq!(&slice[..bytes_written], &[0]);
}

#[test]
fn test_encode_borrowed_enum_tuple_variant() {
    let start = TestEnum2::Baz(5, 10, 1024);
    let mut slice = [0u8; 1024];
    let bytes_written =
        bincode::encode_into_slice(start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(bytes_written, 6);
    assert_eq!(&slice[..bytes_written], &[2, 5, 10, 251, 0, 4]);
}

#[test]
fn test_decode_borrowed_enum_tuple_variant() {
    let start = TestEnum2::Baz(5, 10, 1024);
    let slice = [2, 5, 10, 251, 0, 4];
    let (result, len): (TestEnum2, usize) =
        bincode::borrow_decode_from_slice(&slice, bincode::config::standard()).unwrap();
    assert_eq!(result, start);
    assert_eq!(len, 6);
}

#[derive(bincode::Decode, bincode::Encode, PartialEq, Eq, Debug)]
enum CStyleEnum {
    A = -1,
    B = 2,
    C,
    D = 5,
    E,
}

#[test]
fn test_c_style_enum() {
    fn ser(e: CStyleEnum) -> u8 {
        let mut slice = [0u8; 10];
        let bytes_written =
            bincode::encode_into_slice(e, &mut slice, bincode::config::standard()).unwrap();
        assert_eq!(bytes_written, 1);
        slice[0]
    }

    assert_eq!(ser(CStyleEnum::A), 0);
    assert_eq!(ser(CStyleEnum::B), 1);
    assert_eq!(ser(CStyleEnum::C), 2);
    assert_eq!(ser(CStyleEnum::D), 3);
    assert_eq!(ser(CStyleEnum::E), 4);

    fn assert_de_successfully(num: u8, expected: CStyleEnum) {
        match bincode::decode_from_slice::<CStyleEnum, _>(&[num], bincode::config::standard()) {
            Ok((result, len)) => {
                assert_eq!(len, 1);
                assert_eq!(result, expected)
            }
            Err(e) => panic!("Could not deserialize CStyleEnum idx {num}: {e:?}"),
        }
    }

    fn assert_de_fails(num: u8) {
        match bincode::decode_from_slice::<CStyleEnum, _>(&[num], bincode::config::standard()) {
            Ok(_) => {
                panic!("Expected to not be able to decode CStyleEnum index {num}, but it succeeded")
            }
            Err(DecodeError::UnexpectedVariant {
                type_name: "CStyleEnum",
                allowed: &bincode::error::AllowedEnumVariants::Allowed(&[0, 1, 2, 3, 4]),
                found,
            }) if found == num as u32 => {}
            Err(e) => panic!("Expected DecodeError::UnexpectedVariant, got {e:?}"),
        }
    }

    assert_de_successfully(0, CStyleEnum::A);
    assert_de_successfully(1, CStyleEnum::B);
    assert_de_successfully(2, CStyleEnum::C);
    assert_de_successfully(3, CStyleEnum::D);
    assert_de_successfully(4, CStyleEnum::E);
    assert_de_fails(5);
}

macro_rules! macro_newtype {
    ($name:ident) => {
        #[derive(bincode::Encode, bincode::Decode, PartialEq, Eq, Debug)]
        pub struct $name(pub usize);
    };
}
macro_newtype!(MacroNewType);

#[test]
fn test_macro_newtype() {
    for val in [0, 100, usize::MAX] {
        let mut usize_slice = [0u8; 10];
        let usize_len =
            bincode::encode_into_slice(val, &mut usize_slice, bincode::config::standard()).unwrap();

        let mut newtype_slice = [0u8; 10];
        let newtype_len = bincode::encode_into_slice(
            MacroNewType(val),
            &mut newtype_slice,
            bincode::config::standard(),
        )
        .unwrap();

        assert_eq!(usize_len, newtype_len);
        assert_eq!(usize_slice, newtype_slice);

        let (newtype, len) = bincode::decode_from_slice::<MacroNewType, _>(
            &newtype_slice,
            bincode::config::standard(),
        )
        .unwrap();
        assert_eq!(newtype, MacroNewType(val));
        assert_eq!(len, newtype_len);
    }
}

#[derive(bincode::Encode, bincode::Decode, Debug)]
pub enum EmptyEnum {}

#[derive(bincode::Encode, bincode::BorrowDecode, Debug)]
pub enum BorrowedEmptyEnum {}

#[test]
fn test_empty_enum_decode() {
    match bincode::decode_from_slice::<EmptyEnum, _>(&[], bincode::config::standard()) {
        Ok(_) => panic!("We successfully decoded an empty slice, this should never happen"),
        Err(DecodeError::EmptyEnum {
            type_name: "derive::EmptyEnum",
        }) => {}
        Err(e) => panic!("Expected DecodeError::EmptyEnum, got {e:?}"),
    }
}

#[derive(bincode::Encode, bincode::Decode, PartialEq, Debug, Eq)]
pub enum TestWithGeneric<T> {
    Foo,
    Bar(T),
}

#[test]
fn test_enum_with_generics_roundtrip() {
    let start = TestWithGeneric::Bar(1234);
    let mut slice = [0u8; 10];
    let bytes_written =
        bincode::encode_into_slice(&start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(
        &slice[..bytes_written],
        &[
            1,   // variant 1
            251, // u16
            210, 4 // 1234
        ]
    );

    let decoded: TestWithGeneric<u32> =
        bincode::decode_from_slice(&slice[..bytes_written], bincode::config::standard())
            .unwrap()
            .0;
    assert_eq!(start, decoded);

    let start = TestWithGeneric::<()>::Foo;
    let mut slice = [0u8; 10];
    let bytes_written =
        bincode::encode_into_slice(&start, &mut slice, bincode::config::standard()).unwrap();
    assert_eq!(&slice[..bytes_written], &[0]);

    let decoded: TestWithGeneric<()> =
        bincode::decode_from_slice(&slice[..bytes_written], bincode::config::standard())
            .unwrap()
            .0;
    assert_eq!(start, decoded);
}

mod derive_with_polluted_scope {
    #[allow(dead_code)]
    #[allow(non_snake_case)]
    fn Ok() {}

    #[allow(dead_code)]
    #[allow(non_snake_case)]
    fn Err() {}

    #[derive(bincode::Encode, bincode::Decode)]
    struct A {
        a: u32,
    }

    #[derive(bincode::Encode, bincode::Decode)]
    enum B {
        A,
        B,
    }
}

#[cfg(feature = "alloc")]
mod zoxide {
    extern crate alloc;

    use alloc::borrow::Cow;
    use bincode::{Decode, Encode};

    pub type Rank = f64;
    pub type Epoch = u64;

    #[derive(Encode, Decode)]
    pub struct Dir<'a> {
        pub path: Cow<'a, str>,
        pub rank: Rank,
        pub last_accessed: Epoch,
    }

    #[test]
    fn test() {
        let dirs = vec![
            Dir {
                path: Cow::Borrowed("Foo"),
                rank: 1.23,
                last_accessed: 5,
            },
            Dir {
                path: Cow::Owned(String::from("Bar")),
                rank: 2.34,
                last_accessed: 10,
            },
        ];
        let config = bincode::config::standard();

        let slice = bincode::encode_to_vec(dirs, config).unwrap();
        let decoded: Vec<Dir> = bincode::borrow_decode_from_slice(&slice, config).unwrap().0;

        assert_eq!(decoded.len(), 2);
        assert!(matches!(decoded[0].path, Cow::Borrowed("Foo")));
        assert!(matches!(decoded[1].path, Cow::Borrowed("Bar")));
    }
}
