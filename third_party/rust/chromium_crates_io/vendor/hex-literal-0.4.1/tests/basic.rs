use hex_literal::hex;

#[test]
fn single_literal() {
    assert_eq!(hex!("ff e4"), [0xff, 0xe4]);
}

#[test]
fn empty() {
    let nothing: [u8; 0] = hex!();
    let empty_literals: [u8; 0] = hex!("" "" "");
    let expected: [u8; 0] = [];
    assert_eq!(nothing, expected);
    assert_eq!(empty_literals, expected);
}

#[test]
fn upper_case() {
    assert_eq!(hex!("AE DF 04 B2"), [0xae, 0xdf, 0x04, 0xb2]);
    assert_eq!(hex!("FF BA 8C 00 01"), [0xff, 0xba, 0x8c, 0x00, 0x01]);
}

#[test]
fn mixed_case() {
    assert_eq!(hex!("bF dd E4 Cd"), [0xbf, 0xdd, 0xe4, 0xcd]);
}

#[test]
fn multiple_literals() {
    assert_eq!(
        hex!(
            "01 dd f7 7f"
            "ee f0 d8"
        ),
        [0x01, 0xdd, 0xf7, 0x7f, 0xee, 0xf0, 0xd8]
    );
    assert_eq!(
        hex!(
            "ff"
            "e8 d0"
            ""
            "01 1f"
            "ab"
        ),
        [0xff, 0xe8, 0xd0, 0x01, 0x1f, 0xab]
    );
}

#[test]
fn no_spacing() {
    assert_eq!(hex!("abf0d8bb0f14"), [0xab, 0xf0, 0xd8, 0xbb, 0x0f, 0x14]);
    assert_eq!(
        hex!("09FFd890cbcCd1d08F"),
        [0x09, 0xff, 0xd8, 0x90, 0xcb, 0xcc, 0xd1, 0xd0, 0x8f]
    );
}

#[test]
fn allows_various_spacing() {
    // newlines
    assert_eq!(
        hex!(
            "f
            f
            d
            0
            e
            
            8
            "
        ),
        [0xff, 0xd0, 0xe8]
    );
    // tabs
    assert_eq!(hex!("9f	d		1		f07	3		01	"), [0x9f, 0xd1, 0xf0, 0x73, 0x01]);
    // spaces
    assert_eq!(hex!(" e    e d0  9 1   f  f  "), [0xee, 0xd0, 0x91, 0xff]);
}

#[test]
fn can_use_const() {
    const _: [u8; 4] = hex!("ff d3 01 7f");
}
