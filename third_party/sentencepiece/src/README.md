# SentencePiece

Note: We're actively refactoring the codebase to simplify support for open
source releases and Google internal needs.  During this time, builds and tests
will be unstable and not all features will be supported.

[![Build Status](https://travis-ci.org/google/sentencepiece.svg?branch=master)](https://travis-ci.org/google/sentencepiece)
[![Build status](https://ci.appveyor.com/api/projects/status/vxoub3qx4fwpysyq?svg=true)](https://ci.appveyor.com/project/taku910/sentencepiece)
[![Coverage Status](https://coveralls.io/repos/github/google/sentencepiece/badge.svg?branch=master)](https://coveralls.io/github/google/sentencepiece?branch=master)
[![GitHub Issues](https://img.shields.io/github/issues/google/sentencepiece.svg)](https://github.com/google/sentencepiece/issues)
[![PyPI version](https://badge.fury.io/py/sentencepiece.svg)](https://badge.fury.io/py/sentencepiece)
[![Contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg)](CONTRIBUTING.md)
[![License](https://img.shields.io/badge/License-Apache%202.0-brightgreen.svg)](https://opensource.org/licenses/Apache-2.0)

SentencePiece is an unsupervised text tokenizer and detokenizer mainly for
Neural Network-based text generation systems where the vocabulary size is
predetermined prior to the neural model training. SentencePiece implements three
approaches:

*   **subword units** (or  **byte-pair-encoding (BPE)** [Sennrich et al.](http://www.aclweb.org/anthology/P16-1162))
*   **unigram language model** [Kudo.](https://arxiv.org/abs/1804.10959)]

It supports direct training from raw sentences. SentencePiece allows us to make
a purely end-to-end system that does not depend on language-specific
pre/postprocessing.

NOTE: This is not an official Google product.

## Technical highlights

-   **Purely data driven**: SentencePiece trains tokenization and detokenization
    models from sentences. Pre-tokenization
    ([Moses tokenizer](https://github.com/moses-smt/mosesdecoder/blob/master/scripts/tokenizer/tokenizer.perl)/[MeCab](http://taku910.github.io/mecab/)/[KyTea](http://www.phontron.com/kytea/))
    is not always required.
-   **Language independent**: SentencePiece treats the sentences just as
    sequences of Unicode characters. There is no language-dependent logic.
-   **Multiple subword algorithms**: **BPE**  and **unigram language model** are
    supported.
-   **Subword regularization**: SentencePiece implements subword sampling for
    [subword regularization](https://arxiv.org/abs/1804.10959) which helps to
    improve the robustness and accuracy of NMT models.
-   **Fast and lightweight**: Segmentation speed is around 50k sentences/sec,
    and memory footprint is around 6MB.
-   **Self-contained**: The same tokenization/detokenization is obtained as long
    as the same model file is used.
-   **Direct vocabulary id generation**: SentencePiece manages vocabulary to id
    mapping and can directly generate vocabulary id sequences from raw
    sentences.
-   **NFKC-based normalization**: SentencePiece performs NFKC-based text
    normalization.

## Comparisons with other implementations

|Feature|SentencePiece|[subword-nmt](https://github.com/rsennrich/subword-nmt)|[WordPiece](https://arxiv.org/pdf/1609.08144.pdf)|
|:---|:---:|:---:|:---:|
|Supported algorithm|BPE, unigram, char, word|BPE|BPE|
|OSS?|Yes|Yes|Google internal|
|Subword regularization|[Yes](#subword-regularization)|No|No|
|Python Library (pip)|[Yes](python/README.md)|No|N/A|
|C++ Library|[Yes](doc/api.md)|No|N/A|
|Pre-segmentation required?|[No](#whitespace-is-treated-as-a-basic-symbol)|Yes|Yes|
|Customizable normalization (e.g., NFKC)|[Yes](doc/normalization.md)|No|N/A|
|Direct id generation|[Yes](#end-to-end-example)|No|N/A|

Note that BPE algorithm used in WordPiece is slightly different from the original BPE.

## Overview

### What is SentencePiece?

SentencePiece is a re-implementation of **sub-word units**, an effective way to
alleviate the open vocabulary problems in neural machine translation.
SentencePiece supports two segmentation algorithms,
**byte-pair-encoding (BPE)** [Sennrich et al.](http://www.aclweb.org/anthology/P16-1162)
and
**unigram language model** [Kudo.](https://arxiv.org/abs/1804.10959).
Here are the high level differences from other implementations.

#### The number of unique tokens is predetermined

Neural Machine Translation models typically operate with a fixed vocabulary.
Unlike most unsupervised word segmentation algorithms, which assume an infinite
vocabulary, SentencePiece trains the segmentation model such that the final
vocabulary size is fixed, e.g., 8k, 16k, or 32k.

Note that SentencePiece specifies the final vocabulary size for training, which
is different from [subword-nmt](https://github.com/rsennrich/subword-nmt) that
uses the number of merge operations.  The number of merge operations is a
BPE-specific parameter and not applicable to other segmentation algorithms,
including unigram, word and character.

#### Trains from raw sentences

Previous sub-word implementations assume that the input sentences are
pre-tokenized. This constraint was required for efficient training, but makes
the preprocessing complicated as we have to run language dependent tokenizers in
advance.  The implementation of SentencePiece is fast enough to train the model
from raw sentences. This is useful for training the tokenizer and detokenizer
for Chinese, Japanese and Korean where no explicit spaces exist between words.

#### Whitespace is treated as a basic symbol

The first step of Natural Language processing is text tokenization. For example,
a standard English tokenizer would segment the text "Hello world." into the
following three tokens.

> [Hello] [World] [.]

One observation is that the original input and tokenized sequence are **NOT
reversibly convertible**. For instance, the information that is no space between
“World” and “.” is dropped from the tokenized sequence, since e.g.,
`Tokenize(“World.”) == Tokenize(“World .”)`

SentencePiece treats the input text just as a sequence of Unicode characters.
Whitespace is also handled as a normal symbol. To handle the whitespace as a
basic token explicitly, SentencePiece first escapes the whitespace with a meta
symbol "▁" (U+2581) as follows.

> Hello▁World.

Then, this text is segmented into small pieces, for example:

> [Hello] [▁Wor] [ld] [.]

Since the whitespace is preserved in the segmented text, we can detokenize the
text without any ambiguities.

```
  detokenized = ''.join(pieces).replace('_', ' ')
```

This feature makes it possible to perform detokenization without relying on
language-specific resources.

Note that we cannot apply the same lossless conversions when splitting the
sentence with standard word segmenters, since they treat the whitespace as a
special symbol. Tokenized sequences do not preserve the necessary information to
restore the original sentence.

* (en) Hello world.   → [Hello] [World] [.]   \(A space between Hello and World\)
* (ja) こんにちは世界。  → [こんにちは] [世界] [。] \(No space between こんにちは and 世界\)

#### Subword regularization

Subword regularization [Kudo.](https://arxiv.org/abs/1804.10959)] is a simple
regularization method that virtually augments training data with on-the-fly
subword sampling, which helps to improve the accuracy as well as robustness of
NMT models.

To enable subword regularization, you would like to integrate SentencePiece
library
([C++](doc/api.md#sampling-subword-regularization)/[Python](python/README.md))
into the NMT system to sample one segmentation for each parameter update, which
is different from the standard off-line data preparations. Here's the example of
[Python library](python/README.md). You can find that 'New York' is segmented
differently on each ``SampleEncode`` call. The details of sampling parameters
are found in [`sentencepiece_processor.h`](src/sentencepiece_processor.h).

```python
>>> import sentencepiece as spm
>>> s = spm.SentencePieceProcessor()
>>> s.Load('spm.model')
>>> for n in range(5):
...     s.SampleEncodeAsPieces('New York', -1, 0.1)
...
['▁', 'N', 'e', 'w', '▁York']
['▁', 'New', '▁York']
['▁', 'New', '▁Y', 'o', 'r', 'k']
['▁', 'New', '▁York']
['▁', 'New', '▁York']
```

## Installation

### Pre-requisites

SentencePiece requires the following before setup:

1.   [Bazel](https://bazel.build/)

### C++

Building the underlying C++ libraries is as easy as

```sh
bazel build src:all
```

Testing can be done with

```sh
bazel test src:all
```

### Python module

We provide python API access for the `SentencePieceProcessor` to support
encoding and decoding text.

You can install the Python API with:

```sh
% pip install sentencepiece
```

For more detail, see [Python module](python/README.md)

### TensorFlow module

We will be no longer supported direct integration with Tensorflow.  Tensorflow
users are suggested to adopt the new Tokzenization ops published as part of
TF.Text.  Those ops will soon support running pre-trained SentencePiece models.

## API Examples

### Train a model

Train a sample model with full character coverage and a 8000 vocabulary size:

```sh
spm_train \
  --input=<input> \
  --model_prefix=<model_name> \
  --vocab_size=8000 \
  --character_coverage=1.0 \
  --model_type=<type>
```

Key flags:

*   `input`: one-sentence-per-line **raw** corpus file. No need to run
    tokenizer, normalizer or preprocessor. By default, SentencePiece normalizes
    the input with Unicode NFKC. You can pass a comma-separated list of files.
*   `model_prefix`: output model name prefix. `<model_name>.model` and
    `<model_name>.vocab` are generated.
*   `vocab_size`: vocabulary size, e.g., 8000, 16000, or 32000
*   `character_coverage`: amount of characters covered by the model, good
    defaults are: `0.9995` for languages with rich character set like Japanse or
    Chinese and `1.0` for other languages with small character set.
*   `--model_type`: model type. Choose from `unigram` (default), `bpe`, `char`,
    or `word`. The input sentence must be pretokenized when using `word` type.

The `help` flag will other parameters for training.

### Encode raw text into sentence pieces/ids

The `spm_encode` tool can encode the text into a piece sequence given a
pre-trained model:

```sh
spm_encode \
  --model=<model_file> \
  --output_format=piece \
  'input text' \
  output
```

With `output_format=id`, `spm_encode` will convert the text to an id sequence:

```sh
spm_encode \
  --model=<model_file> \
  --output_format=id \
  'input text' \
  output
```

The `extra_options` flag can be changed insert the end of sentence or begin of
sentence markers or reverse the input sequence:

```sh
spm_encode --extra_options=eos # (add </s> only)
spm_encode --extra_options=bos:eos # (add <s> and </s>)
spm_encode --extra_options=reverse:bos:eos # (reverse input and add <s> and </s>)
```

The `output_format` flag can be changed to support nbest segmentation and nbest
sampling with the `nbest_` and `sample_` prefixes:
```
spm_encode --model=<model_file> --output_format=sample_piece --nbest_size=-1 --alpha=0.5 'input text' output
spm_encode --model=<model_file> --output_format=nbest_id --nbest_size=10 'input_text' output
```

### Decode sentence pieces/ids into raw text

The `spm_decode` tool can convert segmented content into unsegmented text from
either white space separated pieces or an id sequence:

```sh
spm_decode --model=<model_file> --input_format=piece 'input pieces' output
% spm_decode --model=<model_file> --input_format=id 'input ids' output
```

The `extra_options` can be set to reverse the ordering:
```
spm_decode --extra_options=reverse 'input' output
```

### End-to-End Example

We can train a simple 1000 vocabulary sized model on the text in `data/botchan.txt` with the following:

```sh
spm_train \
  --input=data/botchan.txt \
  --model_prefix=m \
  --vocab_size=1000
```

While running, we should see the following text:
```sh
unigram_model_trainer.cc(494) LOG(INFO) Starts training with :
input: "../data/botchan.txt"
... <snip>
unigram_model_trainer.cc(529) LOG(INFO) EM sub_iter=1 size=1100 obj=10.4973 num_tokens=37630 num_tokens/piece=34.2091
trainer_interface.cc(272) LOG(INFO) Saving model: m.model
trainer_interface.cc(281) LOG(INFO) Saving vocabs: m.vocab
```

The resulting model can then be tested with a few command lines:

```sh
% echo "I saw a girl with a telescope." | spm_encode --model=m.model
▁I ▁saw ▁a ▁girl ▁with ▁a ▁ te le s c o pe .

% echo "I saw a girl with a telescope." | spm_encode --model=m.model --output_format=id
9 459 11 939 44 11 4 142 82 8 28 21 132 6

% echo "9 459 11 939 44 11 4 142 82 8 28 21 132 6" | spm_decode --model=m.model --input_format=id
I saw a girl with a telescope.
```

You can find that the original input sentence is restored from the vocabulary id
sequence.

### Export vocabulary list

The `spm_export_vocab` tool emits the learned vocabulary and emission
probabilities.  Vocabulary IDs will correspond to the piece's line number in the
vocab file.

```sh
spm_export_vocab \
  --model=<model_file> \
  --output=<output file>
```

### Redefine special meta tokens

By default, SentencePiece uses the following meta tokens with default reserved
vocabulary IDs:

*   `&lt;unk&gt;` with ID 0 for Unknown tokens.
*   `&lt;s&gt;` with ID 1 for BOS.
*   `&lt;/s&gt;` with ID 2 for EOS.
*   `&lt;pad&gt; with no pre-reserved Id for padding.

We can redefine this mapping in the training phase as follows.

```sh
spm_train \
  --bos_id=0 \
  --eos_id=1 \
  --unk_id=5 \
  --input=... \
  --model_prefix=... \
  --character_coverage=...
```

Each meta token can be disabled by using assigning `-1`.  For example,
```bos_id=-1``` disabled the BOS meta token.

NOTE: This does not work for ```unk_id```.  The unknown token must always be in
the vocabulary.

The full set of pre-defined meta token flags are:

*   ```bos_id```: for BOS.
*   ```eos_id```: for EOS.
*   ```unk_id```: for Unknown.

If you want to assign another special tokens, please see [Use custom
symbols](doc/special_symbols.md).

### Vocabulary restriction

```spm_encode``` accepts a ```--vocabulary``` and a ```--vocabulary_threshold```
option so that ```spm_encode``` will only produce symbols which also appear in
the vocabulary (with at least some frequency). The background of this feature is
described in
[subword-nmt page](https://github.com/rsennrich/subword-nmt#best-practice-advice-for-byte-pair-encoding-in-nmt).

The usage is basically the same as that of ```subword-nmt```. Assuming that L1
and L2 are the two languages (source/target languages), train the shared spm
model, and get resulting vocabulary for each:

```sh
cat {train_file}.L1 {train_file}.L2 | shuffle > train
spm_train --input=train --model_prefix=spm --vocab_size=8000 --character_coverage=0.9995
spm_encode --model=spm.model --generate_vocabulary < {train_file}.L1 > {vocab_file}.L1
spm_encode --model=spm.model --generate_vocabulary < {train_file}.L2 > {vocab_file}.L2
```

```shuffle``` command is used just in case because ```spm_train``` loads the
first 10M lines of corpus by default.


Then segment train/test corpus with ```--vocabulary``` option

```sh
spm_encode --model=spm.model --vocabulary={vocab_file}.L1 --vocabulary_threshold=50 < {test_file}.L1 > {test_file}.seg.L1
spm_encode --model=spm.model --vocabulary={vocab_file}.L2 --vocabulary_threshold=50 < {test_file}.L2 > {test_file}.seg.L2
```

## Advanced topics

*   [SentencePiece Experiments](doc/experiments.md)
*   [SentencePieceProcessor C++ API](doc/api.md)
*   [Use custom text normalization rules](doc/normalization.md)
*   [Use custom symbols](doc/special_symbols.md)
*   [Python Module](python/README.md)
*   [TensorFlow Module](tensorflow/README.md)
*   [Segmentation and training algorithms in detail]
