use crate::{
    fmt::{FmtArg, NumberFmt},
    utils::{Sign, TailShortString},
};

pub(crate) const fn fmt_decimal<const N: usize>(sign: Sign, mut n: u128) -> TailShortString<N> {
    let mut start = N;
    let mut buffer = [0u8; N];

    loop {
        start -= 1;
        let digit = (n % 10) as u8;
        buffer[start] = b'0' + digit;
        n /= 10;
        if n == 0 {
            break;
        }
    }

    if let Sign::Negative = sign {
        start -= 1;
        buffer[start] = b'-';
    }

    // safety: buffer is only ever written ascii, so its automatically valid utf8.
    unsafe { TailShortString::new(start as u8, buffer) }
}

pub(crate) const fn fmt_binary<const N: usize>(
    mut n: u128,
    is_alternate: bool,
) -> TailShortString<N> {
    let mut start = N;
    let mut buffer = [0u8; N];

    loop {
        start -= 1;
        let digit = (n & 1) as u8;
        buffer[start] = b'0' + digit;
        n >>= 1;
        if n == 0 {
            break;
        }
    }

    if is_alternate {
        start -= 1;
        buffer[start] = b'b';
        start -= 1;
        buffer[start] = b'0';
    }

    // safety: buffer is only ever written ascii, so its automatically valid utf8.
    unsafe { TailShortString::new(start as u8, buffer) }
}

pub(crate) const fn fmt_hexadecimal<const N: usize>(
    mut n: u128,
    is_alternate: bool,
) -> TailShortString<N> {
    let mut start = N;
    let mut buffer = [0u8; N];

    loop {
        start -= 1;
        let digit = (n & 0xF) as u8;
        buffer[start] = match digit {
            0..=9 => b'0' + digit,
            _ => b'A' - 10 + digit,
        };
        n >>= 4;
        if n == 0 {
            break;
        }
    }

    if is_alternate {
        start -= 1;
        buffer[start] = b'x';
        start -= 1;
        buffer[start] = b'0';
    }

    // safety: buffer is only ever written ascii, so its automatically valid utf8.
    unsafe { TailShortString::new(start as u8, buffer) }
}

pub(crate) const fn compute_len(sign: Sign, int: u128, bits: u8, fmt: FmtArg) -> u8 {
    match fmt.number_fmt {
        NumberFmt::Decimal => compute_decimal_len(sign, int),
        NumberFmt::Hexadecimal => {
            let with_0x = (fmt.is_alternate as u8) * 2;
            let i = match sign {
                Sign::Negative => bits,
                Sign::Positive => (128 - int.leading_zeros()) as u8,
            };
            let tmp = if i == 0 {
                1
            } else {
                i / 4 + (i % 4 != 0) as u8
            };
            tmp + with_0x
        }
        NumberFmt::Binary => {
            let with_0b = (fmt.is_alternate as u8) * 2;
            let i = match sign {
                Sign::Negative => bits,
                Sign::Positive => (128 - int.leading_zeros()) as u8,
            };
            (if i == 0 { 1 } else { i }) + with_0b
        }
    }
}

const fn compute_decimal_len(sign: Sign, mut n: u128) -> u8 {
    let mut len = matches!(sign, Sign::Negative) as u8 + 1;
    if n >= 1_0000_0000_0000_0000 {
        n /= 1_0000_0000_0000_0000;
        len += 16;
    }
    if n >= 1_0000_0000_0000 {
        n /= 1_0000_0000_0000;
        len += 12;
    }
    if n >= 1_0000_0000 {
        n /= 100_000_000;
        len += 8;
    }
    if n >= 1_0000 {
        n /= 1_0000;
        len += 4;
    }
    if n >= 100 {
        n /= 100;
        len += 2;
    }
    if n >= 10 {
        len += 1;
    }
    len
}
