# Training options

The training options for the `spm_train` can be listed using `spm_train --help`. Since the standard `pip install` of sentencepiece does not necessarily install `spm_train`, the options are also listed here.

```
Usage: ../build/src/spm_train [options] files

   --input (comma separated list of input sentences)  type: std::string default: ""
   --input_format (Input format. Supported format is `text` or `tsv`.)  type: std::string default: ""
   --model_prefix (output model prefix)  type: std::string default: ""
   --model_type (model algorithm: unigram, bpe, word or char)  type: std::string default: "unigram"
   --vocab_size (vocabulary size)  type: int32 default: 8000
   --accept_language (comma-separated list of languages this model can accept)  type: std::string default: ""
   --self_test_sample_size (the size of self test samples)  type: int32 default: 0
   --character_coverage (character coverage to determine the minimum symbols)  type: double default: 0.9995
   --input_sentence_size (maximum size of sentences the trainer loads)  type: std::uint64_t default: 0
   --shuffle_input_sentence (Randomly sample input sentences in advance. Valid when --input_sentence_size > 0)  type: bool default: true
   --seed_sentencepiece_size (the size of seed sentencepieces)  type: int32 default: 1000000
   --shrinking_factor (Keeps top shrinking_factor pieces with respect to the loss)  type: double default: 0.75
   --num_threads (number of threads for training)  type: int32 default: 16
   --num_sub_iterations (number of EM sub-iterations)  type: int32 default: 2
   --max_sentencepiece_length (maximum length of sentence piece)  type: int32 default: 16
   --max_sentence_length (maximum length of sentence in byte)  type: int32 default: 4192
   --split_by_unicode_script (use Unicode script to split sentence pieces)  type: bool default: true
   --split_by_number (split tokens by numbers (0-9))  type: bool default: true
   --split_by_whitespace (use a white space to split sentence pieces)  type: bool default: true
   --split_digits (split all digits (0-9) into separate pieces)  type: bool default: false
   --treat_whitespace_as_suffix (treat whitespace marker as suffix instead of prefix.)  type: bool default: false
   --allow_whitespace_only_pieces (allow pieces that only contain (consecutive) whitespace tokens)  type: bool default: false
   --control_symbols (comma separated list of control symbols)  type: std::string default: ""
   --control_symbols_file (load control_symbols from file.)  type: std::string default: ""
   --user_defined_symbols (comma separated list of user defined symbols)  type: std::string default: ""
   --user_defined_symbols_file (load user_defined_symbols from file.)  type: std::string default: ""
   --required_chars (UTF8 characters in this flag are always used in the character set regardless of --character_coverage)  type: std::string default: ""
   --required_chars_file (load required_chars from file.)  type: std::string default: ""
   --byte_fallback (decompose unknown pieces into UTF-8 byte pieces)  type: bool default: false
   --vocabulary_output_piece_score (Define score in vocab file)  type: bool default: true
   --normalization_rule_name (Normalization rule name. Choose from nfkc or identity)  type: std::string default: "nmt_nfkc"
   --normalization_rule_tsv (Normalization rule TSV file. )  type: std::string default: ""
   --denormalization_rule_tsv (Denormalization rule TSV file.)  type: std::string default: ""
   --add_dummy_prefix (Add dummy whitespace at the beginning of text)  type: bool default: true
   --remove_extra_whitespaces (Removes leading, trailing, and duplicate internal whitespace)  type: bool default: true
   --hard_vocab_limit (If set to false, --vocab_size is considered as a soft limit.)  type: bool default: true
   --use_all_vocab (If set to true, use all tokens as vocab. Valid for word/char models.)  type: bool default: false
   --unk_id (Override UNK (<unk>) id.)  type: int32 default: 0
   --bos_id (Override BOS (<s>) id. Set -1 to disable BOS.)  type: int32 default: 1
   --eos_id (Override EOS (</s>) id. Set -1 to disable EOS.)  type: int32 default: 2
   --pad_id (Override PAD (<pad>) id. Set -1 to disable PAD.)  type: int32 default: -1
   --unk_piece (Override UNK (<unk>) piece.)  type: std::string default: "<unk>"
   --bos_piece (Override BOS (<s>) piece.)  type: std::string default: "<s>"
   --eos_piece (Override EOS (</s>) piece.)  type: std::string default: "</s>"
   --pad_piece (Override PAD (<pad>) piece.)  type: std::string default: "<pad>"
   --unk_surface (Dummy surface string for <unk>. In decoding <unk> is decoded to `unk_surface`.)  type: std::string default: " ⁇ "
   --train_extremely_large_corpus (Increase bit depth for unigram tokenization.)  type: bool default: false
   --random_seed (Seed value for random generator.)  type: uint32 default: 4294967295
   --enable_differential_privacy (Whether to add DP while training. Currently supported only by UNIGRAM model.)  type: bool default: false
   --differential_privacy_noise_level (Amount of noise to add for DP)  type: float default: 0
   --differential_privacy_clipping_threshold (Threshold for clipping the counts for DP)  type: std::uint64_t default: 0
   --help (show help)  type: bool default: false
   --version (show version)  type: bool default: false
   --minloglevel (Messages logged at a lower level than this don't actually get logged anywhere)  type: int default: 0
```
