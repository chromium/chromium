# SentencePieceProcessor C++ API

## Load SentencePiece model
To start working with the SentencePiece model, you will want to include the `sentencepiece_processor.h` header file.
Then instantiate sentencepiece::SentencePieceProcessor class and calls `Load` method to load the model with file path or std::istream.

```C++
#include <sentencepiece_processor.h>

sentencepiece::SentencePieceProcessor processor;
const auto status = processor.Load("//path/to/model.model");
if (!status.ok()) {
   std::cerr << status.ToString() << std::endl;
   // error
}

// You can also load a serialized model from std::string.
// const std::stirng str = // Load blob contents from a file.
// auto status = processor.LoadFromSerializedProto(str);
```

## Tokenize text (preprocessing)
Calls `SentencePieceProcessor::Encode` method to tokenize text.

```C++
std::vector<std::string> pieces;
processor.Encode("This is a test.", &pieces);
for (const std::string &token : pieces) {
  std::cout << token << std::endl;
}
```

You will obtain the sequence of vocab ids as follows:

```C++
std::vector<int> ids;
processor.Encode("This is a test.", &ids);
for (const int id : ids) {
  std::cout << id << std::endl;
}
```

## Detokenize text (postprocessing)
Calls `SentencePieceProcessor::Decode` method to detokenize a sequence of pieces or ids into a text. Basically it is guaranteed that the detokenization is an inverse operation of Encode, i.e., `Decode(Encode(Normalize(input))) == Normalize(input)`.

```C++
std::vector<std::string> pieces = { "▁This", "▁is", "▁a", "▁", "te", "st", "." };   // sequence of pieces
std::string text
processor.Decode(pieces, &text);
std::cout << text << std::endl;

std::vector<int> ids = { 451, 26, 20, 3, 158, 128, 12  };   // sequence of ids
processor.Decode(ids, &text);
std::cout << text << std::endl;
```

## Sampling (subword regularization)
Calls `SentencePieceProcessor::SampleEncode` method to sample one segmentation.

```C++
std::vector<std::string> pieces;
processor.SampleEncode("This is a test.", &pieces, -1, 0.2);

std::vector<int> ids;
processor.SampleEncode("This is a test.", &ids, -1, 0.2);
```
SampleEncode has two sampling parameters, `nbest_size` and `alpha`, which correspond to `l` and `alpha` in the [original paper](https://arxiv.org/abs/1804.10959). When `nbest_size` is -1, one segmentation is sampled from all hypothesis with forward-filtering and backward sampling algorithm.

## Training
Calls `SentencePieceTrainer::Train` function to train sentencepiece model. You can pass the same parameters of [spm_train](https://github.com/google/sentencepiece#train-sentencepiece-model) as a single string.

```C++
#include <sentencepiece_trainer.h>

sentencepiece::SentencePieceTrainer::Train("--input=test/botchan.txt --model_prefix=m --vocab_size=1000");
```

## ImmutableSentencePieceText
You will want to use `ImmutableSentencePieceText` class to obtain the pieces and ids at the same time.
This proto also encodes a utf8-byte offset of each piece over user input or detokenized text.

```C++
#include <sentencepiece_processor.h>

sentencepiece::ImmutableSentencePieceText spt;

// Encode
processor.Encode("This is a test.", spt.mutable_proto());

// or
// spt = processor.EncodeAsImmutableProto("This is a test.");

std::cout << spt.text() << std::endl;   // This is the same as the input.
for (const auto &piece : spt.pieces()) {
   std::cout << piece.begin() << std::endl;   // beginning of byte offset
   std::cout << piece.end() << std::endl;     // end of byte offset
   std::cout << piece.piece() << std::endl;   // internal representation.
   std::cout << piece.surface() << std::endl; // external representation. spt.text().substr(begin, end - begin) == surface().
   std::cout << piece.id() << std::endl;      // vocab id
}

// Decode
processor.Decode({10, 20, 30}, spt.mutable_proto());
std::cout << spt.text() << std::endl;   // This is the same as the decoded string.
for (const auto &piece : spt.pieces()) {
   // the same as above.
}
```

## Vocabulary management
You will want to use the following methods to obtain ids from/to pieces.

```C++
processor.GetPieceSize();   // returns the size of vocabs.
processor.PieceToId("foo");  // returns the vocab id of "foo"
processor.IdToPiece(10);     // returns the string representation of id 10.
processor.IsUnknown(0);      // returns true if the given id is an unknown token. e.g., <unk>
processor.IsControl(10);     // returns true if the given id is a control token. e.g., <s>, </s>
```

## Extra Options
Use `SetEncodeExtraOptions` and `SetDecodeExtraOptions` methods to set extra options for encoding and decoding respectively. These methods need to be called just after `Load` methods.

```C++
processor.SetEncodeExtraOptions("bos:eos");   // add <s> and </s>.
processor.SetEncodeExtraOptions("reverse:bos:eos");   // reverse the input and then add <s> and </s>.

processor.SetDecodeExtraOptions("reverse");   // the decoder's output is reversed.
```
