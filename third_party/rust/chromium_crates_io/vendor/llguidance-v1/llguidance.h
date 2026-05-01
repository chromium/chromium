/* See src/ffi.rs for full API documentation. */

#ifndef LLGUIDANCE_H
#define LLGUIDANCE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Do not include special tokens and keep invalid UTF-8 as-is.
 */
#define LLG_DECODE_NONE 0

/**
 * Include special tokens in the output.
 * They may look like `<|something|>`, `<something_else>`, or `<[12345]>` if they don't have a name.
 */
#define LLG_DECODE_INCLUDE_SPECIAL 1

/**
 * Replace invalid UTF-8 with the replacement character.
 */
#define LLG_DECODE_VALID_UTF8 2

/**
 * Opaque handle to a grammar constraint.
 *
 * Created by one of the `llg_new_constraint*` functions (e.g.
 * [`llg_new_constraint()`], [`llg_new_constraint_json()`]).
 * Always check for errors after creation with [`llg_get_error()`].
 * Free with [`llg_free_constraint()`] when done.
 */
typedef struct LlgConstraint LlgConstraint;

/**
 * Opaque handle to a grammar matcher.
 *
 * Created with [`llg_new_matcher()`]. Check for errors with
 * [`llg_matcher_get_error()`]. Free with [`llg_free_matcher()`].
 */
typedef struct LlgMatcher LlgMatcher;

/**
 * Handle to a stop-sequence controller.
 *
 * Tracks generated tokens and detects when a stop sequence has been produced.
 * Created with [`llg_new_stop_controller()`] and freed with
 * [`llg_free_stop_controller()`].
 */
typedef struct LlgStopController LlgStopController;

/**
 * Tokenizer handle used by the C API.
 *
 * Wraps a [`ParserFactory`] and the associated token environment. Create one
 * with [`llg_new_tokenizer()`] or [`llg_new_tokenizer_v2()`], and free it
 * with [`llg_free_tokenizer()`].
 */
typedef struct LlgTokenizer LlgTokenizer;

typedef struct NodeRef NodeRef;

typedef struct LlgParserLimits {
  /**
   * For non-ambiguous grammars, this is the maximum "branching factor" of the grammar.
   * For ambiguous grammars, this might get hit much quicker.
   * Default: 2000
   */
  size_t max_items_in_row;
  /**
   * How much "fuel" are we willing to spend to build initial lexer regex AST nodes.
   * Default: 1_000_000
   * Speed: 50k/ms
   */
  uint64_t initial_lexer_fuel;
  /**
   * Maximum lexer fuel for computation of the whole token mask.
   * Default: 200_000
   * Speed: 14k/ms
   */
  uint64_t step_lexer_fuel;
  /**
   * Number of Earley items created for the whole token mask.
   * Default: 50_000
   * Speed: 20k/ms
   */
  size_t step_max_items;
  /**
   * Maximum number of lexer states.
   * Affects memory consumption, but not the speed for the most part.
   * Default: 250_000
   * Speed: ~1-2kB of memory per state
   */
  size_t max_lexer_states;
  /**
   * Maximum size of the grammar (symbols in productions)
   * Default: 500_000 (a few megabytes of JSON)
   */
  size_t max_grammar_size;
  /**
   * If true, we'll run any extremely large regexes against the whole
   * trie of the tokenizer while constructing the lexer.
   * This reduces future mask computation time, but increases
   * the time it takes to construct the lexer.
   * Default: true
   */
  bool precompute_large_lexemes;
  /**
   * If true, include parser state (including tokens so far) and grammar in
   * errors.
   * Default: true
   */
  bool verbose_errors;
} LlgParserLimits;

/**
 * Configuration for creating a new constraint or matcher.
 *
 * Use [`llg_constraint_init_set_defaults()`] to populate sane defaults, then
 * override individual fields as needed.
 */
