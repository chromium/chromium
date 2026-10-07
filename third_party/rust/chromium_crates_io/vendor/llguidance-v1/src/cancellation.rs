use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};

/// A permanent cancellation request for one parser or matcher.
///
/// Cloned handles control the same cancellation state. The handle can outlive the parser or
/// matcher. Cancellation does not wait for the worker; join it before accessing the cancelled
/// object.
#[derive(Clone, Debug, Default)]
pub struct CancellationHandle(Arc<AtomicBool>);

impl CancellationHandle {
    pub fn cancel(&self) {
        self.0.store(true, Ordering::Relaxed);
    }
    pub fn is_cancelled(&self) -> bool {
        self.0.load(Ordering::Relaxed)
    }
    pub(crate) fn check(&self) -> Result<(), Cancelled> {
        if self.is_cancelled() {
            Err(Cancelled)
        } else {
            Ok(())
        }
    }
    pub(crate) fn snapshot(&self) -> Self {
        Self(Arc::new(AtomicBool::new(self.is_cancelled())))
    }
}

/// A parser or matcher observed a permanent cancellation request.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Cancelled;

impl std::fmt::Display for Cancelled {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("operation cancelled")
    }
}
impl std::error::Error for Cancelled {}

#[cfg(all(test, feature = "lark"))]
thread_local! {
    pub(crate) static IN_START_TRANSITION: std::cell::Cell<bool> = const { std::cell::Cell::new(false) };
    static PROGRESS: std::cell::RefCell<Option<tests::Progress>> = const { std::cell::RefCell::new(None) };
}

#[cfg(all(test, feature = "lark"))]
pub(crate) fn checkpoint(point: &'static str) {
    let point = if point == "lexer" && IN_START_TRANSITION.with(|flag| flag.get()) {
        "start_transition"
    } else {
        point
    };
    PROGRESS.with_borrow_mut(|progress| {
        if let Some(progress) = progress {
            if progress.point == point {
                progress.work += 1;
                if progress.work == progress.threshold {
                    progress.reached.send(()).unwrap();
                    progress.resume.recv().unwrap();
                }
            }
        }
    });
}

#[cfg(all(test, feature = "lark"))]
mod tests {
    use super::*;
    use crate::{
        api::{StopReason, TopLevelGrammar},
        Matcher, ParserFactory,
    };
    use std::{sync::mpsc, thread, time::Duration};
    use toktrie::{ApproximateTokEnv, InferenceCapabilities, TokEnv, TokRxInfo, TokTrie};

    pub(super) struct Progress {
        pub point: &'static str,
        pub work: usize,
        pub threshold: usize,
        pub reached: mpsc::SyncSender<()>,
        pub resume: mpsc::Receiver<()>,
    }

    fn matcher(grammar: &str, slices: &[String], canonical: bool) -> Matcher {
        let env: TokEnv = if canonical {
            ApproximateTokEnv::single_byte_env()
        } else {
            let mut words = (0..=255).map(|x| vec![x]).collect::<Vec<_>>();
            words.push(Vec::new());
            for n in 0..8192 {
                let mut n = n;
                let mut word = vec![b'a'; 4];
                for b in &mut word {
                    *b += (n % 26) as u8;
                    n /= 26;
                }
                words.push(word);
            }
            Arc::new(ApproximateTokEnv::new(TokTrie::from(
                &TokRxInfo::new(words.len() as u32, 256),
                &words,
            )))
        };
        let factory = ParserFactory::new(&env, InferenceCapabilities::default(), slices).unwrap();
        let matcher =
            Matcher::new(factory.create_parser(TopLevelGrammar::from_lark(grammar.to_string())))
                .into_cancellable();
        assert!(!matcher.is_error(), "{:?}", matcher.get_error());
        matcher
    }

    fn token_parser(grammar: &str) -> crate::TokenParser {
        let env = ApproximateTokEnv::single_byte_env();
        let factory = ParserFactory::new(&env, InferenceCapabilities::default(), &[]).unwrap();
        let mut parser = factory
            .create_parser(TopLevelGrammar::from_lark(grammar.to_string()))
            .unwrap();
        parser.start_without_prompt();
        parser
    }

