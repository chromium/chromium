use derivre::{JsonQuoteOptions, NextByte, Regex, RegexAst, RegexBuilder};

fn check_is_match(rx: &mut Regex, s: &str, exp: bool) {
    if rx.is_match(s) == exp {
    } else {
        panic!(
            "error for: {:?}; expected {}",
            s,
            if exp { "match" } else { "no match" }
        );
    }
}

fn match_(rx: &mut Regex, s: &str) {
    check_is_match(rx, s, true);
}

fn match_many(rx: &mut Regex, ss: &[&str]) {
    for s in ss {
        match_(rx, s);
    }
}

fn no_match(rx: &mut Regex, s: &str) {
    check_is_match(rx, s, false);
}

fn no_match_many(rx: &mut Regex, ss: &[&str]) {
    for s in ss {
        no_match(rx, s);
    }
}

fn look(rx: &mut Regex, s: &str, exp: Option<usize>) {
    let res = rx.lookahead_len(s);
    if res == exp {
    } else {
        panic!(
            "lookahead len error for: {:?}; expected {:?}, got {:?}",
            s, exp, res
        )
    }
}

#[test]
fn test_basic() {
    let mut rx = Regex::new("a[bc](de|fg)").unwrap();
    println!("{:?}", rx);
    no_match(&mut rx, "abd");
    match_(&mut rx, "abde");

    no_match(&mut rx, "abdea");
    println!("{:?}", rx);

    let mut rx = Regex::new("a[bc]*(de|fg)*x").unwrap();

    no_match_many(&mut rx, &["", "a", "b", "axb"]);
    match_many(&mut rx, &["ax", "abdex", "abcbcbcbcdex", "adefgdefgx"]);
    println!("{:?}", rx);

    let mut rx = Regex::new("(A|foo)*").unwrap();
    match_many(
        &mut rx,
        &["", "A", "foo", "Afoo", "fooA", "foofoo", "AfooA", "Afoofoo"],
    );

    let mut rx = Regex::new("[abcquv][abdquv]").unwrap();
    match_many(
        &mut rx,
        &["aa", "ab", "ba", "ca", "cd", "ad", "aq", "qa", "qd"],
    );
    no_match_many(&mut rx, &["cc", "dd", "ac", "ac", "bc"]);

    println!("{:?}", rx);

    let mut rx = Regex::new("ab{3,5}c").unwrap();
    match_many(&mut rx, &["abbbc", "abbbbc", "abbbbbc"]);
    no_match_many(
        &mut rx,
        &["", "ab", "abc", "abbc", "abbb", "abbbx", "abbbbbbc"],
    );

    let mut rx = Regex::new("x*A[0-9]{5}").unwrap();
    match_many(&mut rx, &["A12345", "xxxxxA12345", "xA12345"]);
    no_match_many(&mut rx, &["A1234", "xxxxxA123456", "xA123457"]);
}

#[test]
fn test_unicode() {
    let mut rx = Regex::new("źółw").unwrap();
    println!("{:?}", rx);
    no_match(&mut rx, "zolw");
    match_(&mut rx, "źółw");
    no_match(&mut rx, "źół");
    println!("{:?}", rx);

    let mut rx = Regex::new("[źó]łw").unwrap();
    match_(&mut rx, "ółw");
    match_(&mut rx, "źłw");
    no_match(&mut rx, "źzłw");

    let mut rx = Regex::new("x[©ª«]y").unwrap();
    match_many(&mut rx, &["x©y", "xªy", "x«y"]);
    no_match_many(&mut rx, &["x®y", "x¶y", "x°y", "x¥y"]);

    let mut rx = Regex::new("x[ab«\u{07ff}\u{0800}]y").unwrap();
    match_many(&mut rx, &["xay", "xby", "x«y", "x\u{07ff}y", "x\u{0800}y"]);
    no_match_many(&mut rx, &["xcy", "xªy", "x\u{07fe}y", "x\u{0801}y"]);

    let mut rx = Regex::new("x[ab«\u{07ff}-\u{0801}]y").unwrap();
    match_many(
        &mut rx,
        &[
            "xay",
            "xby",
            "x«y",
            "x\u{07ff}y",
            "x\u{0800}y",
            "x\u{0801}y",
        ],
    );
    no_match_many(&mut rx, &["xcy", "xªy", "x\u{07fe}y", "x\u{0802}y"]);

    let mut rx = Regex::new(".").unwrap();
    no_match(&mut rx, "\n");
    match_many(&mut rx, &["a", "1", " ", "\r"]);

    let mut rx = Regex::new("a.*b").unwrap();
    match_many(&mut rx, &["ab", "a123b", "a \r\t123b"]);
    no_match_many(&mut rx, &["a", "a\nb", "a1\n2b"]);
}

