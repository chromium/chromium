Below are the steps to generate the 'fast_wordpiece_tokenizer_model.fb' file
that is used in
third_party/tensorflow_text/google/core/kernels/fast_wordpiece_tokenizer_test.cc:

(1) Create a vocab file '/tmp/test_vocab.txt' with the following content:
```a
abc
abcdefghi
##de
##defgxy
##deh
##f
##ghz
<unk>```

(2) Run the following command:
```
blaze run \
  third_party/tensorflow_text/google/tools:build_fast_wordpiece_model \
  -- --vocab_file=/tmp/test_vocab.txt --max_bytes_per_token 100 \
  --suffix_indicator="##" --unk_token="<unk>" \
  --output_model_file=/tmp/fast_wordpiece_tokenizer_model.fb
```

(3) Copy /tmp/fast_wordpiece_tokenizer_model.fb to
third_party/tensorflow_text/google/core/kernels/testdata/.