    fn interrupt_token_parser<T: Send + 'static>(
        mut parser: crate::TokenParser,
        operation: fn(&mut crate::TokenParser) -> anyhow::Result<T>,
    ) -> (anyhow::Result<T>, crate::TokenParser) {
        let handle = parser.cancellation_handle().unwrap();
        let (reached_tx, reached_rx) = mpsc::sync_channel(0);
        let (resume_tx, resume_rx) = mpsc::sync_channel(0);
        let worker = thread::spawn(move || {
            PROGRESS.with_borrow_mut(|progress| {
                *progress = Some(Progress {
                    point: "trie",
                    work: 0,
                    threshold: 1,
                    reached: reached_tx,
                    resume: resume_rx,
                });
            });
            let result = operation(&mut parser);
            (result, parser)
        });

        let reached = reached_rx.recv_timeout(Duration::from_secs(10));
        handle.cancel();
        let _ = resume_tx.send(());
        let result = worker.join().unwrap();
        reached.unwrap();
        result
    }

    #[test]
    fn forced_results_keep_failure_in_matcher_state() {
        let mut tested = matcher(r#"start: "abcdef""#, &[], true);
        let expected = b"abcdef".iter().map(|b| *b as u32).collect::<Vec<_>>();
        assert_eq!(tested.compute_ff_tokens(), expected);
        assert_eq!(tested.compute_ff_bytes(), b"abcdef");
        assert_eq!(tested.consume_ff_tokens(), expected);
        assert!(!tested.is_error());
        assert!(!tested.is_cancelled());
        assert!(tested.get_error().is_none());
        assert_eq!(tested.stop_reason(), StopReason::NoExtension);

        let mut cancelled = matcher(r#"start: "abcdef""#, &[], true).into_cancellable();
        cancelled.cancellation_handle().unwrap().cancel();
        assert!(cancelled.compute_ff_tokens().is_empty());
        assert!(cancelled.compute_ff_bytes().is_empty());
        assert!(cancelled.consume_ff_tokens().is_empty());
        assert!(cancelled.is_error());
        assert!(cancelled.is_cancelled());
        assert_eq!(
            cancelled.get_error().as_deref(),
            Some("operation cancelled")
        );
        assert_eq!(cancelled.stop_reason(), StopReason::Cancelled);

        let mut healthy_empty = matcher("start: /[a-z]+/", &[], true);
        assert!(healthy_empty.compute_ff_tokens().is_empty());
        assert!(!healthy_empty.is_error());
        assert!(!healthy_empty.is_cancelled());
        assert!(healthy_empty.get_error().is_none());
        assert_eq!(healthy_empty.stop_reason(), StopReason::NotStopped);
    }

    fn assert_cancelled<T: std::fmt::Debug>(r: anyhow::Result<T>) {
        assert!(r.unwrap_err().is::<Cancelled>());
    }

    #[test]
    fn cancellation_is_opt_in() {
        let env = ApproximateTokEnv::single_byte_env();
        let factory = ParserFactory::new(&env, InferenceCapabilities::default(), &[]).unwrap();
        let mut matcher = Matcher::new(
            factory.create_parser(TopLevelGrammar::from_lark("start: /[a-z]+/".to_string())),
        );
        assert!(matcher.cancellation_handle().is_none());
        assert!(!matcher.is_cancelled());
        assert!(matcher.clone().cancellation_handle().is_none());
        assert!(matcher.deep_clone().cancellation_handle().is_none());
        let disabled_mask = matcher.clone().compute_mask().unwrap();
        let mut enabled = matcher.clone().into_cancellable();
        assert_eq!(enabled.compute_mask().unwrap(), disabled_mask);
        matcher = matcher.into_cancellable();
        assert!(matcher.cancellation_handle().is_some());
        matcher.cancellation_handle().unwrap().cancel();
        assert!(matcher.is_cancelled());
    }

    #[test]
    fn token_parser_clones_snapshot_cancellation_independently() {
        let mut parser = token_parser("start: /[a-z]+/");
        assert!(parser.cancellation_handle().is_none());

        let first = parser.enable_cancellation();
        let second = parser.enable_cancellation();
        let mut shallow = parser.clone();

        first.cancel();
        let mut deep = parser.deep_clone();
        let shallow_handle = shallow.cancellation_handle().unwrap();
        let deep_handle = deep.cancellation_handle().unwrap();
        assert!(second.is_cancelled());
        assert!(!shallow_handle.is_cancelled());
        assert!(deep_handle.is_cancelled());
        assert_cancelled(parser.compute_mask());
        assert!(shallow.compute_mask().is_ok());
        assert_cancelled(deep.compute_mask());
    }

    #[test]
    fn token_parser_does_not_publish_mask_after_in_flight_cancellation() {
        let mut parser = token_parser("start: /[a-z]+/");
        parser.enable_cancellation();
        let (result, parser) = interrupt_token_parser(parser, crate::TokenParser::compute_mask);

        assert_cancelled(result);
        assert_eq!(parser.stop_reason(), StopReason::Cancelled);
    }

    #[test]
    fn token_parser_does_not_publish_partial_validation_after_cancellation() {
        let mut parser = token_parser("start: /[a-z]+/");
        parser.enable_cancellation();
        let (result, parser) = interrupt_token_parser(parser, |parser| {
            parser.validate_tokens_raw(&[b'a' as u32; 32])
        });

        assert_cancelled(result);
        assert_eq!(parser.stop_reason(), StopReason::Cancelled);
    }

    #[test]
    fn matcher_adopts_token_parser_cancellation() {
        let mut parser = token_parser("start: /[a-z]+/");
        let handle = parser.enable_cancellation();
        let mut matcher = Matcher::new(Ok(parser));
        let mut independent = matcher.clone();

        assert!(matcher.cancellation_handle().is_some());
        handle.cancel();
        assert!(matcher.is_cancelled());
        assert_cancelled(matcher.compute_mask());
        assert!(independent.compute_mask().is_ok());
    }

    #[test]
    fn terminal_cancellation_and_clone_ownership() {
        let mut original = matcher("start: /[a-z]+/", &[], false);
        let mut shallow = original.clone();
        let mut deep = original.deep_clone();
        let handle = original.cancellation_handle().unwrap();
        let second_handle = handle.clone();
        drop(handle);
        second_handle.cancel();
        second_handle.cancel();
        assert!(original.is_cancelled());
        assert_eq!(original.stop_reason(), StopReason::Cancelled);
        assert_cancelled(original.clone().compute_mask());
        assert_cancelled(original.deep_clone().compute_mask());
        assert_cancelled(original.compute_mask_or_eos());
        assert_cancelled(original.consume_token(b'a' as u32));
        assert!(original.compute_ff_tokens().is_empty());
        assert!(original.compute_ff_bytes().is_empty());
        assert!(original.consume_ff_tokens().is_empty());
        assert!(original.is_error());
        assert!(original.is_cancelled());
        assert_eq!(original.get_error().as_deref(), Some("operation cancelled"));
        assert_eq!(original.stop_reason(), StopReason::Cancelled);
        assert_cancelled(original.is_accepting());
        assert_cancelled(original.reset());
        assert_cancelled(original.rollback(0));
        assert_cancelled(original.validate_tokens(&[]));
        assert_cancelled(original.try_consume_tokens(&[]));
        assert_cancelled(original.clone().compute_mask());
        assert_cancelled(original.deep_clone().compute_mask());
        assert_eq!(
            shallow.compute_mask().unwrap(),
            deep.compute_mask().unwrap()
        );
        drop(original);
        second_handle.cancel();
        let mut error = Matcher::new(Err(anyhow::anyhow!("original error"))).into_cancellable();
        error.cancellation_handle().unwrap().cancel();
        assert!(!error.is_cancelled());
        assert!(error.compute_ff_tokens().is_empty());
        assert!(error.compute_ff_bytes().is_empty());
        assert!(error.consume_ff_tokens().is_empty());
        assert!(error.is_error());
        assert_eq!(error.get_error().as_deref(), Some("original error"));
        assert_eq!(error.stop_reason(), StopReason::InternalError);
        assert_eq!(
            error.compute_mask().unwrap_err().to_string(),
            "original error"
        );
    }

    // Both runs wait at the same checkpoint after completed parser work.
    fn interrupt(
        mut matcher: Matcher,
        point: &'static str,
        threshold: usize,
        cancel: bool,
        operation: fn(&mut Matcher) -> anyhow::Result<()>,
    ) -> (usize, Matcher) {
        let handle = matcher.cancellation_handle().unwrap();
        let mut sibling = matcher.clone();
        let mut control = matcher.deep_clone();
        let (reached_tx, reached_rx) = mpsc::sync_channel(0);
        let (resume_tx, resume_rx) = mpsc::sync_channel(0);
        let worker = thread::spawn(move || {
            PROGRESS.with_borrow_mut(|p| {
                *p = Some(Progress {
                    point,
                    work: 0,
                    threshold,
                    reached: reached_tx,
                    resume: resume_rx,
                })
            });
            let result = operation(&mut matcher);
            let work = PROGRESS.with_borrow_mut(|p| p.take().unwrap().work);
            (result, work, matcher)
        });
        // A timeout releases the worker before joining it, so no worker is detached.
        let reached = reached_rx.recv_timeout(Duration::from_secs(10));
        if cancel || reached.is_err() {
            handle.cancel();
        }
        let _ = resume_tx.send(());
        let (result, work, matcher) = worker.join().unwrap();
        reached.unwrap();
        if cancel {
            assert_cancelled(result);
            operation(&mut sibling).unwrap();
            operation(&mut control).unwrap();
            assert_eq!(
                sibling.compute_mask().unwrap(),
                control.compute_mask().unwrap()
            );
        } else {
            result.unwrap();
        }
        (work, matcher)
    }

    fn mask(matcher: &mut Matcher) -> anyhow::Result<()> {
        matcher.compute_mask().map(|_| ())
    }
    fn forced(matcher: &mut Matcher) -> anyhow::Result<()> {
        let result = matcher.compute_ff_bytes();
        if matcher.is_cancelled() {
            assert!(result.is_empty());
            return Err(Cancelled.into());
        }
        Ok(())
    }
    fn consume_forced(matcher: &mut Matcher) -> anyhow::Result<()> {
        let result = matcher.consume_ff_tokens();
        if matcher.is_cancelled() {
            assert!(result.is_empty());
            return Err(Cancelled.into());
        }
        Ok(())
    }
    fn consume(matcher: &mut Matcher) -> anyhow::Result<()> {
        matcher.consume_token(b'x' as u32)
    }

    #[test]
    fn cancellation_inside_trie_is_bounded_and_restores_shared_lexer() {
        let mut original = matcher("start: /[a-z]+/", &[], false);
        original.consume_token(b'a' as u32).unwrap();
        let mut sibling = original.clone();
        let mut control = original.deep_clone();
        let (full_work, _) = interrupt(original.deep_clone(), "trie", 128, false, mask);
        assert!(full_work > 512 + 1024, "fixture work was only {full_work}");
        let (cancelled_work, cancelled) = interrupt(original, "trie", 512, true, mask);
        assert!(
            cancelled_work <= 512 + 1024,
            "performed {cancelled_work} byte visits after a request at visit 512"
        );
        drop(cancelled);
        assert_eq!(
            sibling.compute_mask().unwrap(),
            control.compute_mask().unwrap()
        );
    }

    #[test]
    fn cancellation_inside_short_trie_returns_typed_error() {
        let original = matcher("start: /[a-z]+/", &[], true);
        let (visits, cancelled) = interrupt(original, "trie", 1, true, mask);
        assert!((1..1024).contains(&visits));
        assert!(cancelled.is_cancelled());
    }

    #[test]
    fn cancellation_inside_forced_bytes() {
        let grammar = format!("start: {:?}", "a".repeat(1024));
        let original = matcher(&grammar, &[], true);
        let (full_work, _) = interrupt(original.deep_clone(), "forced", 16, false, forced);
        let (cancelled_work, cancelled) = interrupt(original, "forced", 16, true, forced);
        assert!(cancelled_work < full_work / 2);
        assert!(cancelled.is_error());
        assert!(cancelled.is_cancelled());
        assert_eq!(
            cancelled.get_error().as_deref(),
            Some("operation cancelled")
        );
        assert_eq!(cancelled.stop_reason(), StopReason::Cancelled);
    }

    #[test]
    fn cancellation_during_forced_token_consumption_discards_tokens() {
        let original = matcher(r#"start: "abcdef" /[a-z]+/"#, &[], true);
        let (work, cancelled) = interrupt(original, "publication", 2, true, consume_forced);
        assert_eq!(work, 2);
        assert!(cancelled.is_error());
        assert!(cancelled.is_cancelled());
        assert_eq!(
            cancelled.get_error().as_deref(),
            Some("operation cancelled")
        );
        assert_eq!(cancelled.stop_reason(), StopReason::Cancelled);
    }

    #[test]
    fn cancellation_inside_earley_prediction() {
        let alternatives = (0..1000)
            .map(|i| format!("r{i}"))
            .collect::<Vec<_>>()
            .join(" | ");
        let mut grammar = format!(
            "start: \"x\" choices
choices: {alternatives}
"
        );
        for i in 0..1000 {
            grammar.push_str(&format!(
                "r{i}: \"a{i}\"
"
            ));
        }
        let original = matcher(&grammar, &[], false);
        let (full_work, _) = interrupt(original.deep_clone(), "agenda", 32, false, consume);
        let (cancelled_work, _) = interrupt(original, "agenda", 32, true, consume);
        assert!(cancelled_work < full_work / 2);
    }

    #[test]
    fn cancellation_while_copying_skip_row_does_not_panic() {
        let alternatives = (0..1000)
            .map(|i| format!("r{i}"))
            .collect::<Vec<_>>()
            .join(" | ");
        let mut grammar = format!(
            "start: \"x\" choices
choices: {alternatives}
%ignore /[ ]+/
"
        );
        for i in 0..1000 {
            grammar.push_str(&format!(
                "r{i}: \"a{i}\"
"
            ));
        }

        let env = ApproximateTokEnv::single_byte_env();
        let mut factory = ParserFactory::new(&env, InferenceCapabilities::default(), &[]).unwrap();
        factory.quiet();
        let mut parser = factory
            .create_parser(TopLevelGrammar::from_lark(grammar))
            .unwrap();
        parser.start_without_prompt();
        parser.parser.apply_token(b"x", b'x' as u32).unwrap();
        parser.parser.apply_token(b" ", b' ' as u32).unwrap();

        let handle = CancellationHandle::default();
        parser.parser.set_cancellation_handle(handle.clone());
        let (reached_tx, reached_rx) = mpsc::sync_channel(0);
        let (resume_tx, resume_rx) = mpsc::sync_channel(0);
        let worker = thread::spawn(move || {
            PROGRESS.with_borrow_mut(|p| {
                *p = Some(Progress {
                    point: "skip",
                    work: 0,
                    threshold: 32,
                    reached: reached_tx,
                    resume: resume_rx,
                })
            });
            parser.parser.apply_token(b"a", b'a' as u32)
        });

        reached_rx.recv_timeout(Duration::from_secs(10)).unwrap();
        handle.cancel();
        resume_tx.send(()).unwrap();
        let result = worker.join().expect("skip cancellation panicked");
        assert_cancelled(result.map(|_| ()));
    }

    #[test]
    fn cancellation_inside_lexer_does_not_publish_partial_transition() {
        let grammar = (0..128)
            .map(|i| format!("/[a-z]{{{}}}/", i + 1))
            .collect::<Vec<_>>()
            .join(" | ");
        let mut original = matcher(&format!("start: {grammar}"), &[], false);
        original.consume_token(b'a' as u32).unwrap();
        let mut sibling = original.clone();
        let mut control = original.deep_clone();
        let (_, cancelled) = interrupt(original, "lexer", 8, true, mask);
        drop(cancelled);
        assert_eq!(
            sibling.compute_mask().unwrap(),
            control.compute_mask().unwrap()
        );
    }

    #[test]
    fn cancellation_inside_start_transition_preserves_shared_cache() {
        let choices = (1..=128)
            .map(|n| format!("/[b-z]{{{n}}}/"))
            .collect::<Vec<_>>()
            .join(" | ");
        let grammar = format!("start: /a+/ choices\nchoices: {choices}");
        let mut original = matcher(&grammar, &[], false);
        original.consume_token(b'a' as u32).unwrap();
        fn start_next_lexeme(matcher: &mut Matcher) -> anyhow::Result<()> {
            matcher.consume_token(b'b' as u32)
        }
        let (full_work, _) = interrupt(
            original.deep_clone(),
            "start_transition",
            8,
            false,
            start_next_lexeme,
        );
        let (cancelled_work, _) =
            interrupt(original, "start_transition", 8, true, start_next_lexeme);
        assert!(
            cancelled_work < full_work / 2,
            "{cancelled_work} / {full_work}"
        );
    }

    #[test]
    fn cancellation_inside_hidden_replay_restores_stack() {
        let stop = "abcdefghijklmnopqrstuvwxyz";
        let grammar = format!("start: text {stop:?} \"!\"\ntext[stop={stop:?}]: /x+/\n");
        let mut original = matcher(&grammar, &[], false);
        let prefix = format!("x{}", &stop[..stop.len() - 1]);
        original
            .consume_tokens(&prefix.bytes().map(u32::from).collect::<Vec<_>>())
            .unwrap();
        fn finish_stop(matcher: &mut Matcher) -> anyhow::Result<()> {
            matcher.consume_token(b'z' as u32)
        }
        let (full_work, _) = interrupt(original.deep_clone(), "hidden", 4, false, finish_stop);
        let (cancelled_work, _) = interrupt(original, "hidden", 4, true, finish_stop);
        assert!(cancelled_work < full_work / 2);
    }

    #[test]
    fn cancellation_at_hidden_recursive_advance_restores_stack() {
        let stop = "abcdefghijklmnopqrstuvwxyz";
        let grammar = format!(
            "start: text ending \"!\"\ntext[stop={stop:?}]: /x+/\nending[stop=\"\"]: {stop:?}\n"
        );
        let mut original = matcher(&grammar, &[], false);
        let prefix = format!("x{}", &stop[..stop.len() - 1]);
        original
            .consume_tokens(&prefix.bytes().map(u32::from).collect::<Vec<_>>())
            .unwrap();
        fn finish_stop(matcher: &mut Matcher) -> anyhow::Result<()> {
            matcher.consume_token(b'z' as u32)
        }
        interrupt(
            original.deep_clone(),
            "hidden_recursive",
            1,
            false,
            finish_stop,
        );
        interrupt(original, "hidden_recursive", 1, true, finish_stop);
    }

    #[test]
    fn cancellation_at_mask_publication_discards_complete_result() {
        let original = matcher("start: /[a-z]+/", &[], false);
        interrupt(original.deep_clone(), "publication", 1, false, mask);
        interrupt(original, "publication", 1, true, mask);
    }

    #[test]
    fn cancellation_at_forced_result_publication_returns_empty() {
        let original = matcher(r#"start: "abcdef""#, &[], true);
        interrupt(original.deep_clone(), "publication", 1, false, forced);
        let (_, cancelled) = interrupt(original, "publication", 1, true, forced);
        assert!(cancelled.is_error());
        assert!(cancelled.is_cancelled());
        assert_eq!(
            cancelled.get_error().as_deref(),
            Some("operation cancelled")
        );
        assert_eq!(cancelled.stop_reason(), StopReason::Cancelled);
    }

    #[test]
    fn cancellation_defeats_cached_and_forced_masks() {
        let mut cached = matcher("start: /[a-z]+/", &[], false);
        cached.consume_token(b'a' as u32).unwrap();
        let expected = cached.compute_mask().unwrap();
        assert_eq!(cached.compute_mask().unwrap(), expected);
        cached.cancellation_handle().unwrap().cancel();
        assert_cancelled(cached.compute_mask());

        let mut forced = matcher("start: \"abcdef\"", &[], true);
        assert!(!forced.compute_ff_tokens().is_empty());
        forced.cancellation_handle().unwrap().cancel();
        assert_cancelled(forced.compute_mask_or_eos());

        let mut stopped = matcher("start: \"a\"", &[], false);
        stopped.consume_token(b'a' as u32).unwrap();
        assert!(stopped.is_stopped());
        stopped.cancellation_handle().unwrap().cancel();
        assert_cancelled(stopped.compute_mask_or_eos());
    }

    #[test]
    fn cancellation_inside_slice_discards_mask() {
        let original = matcher("start: /[a-z]+/", &["[a-z]+".to_string()], false);
        let (_, cancelled) = interrupt(original, "slice", 1, true, mask);
        assert!(cancelled.is_cancelled());
    }
}

#[cfg(all(test, not(feature = "lark")))]
pub(crate) fn checkpoint(_: &'static str) {}
