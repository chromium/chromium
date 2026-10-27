# SentencePiece Training Options

This document describes the training options available in SentencePiece. These options can be passed as command-line flags to `spm_train` or as keyword arguments to the Python API `sentencepiece.SentencePieceTrainer.train()`.

---

## Quick Start Example

Here is a minimal example to train a model using a raw text corpus (`input.txt`):

### Command Line (CLI)
```bash
spm_train --input=input.txt --model_prefix=m --vocab_size=8000
```

### Python API
```python
import sentencepiece as spm
spm.SentencePieceTrainer.train(input='input.txt', model_prefix='m', vocab_size=8000)
```

Both methods will output `m.model` and `m.vocab` files.

---

## 1. General & I/O Options

These options control the input/output files, the tokenization algorithm type, and general runtime settings.

*   **`input`** (string or list of strings, default: `""`)
    *   Comma-separated list of input text files for training.
    *   *Python Example*: `input=['data/corpus1.txt', 'data/corpus2.txt']`
    *   *CLI Example*: `--input=data/corpus1.txt,data/corpus2.txt`
*   **`input_format`** (string, default: `"text"`)
    *   Format of input files. Supported formats:
        *   `text`: Raw text files (one sentence per line).
        *   `tsv`: Tab-separated values. The input file must contain exactly two columns: `word \t frequency` (where frequency is a positive integer).
            *   *Important Limitation (Unigram)*: When training a `unigram` model, the frequency information is **not** used during the initial seed vocabulary extraction stage (which uses a Suffix Array on the raw word list, treating each unique word as appearing once). This can prevent short, high-frequency words (like `am` or `on`) from being extracted as candidate pieces, leading to them being tokenized as individual characters (e.g., `a`, `m`).
            *   *Workarounds*: Use `model_type=bpe` (which correctly utilizes frequencies during training), repeat high-frequency words multiple times in the TSV, or add them to `user_defined_symbols`.
            *   *References*: See [GitHub Issue #967](https://github.com/google/sentencepiece/issues/967) and [GitHub Issue #1047](https://github.com/google/sentencepiece/issues/1047) for details.
*   **`model_prefix`** (string, default: `""`)
    *   Prefix for the output files. Training generates `<model_prefix>.model` (binary model file) and `<model_prefix>.vocab` (human-readable vocabulary list).
*   **`model_type`** (string, default: `"unigram"`)
    *   The tokenization algorithm to use. Choose from:
        *   `unigram`: Unigram language model (recommended). It fits a probabilistic model and prunes the vocabulary.
        *   `bpe`: Byte-Pair Encoding. It starts with characters and merges frequent pairs.
        *   `word`: Word segmentation. Splits by space (only useful for languages that use spaces, essentially acting as a word-frequency tokenizer).
        *   `char`: Character-level segmentation.
*   **`vocab_size`** (int32, default: `8000`)
    *   Desired vocabulary size (including special symbols).

*   **`accept_language`** (string or list of strings, default: `""`)
    *   Comma-separated list of ISO language codes (e.g., `ja,en`).
    *   *Python Example*: `accept_language=['ja', 'en']`
    *   *Note*: This option is currently not used by the training logic and does not affect the model behavior. It is kept for backward compatibility and can be used to store language metadata inside the model file.
*   **`num_threads`** (int32, default: `16`)
    *   Number of threads to use during training.
*   **`random_seed`** (uint32, default: `4294967295` (max uint32))
    *   Seed for the random number generator. Used for EM initialization in Unigram and BPE-dropout.
    *   *Note on Reproducibility*: SentencePiece uses [Abseil Random](https://abseil.io/docs/cpp/guides/random) (specifically `absl::BitGen`) internally. Abseil does not guarantee stability of the generated random sequence across different library versions or platforms. Therefore, passing a fixed `random_seed` does not guarantee permanent reproducibility across updates.
*   **`minloglevel`** (int, default: `0`)
    *   Minimum logging level (0: INFO, 1: WARNING, 2: ERROR, 3: FATAL).

---

## 2. Training Data Sampling & Coverage

These options control how SentencePiece processes and samples the training corpus.

*   **`character_coverage`** (double, default: `0.9995`)
    *   The ratio of characters in the training corpus that must be covered by the vocabulary.
    *   *How it works*: Characters are sorted by frequency. SentencePiece accumulates them until the target ratio is met. Characters outside this limit are excluded from the alphabet and will be mapped to `<unk>` (or byte fallback).
    *   *Recommendation*: Use `1.0` for languages with small alphabets (English, German, etc.). Use `0.9995` (default) for languages with large character sets (Chinese, Japanese, Korean) to prune rare noise characters/emojis.
*   **`input_sentence_size`** (uint64, default: `0`)
    *   Maximum number of sentences to load from the input corpus. If `0`, the entire corpus is loaded. Setting this is highly recommended for very large datasets to prevent running out of memory.
*   **`shuffle_input_sentence`** (bool, default: `true`)
    *   If `true`, randomly samples `input_sentence_size` sentences from the corpus. Only effective when `input_sentence_size > 0`.
*   **`hard_vocab_limit`** (bool, default: `true`)
    *   If `true`, training will fail with an error if the corpus does not contain enough unique subwords to reach the requested `vocab_size`. If `false`, training will succeed and automatically shrink the vocabulary size in the output model to the maximum possible size.
*   **`train_extremely_large_corpus`** (bool, default: `false`)
    *   *(Unigram only)* Enables training on massive corpora containing more than 2 billion characters. It switches the internal suffix array index type from 32-bit to 64-bit integers to support the larger memory footprint.

---

## 3. Subword Learning Options

These options tune the core vocabulary learning process (primarily for Unigram and BPE).

*   **`seed_sentencepiece_size`** (int32, default: `1000000`)
    *   *(Unigram only)* Initial size of the candidate vocabulary before EM optimization starts. SentencePiece extracts this many frequent substrings as seeds, then iteratively prunes them.
*   **`seed_sentencepieces_file`** (string, default: `""`)
    *   *(Unigram only)* Path to a file containing pre-defined subwords (one per line, format: `piece \t frequency`) to seed the vocabulary, instead of extracting them from the corpus.
*   **`shrinking_factor`** (double, default: `0.75`)
    *   *(Unigram only)* The ratio by which the vocabulary is pruned in each iteration of EM training. In each step, it keeps the top pieces with respect to the loss, reducing the candidate size by `vocab_size * shrinking_factor` until it reaches `vocab_size`.
*   **`num_sub_iterations`** (int32, default: `2`)
    *   *(Unigram only)* Number of EM optimization iterations per vocabulary pruning step.
*   **`max_sentence_length`** (int32, default: `4192`)
    *   Maximum length (in bytes) of a sentence loaded by the trainer. Longer sentences are silently truncated or skipped to avoid performance bottlenecks.
*   **`use_all_vocab`** (bool, default: `false`)
    *   *(Word/Char models only)* If `true`, forces the model to include all words/characters found in the corpus into the vocabulary, ignoring frequency.
*   **`vocabulary_output_piece_score`** (bool, default: `true`)
    *   If `true`, outputs the log-likelihood (score) of each piece in the generated `<model_prefix>.vocab` file (as `piece \t score`). If `false`, the vocab file contains only the pieces (one per line).

---

## 4. Tokenization & Vocabulary Constraints

These options apply training-time constraints to control what constitutes a valid vocabulary piece.

*(See [Vocabulary Piece Constraints](piece_constraints.md) for a detailed explanation of these constraints.)*

*   **`max_sentencepiece_length`** (int32, default: `16`)
    *   Maximum length (in Unicode characters) of a learned subword.
*   **`split_by_unicode_script`** (bool, default: `true`)
    *   Prevents a single subword piece from crossing Unicode script boundaries (e.g. mixing Latin characters and Kanji).
*   **`split_by_number`** (bool, default: `true`)
    *   Splits numbers (0-9) from other characters, preventing subwords like `abc123`.
*   **`split_digits`** (bool, default: `false`)
    *   Splits all digits (0-9) into individual single-character pieces (e.g., `123` is guaranteed to be tokenized as `1`, `2`, `3`). Recommended for math or financial applications.
*   **`split_by_whitespace`** (bool, default: `true`)
    *   Prevents a single subword piece from crossing whitespace boundaries.
*   **`pretokenization_delimiter`** (string, default: `""`)
    *   *(Unigram only)* The most general way to introduce arbitrary segmentation boundaries. During training, input is split at this delimiter, and the delimiter itself is removed, preventing subwords from crossing the boundary.
*   **`treat_whitespace_as_suffix`** (bool, default: `false`)
    *   Attaches the whitespace marker `▁` as a suffix instead of a prefix (e.g., `world▁` instead of `▁world`).
*   **`allow_whitespace_only_pieces`** (bool, default: `false`)
    *   Allows the vocabulary to contain pieces made entirely of whitespace characters.

---

## 5. Special Symbols & Vocabulary IDs

These options define special symbols (BOS, EOS, PAD, custom control tokens) and how they map to vocabulary IDs.

*(See [Special Symbols](special_symbols.md) for details on control vs. user-defined symbols and security implications.)*

*   **`control_symbols`** (string or list of strings, default: `""`)
    *   Comma-separated list of custom control symbols (e.g., `<system>`, `<user>`). Control symbols are never tokenized from raw text and decode to empty strings.
    *   *Python Example*: `control_symbols=['<system>', '<user>']`
*   **`control_symbols_file`** (string, default: `""`)
    *   Path to a file containing control symbols (one per line).
*   **`user_defined_symbols`** (string or list of strings, default: `""`)
    *   Comma-separated list of custom user-defined symbols (e.g., emojis, HTML tags). User-defined symbols are matched from raw text as indivisible tokens.
    *   *Python Example*: `user_defined_symbols=['<emoji1>', '<emoji2>']`
*   **`user_defined_symbols_file`** (string, default: `""`)
    *   Path to a file containing user-defined symbols (one per line).
*   **`required_chars`** (string, default: `""`)
    *   List of UTF-8 characters that *must* be included in the alphabet (forcing them to never map to `<unk>`), regardless of the `--character_coverage` setting.
*   **`required_chars_file`** (string, default: `""`)
    *   Path to a file containing required characters (one per line).
*   **`byte_fallback`** (bool, default: `false`)
    *   If `true`, decomposes out-of-vocabulary characters into UTF-8 byte tokens (e.g., `<0xE3>`), completely avoiding `<unk>` tokens. Highly recommended for modern LLMs.
*   **`unk_id`** (int32, default: `0`), **`bos_id`** (int32, default: `1`), **`eos_id`** (int32, default: `2`), **`pad_id`** (int32, default: `-1`)
    *   Override default vocabulary IDs for special tokens. Set `-1` to disable the symbol (except for `unk_id` which cannot be disabled).
*   **`unk_piece`** (string, default: `"<unk>"`), **`bos_piece`** (string, default: `"<s>"`), **`eos_piece`** (string, default: `"</s>"`), **`pad_piece`** (string, default: `"<pad>"`)
    *   Override the string representations for the default special symbols.
*   **`unk_surface`** (string, default: `" ⁇ "` (U+2047 double question mark))
    *   Dummy surface string for `<unk>`. During decoding, the `<unk>` token ID is decoded back to this string.

---

## 6. Normalization & Whitespace Handling

These options control how text is normalized and how spaces are processed before vocabulary learning and tokenization.

*(See [Text Normalization](normalization.md) for details on Unicode normalization and custom rules.)*

*   **`normalization_rule_name`** (string, default: `"nmt_nfkc"`)
    *   Pre-defined normalization rule (choose from `nmt_nfkc` (default), `nfkc`, `nmt_nfkc_cf`, `nfkc_cf`, or `identity` (no normalization)).
*   **`normalization_rule_tsv`** (string, default: `""`)
    *   Path to a TSV file containing custom normalization rules.
*   **`denormalization_rule_tsv`** (string, default: `""`)
    *   Path to a TSV file containing custom denormalization rules applied during decoding.
*   **`add_dummy_prefix`** (bool, default: `true`)
    *   Prepends a dummy whitespace `▁` to the beginning of the sentence to ensure start-of-sentence words are tokenized identically to middle-of-sentence words.
*   **`remove_extra_whitespaces`** (bool, default: `true`)
    *   Collapses duplicate internal spaces and strips leading/trailing spaces.
*   **`escape_whitespaces`** (bool, default: `true`)
    *   Replaces normal spaces with the meta-symbol `▁` to preserve whitespace information in the token sequence.
    *   *Note*: This is a `NormalizerSpec` property and can only be modified via Python/C++ API configuration maps, not via `spm_train` CLI flags (where it defaults to `true` for normalizer and `false` for denormalizer).


