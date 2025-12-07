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
