use serde_json::Value;

fn assert_invalid_unicode_at(input: &[u8], column: usize) {
    let err = serde_json::from_slice::<Value>(input).unwrap_err();
    assert_eq!(
        err.to_string(),
        format!("invalid unicode code point at line 1 column {column}"),
        "from_slice input={input:?}",
    );
    assert_eq!(err.column(), column);

    #[cfg(feature = "std")]
    {
        let err = serde_json::from_reader::<_, Value>(std::io::Cursor::new(input)).unwrap_err();
        assert_eq!(
            err.to_string(),
            format!("invalid unicode code point at line 1 column {column}"),
            "from_reader input={input:?}",
        );
        assert_eq!(err.column(), column);
    }
}

// https://github.com/serde-rs/json/issues/1083
#[test]
fn invalid_utf8_after_escape_reports_the_bad_byte() {
    // " \" \xCE , \xA3 "
    assert_invalid_unicode_at(&[b'"', b'\\', b'"', 0xCE, b',', 0xA3, b'"'], 4);
}

// https://github.com/serde-rs/json/issues/1110
#[test]
fn invalid_utf8_inside_string_reports_the_bad_byte() {
    assert_invalid_unicode_at(&[b'"', 128, b' ', b' ', b' ', b'"'], 2);
}

#[test]
fn invalid_utf8_at_start_of_string() {
    assert_invalid_unicode_at(&[b'"', 159, 146, 150, b'"'], 2);
}

#[cfg(feature = "raw_value")]
#[test]
fn invalid_utf8_raw_value_matches_value() {
    let input = &[b'"', 0xCE, 0xF8, b'"'];
    let value_err = serde_json::from_slice::<Value>(input).unwrap_err();
    let raw_err = serde_json::from_slice::<Box<serde_json::value::RawValue>>(input).unwrap_err();
    assert_eq!(value_err.to_string(), raw_err.to_string());
    assert_eq!(value_err.column(), 2);

    #[cfg(feature = "std")]
    {
        let raw_reader_err = serde_json::from_reader::<_, Box<serde_json::value::RawValue>>(
            std::io::Cursor::new(input),
        )
        .unwrap_err();
        assert_eq!(value_err.to_string(), raw_reader_err.to_string());
    }
}

#[cfg(feature = "raw_value")]
#[test]
fn invalid_utf8_in_multiline_raw_value() {
    for (input, line, column) in [
        (b"[\n \"\xff\",\n 0\n]".as_slice(), 2, 3),
        (b" \n  [\"\xff\",\n 0\n]".as_slice(), 2, 5),
    ] {
        let err = serde_json::from_slice::<Box<serde_json::value::RawValue>>(input)
            .unwrap_err();
        assert_eq!((err.line(), err.column()), (line, column));

        #[cfg(feature = "std")]
        {
            let err = serde_json::from_reader::<_, Box<serde_json::value::RawValue>>(
                std::io::Cursor::new(input),
            )
            .unwrap_err();
            assert_eq!((err.line(), err.column()), (line, column));
        }
    }
}
