# Use custom symbols
SentencePiece model supports two types of special symbols.

## Control symbol
Control symbols are used to encode special indicators for the decoder to change the behavior dynamically.
Example includes the language indicators in multi-lingual models. `<s>` and `</s>` are reserved control symbols.
Control symbols must be inserted outside of the SentencePiece segmentation. Developers need to take the responsibility to insert these symbols in data generation and decoding.

It is guaranteed that control symbols have no corresponding surface strings in the original user input. Control symbols are decoded into empty strings.

## User defined symbol
User defined symbol is handled as one piece in any context. If this symbol is included in the input text, this symbol is always extracted as one piece.

## Specify special symbols in training time
Use `--control_symbols` and `--user_defined_symbols` flags as follows

```
% spm_train --control_symbols=<foo>,<bar> --user_defined_symbols=<user1>,<user2> --input=<input file> --model_prefix=<model file> --vocab_size=8000
```