#[test]
fn test_lookaround() {
    let mut rx = Regex::new("[ab]*(?P<stop>xx)").unwrap();
    match_(&mut rx, "axx");
    look(&mut rx, "axx", Some(2));
    look(&mut rx, "ax", None);

    let mut rx = Regex::new("[ab]*(?P<stop>x*y)").unwrap();
    look(&mut rx, "axy", Some(2));
    look(&mut rx, "ay", Some(1));
    look(&mut rx, "axxy", Some(3));
    look(&mut rx, "aaaxxy", Some(3));
    look(&mut rx, "abaxxy", Some(3));
    no_match_many(&mut rx, &["ax", "bx", "aaayy", "axb", "axyxx"]);

    let mut rx = Regex::new("[abx]*(?P<stop>[xq]*y)").unwrap();
    look(&mut rx, "axxxxxxxy", Some(1));
    look(&mut rx, "axxxxxxxqy", Some(2));
    look(&mut rx, "axxxxxxxqqqy", Some(4));

    let mut rx = Regex::new("(f|foob)(?P<stop>o*y)").unwrap();
    look(&mut rx, "fooby", Some(1));
    look(&mut rx, "fooy", Some(3));
    look(&mut rx, "fy", Some(1));
}

#[test]
fn utf8_dfa() {
    let parser = regex_syntax::ParserBuilder::new()
        .unicode(false)
        .utf8(false)
        .ignore_whitespace(true)
        .build();

    let utf8_rx = r#"
   ( [\x00-\x7F]                        # ASCII
   | [\xC2-\xDF][\x80-\xBF]             # non-overlong 2-byte
   |  \xE0[\xA0-\xBF][\x80-\xBF]        # excluding overlongs
   | [\xE1-\xEC\xEE\xEF][\x80-\xBF]{2}  # straight 3-byte
   |  \xED[\x80-\x9F][\x80-\xBF]        # excluding surrogates
   |  \xF0[\x90-\xBF][\x80-\xBF]{2}     # planes 1-3
   | [\xF1-\xF3][\x80-\xBF]{3}          # planes 4-15
   |  \xF4[\x80-\x8F][\x80-\xBF]{2}     # plane 16
   )*
   "#;

    let mut rx = Regex::new_with_parser(parser, utf8_rx).unwrap();
    println!("UTF8 {:?}", rx);
    //match_many(&mut rx, &["a", "ą", "ę", "ó", "≈ø¬", "\u{1f600}"]);
    println!("UTF8 {:?}", rx);
    let compiled = rx.dfa();
    println!("UTF8 {:?}", rx);
    println!("mapping ({}) {:?}", rx.alpha().len(), &compiled[0..256]);
    println!("states {:?}", &compiled[256..]);
    println!("initial {:?}", rx.initial_state());
}

#[test]
fn utf8_restrictions() {
    let mut rx = Regex::new("(.|\n)*").unwrap();
    println!("{:?}", rx);
    match_many(&mut rx, &["", "a", "\n", "\n\n", "\x00", "\x7f"]);
    let s0 = rx.initial_state();
    assert!(rx.transition(s0, 0x80).is_dead());
    assert!(rx.transition(s0, 0xC0).is_dead());
    assert!(rx.transition(s0, 0xC1).is_dead());
    // more overlong:
    assert!(rx.transition_bytes(s0, &[0xE0, 0x80]).is_dead());
    assert!(rx.transition_bytes(s0, &[0xE0, 0x9F]).is_dead());
    assert!(rx.transition_bytes(s0, &[0xF0, 0x80]).is_dead());
    assert!(rx.transition_bytes(s0, &[0xF0, 0x8F]).is_dead());
    // surrogates:
    assert!(rx.transition_bytes(s0, &[0xED, 0xA0]).is_dead());
    assert!(rx.transition_bytes(s0, &[0xED, 0xAF]).is_dead());
    assert!(rx.transition_bytes(s0, &[0xED, 0xBF]).is_dead());
}

