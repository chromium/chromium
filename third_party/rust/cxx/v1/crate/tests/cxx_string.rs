#![allow(
    clippy::items_after_statements,
    clippy::uninlined_format_args,
    clippy::unused_async
)]

use cxx::{let_cxx_string, CxxString};
use std::fmt::Write as _;

#[test]
fn test_async_cxx_string() {
    async fn f() {
        let_cxx_string!(s = "...");

        async fn g(_: &CxxString) {}
        g(&s).await;
    }

    // https://github.com/dtolnay/cxx/issues/693
    fn assert_send(_: impl Send) {}
    assert_send(f());
}

#[test]
fn test_debug() {
    let_cxx_string!(s = "x\"y\'z");

    assert_eq!(format!("{:?}", s), r#""x\"y'z""#);
}

#[test]
fn test_fmt_write() {
    let_cxx_string!(s = "");

    let name = "world";
    write!(s, "Hello, {name}!").unwrap();
    assert_eq!(s.to_str(), Ok("Hello, world!"));
}

#[test]
fn test_io_write() {
    let_cxx_string!(s = "");
    let mut reader: &[u8] = b"Hello, world!";

    std::io::copy(&mut reader, &mut s).unwrap();
    assert_eq!(s.to_str(), Ok("Hello, world!"));
}
