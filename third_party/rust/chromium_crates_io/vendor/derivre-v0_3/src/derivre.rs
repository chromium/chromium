use derivre::Regex;

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

fn main() {
    let mut rx = Regex::new("[ab]c").unwrap();
    assert!(rx.is_match("ac"));
    assert!(rx.is_match("bc"));
    assert!(!rx.is_match("xxac"));
    assert!(!rx.is_match("acxx"));

    let mut rx = Regex::new("[abx]*(?P<stop>[xq]*y)").unwrap();
    assert!(rx.lookahead_len("axxxxxxxy") == Some(1));
    assert!(rx.lookahead_len("axxxxxxxqqqy") == Some(4));
    assert!(rx.lookahead_len("axxxxxxxqqq").is_none());
    assert!(rx.lookahead_len("ccqy").is_none());

    let mut rx = Regex::new("a[bc](de|fg)").unwrap();
    no_match(&mut rx, "abd");
    match_(&mut rx, "abde");
    look(&mut rx, "abde", Some(0));

    no_match(&mut rx, "abdea");
    println!("{:?}", rx);

    let mut rx = Regex::new("a[bc]*(de|fg)*x").unwrap();
    no_match_many(&mut rx, &["", "a", "b", "axb"]);
    match_many(&mut rx, &["ax", "abdex", "abcbcbcbcdex", "adefgdefgx"]);
    println!("{:?}", rx);
    //
    //
    //

    eprintln!("\n\n\nSTART");
    let parser = regex_syntax::ParserBuilder::new()
        // .dot_matches_new_line(false)
        // .unicode(false)
        // .utf8(false)
        .build();
    let mut rx = Regex::new_with_parser(parser, "a(bc+|b[eh])g|.h").unwrap();
    println!("{:?}", rx);
    no_match(&mut rx, "abh");
    println!("{:?}", rx);
}
