#![allow(clippy::disallowed_names)]
#![cfg(feature = "std")]

mod utils;

use bincode::error::DecodeError;
use std::{
    ffi::CString,
    io::{Cursor, Seek, SeekFrom},
    net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6},
    path::{Path, PathBuf},
    sync::{Mutex, RwLock},
};
use utils::the_same;

use crate::utils::the_same_with_comparer;

struct Foo {
    pub a: u32,
    pub b: u32,
}

impl bincode::Encode for Foo {
    fn encode<E: bincode::enc::Encoder>(
        &self,
        encoder: &mut E,
    ) -> Result<(), bincode::error::EncodeError> {
        self.a.encode(encoder)?;
        self.b.encode(encoder)?;
        Ok(())
    }
}

impl<Context> bincode::Decode<Context> for Foo {
    fn decode<D: bincode::de::Decoder>(
        decoder: &mut D,
    ) -> Result<Self, bincode::error::DecodeError> {
        Ok(Self {
            a: bincode::Decode::decode(decoder)?,
            b: bincode::Decode::decode(decoder)?,
        })
    }
}

#[test]
fn test_std_cursor() {
    let mut cursor = Cursor::<&[u8]>::new(&[5, 10]);
    let foo: Foo = bincode::decode_from_std_read(&mut cursor, bincode::config::standard()).unwrap();

    assert_eq!(foo.a, 5);
    assert_eq!(foo.b, 10);
}

#[test]
#[cfg_attr(miri, ignore)] // miri does not like `tempfile`
fn test_std_file() {
    let mut file = tempfile::tempfile().expect("Could not create temp file");

    let bytes_written = bincode::encode_into_std_write(
        Foo { a: 30, b: 50 },
        &mut file,
        bincode::config::standard(),
    )
    .unwrap();
    assert_eq!(bytes_written, 2);
    file.seek(SeekFrom::Start(0)).unwrap();

    let foo: Foo = bincode::decode_from_std_read(&mut file, bincode::config::standard()).unwrap();

    assert_eq!(foo.a, 30);
    assert_eq!(foo.b, 50);
}

#[test]
fn test_std_commons() {
    the_same(CString::new("Hello world").unwrap());
    the_same(PathBuf::from("C:/Program Files/Foo"));
    the_same(Ipv4Addr::LOCALHOST);
    the_same(Ipv6Addr::LOCALHOST);
    the_same(IpAddr::V4(Ipv4Addr::LOCALHOST));
    the_same(IpAddr::V6(Ipv6Addr::LOCALHOST));
    the_same(SocketAddrV4::new(Ipv4Addr::LOCALHOST, 12345));
    the_same(SocketAddrV6::new(Ipv6Addr::LOCALHOST, 12345, 0, 0));
    the_same(SocketAddr::V4(SocketAddrV4::new(
        Ipv4Addr::LOCALHOST,
        12345,
    )));
    the_same(SocketAddr::V6(SocketAddrV6::new(
        Ipv6Addr::LOCALHOST,
        12345,
        0,
        0,
    )));
    the_same_with_comparer(Mutex::new("Hello world".to_string()), |a, b| {
        *a.lock().unwrap() == *b.lock().unwrap()
    });
    the_same_with_comparer(RwLock::new("Hello world".to_string()), |a, b| {
        *a.read().unwrap() == *b.read().unwrap()
    });

    let mut map = std::collections::HashMap::new();
    map.insert("Hello".to_owned(), "world".to_owned());
    map.insert("How".to_owned(), "are".to_owned());
    map.insert("you".to_owned(), "doing?".to_owned());
    the_same(map);

    let mut set = std::collections::HashSet::new();
    set.insert("Hello".to_string());
    set.insert("World".to_string());
    the_same(set);

    // HashMap and HashSet with custom hash algorithm
    type MyBuildHasher = std::hash::BuildHasherDefault<ExampleCustomHasher>;
    let mut custom_map: std::collections::HashMap<String, String, MyBuildHasher> =
        Default::default();
    custom_map.insert("Hello".to_owned(), "world".to_owned());
    custom_map.insert("How".to_owned(), "are".to_owned());
    custom_map.insert("you".to_owned(), "doing?".to_owned());
    the_same(custom_map);

    let mut custom_set: std::collections::HashSet<String, MyBuildHasher> = Default::default();
    custom_set.insert("Hello".to_string());
    custom_set.insert("World".to_string());
    the_same(custom_set);

    // Borrowed values
    let config = bincode::config::standard();
    let mut buffer = [0u8; 1024];

    // &CStr
    let cstr = c"Hello world";
    let len = bincode::encode_into_slice(cstr, &mut buffer, config).unwrap();
    let (decoded, len): (CString, usize) =
        bincode::decode_from_slice(&buffer[..len], config).unwrap();
    assert_eq!(cstr, decoded.as_c_str());
    assert_eq!(len, 12);

    // Path
    let path = Path::new("C:/Program Files/Foo");
    let len = bincode::encode_into_slice(path, &mut buffer, config).unwrap();
    let (decoded, len): (&Path, usize) =
        bincode::borrow_decode_from_slice(&buffer[..len], config).unwrap();
    assert_eq!(path, decoded);
    assert_eq!(len, 21);
}

#[test]
fn test_system_time_out_of_range() {
    let input = [0xfd, 0x90, 0x0c, 0xfd, 0xfd, 0x90, 0x0c, 0xfd, 0x90, 0x90];

    let result: Result<(std::time::SystemTime, usize), _> =
        bincode::decode_from_slice(&input, bincode::config::standard());

    match result {
        Ok(_) => panic!("Expected the decode to fail, but it succeeded"),
        Err(DecodeError::InvalidSystemTime { duration }) => {
            assert_eq!(
                duration,
                std::time::Duration::new(10447520527445462160, 144)
            )
        }
        Err(e) => panic!("Expected DecodeError::InvalidSystemTime, got {e:?}"),
    }
}

/// Simple example of user-defined hasher to test encoding/decoding HashMap and HashSet with custom hash algorithms.
#[derive(Copy, Clone, Default)]
pub struct ExampleCustomHasher {
    pub hash: u64,
}

impl std::hash::Hasher for ExampleCustomHasher {
    fn write(&mut self, value: &[u8]) {
        for (index, &item) in value.iter().enumerate() {
            self.hash ^= u64::from(item) << ((index % 8) * 8);
        }
    }

    fn finish(&self) -> u64 {
        self.hash
    }
}

#[test]
fn test_decode_borrow_str_in_array() {
    let (strs, _): (Vec<&str>, usize) = bincode::borrow_decode_from_slice(
        &[
            3, 3, b'a', b'b', b'c', 3, b'd', b'e', b'f', 3, b'g', b'h', b'i',
        ],
        bincode::config::standard(),
    )
    .unwrap();
    assert_eq!(strs, vec!["abc", "def", "ghi"]);

    let (strs, _): ([&str; 3], usize) = bincode::borrow_decode_from_slice(
        &[
            3, b'a', b'b', b'c', 3, b'd', b'e', b'f', 3, b'g', b'h', b'i',
        ],
        bincode::config::standard(),
    )
    .unwrap();
    assert_eq!(strs, ["abc", "def", "ghi"]);
}
