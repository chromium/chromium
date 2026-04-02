#![cfg(all(feature = "serde", feature = "alloc", feature = "derive"))]

extern crate alloc;

use serde_derive::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, bincode::Encode, bincode::Decode)]
pub struct SerdeRoundtrip {
    pub a: u32,
    #[serde(skip)]
    pub b: u32,
    pub c: TupleS,
}

#[derive(Serialize, Deserialize, bincode::Encode, bincode::Decode, PartialEq, Debug)]
pub struct TupleS(f32, f32, f32);

#[test]
fn test_serde_round_trip() {
    // validate serde attribute working
    let json = serde_json::to_string(&SerdeRoundtrip {
        a: 5,
        b: 5,
        c: TupleS(2.0, 3.0, 4.0),
    })
    .unwrap();
    assert_eq!("{\"a\":5,\"c\":[2.0,3.0,4.0]}", json);

    let result: SerdeRoundtrip = serde_json::from_str(&json).unwrap();
    assert_eq!(result.a, 5);
    assert_eq!(result.b, 0);

    // validate bincode working
    let bytes = bincode::serde::encode_to_vec(
        SerdeRoundtrip {
            a: 15,
            b: 15,
            c: TupleS(2.0, 3.0, 4.0),
        },
        bincode::config::standard(),
    )
    .unwrap();
    assert_eq!(bytes, &[15, 0, 0, 0, 64, 0, 0, 64, 64, 0, 0, 128, 64]);
    let (result, len): (SerdeRoundtrip, usize) =
        bincode::serde::decode_from_slice(&bytes, bincode::config::standard()).unwrap();
    assert_eq!(result.a, 15);
    assert_eq!(result.b, 0); // remember: b is skipped
    assert_eq!(result.c, TupleS(2.0, 3.0, 4.0));
    assert_eq!(len, 13);
}

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
pub struct SerdeWithBorrowedData<'a> {
    pub a: u32,
    #[serde(skip)]
    pub b: u32,
    pub str: &'a str,
}

#[test]
fn test_serialize_deserialize_borrowed_data() {
    let input = SerdeWithBorrowedData {
        a: 5,
        b: 5,
        str: "Hello world",
    };

    #[rustfmt::skip]
    let expected = &[
        5, // a
        // b is skipped
        11, // str length
        b'H', b'e', b'l', b'l', b'o', b' ', b'w', b'o', b'r', b'l', b'd' // str
    ];

    let mut result = [0u8; 20];
    let len = bincode::serde::encode_into_slice(&input, &mut result, bincode::config::standard())
        .unwrap();
    let result = &result[..len];
    assert_eq!(result, expected);

    let result = bincode::serde::encode_to_vec(&input, bincode::config::standard()).unwrap();

    assert_eq!(result, expected);

    let (output, len): (SerdeWithBorrowedData, usize) =
        bincode::serde::borrow_decode_from_slice(&result, bincode::config::standard()).unwrap();
    assert_eq!(
        SerdeWithBorrowedData {
            b: 0, // remember: b is skipped
            ..input
        },
        output
    );
    assert_eq!(len, 13);
}

#[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
pub struct SerdeWithOwnedData {
    pub a: u32,
    #[serde(skip)]
    pub b: u32,
    pub str: String,
}

#[test]
fn test_serialize_deserialize_owned_data() {
    let input = SerdeWithOwnedData {
        a: 5,
        b: 5,
        str: String::from("Hello world"),
    };

    #[rustfmt::skip]
    let expected = &[
        5, // a
        // b is skipped
        11, // str length
        b'H', b'e', b'l', b'l', b'o', b' ', b'w', b'o', b'r', b'l', b'd' // str
    ];

    let mut result = [0u8; 20];
    let len = bincode::serde::encode_into_slice(&input, &mut result, bincode::config::standard())
        .unwrap();
    let result = &result[..len];
    assert_eq!(result, expected);

    let result = bincode::serde::encode_to_vec(&input, bincode::config::standard()).unwrap();

    assert_eq!(result, expected);

    let (output, len): (SerdeWithOwnedData, usize) =
        bincode::serde::decode_from_slice(&result, bincode::config::standard()).unwrap();
    assert_eq!(
        SerdeWithOwnedData {
            b: 0, // remember: b is skipped
            ..input
        },
        output
    );
    assert_eq!(len, 13);
}