#[test]
fn trie() {
    let mut rx = Regex::new("(foo|far|bar|baz)").unwrap();
    match_many(&mut rx, &["foo", "far", "bar", "baz"]);
    no_match_many(&mut rx, &["fo", "fa", "b", "ba", "baa", "f", "faz"]);

    let mut rx = Regex::new("(foobarbazqux123|foobarbazqux124)").unwrap();
    match_many(&mut rx, &["foobarbazqux123", "foobarbazqux124"]);
    no_match_many(
        &mut rx,
        &["foobarbazqux12", "foobarbazqux125", "foobarbazqux12x"],
    );

    let mut rx = Regex::new("(1a|12a|123a|1234a|12345a|123456a)").unwrap();
    match_many(
        &mut rx,
        &["1a", "12a", "123a", "1234a", "12345a", "123456a"],
    );
    no_match_many(
        &mut rx,
        &["1234567a", "123456", "12345", "1234", "123", "12", "1"],
    );
}

#[test]
fn unicode_case() {
    let mut rx = Regex::new("(?i)Żółw").unwrap();
    match_many(&mut rx, &["Żółw", "żółw", "ŻÓŁW", "żóŁw"]);
    no_match_many(&mut rx, &["zółw"]);

    let mut rx = Regex::new("Żółw").unwrap();
    match_(&mut rx, "Żółw");
    no_match_many(&mut rx, &["żółw", "ŻÓŁW", "żóŁw"]);
}

fn validate_next_byte(rx: &mut Regex, data: Vec<(NextByte, u8)>) {
    let mut s = rx.initial_state();
    for (exp, b) in data {
        println!("next_byte {:?} {:?}", exp, b as char);
        let nb = rx.next_byte(s);
        if nb != exp {
            panic!("expected {:?}, got {:?}", exp, nb);
        }
        if nb == NextByte::ForcedEOI {
            assert!(rx.is_accepting(s));
        } else if nb == NextByte::Dead {
            assert!(s.is_dead());
        }
        s = rx.transition(s, b);
        if nb == NextByte::ForcedEOI {
            assert!(s.is_dead());
            assert!(rx.next_byte(s) == NextByte::Dead);
        }
    }
}

#[test]
fn next_byte() {
    let mut rx = Regex::new("a[bc]*dx").unwrap();
    validate_next_byte(
        &mut rx,
        vec![
            (NextByte::ForcedByte(b'a'), b'a'),
            (NextByte::SomeBytes2([b'b', b'c']), b'b'),
            (NextByte::SomeBytes2([b'b', b'c']), b'd'),
            (NextByte::ForcedByte(b'x'), b'x'),
            (NextByte::ForcedEOI, b'x'),
        ],
    );

    rx = Regex::new("abdx|aBDy").unwrap();
    validate_next_byte(
        &mut rx,
        vec![
            (NextByte::ForcedByte(b'a'), b'a'),
            (NextByte::SomeBytes2([b'B', b'b']), b'B'),
            (NextByte::ForcedByte(b'D'), b'D'),
        ],
    );

    rx = Regex::new("foo|bar").unwrap();
    validate_next_byte(
        &mut rx,
        vec![
            (NextByte::SomeBytes2([b'b', b'f']), b'f'),
            (NextByte::ForcedByte(b'o'), b'o'),
            (NextByte::ForcedByte(b'o'), b'o'),
            (NextByte::ForcedEOI, b'X'),
        ],
    );
}

