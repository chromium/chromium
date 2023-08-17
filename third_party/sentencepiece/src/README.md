# SentencePiece

[![Build C++](https://github.com/google/sentencepiece/actions/workflows/cmake.yml/badge.svg)](https://github.com/google/sentencepiece/actions/workflows/cmake.yml)
[![Build Wheels](https://github.com/google/sentencepiece/actions/workflows/wheel.yml/badge.svg)](https://github.com/google/sentencepiece/actions/workflows/wheel.yml)
[![GitHub Issues](https://img.shields.io/github/issues/google/sentencepiece.svg)](https://github.com/google/sentencepiece/issues)
[![PyPI version](https://badge.fury.io/py/sentencepiece.svg)](https://badge.fury.io/py/sentencepiece)
[![PyPi downloads](https://img.shields.io/pypi/dm/sentencepiece?style=flat-square&logo=pypi&logoColor=white)](https://pypi.org/project/sentencepiece/)
[![Contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg)](CONTRIBUTING.md)
[![License](https://img.shields.io/badge/License-Apache%202.0-brightgreen.svg)](https://opensource.org/licenses/Apache-2.0)
[![SLSA 3](https://slsa.dev/images/gh-badge-level3.svg)](https://slsa.dev)

SentencePiece is an unsupervised text tokenizer and detokenizer mainly for
Neural Network-based text generation systems where the vocabulary size
is predetermined prior to the neural model training. SentencePiece implements
**subword units** (e.g., **byte-pair-encoding (BPE)** [[Sennrich et al.](https://www.aclweb.org/anthology/P16-1162)]) and
**unigram language model** [[Kudo.](https://arxiv.org/abs/1804.10959)])
with the extension of direct training from raw sentences. SentencePiece allows us to make a purely end-to-end system that does not depend on language-specific pre/postprocessing.

**This is not an official Google product.**

## Technical highlights
- **Purely data driven**: SentencePiece trains tokenization and detokenization
  models from sentences. Pre-tokenization ([Moses tokenizer](https://github.com/moses-smt/mosesdecoder/blob/master/scripts/tokenizer/tokenizer.perl)/[MeCab](http://taku910.github.io/mecab/)/[KyTea](http://www.phontron.com/kytea/)) is not always required.
- **Language independent**: SentencePiece treats the sentences just as sequences of Unicode characters. There is no language-dependent logic.
- **Multiple subword algorithms**: **BPE**  [[Sennrich et al.](https://www.aclweb.org/anthology/P16-1162)] and **unigram language model** [[Kudo.](https://arxiv.org/abs/1804.10959)] are supported.
- **Subword regularization**: SentencePiece implements subword sampling for [subword regularization](https://arxiv.org/abs/1804.10959) and [BPE-dropout](https://arxiv.org/abs/1910.13267) which help to improve the robustness and accuracy of NMT models.
- **Fast and lightweight**: Segmentation speed is around 50k sentences/sec, and memory footprint is around 6MB.
- **Self-contained**: The same tokenization/detokenization is obtained as long as the same model file is used.
- **Direct vocabulary id generation**: SentencePiece manages vocabulary to id mapping and can directly generate vocabulary id sequences from raw sentences.
- **NFKC-based normalization**: SentencePiece performs NFKC-based text normalization.

For those unfamiliar with SentencePiece as a software/algorithm, one can read [a gentle introduction here](https://medium.com/@jacky2wong/understanding-sentencepiece-under-standing-sentence-piece-ac8da59f6b08).


## Comparisons with other implementations
|Feature|SentencePiece|[subword-nmt](https://github.com/rsennrich/subword-nmt)|[WordPiece](https://arxiv.org/pdf/1609.08144.pdf)|
|:---|:---:|:---:|:---:|
|Supported algorithm|BPE, unigram, char, word|BPE|BPE*|
|OSS?|Yes|Yes|Google internal|
|Subword regularization|[Yes](#subword-regularization-and-bpe-dropout)|No|No|
|Python Library (pip)|[Yes](python/README.md)|No|N/A|
|C++ Library|[Yes](doc/api.md)|No|N/A|
|Pre-segmentation required?|[No](#whitespace-is-treated-as-a-basic-symbol)|Yes|Yes|
|Customizable normalization (e.g., NFKC)|[Yes](doc/normalization.md)|No|N/A|
|Direct id generation|[Yes](#end-to-end-example)|No|N/A|

Note that BPE algorithm used in WordPiece is slightly different from the original BPE.

## Overview
### What is SentencePiece?
SentencePiece is a re-implementation of **sub-word units**, an effective way to alleviate the open vocabulary
  problems in neural machine translation. SentencePiece supports two segmentation algorithms, **byte-pair-encoding (BPE)** [[Sennrich et al.](http://www.aclweb.org/anthology/P16-1162)] and **unigram language model** [[Kudo.](https://arxiv.org/abs/1804.10959)]. Here are the high level differences from other implementations.

#### The number of unique tokens is predetermined
Neural Machine Translation models typically operate with a fixed
vocabulary. Unlike most unsupervised word segmentation algorithms, which
assume an infinite vocabulary, SentencePiece trains the segmentation model such
that the final vocabulary size is fixed, e.g., 8k, 16k, or 32k.

Note that SentencePiece specifies the final vocabulary size for training, which is different from
[subword-nmt](https://github.com/rsennrich/subword-nmt) that uses the number of merge operations.
The number of merge operations is a BPE-specific parameter and not applicable to other segmentation algorithms, including unigram, word and character.

#### Trains from raw sentences
Previous sub-word implementations assume that the input sentences are pre-tokenized. This constraint was required for efficient training, but makes the preprocessing complicated as we have to run language dependent tokenizers in advance.
The implementation of SentencePiece is fast enough to train the model from raw sentences. This is useful for training the tokenizer and detokenizer for Chinese and Japanese where no explicit spaces exist between words.

#### Whitespace is treated as a basic symbol
The first step of Natural Language processing is text tokenization. For
example, a standard English tokenizer would segment the text "Hello world." into the
following three tokens.

> [Hello] [World] [.]

One observation is that the original input and tokenized sequence are **NOT
reversibly convertible**. For instance, the information that is no space between
“World” and “.” is dropped from the tokenized sequence, since e.g., `Tokenize(“World.”) == Tokenize(“World .”)`

SentencePiece treats the input text just as a sequence of Unicode characters. Whitespace is also handled as a normal symbol. To handle the whitespace as a basic token explicitly, SentencePiece first escapes the whitespace with a meta symbol "▁" (U+2581) as follows.

> Hello▁World.

Then, this text is segmented into small pieces, for example:

> [Hello] [▁Wor] [ld] [.]

Since the whitespace is preserved in the segmented text, we can detokenize the text without any ambiguities.

```
  detokenized = ''.join(pieces).replace('▁', ' ')
```

This feature makes it possible to perform detokenization without relying on language-specific resources.

Note that we cannot apply the same lossless conversions when splitting the
sentence with standard word segmenters, since they treat the whitespace as a
special symbol. Tokenized sequences do not preserve the necessary information to restore the original sentence.

* (en) Hello world.   → [Hello] [World] [.]   \(A space between Hello and World\)
* (ja) こんにちは世界。  → [こんにちは] [世界] [。] \(No space between こんにちは and 世界\)

#### Subword regularization and BPE-dropout
Subword regularization [[Kudo.](https://arxiv.org/abs/1804.10959)] and BPE-dropout [Provilkov et al](https://arxiv.org/abs/1910.13267) are simple regularization methods
that virtually augment training data with on-the-fly subword sampling, which helps to improve the accuracy as well as robustness of NMT models.

To enable subword regularization, you would like to integrate SentencePiece library
([C++](doc/api.md#sampling-subword-regularization)/[Python](python/README.md)) into the NMT system to sample one segmentation for each parameter update, which is different from the standard off-line data preparations. Here's the example of [Python library](python/README.md). You can find that 'New York' is segmented differently on each ``SampleEncode (C++)`` or ``encode with enable_sampling=True (Python)`` calls. The details of sampling parameters are found in [sentencepiece_processor.h](src/sentencepiece_processor.h).

```
>>> import sentencepiece as spm
>>> s = spm.SentencePieceProcessor(model_file='spm.model')
>>> for n in range(5):
...     s.encode('New York', out_type=str, enable_sampling=True, alpha=0.1, nbest_size=-1)
...
['▁', 'N', 'e', 'w', '▁York']
['▁', 'New', '▁York']
['▁', 'New', '▁Y', 'o', 'r', 'k']
['▁', 'New', '▁York']
['▁', 'New', '▁York']
```

## Installation

### Python module
SentencePiece provides Python wrapper that supports both SentencePiece training and segmentation.
You can install Python binary package of SentencePiece with.

```
pip install sentencepiece
```

For more detail, see [Python module](python/README.md)

### Build and install SentencePiece command line tools from C++ source
The following tools and libraries are required to build SentencePiece:

* [cmake](https://cmake.org/)
* C++11 compiler
* [gperftools](https://github.com/gperftools/gperftools) library (optional, 10-40% performance improvement can be obtained.)

On Ubuntu, the build tools can be installed with apt-get:
```
% sudo apt-get install cmake build-essential pkg-config libgoogle-perftools-dev
```

Then, you can build and install command line tools as follows.
```
% git clone https://github.com/google/sentencepiece.git 
% cd sentencepiece
% mkdir build
% cd build
% cmake ..
% make -j $(nproc)
% sudo make install
% sudo ldconfig -v
```
On OSX/macOS, replace the last command with `sudo update_dyld_shared_cache`

### Build and install using vcpkg

You can download and install sentencepiece using the [vcpkg](https://github.com/Microsoft/vcpkg) dependency manager:

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg install sentencepiece

The sentencepiece port in vcpkg is kept up to date by Microsoft team members and community contributors. If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.

### Download and install SentencePiece from signed released wheels

You can download the wheel from the [GitHub releases page](https://github.com/google/sentencepiece/releases/latest).
We generate [SLSA3 signatures](slsa.dev) using the OpenSSF's [slsa-framework/slsa-github-generator](https://github.com/slsa-framework/slsa-github-generator) during the release process. To verify a release binary:
1. Install the verification tool from [slsa-framework/slsa-verifier#installation](https://github.com/slsa-framework/slsa-verifier#installation).
2. Download the provenance file `attestation.intoto.jsonl` from the [GitHub releases page](https://github.com/google/sentencepiece/releases/latest).
3. Run the verifier:
```shell
slsa-verifier -artifact-path <the-wheel> -provenance attestation.intoto.jsonl -source github.com/google/sentencepiece -tag <the-tag>
```

pip install wheel_file.whl

## Usage instructions
### Train SentencePiece Model
```
% spm_train --input=<input> --model_prefix=<model_name> --vocab_size=8000 --character_coverage=1.0 --model_type=<type>
```
* `--input`: one-sentence-per-line **raw** corpus file. No need to run
  tokenizer, normalizer or preprocessor. By default, SentencePiece normalizes
  the input with Unicode NFKC. You can pass a comma-separated list of files.
* `--model_prefix`: output model name prefix. `<model_name>.model` and `<model_name>.vocab` are generated.
* `--vocab_size`: vocabulary size, e.g., 8000, 16000, or 32000
* `--character_coverage`: amount of characters covered by the model, good defaults are: `0.9995` for languages with rich character set like Japanese or Chinese and `1.0` for other languages with small character set.
* `--model_type`: model type. Choose from `unigram` (default), `bpe`, `char`, or `word`. The input sentence must be pretokenized when using `word` type.

Use `--help` flag to display all parameters for training, or see [here](doc/options.md) for an overview.

### Encode raw text into sentence pieces/ids
```
% spm_encode --model=<model_file> --output_format=piece < input > output
% spm_encode --model=<model_file> --output_format=id < input > output
```

Use `--extra_options` flag to insert the BOS/EOS markers or reverse the input sequence.
```
% spm_encode --extra_options=eos (add </s> only)
% spm_encode --extra_options=bos:eos (add <s> and </s>)
% spm_encode --extra_options=reverse:bos:eos (reverse input and add <s> and </s>)
```

SentencePiece supports nbest segmentation and segmentation sampling with `--output_format=(nbest|sample)_(piece|id)` flags.
```
% spm_encode --model=<model_file> --output_format=sample_piece --nbest_size=-1 --alpha=0.5 < input > output
% spm_encode --model=<model_file> --output_format=nbest_id --nbest_size=10 < input > output
```

### Decode sentence pieces/ids into raw text
```
% spm_decode --model=<model_file> --input_format=piece < input > output
% spm_decode --model=<model_file> --input_format=id < input > output
```
Use `--extra_options` flag to decode the text in reverse order.
```
% spm_decode --extra_options=reverse < input > output
```

### End-to-End Example
```
% spm_train --input=data/botchan.txt --model_prefix=m --vocab_size=1000
unigram_model_trainer.cc(494) LOG(INFO) Starts training with :
input: "../data/botchan.txt"
... <snip>
unigram_model_trainer.cc(529) LOG(INFO) EM sub_iter=1 size=1100 obj=10.4973 num_tokens=37630 num_tokens/piece=34.2091
trainer_interface.cc(272) LOG(INFO) Saving model: m.model
trainer_interface.cc(281) LOG(INFO) Saving vocabs: m.vocab

% echo "I saw a girl with a telescope." | spm_encode --model=m.model
▁I ▁saw ▁a ▁girl ▁with ▁a ▁ te le s c o pe .

% echo "I saw a girl with a telescope." | spm_encode --model=m.model --output_format=id
9 459 11 939 44 11 4 142 82 8 28 21 132 6

% echo "9 459 11 939 44 11 4 142 82 8 28 21 132 6" | spm_decode --model=m.model --input_format=id
I saw a girl with a telescope.
```
You can find that the original input sentence is restored from the vocabulary id sequence.

### Export vocabulary list
```
% spm_export_vocab --model=<model_file> --output=<output file>
```
```<output file>``` stores a list of vocabulary and emission log probabilities. The vocabulary id corresponds to the line number in this file.

### Redefine special meta tokens
  By default, SentencePiece uses Unknown (&lt;unk&gt;), BOS (&lt;s&gt;) and EOS (&lt;/s&gt;) tokens which have the ids of 0, 1, and 2 respectively. We can redefine this mapping in the training phase as follows.

```
% spm_train --bos_id=0 --eos_id=1 --unk_id=5 --input=... --model_prefix=... --character_coverage=...
```
When setting -1 id e.g., ```bos_id=-1```, this special token is disabled. Note that the unknown id cannot be disabled.  We can define an id for padding (&lt;pad&gt;) as ```--pad_id=3```.  

If you want to assign another special tokens, please see [Use custom symbols](doc/special_symbols.md).

### Vocabulary restriction
```spm_encode``` accepts a ```--vocabulary``` and a ```--vocabulary_threshold``` option so that ```spm_encode``` will only produce symbols which also appear in the vocabulary (with at least some frequency). The background of this feature is described in [subword-nmt page](https://github.com/rsennrich/subword-nmt#best-practice-advice-for-byte-pair-encoding-in-nmt).

The usage is basically the same as that of ```subword-nmt```. Assuming that L1 and L2 are the two languages (source/target languages), train the shared spm model, and get resulting vocabulary for each:

```
% cat {train_file}.L1 {train_file}.L2 | shuffle > train
% spm_train --input=train --model_prefix=spm --vocab_size=8000 --character_coverage=0.9995
% spm_encode --model=spm.model --generate_vocabulary < {train_file}.L1 > {vocab_file}.L1
% spm_encode --model=spm.model --generate_vocabulary < {train_file}.L2 > {vocab_file}.L2
```

```shuffle``` command is used just in case because ```spm_train``` loads the first 10M lines of corpus by default.


Then segment train/test corpus with ```--vocabulary``` option
```
% spm_encode --model=spm.model --vocabulary={vocab_file}.L1 --vocabulary_threshold=50 < {test_file}.L1 > {test_file}.seg.L1
% spm_encode --model=spm.model --vocabulary={vocab_file}.L2 --vocabulary_threshold=50 < {test_file}.L2 > {test_file}.seg.L2
```

## Advanced topics

* [SentencePiece Experiments](doc/experiments.md)
* [SentencePieceProcessor C++ API](doc/api.md)
* [Use custom text normalization rules](doc/normalization.md)
* [Use custom symbols](doc/special_symbols.md)
* [Python Module](python/README.md)
* [Segmentation and training algorithms in detail]

