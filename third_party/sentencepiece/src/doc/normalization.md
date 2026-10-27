# Text Normalization in SentencePiece

By default, SentencePiece normalizes input sentences using a custom variant of Unicode **NFKC** normalization (see [Unicode Standard Annex #15: Unicode Normalization Forms](https://unicode.org/reports/tr15/) and [Unicode Equivalence on Wikipedia](https://en.wikipedia.org/wiki/Unicode_equivalence)). 

SentencePiece also allows you to define custom normalization rules, which are compiled and embedded directly into the model file. Normalization always happens *before* tokenization, meaning the vocabulary is learned from and applied to normalized text.

---

## Pre-defined Normalization Rules

SentencePiece provides several pre-defined normalization rules. We recommend using one of these default rules unless you have a specific requirement for custom normalization.

*   **`nmt_nfkc`**: NFKC normalization with additional space-related normalization (e.g., removing redundant spaces, converting full-width spaces). This is the **default** rule.
*   **`nfkc`**: Original Unicode NFKC normalization.
*   **`nmt_nfkc_cf`**: `nmt_nfkc` combined with [Unicode Case Folding](https://www.w3.org/International/wiki/Case_folding) (primarily converts text to lowercase).
*   **`nfkc_cf`**: `nfkc` combined with Unicode Case Folding.
*   **`identity`**: No normalization is applied (raw text is preserved).

You can specify the normalization rule during training using the `--normalization_rule_name` flag:

```bash
spm_train --normalization_rule_name=identity --input=<input> --model_prefix=<model> --vocab_size=8000
```

> [!NOTE]
> Due to algorithm limitations, SentencePiece does not implement the *entirety* of Unicode NFKC normalization. For examples of specific character sequences that are not normalized by our implementation (such as multiple combining marks), see the comments in [builder.h](../src/builder.h#L59-L90).

To see the exact differences between `nmt_nfkc` and `nfkc`, you can compare their definition files:
```bash
diff -u data/nfkc.tsv data/nmt_nfkc.tsv
```

---

## Custom Normalization Rules

SentencePiece supports custom normalization rules defined via user-provided string-to-string mappings. The normalizer applies these mappings using a **leftmost-longest matching** strategy.

To define a custom normalization rule, prepare a Tab-Separated Values (TSV) file with the following format:

```text
41 302 300      1EA6
41 302 301      1EA4
41 302 303      1EAA
...
```

*   **Format Details**:
    *   Each line represents a mapping rule.
    *   The **left column** is the source character sequence, and the **right column** is the target character sequence.
    *   Columns must be separated by a **single tab character** (`\t`).
    *   Characters within a sequence are represented by their **Unicode code point in hex** (UCS4), separated by spaces.
    *   To remove specific characters, you can leave the target sequence empty (i.e., a line with only the source sequence followed by a tab).
*   **Ambiguity Resolution**: If multiple rules match the same input sequence, SentencePiece always selects the **longest matching rule** (leftmost-longest).

For a concrete example, see [data/nfkc.tsv](../data/nfkc.tsv).

Once your TSV file is ready, pass it to the trainer using the `--normalization_rule_tsv` flag:

```bash
spm_train --normalization_rule_tsv=path/to/rule.tsv --input=<input> --model_prefix=<model> --vocab_size=8000
```

The compiled normalization rules are **embedded directly inside the generated `.model` file**. This ensures that the exact same normalization is consistently applied during both training and inference.

---

## Pre-processing and Whitespace Handling

In addition to character normalization rules, SentencePiece's `NormalizerSpec` contains several flags that control how whitespaces and prefixes are handled:

*   **`add_dummy_prefix`** (default: `true`): Prepends a dummy whitespace (represented by `▁` U+2581) to the beginning of the sentence. This ensures that words at the start of a sentence are tokenized similarly to words in the middle (which are preceded by spaces).
*   **`remove_extra_whitespaces`** (default: `true`): Collapses consecutive spaces into a single space and strips leading/trailing spaces.
*   **`escape_whitespaces`** (default: `true`): Replaces normal spaces with the meta-symbol `▁` (U+2581) to preserve whitespace information in the token sequence.

You can modify these flags during training:

```bash
spm_train --add_dummy_prefix=false --remove_extra_whitespaces=false --input=<input> --model_prefix=<model> --vocab_size=8000
```

---

## Command Line Tool to Perform Normalization

SentencePiece includes a standalone command-line tool `spm_normalize` to test or apply normalization rules to text files:

```bash
# Normalize using rules embedded in a model file
spm_normalize --model=path/to/model.model input_file.txt > normalized_output.txt

# Normalize using a raw TSV rule file (useful for debugging rules)
spm_normalize --normalization_rule_tsv=custom_rules.tsv input_file.txt > normalized_output.txt
```

*   The first command applies the normalization rules compiled and stored inside the specified model.
*   The second command applies the rules directly from a TSV file. This is highly useful for iterating on and debugging custom rules interactively before training a model.

---

## Python API Usage

You can perform normalization directly in Python using `sentencepiece.SentencePieceNormalizer`. This allows you to inspect how text is normalized without running the full tokenization pipeline.

```python
import sentencepiece as spm

# 1. Load normalizer with a pre-defined rule name
# Note: In the Python API, whitespace-handling flags (add_dummy_prefix,
# escape_whitespaces, remove_extra_whitespaces) default to False.
normalizer = spm.SentencePieceNormalizer(rule_name='nmt_nfkc')
print(normalizer.normalize('ＫＡＤＯＫＡＷＡABC'))  # Output: KADOKAWAABC

# To enable whitespace handling (like stripping) to match NMT training defaults:
normalizer_nmt = spm.SentencePieceNormalizer(
    rule_name='nmt_nfkc',
    remove_extra_whitespaces=True,
    add_dummy_prefix=True,
    escape_whitespaces=True
)
print(normalizer_nmt.normalize('  hello   world  '))  # Output: ▁hello▁world

# 2. Load normalizer from an existing model file
# Note: The Python constructor flags default to False and will OVERRIDE
# the settings embedded in the model file unless explicitly passed.
normalizer_model = spm.SentencePieceNormalizer(
    model_file='path/to/model.model',
    remove_extra_whitespaces=True  # Explicitly pass if your model expects it
)

# 3. Load normalizer directly from a custom TSV rule file
normalizer_custom = spm.SentencePieceNormalizer(rule_tsv='path/to/custom_rules.tsv')

# 4. Load normalizer directly from a Python list of mappings (norm_map)
normalizer_map = spm.SentencePieceNormalizer(norm_map=[('foo', 'bar'), ('baz', 'qux')])
print(normalizer_map.normalize('foobar'))  # Output: barbar

# 5. Decompile normalization rules from a loaded normalizer back to a list of mappings
# (Works with normalizers loaded from model, TSV, rule name, or norm_map)
rules = normalizer.decompile()
# rules is a list of tuples: [('foo', 'bar'), ...]

# 6. Train a model using a custom normalizer instance
# You can define a normalizer at runtime and pass it directly to the trainer.
# This embeds the custom normalization rules directly into the trained model.
custom_norm = spm.SentencePieceNormalizer(norm_map=[('foo', 'bar')], escape_whitespaces=True)
spm.SentencePieceTrainer.train(
    input='input.txt',
    model_prefix='m',
    vocab_size=8000,
    normalizer=custom_norm
)
```

For more details on the Python wrapper API, see the [Python Module README](../python/README.md).
