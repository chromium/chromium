// A regression test for checking that minimization correctly translates
// whether a state is a match state or not. Previously, it was possible for
// minimization to mark a non-matching state as matching.
#[test]
#[cfg(not(miri))]
fn minimize_sets_correct_match_states() {
    use regex_automata::{
        dfa::{dense::DFA, Automaton, StartKind},
        Anchored, Input,
    };

    let pattern =
        // This is a subset of the grapheme matching regex. I couldn't seem
        // to get a repro any smaller than this unfortunately.
        r"(?x)
            (?:
                \p{gcb=Prepend}*
                (?:
                    (?:
                        (?:
                            \p{gcb=L}*
                            (?:\p{gcb=V}+|\p{gcb=LV}\p{gcb=V}*|\p{gcb=LVT})
                            \p{gcb=T}*
                        )
                        |
                        \p{gcb=L}+
                        |
                        \p{gcb=T}+
                    )
                    |
                    \p{Extended_Pictographic}
                    (?:\p{gcb=Extend}*\p{gcb=ZWJ}\p{Extended_Pictographic})*
                    |
                    [^\p{gcb=Control}\p{gcb=CR}\p{gcb=LF}]
                )
                [\p{gcb=Extend}\p{gcb=ZWJ}\p{gcb=SpacingMark}]*
            )
        ";

    let dfa = DFA::builder()
        .configure(
            DFA::config().start_kind(StartKind::Anchored).minimize(true),
        )
        .build(pattern)
        .unwrap();
    let input = Input::new(b"\xE2").anchored(Anchored::Yes);
    assert_eq!(Ok(None), dfa.try_search_fwd(&input));
}

// A prefilter must not skip to a new candidate after the DFA has already
// recorded a match. The DFA is still resolving the priority of that match,
// and skipping can cause a later match to overwrite it.
#[test]
#[cfg(feature = "perf-literal-multisubstring")]
fn prefilter_does_not_skip_pending_match() {
    use regex_automata::{
        dfa::{dense::DFA, Automaton},
        util::prefilter::Prefilter,
        HalfMatch, Input, MatchKind,
    };

    let pre = Prefilter::new(MatchKind::LeftmostFirst, &["ab", "c"])
        .expect("prefilter");
    let dfa = DFA::builder()
        .configure(
            DFA::config().prefilter(Some(pre)).starts_for_each_pattern(true),
        )
        .build(r"(?:ab|c)*c")
        .unwrap();
    let expected = Ok(Some(HalfMatch::must(0, 1)));
    let input = Input::new("cabac");

    assert_eq!(expected, dfa.try_search_fwd(&input));
    assert_eq!(expected, dfa.to_sparse().unwrap().try_search_fwd(&input));
}