typedef struct LlgConstraintInit {
  /**
   * The tokenizer to use, created with [`llg_new_tokenizer()`] or
   * [`llg_new_tokenizer_v2()`].
   */
  const struct LlgTokenizer *tokenizer;
  /**
   * The log level for the buffer that is kept inside the constraint.
   * 0 — no logging, 1 — warnings only, 2 — info.
   */
  uint32_t log_buffer_level;
  /**
   * The log level for writing to stderr.
   */
  uint32_t log_stderr_level;
  /**
   * Whether the engine supports fast-forward tokens
   * (appending more than one token to output at once).
   */
  bool ff_tokens_ok;
  /**
   * Whether the engine supports backtracking
   * (removing tokens from the output).
   */
  bool backtrack_ok;
  /**
   * The resource limits for the parser.
   * Default values will be used for all fields that are 0.
   */
  struct LlgParserLimits limits;
} LlgConstraintInit;

/**
 * Result of [`llg_compute_mask()`].
 */
typedef struct LlgMaskResult {
  /**
   * One bit per vocab token.
   * This is valid until any subsequent `llg_*` call on the same constraint.
   */
  const uint32_t *sample_mask;
  /**
   * Temperature to use for sampling.
   */
  float temperature;
  /**
   * Whether the sequence should stop.
   */
  bool is_stop;
} LlgMaskResult;

/**
 * Token ID type used throughout the C API (alias for `u32`).
 */
typedef uint32_t LlgToken;

/**
 * Result of [`llg_commit_token()`].
 */
typedef struct LlgCommitResult {
  /**
   * The tokens to append to the output, if any.
   * This is valid until any subsequent `llg_*` call on the same constraint.
   */
  const uint32_t *tokens;
  /**
   * The number of tokens in the `tokens` array (can be 0).
   */
  uint32_t n_tokens;
  /**
   * Whether the sequence should stop.
   */
  bool is_stop;
} LlgCommitResult;

/**
 * Describes one step for [`llg_par_compute_mask()`].
 *
 * Each step pairs a constraint with a destination buffer for the resulting
 * token mask.
 */
typedef struct LlgConstraintStep {
  /**
   * The constraint to compute mask for.
   */
  struct LlgConstraint *constraint;
  /**
   * Pointer to memory where the mask should be written.
   */
  uint32_t *mask_dest;
  /**
   * The length of the `mask_dest` array in bytes (not elements).
   */
  size_t mask_byte_len;
} LlgConstraintStep;

/**
 * Callback that llguidance invokes when an operation completes.
 */
typedef void (*LlgCallback)(const void *user_data);

/**
 * Tokenization function.
 *
 * Will not write more than `output_tokens_len` tokens (which can be 0).
 * Returns the total number of tokens (which can be more than
 * `output_tokens_len`).
 *
 * **This function must be thread-safe.**
 */
typedef size_t (*LlgTokenizeFn)(const void *user_data,
                                const uint8_t *bytes,
                                size_t bytes_len,
                                uint32_t *output_tokens,
                                size_t output_tokens_len);

/**
 * V1 tokenizer initialization parameters.
 *
 * This struct must be zero-initialized (e.g., `= {}` in C/C++) before setting fields.
 * New fields may be appended in future versions, and zero-initialization ensures
 * they receive safe default values.
 *
 * For multi-EOS support, use [`LlgTokenizerInitV2`] with [`llg_new_tokenizer_v2()`]
 * instead.
 */
