use super::parse_usize;

use core::fmt::Write;

type UsizeFmtBuffer = arrayvec::ArrayString<{ (usize::BITS as usize) / 2 }>;

fn format_usize(n: usize) -> UsizeFmtBuffer {
    let mut s = UsizeFmtBuffer::new();
    write!(s, "{}", n).unwrap();
    s
}

#[track_caller]
fn assert_parses_ok(s: &str) {
    let parsed = parse_usize(s);
    assert!(parsed.is_some(), "s = {:?}  parsed = {:?}", s, parsed);

    assert_eq!(parsed, s.parse::<usize>().ok(), "s = {:?}", s);
}

#[test]
fn from_literals_ok_parsing_test() {
    assert_parses_ok("0");
    assert_parses_ok("1");
    assert_parses_ok("6");
    assert_parses_ok("9");
    assert_parses_ok("09");
    assert_parses_ok("000012");
    assert_parses_ok("10");
    assert_parses_ok("16");
    assert_parses_ok("99");
    assert_parses_ok("100");
    assert_parses_ok("101");
    assert_parses_ok("10003");
    assert_parses_ok("12345");
    assert_parses_ok("54321");
}

#[track_caller]
fn assert_err(s: &str) {
    let parsed = parse_usize(s);
    assert!(parsed.is_none(), "s = {:?}  parsed = {:?}", s, parsed);

    assert_eq!(parsed, s.parse::<usize>().ok(), "s = {:?}", s);
}

#[test]
fn err_parsing_test() {
    {
        let mut too_large = UsizeFmtBuffer::new();
        if usize::BITS < 128 {
            write!(too_large, "{}", u128::try_from(usize::MAX).unwrap() + 1).unwrap();
        } else if usize::BITS == 128 {
            // u128::MAX + 1
            too_large.push_str("340282366920938463463374607431768211456");
        } else {
            panic!("usizes are too large")
        }
        assert_err(&too_large);
    }

    {
        let mut too_large = format_usize(usize::MAX);
        too_large.push('0');
        assert_err(&too_large);
    }

    assert_err("");
    assert_err("_");
    assert_err(" ");
    assert_err("1A4");
    assert_err("0x");
    assert_err("0x9");
    assert_err("_100003");
    assert_err("100_003");
    assert_err("100003_");
}

#[test]
fn first_and_last_integers_test() {
    let mid = isize::MAX as usize;

    for n in (0..=200)
        .chain((mid - 2)..=(mid + 2))
        .chain((usize::MAX - 2)..=usize::MAX)
    {
        let s = format_usize(n);
        assert_parses_ok(&s);
    }
}