fn check_one_quote(rx: &str, options: &JsonQuoteOptions, allow_nl: bool) -> Regex {
    let valid_any_string = &[
        "a", "A", "!", " ", "\\\"", "\\b", "\\f", "\\r", "\\t", "\\\\",
    ];
    let valid_any_string_unicode = &["\\u001A", "\\u001a", "\\u0000", "\\u0001", "\\u0008"];
    let invalid_any_string = &["\n", "\t", "\"", "\\", "\\'", "aa"];
    let string_allowing_nl = if options.is_allowed(b'u') {
        &["\\n", "\\u000A", "\\u000a"]
    } else {
        &["\\n", "\\n", "\\n"]
    };

    let mut b = RegexBuilder::new();

    let e = b.mk_regex(rx).unwrap();
    let e = b.json_quote(e, options).unwrap();
    println!("*** {:?} {}", rx, b.exprset().expr_to_string(e));

    let mut rx = b.to_regex(e);
    match_many(&mut rx, valid_any_string);
    no_match_many(&mut rx, invalid_any_string);
    if options.is_allowed(b'u') {
        match_many(&mut rx, valid_any_string_unicode);
    } else {
        no_match_many(&mut rx, valid_any_string_unicode);
    }
    if allow_nl {
        match_many(&mut rx, string_allowing_nl);
    } else {
        no_match_many(&mut rx, string_allowing_nl);
    }

    rx
}

fn check_json_quote(
    options: &JsonQuoteOptions,
    rx: &str,
    should_match: &[&str],
    should_not_match: &[&str],
) {
    let mut b = RegexBuilder::new();
    let e = b
        .mk(&RegexAst::JsonQuote(
            Box::new(RegexAst::Regex(rx.to_string())),
            options.clone(),
        ))
        .unwrap();
    let mut rx = b.to_regex(e);
    match_many(&mut rx, should_match);
    no_match_many(&mut rx, should_not_match);
}