typedef struct LlgTokenizerInit {
  /**
   * The number of tokens in the vocabulary.
   */
  uint32_t vocab_size;
  /**
   * The token ID for the end-of-sentence token.
   * For chat mode, set this to the end-of-turn token.
   */
  LlgToken tok_eos;
  /**
   * An array of the lengths of the token strings (`vocab_size` elements).
   */
  const uint32_t *token_lens;
  /**
   * A pointer to the token strings.
   * The length of this is the sum of all `token_lens`.
   */
  const uint8_t *token_bytes;
  /**
   * Instead of passing `token_lens` and `token_bytes`, this can be set to
   * the contents of a HuggingFace `tokenizer.json` file.
   */
  const char *tokenizer_json;
  /**
   * Set to true to enable a workaround for tokenize functions that only
   * accept valid UTF-8 strings and possibly prepend `<BOS>` etc.
   * TODO: the `<BOS>` bit is not implemented yet.
   */
  bool tokenize_assumes_string;
  /**
   * Tokenization function; see [`LlgTokenizeFn`] for details.
   * It should only tokenize the bytes and not add
   * any `<BOS>` etc. It should also work on any byte sequence, including
   * invalid UTF-8. If this is not the case, set `tokenize_assumes_string` to true.
   * Either way, this function must be thread-safe.
   */
  LlgTokenizeFn tokenize_fn;
  /**
   * Set to true to skip `tokenize_fn` and instead tokenize greedily,
   * which is often incorrect and may reduce accuracy.
   */
  bool use_approximate_greedy_tokenize_fn;
  /**
   * User data passed as the first argument to [`LlgTokenizeFn`].
   */
  const void *tokenize_user_data;
  /**
   * Tokenizer partitions for the slicer optimization.
   * This is array of pointers to strings, terminated with NULL (argv style).
   * Pass NULL to use defaults. Pass empty array to disable.
   */
  const char *const *slices;
} LlgTokenizerInit;

/**
 * V2 tokenizer initialization parameters.
 *
 * Extends [`LlgTokenizerInit`] with support for multiple EOS tokens.
 * Use with [`llg_new_tokenizer_v2()`].
 *
 * Initialize with: `LlgTokenizerInitV2 init = {}; init.struct_size = sizeof(init);`
 * The library only reads `struct_size` bytes from the pointer, so callers
 * compiled against an older header (with a smaller struct) will work with
 * newer library versions — any new fields default to zero.
 */
typedef struct LlgTokenizerInitV2 {
  /**
   * Must be set to `sizeof(LlgTokenizerInitV2)`.
   * The library uses this to determine how many bytes to read, enabling
   * forward compatibility when new fields are appended in future versions.
   */
  size_t struct_size;
  /**
   * The number of tokens in the vocabulary.
   */
  uint32_t vocab_size;
  /**
   * The token ID for the end-of-sentence token.
   * For chat mode, set this to the end-of-turn token.
   */
  LlgToken tok_eos;
  /**
   * An array of the lengths of the token strings (`vocab_size` elements).
   */
  const uint32_t *token_lens;
  /**
   * A pointer to the token strings.
   * The length of this is the sum of all `token_lens`.
   */
  const uint8_t *token_bytes;
  /**
   * Instead of passing `token_lens` and `token_bytes`, this can be set to
   * the contents of a HuggingFace `tokenizer.json` file.
   */
  const char *tokenizer_json;
  /**
   * Set to true to enable a workaround for tokenize functions that only
   * accept valid UTF-8 strings and possibly prepend `<BOS>` etc.
   * TODO: the `<BOS>` bit is not implemented yet.
   */
  bool tokenize_assumes_string;
  /**
   * Tokenization function; see [`LlgTokenizeFn`] for details.
   * It should only tokenize the bytes and not add
   * any `<BOS>` etc. It should also work on any byte sequence, including
   * invalid UTF-8. If this is not the case, set `tokenize_assumes_string` to true.
   * Either way, this function must be thread-safe.
   */
  LlgTokenizeFn tokenize_fn;
  /**
   * Set to true to skip `tokenize_fn` and instead tokenize greedily,
   * which is often incorrect and may reduce accuracy.
   */
  bool use_approximate_greedy_tokenize_fn;
  /**
   * User data passed as the first argument to [`LlgTokenizeFn`].
   */
  const void *tokenize_user_data;
  /**
   * Tokenizer partitions for the slicer optimization.
   * This is array of pointers to strings, terminated with NULL (argv style).
   * Pass NULL to use defaults. Pass empty array to disable.
   */
  const char *const *slices;
  /**
   * Additional EOS token IDs beyond [`tok_eos`](Self::tok_eos).
   * Points to an array of [`tok_eos_extra_count`](Self::tok_eos_extra_count) elements.
   * When `NULL` (the default for zero-initialized structs), only `tok_eos` is used.
   */
  const LlgToken *tok_eos_extra;
  /**
   * Number of elements in the [`tok_eos_extra`](Self::tok_eos_extra) array.
   */
  uint32_t tok_eos_extra_count;
} LlgTokenizerInitV2;



