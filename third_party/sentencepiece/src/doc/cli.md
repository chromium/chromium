# SentencePiece Command Line Interface (CLI) & Build Guide

This document describes how to build and install the SentencePiece C++ libraries and command-line tools, and how to use them to train models, encode text, and decode token sequences.

---

## 1. Installation & Build

### Prerequisites

The following tools and libraries are required to build SentencePiece:

- **CMake** (3.1 or later)
- **C++11 compiler** (e.g., gcc 4.8+ or clang 3.3+)
- **gperftools** library (optional, provides a 10-40% performance improvement)

### Building from Source (Linux/macOS)

On Ubuntu/Debian-based systems, install the prerequisites using `apt-get`:
```bash
sudo apt-get install cmake build-essential pkg-config libgoogle-perftools-dev
```

Then, clone the repository and build the command line tools:
```bash
git clone https://github.com/google/sentencepiece.git
cd sentencepiece
mkdir build
cd build
cmake ..
make -j $(nproc)
sudo make install
sudo ldconfig -v
```

*Note for macOS:* Replace the last command (`sudo ldconfig -v`) with:
```bash
sudo update_dyld_shared_cache
```

### Installing via vcpkg

You can download and install SentencePiece using the [vcpkg](https://github.com/Microsoft/vcpkg) dependency manager:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install sentencepiece
```

### Verifying Signed Release Wheels (SLSA3)

Pre-built wheels are available on the [GitHub releases page](https://github.com/google/sentencepiece/releases/latest), secured with SLSA Level 3 signatures. To verify and install:

1. Install the verification tool from [slsa-verifier](https://github.com/slsa-framework/slsa-verifier#installation).
2. Download the provenance file `attestation.intoto.jsonl` from the releases page.
3. Run the verifier:
   ```bash
   slsa-verifier -artifact-path <the-wheel> -provenance attestation.intoto.jsonl -source github.com/google/sentencepiece -tag <the-tag>
   ```
4. Install the verified wheel:
   ```bash
   pip install wheel_file.whl
   ```

---

## 2. CLI Usage Instructions

### Train SentencePiece Model (`spm_train`)

Use `spm_train` to train a new tokenization model from a raw text corpus.

```bash
spm_train --input=<input> --model_prefix=<model_name> --vocab_size=8000 --character_coverage=1.0 --model_type=<type>
```

#### Key Parameters:
- **`--input`**: Path to a one-sentence-per-line **raw** corpus file. No pre-tokenization or preprocessing is required. Supports a comma-separated list of files.
- **`--model_prefix`**: Output model name prefix. Generates `<model_name>.model` (binary model) and `<model_name>.vocab` (human-readable vocabulary list).
- **`--vocab_size`**: Desired vocabulary size (e.g., 8000, 16000, 32000).
- **`--character_coverage`**: The ratio of characters in the corpus covered by the model. Recommended defaults:
  - `0.9995` for languages with rich character sets (e.g., Japanese, Chinese, Korean) to prune rare noise characters/emojis.
  - `1.0` for languages with small alphabets (e.g., English, European languages).
- **`--model_type`**: The tokenization algorithm: `unigram` (default), `bpe`, `char`, or `word` (requires pre-tokenized input).

*For a complete list of training options, see [Training Options](options.md).*

---

### Encode Raw Text (`spm_encode`)

Segment raw text into subword pieces or vocabulary IDs.

#### Basic Encoding
*   Encode to subword pieces:
    ```bash
    spm_encode --model=<model_file> --output_format=piece < input.txt > output.txt
    ```
*   Encode to vocabulary IDs:
    ```bash
    spm_encode --model=<model_file> --output_format=id < input.txt > output.txt
    ```

#### Adding Special Tokens
Use `--extra_options` to insert BOS/EOS markers:
*   Add EOS only:
    ```bash
    spm_encode --model=<model_file> --extra_options=eos < input.txt
    ```
*   Add both BOS and EOS:
    ```bash
    spm_encode --model=<model_file> --extra_options=bos:eos < input.txt
    ```

#### N-best Segmentation and Sampling (Subword Regularization)
*   Sample pieces (with temperature/alpha):
    ```bash
    spm_encode --model=<model_file> --output_format=sample_piece --nbest_size=-1 --alpha=0.5 < input.txt
    ```
*   Generate top 10 segmentations as IDs:
    ```bash
    spm_encode --model=<model_file> --output_format=nbest_id --nbest_size=10 < input.txt
    ```

---

### Decode Pieces/IDs into Raw Text (`spm_decode`)

Restore raw text from subword pieces or IDs.

*   Decode from pieces:
    ```bash
    spm_decode --model=<model_file> --input_format=piece < input.txt > output.txt
    ```
*   Decode from IDs:
    ```bash
    spm_decode --model=<model_file> --input_format=id < input.txt > output.txt
    ```

---

### End-to-End Example

Here is a complete workflow using a sample corpus:

```bash
# 1. Train a model with a vocabulary size of 1000
spm_train --input=data/botchan.txt --model_prefix=m --vocab_size=1000

# 2. Encode text to subword pieces
echo "I saw a girl with a telescope." | spm_encode --model=m.model
# Output: ▁I ▁saw ▁a ▁girl ▁with ▁a ▁ te le s c o pe .

# 3. Encode text to IDs
echo "I saw a girl with a telescope." | spm_encode --model=m.model --output_format=id
# Output: 9 459 11 939 44 11 4 142 82 8 28 21 132 6

# 4. Decode IDs back to raw text
echo "9 459 11 939 44 11 4 142 82 8 28 21 132 6" | spm_decode --model=m.model --input_format=id
# Output: I saw a girl with a telescope.
```

---

### Export Vocabulary (`spm_export_vocab`)

Export the vocabulary list along with emission log probabilities:
```bash
spm_export_vocab --model=<model_file> --output=<output_file>
```
The output file stores a list of vocabulary pieces and their scores. The vocabulary ID corresponds to the 0-indexed line number in this file.