#[test]
fn test_json_quote() {
    let mut b = RegexBuilder::new();

    for options in [
        JsonQuoteOptions::no_unicode_raw(),
        JsonQuoteOptions::with_unicode_raw(),
    ] {
        let e = b.mk_regex(r#"[abc"]"#).unwrap();
        let e = b.json_quote(e, &options).unwrap();

        let mut rx = b.to_regex(e);
        match_many(&mut rx, &["a", "b", "c", "\\\""]);
        no_match_many(&mut rx, &["A", "\"", "\\"]);

        check_json_quote(&options, r#"""#, &["\\\""], &["\"", "a", ""]);
        check_json_quote(&options, r#"\\"#, &["\\\\"], &["\\", "a", ""]);
        if options.is_allowed(b'u') {
            check_json_quote(&options, r#"\x7F"#, &["\\u007F"], &["\x7F", "a", ""]);
        }

        check_one_quote(r#"."#, &options, false);
        check_one_quote(r#".|\n"#, &options, true);

        let mut rx = check_one_quote(r#"[^\u0017]"#, &options, true);
        no_match_many(&mut rx, &["\\u0017"]);
    }
}

#[test]
fn test_json_qbig() {
    let mut b = RegexBuilder::new();
    let options = JsonQuoteOptions::with_unicode_raw();
    let rx = "\\w+[\\\\](\\w+\\.)*\\w+\\.dll";
    // let rx = "a[\\\\]b";
    let t0 = std::time::Instant::now();
    let e0 = b.mk_regex(rx).unwrap();
    let el0 = t0.elapsed();
    let c0 = b.exprset().cost();
    let e = b.json_quote(e0, &options).unwrap();
    let c1 = b.exprset().cost();
    println!("*** {:?} {}", rx, b.exprset().expr_to_string(e0).len());
    println!("  >>> {}", b.exprset().expr_to_string(e).len());
    println!("  cost {} {} {:?}", c0, c1, el0);
}

#[test]
fn test_json_uxxxx() {
    let mut b = RegexBuilder::new();
    let options = JsonQuoteOptions::with_unicode_raw();
    let e0 = b.mk_regex(".").unwrap();
    let e = b.json_quote(e0, &options).unwrap();
    let mut rx = b.to_regex(e);
    for x in 0..=0xffff {
        for s in &[format!("\\u{:04X}", x), format!("\\u{:04x}", x)] {
            if x == 0x007f || ((0x0000..=0x001f).contains(&x) && x != 0x000a) {
                match_(&mut rx, s);
            } else {
                no_match(&mut rx, s);
            }
        }
    }
}

#[test]
fn test_json_and() {
    let mut b = RegexBuilder::new();
    let options = JsonQuoteOptions::with_unicode_raw();

    let e0 = b.mk_regex_and(&["[a-z]+", "(foo|bar|Baz)"]).unwrap();
    let e = b.json_quote(e0, &options).unwrap();
    let mut rx = b.to_regex(e);
    match_many(&mut rx, &["foo", "bar"]);
    no_match_many(&mut rx, &["xoo", "Baz"]);

    let e0 = b.mk_regex_and(&["[a-z\n]+", "(foo\n|bar|Baz)"]).unwrap();
    let e = b.json_quote(e0, &options).unwrap();
    let mut rx = b.to_regex(e);
    match_many(&mut rx, &["foo\\n", "bar"]);
    no_match_many(&mut rx, &["foo\n", "xoo", "Baz"]);

    // contained_in(a,b) == a & ~b
    let e0 = b.mk_contained_in("[a-z\n]+", "(foo\n|bar|Baz)").unwrap();
    let e = b.json_quote(e0, &options).unwrap();
    let mut rx = b.to_regex(e);
    no_match_many(
        &mut rx,
        &["foo\\n", "q\n", "foo\\u000a", "bar", "Baz", "QUX"],
    );
    match_many(&mut rx, &["foo", "fooo\\n", "baar"]);
}

fn mk_search_regex(rx: &str) -> Regex {
    let mut b = RegexBuilder::new();
    let e0 = b.mk_regex_for_serach(rx).unwrap();
    b.to_regex(e0)
}

fn assert_search(search_rx: &str, match_rx: &str) {
    let mut b = RegexBuilder::new();
    let e0 = b.mk_regex_for_serach(search_rx).unwrap();
    let e1 = b.mk_regex(&format!("(?s:{})", match_rx)).unwrap();
    if e0 != e1 {
        panic!(
            "search regex {:?} != match regex {:?} (based on {:?} != {:?})",
            b.exprset().expr_to_string(e0),
            b.exprset().expr_to_string(e1),
            search_rx,
            match_rx
        );
    }
}

#[test]
fn test_search() {
    let mut rx = mk_search_regex("foo");
    match_many(&mut rx, &["foo", "fooa", "afoo", "afooa"]);
    no_match_many(&mut rx, &["fo", "foO", ""]);

    let mut rx = mk_search_regex("^foo");
    match_many(&mut rx, &["foo", "fooa"]);
    no_match_many(&mut rx, &["fo", "foO", "afoo", "afooa"]);

    assert_search("foo", ".*foo.*");
    assert_search("^foo", "foo.*");
    assert_search("foo$", ".*foo");
    assert_search("[ab]$", ".*[ab]");
    assert_search("^$|foo", "|.*foo.*");
    assert_search("^$|foo$", "|.*foo");
    assert_search("^$|^foo$", "|foo");
    assert_search("^$|^foo", "|foo.*");
    assert_search("(^$)|(foo$)", "|.*foo");
    assert_search("(?s:a.*b)", ".*a.*b.*");
    assert_search("(abc)", ".*(abc).*");
    assert_search("baz|qux", ".*(baz|qux).*");
    assert_search("^hello", "hello.*");
    assert_search("world$", ".*world");
    assert_search("^hello$", "hello");
    assert_search("\\d+", ".*\\d+.*");
    assert_search("(?:abc)+", ".*(?:abc)+.*");
    assert_search("^[a-z]*$", "[a-z]*");
    assert_search("^(a|b)$", "a|b");
    assert_search("^(foo)*$", "(foo)*");
    assert_search("colou?r", ".*colou?r.*");
    assert_search("[0-9]{2,4}", ".*[0-9]{2,4}.*");
    assert_search("^[^a-z]+$", "[^a-z]+");
    assert_search("file\\.txt", ".*file\\.txt.*");
}