#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Set default values for an [`LlgConstraintInit`].
 *
 * Disables fast-forward tokens and backtracking, enables warnings on stderr,
 * and sets all logging to the buffer (retrieve with [`llg_flush_logs()`]).
 * You still need to set the `tokenizer` field manually.
 */
void llg_constraint_init_set_defaults(struct LlgConstraintInit *init,
                                      const struct LlgTokenizer *tokenizer);

/**
 * Create a new constraint from a grammar JSON string.
 *
 * Always returns a non-null value. Call [`llg_get_error()`] on the result to
 * check for errors.
 */
struct LlgConstraint *llg_new_constraint(const struct LlgConstraintInit *init,
                                         const char *llguidance);

/**
 * Create a new constraint from a regular expression.
 *
 * Always returns a non-null value. Call [`llg_get_error()`] on the result to
 * check for errors.
 */
struct LlgConstraint *llg_new_constraint_regex(const struct LlgConstraintInit *init,
                                               const char *regex);

/**
 * Create a new constraint from a JSON schema.
 *
 * Always returns a non-null value. Call [`llg_get_error()`] on the result to
 * check for errors.
 */
struct LlgConstraint *llg_new_constraint_json(const struct LlgConstraintInit *init,
                                              const char *json_schema);

/**
 * Create a new constraint from a Lark grammar.
 *
 * Always returns a non-null value. Call [`llg_get_error()`] on the result to
 * check for errors.
 */
struct LlgConstraint *llg_new_constraint_lark(const struct LlgConstraintInit *init,
                                              const char *lark);

/**
 * Create a new constraint with a specified type.
 *
 * `constraint_type` can be one of `"regex"`, `"json_schema"` (or `"json"`),
 * `"lark"`, `"llguidance"` (or `"guidance"`).
 *
 * Always returns a non-null value. Call [`llg_get_error()`] on the result to
 * check for errors.
 */
struct LlgConstraint *llg_new_constraint_any(const struct LlgConstraintInit *init,
                                             const char *constraint_type,
                                             const char *data);

/**
 * Get the error message from the constraint, or null if there is no error.
 *
 * After it returns a non-null value, it will always return that same pointer
 * until the constraint is freed with [`llg_free_constraint()`] (at which
 * point the pointer becomes invalid).
 */
const char *llg_get_error(const struct LlgConstraint *cc);

/**
 * Get the current temperature of the constraint.
 *
 * Updated by [`llg_compute_mask()`].
 */
float llg_get_temperature(const struct LlgConstraint *cc);

/**
 * Check whether the constraint is stopped (cannot be extended further).
 */
bool llg_is_stopped(const struct LlgConstraint *cc);

/**
 * Compute the token mask for the next sampling step.
 *
 * This typically takes up to a millisecond for a 100k tokenizer, so it should
 * be called on a background thread. Returns 0 on success and −1 on error
 * (use [`llg_get_error()`] to get the exact error). When 0 is returned, the
 * result is written to `*res_p`.
 */
int32_t llg_compute_mask(struct LlgConstraint *cc, struct LlgMaskResult *res_p);

