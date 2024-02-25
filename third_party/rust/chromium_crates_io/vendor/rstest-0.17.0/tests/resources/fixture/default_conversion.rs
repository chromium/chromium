use rstest::{fixture, rstest};
use std::net::{Ipv4Addr, SocketAddr};

struct MyType(String);
struct E;
impl core::str::FromStr for MyType {
    type Err = E;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "error" => Err(E),
            inner => Ok(MyType(inner.to_owned())),
        }
    }
}

#[fixture]
fn base(#[default("1.2.3.4")] ip: Ipv4Addr, #[default(r#"8080"#)] port: u16) -> SocketAddr {
    SocketAddr::new(ip.into(), port)
}

#[fixture]
fn fail(#[default("error")] t: MyType) -> MyType {
    t
}

#[fixture]
fn valid(#[default("some")] t: MyType) -> MyType {
    t
}

#[rstest]
fn test_base(base: SocketAddr) {
    assert_eq!(base, "1.2.3.4:8080".parse().unwrap());
}

#[fixture]
fn byte_array(#[default(b"1234")] some: &[u8]) -> usize {
    some.len()
}

#[rstest]
fn test_byte_array(byte_array: usize) {
    assert_eq!(4, byte_array);
}

#[rstest]
fn test_convert_custom(valid: MyType) {}

#[rstest]
fn test_fail_conversion(fail: MyType) {}