#[cfg(feature = "derive")]
mod derive {
    use bincode::{
        serde::{BorrowCompat, Compat},
        Decode, Encode,
    };
    use serde_derive::{Deserialize, Serialize};

    #[derive(Serialize, Deserialize, PartialEq, Eq, Debug)]
    pub struct SerdeType {
        pub a: u32,
    }

    #[derive(Decode, Encode, PartialEq, Eq, Debug)]
    pub struct StructWithSerde {
        #[bincode(with_serde)]
        pub serde: SerdeType,
    }

    #[derive(Decode, Encode, PartialEq, Eq, Debug)]
    pub enum EnumWithSerde {
        Unit(#[bincode(with_serde)] SerdeType),
        Struct {
            #[bincode(with_serde)]
            serde: SerdeType,
        },
    }

    #[test]
    fn test_serde_derive() {
        fn test_encode_decode<T>(start: T, expected_len: usize)
        where
            T: bincode::Encode + bincode::Decode<()> + PartialEq + core::fmt::Debug,
        {
            let mut slice = [0u8; 100];
            let len = bincode::encode_into_slice(&start, &mut slice, bincode::config::standard())
                .unwrap();
            assert_eq!(len, expected_len);
            let slice = &slice[..len];
            let (result, len): (T, usize) =
                bincode::decode_from_slice(slice, bincode::config::standard()).unwrap();

            assert_eq!(start, result);
            assert_eq!(len, expected_len);
        }
        test_encode_decode(
            StructWithSerde {
                serde: SerdeType { a: 5 },
            },
            1,
        );
        test_encode_decode(EnumWithSerde::Unit(SerdeType { a: 5 }), 2);
        test_encode_decode(
            EnumWithSerde::Struct {
                serde: SerdeType { a: 5 },
            },
            2,
        );
    }

    #[test]
    fn test_vec_compat_debug() {
        let compat = Compat(vec![0, 1, 2, 3]);
        let debug_view = format!("{:?}", compat);
        assert_eq!(debug_view, "Compat([0, 1, 2, 3])")
    }

    #[test]
    fn test_i32_compat_debug() {
        let compat = Compat(1337_i32);
        let debug_view = format!("{:?}", compat);
        assert_eq!(debug_view, "Compat(1337)")
    }

    #[test]
    fn test_i32_compat_display() {
        let compat = Compat(1337_i32);
        let debug_view = format!("{}", compat);
        assert_eq!(debug_view, "1337")
    }

    #[test]
    fn test_f32_compat_display() {
        let compat = Compat(1.5_f32);
        let debug_view = format!("{}", compat);
        assert_eq!(debug_view, "1.5")
    }

    #[test]
    fn test_vec_borrow_compat_debug() {
        let vector = vec![0, 1, 2, 3];
        let borrow_compat = BorrowCompat(&vector);
        let debug_view = format!("{:?}", borrow_compat);
        assert_eq!(debug_view, "BorrowCompat([0, 1, 2, 3])")
    }

    #[test]
    fn test_str_borrow_compat_debug() {
        let borrow_compat = BorrowCompat("Hello World!");
        let debug_view = format!("{:?}", borrow_compat);
        assert_eq!(debug_view, "BorrowCompat(\"Hello World!\")")
    }

    #[test]
    fn test_str_borrow_compat_display() {
        let borrow_compat = BorrowCompat("Hello World!");
        let debug_view = format!("{}", borrow_compat);
        assert_eq!(debug_view, "Hello World!")
    }
}