/**
 * Commit the token sampled with the mask returned from [`llg_compute_mask()`].
 *
 * Can be run on the critical path of sampling (it is fast). Returns 0 on
 * success and −1 on error (use [`llg_get_error()`] to get the exact error).
 * When 0 is returned, the result is written to `*res_p`.
 */
int32_t llg_commit_token(struct LlgConstraint *cc, LlgToken token, struct LlgCommitResult *res_p);

/**
 * Compute masks for several constraints in parallel.
 *
 */
void llg_par_compute_mask(const struct LlgConstraintStep *steps,
                          size_t n_steps,
                          const void *user_data,
                          LlgCallback done_cb);

/**
 * Clone the constraint.
 */
struct LlgConstraint *llg_clone_constraint(const struct LlgConstraint *cc);

/**
 * Construct a new tokenizer from a [`LlgTokenizerInit`].
 *
 */
struct LlgTokenizer *llg_new_tokenizer(const struct LlgTokenizerInit *tok_init,
                                       char *error_string,
                                       size_t error_string_len);

/**
 * Create a new tokenizer from a [`LlgTokenizerInitV2`] struct.
 *
 * This is the v2 API that supports multiple EOS tokens.
 *
 * The `tok_init` pointer must be valid and `tok_init->struct_size` must be set
 * to `sizeof(LlgTokenizerInitV2)` as known by the caller. The library will
 * only read `struct_size` bytes, so callers compiled against an older (smaller)
 * version of the struct will work with newer library versions — new fields
 * default to zero.
 *
 * `tok_init` must point to at least `tok_init->struct_size` bytes of
 * initialized memory, and `struct_size` must be at least
 * `offset_of!(LlgTokenizerInitV2, token_lens)` (i.e., it must include
 * `struct_size`, `vocab_size`, and the complete `tok_eos` field).
 */
struct LlgTokenizer *llg_new_tokenizer_v2(const struct LlgTokenizerInitV2 *tok_init,
                                          char *error_string,
                                          size_t error_string_len);

/**
 * Clone a tokenizer.
 *
 * This increments a reference count and performs a small allocation.
 */
struct LlgTokenizer *llg_clone_tokenizer(const struct LlgTokenizer *tok);

/**
 * Tokenize the given bytes and return the tokens.
 *
 * Always returns the number of tokens that would be written to
 * `output_tokens` if `output_tokens_len` were large enough.
 *
 */
size_t llg_tokenize_bytes(const struct LlgTokenizer *tok,
                          const uint8_t *bytes,
                          size_t bytes_len,
                          uint32_t *output_tokens,
                          size_t output_tokens_len);

/**
 * Tokenize the given bytes and return the tokens.
 *
 * Special tokens will be tokenized if they follow a `0xFF` byte prefix.
 * Always returns the number of tokens that would be written to
 * `output_tokens` if `output_tokens_len` were large enough.
 *
 */
size_t llg_tokenize_bytes_marker(const struct LlgTokenizer *tok,
                                 const uint8_t *bytes,
                                 size_t bytes_len,
                                 uint32_t *output_tokens,
                                 size_t output_tokens_len);

/**
 * Return a string representation of the tokens, useful for debugging.
 *
 * The output is NUL-terminated. Returns the number of bytes that would be
 * written to `output` if `output_len` were large enough.
 *
 */
size_t llg_stringify_tokens(const struct LlgTokenizer *tok,
                            const uint32_t *tokens,
                            size_t n_tokens,
                            char *output,
                            size_t output_len);

/**
 * Return a string representation of the tokens, useful for debugging.
 *
 * The output is NUL-terminated. Returns the number of bytes that would be
 * written to `output` if `output_len` were large enough.
 * `flags` is a combination of [`LLG_DECODE_NONE`], [`LLG_DECODE_INCLUDE_SPECIAL`],
 * and [`LLG_DECODE_VALID_UTF8`].
 *
 */
