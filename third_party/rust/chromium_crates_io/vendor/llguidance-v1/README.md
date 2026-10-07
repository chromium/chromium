# Low-level Guidance Parser (llguidance)

This crate implements a parser for llguidance grammars.

The main entry point is the [Constraint struct](./src/constraint.rs).
You will need a token parser, built with
[TokenParser::from_llguidance_json](./src/tokenparser.rs#L64).
This in turn requires a JSON-encoded grammar,
see [TopLevelGrammar struct](./src/api.rs).

If you're dealing with a compilation (non-chat) model,
call `constraint.process_prompt()` first.

Once you have a constraint, do the following in a loop:
- call `constraint.compute_mask()` to get sampling mask for the next token
- sample token using mask and `constraint.temperature`
- pass the token to `constraint.commit_token()`
- append all the tokens returned to your output (if you enabled `ff_tokens`,
  more than one token can be returned)

If either `compute_mask()` or `commit_token()` return a stop result, you need to terminate
the sequence.

If you're accepting arbitrary grammars, you likely should stream the parser
results to the user.
The easiest way to do this is to set `constraint.log_json_progress`
and then forward results of `constraint.flush_logs()` after `commit_token()` and
right before terminating the sequence.

The `compute_mask()` function can take more than a millisecond for larger tokenizers
and/or grammars, so you should arrange for it be executed in background,
while the logits are computed on the GPU or other CPU cores.
The `commit_token()` function is very fast and can be called in the main loop.

See [sample parser](../sample_parser/src/minimal.rs) for an example of how to use this crate.

## Matcher cancellation

Rust matchers do not enable cancellation by default. Call
`Matcher::into_cancellable()` before moving a matcher to a worker, then obtain
`Matcher::cancellation_handle()`.
`Matcher::cancellation_handle()` returns `None` until cancellation is enabled.
Call `CancellationHandle::cancel()` from another thread when the result is no longer needed.
The call sets a permanent request and does not wait for the worker.
Join the worker before using or dropping its matcher.

The matcher checks the request during parser, lexer, trie, and slice work.
Fallible operations return the typed `Cancelled` error.
When cancellation is observed, `compute_ff_tokens()`, `consume_ff_tokens()`,
and `compute_ff_bytes()` return an empty result and set `StopReason::Cancelled`.
Use `is_error`, `is_cancelled`, `get_error`, or `stop_reason` to inspect the matcher state.
Cancellation cannot return a successful partial mask or force EOS.
Reset and rollback cannot resume a cancelled matcher.
A non-cancellation error already stored in the matcher keeps its original cause.

Cloned handles control the same matcher and can outlive it.
Cloned matchers have independent cancellation state, including `deep_clone()`.
A matcher clone retains a request present when its cancellation state is sampled.
Later requests to the original matcher do not affect the clone.

Cancellation is cooperative. A dependency call, tokenizer callback, shared lexer lock wait,
or cache insertion must finish before the next check.
A complete result can win a race with a request after the final check.
There is no fixed cancellation deadline.

For C, `llg_matcher_get_cancellation_handle()` enables cancellation on an idle matcher.
Use it with `llg_cancel()` and
`llg_free_cancellation_handle()`. Each cloned C handle requires its own free call.
Do not free a handle allocation while another thread uses that allocation.
Only cancellation handle operations may run concurrently with matcher mutation.
After cancellation, mask computation returns `-1` and `llg_matcher_get_mask()` returns null.