size_t llg_decode_tokens(const struct LlgTokenizer *tok,
                         const uint32_t *tokens,
                         size_t n_tokens,
                         char *output,
                         size_t output_len,
                         uint32_t flags);

/**
 * Free the tokenizer.
 *
 * Must **not** be called while there are still constraints using it.
 *
 */
void llg_free_tokenizer(struct LlgTokenizer *tok);

/**
 * Free the constraint.
 *
 */
void llg_free_constraint(struct LlgConstraint *cc);

/**
 * Get the logs from the constraint since the last call to this function.
 *
 * The returned string is NUL-terminated and remains valid until the next
 * call to this function or until the constraint is freed.
 */
const char *llg_flush_logs(struct LlgConstraint *cc);

/**
 * Create a new stop-sequence controller.
 *
 */
struct LlgStopController *llg_new_stop_controller(const struct LlgTokenizer *tokenizer,
                                                  const uint32_t *stop_tokens,
                                                  size_t stop_tokens_len,
                                                  const char *stop_rx,
                                                  char *error_string,
                                                  size_t error_string_len);

/**
 * Commit a token to the stop-sequence controller.
 *
 * Returns a pointer to a valid UTF-8 string to be returned to the user (which
 * may be empty) and sets `*is_stopped_p` to indicate whether the sequence
 * should then be finished. The returned string is valid until the next call
 * to this function or until the controller is freed.
 */
const char *llg_stop_commit_token(struct LlgStopController *stop_ctrl,
                                  uint32_t token,
                                  size_t *output_len_p,
                                  bool *is_stopped_p);

/**
 * Clone the stop-sequence controller.
 *
 * The cloned controller shares (under a mutex) regex caches, if any, so
 * cloning is cheap.
 */
struct LlgStopController *llg_clone_stop_controller(const struct LlgStopController *stop_ctrl);

/**
 * Free the stop-sequence controller.
 *
 */
void llg_free_stop_controller(struct LlgStopController *stop_ctrl);

/**
 * Create a new matcher from the given [`LlgConstraintInit`].
 *
 * Always returns a non-null value. Call [`llg_matcher_get_error()`] on the
 * result to check for errors.
 *
 * `init.ff_tokens_ok` and `init.backtrack_ok` are ignored
 * (backtracking is always disabled, and fast-forward tokens can be retrieved
 * using [`llg_matcher_compute_ff_tokens()`]).
 *
 * The `data` argument is interpreted differently depending on
 * `constraint_type`:
 * - `"regex"` — data is a regular expression in Rust regex format;
 *   see <https://docs.rs/regex/latest/regex/#syntax>
 * - `"json"` or `"json_schema"` — data is a (stringified) JSON schema;
 *   see <https://github.com/guidance-ai/llguidance/blob/main/docs/json_schema.md>
 * - `"json_object"` — equivalent to JSON schema `{"type":"object"}`
 * - `"lark"` — data is a grammar in a variant of Lark syntax;
 *   see <https://github.com/guidance-ai/llguidance/blob/main/docs/syntax.md>
 * - `"llguidance"` or `"guidance"` — data is a list of Lark or JSON schemas
 *   in JSON format
 *
 */
struct LlgMatcher *llg_new_matcher(const struct LlgConstraintInit *init,
                                   const char *constraint_type,
                                   const char *data);

/**
 * Check if the given grammar is valid.
 *
 * This is about twice as fast as creating a matcher (which also validates).
 * See [`llg_new_matcher()`] for the grammar format. Returns 0 on success,
 * −1 on error, and 1 on warning. The error message or warning is written to
 * `message`, which is `message_len` bytes long. It is always NUL-terminated.
 *
 */
int32_t llg_validate_grammar(const struct LlgConstraintInit *init,
                             const char *constraint_type,
                             const char *data,
                             char *message,
                             size_t message_len);

/**
 * Compute the set of allowed tokens for the current state into a
 * caller-provided buffer.
 *
 * `mask_byte_len` must equal the value returned by
 * [`llg_matcher_get_mask_byte_size()`]. Returns 0 on success and −1 on error.
 *
 */
int32_t llg_matcher_compute_mask_into(struct LlgMatcher *matcher,
                                      uint32_t *mask_dest,
                                      size_t mask_byte_len);

/**
 * Compute the set of allowed tokens for the current state.
 *
 * Use [`llg_matcher_get_mask()`] to retrieve the result.
 * Returns 0 on success and −1 on error.
 */
int32_t llg_matcher_compute_mask(struct LlgMatcher *matcher);

/**
 * Return a pointer to the mask computed by [`llg_matcher_compute_mask()`],
 * or null if no mask has been computed yet.
 */
const uint32_t *llg_matcher_get_mask(struct LlgMatcher *matcher);

/**
 * Return the size of the mask in bytes.
 */
size_t llg_matcher_get_mask_byte_size(struct LlgMatcher *matcher);

/**
 * Advance the matcher by one token.
 *
 * Returns 0 on success and −1 on error.
 */
int32_t llg_matcher_consume_token(struct LlgMatcher *matcher, uint32_t token);

/**
 * Advance the matcher by several tokens.
 *
 * Returns 0 on success and −1 on error.
 *
 */
int32_t llg_matcher_consume_tokens(struct LlgMatcher *matcher,
                                   const uint32_t *tokens,
                                   size_t n_tokens);

/**
 * Get the error message from the matcher, or null if there is no error.
 *
 * After it returns a non-null value, it will always return that same pointer
 * until the matcher is freed with [`llg_free_matcher()`] (at which point the
 * pointer becomes invalid).
 *
 */
const char *llg_matcher_get_error(struct LlgMatcher *matcher);

/**
 * Check whether the matcher is in an error state.
 */
bool llg_matcher_is_error(struct LlgMatcher *matcher);

/**
 * Free the matcher.
 *
 */
void llg_free_matcher(struct LlgMatcher *matcher);

/**
 * Roll back the matcher state by `num_tokens`.
 *
 * Returns 0 on success and −1 on error.
 */
int32_t llg_matcher_rollback(struct LlgMatcher *matcher, size_t num_tokens);

/**
 * Reset the matcher to the initial state.
 *
 * A matcher in an error state cannot be reset.
 * Returns 0 on success and −1 on error.
 */
int32_t llg_matcher_reset(struct LlgMatcher *matcher);

/**
 * Check whether the grammar can fully accept the input so far.
 */
bool llg_matcher_is_accepting(struct LlgMatcher *matcher);

/**
 * Check whether the matcher will force an EOS token.
 *
 * Also returns true in the error state, since that is a forced stop.
 */
bool llg_matcher_is_stopped(const struct LlgMatcher *matcher);

/**
 * Check how many tokens can be consumed from the given token sequence.
 *
 * Returns the number of tokens that can be consumed, or −1 on error.
 *
 */
int32_t llg_matcher_validate_tokens(struct LlgMatcher *matcher,
                                    const uint32_t *tokens,
                                    size_t n_tokens);

/**
 * Compute the fast-forward (forced) tokens for the current state.
 *
 * The result is written to `output`. Returns the number of tokens written
 * (which can be 0) or −1 on error.
 *
 */
int32_t llg_matcher_compute_ff_tokens(struct LlgMatcher *matcher,
                                      uint32_t *output,
                                      size_t output_len);

/**
 * Clone the matcher.
 */
struct LlgMatcher *llg_clone_matcher(const struct LlgMatcher *matcher);

/**
 * Returns the version string of llguidance and its key dependencies.
 * This also allows dumping the version of the binary using
 * `strings libllguidance.so | grep -oE "(llguidance|derivre)@[0-9.]+"`
 *
 * The returned pointer is valid for the lifetime of the process and must not
 * be freed by the caller.
 */
const char *llg_get_version(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* LLGUIDANCE_H */
