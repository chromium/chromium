# Use custom normalization rule
By default, SentencePiece normalizes the input sentence with a variant of Unicode
[NFKC](https://en.wikipedia.org/wiki/Unicode_equivalence).

SentencePiece allows us to define custom normalization rule, which is stored in the model file.

## Use pre-defined normalization rule
SentencePiece provides the following pre-defined normalization rule. It is recommended to use one of them unless you have any special reasons.

* **nmt_nfkc**: [NFKC](https://en.wikipedia.org/wiki/Unicode_equivalence) normalization with some additional normalization around spaces. (default)
* **nfkc**:  original NFKC normalization.
* **nmt_nfkc_cf**: nmt_nfkc + [Unicode case folding](https://www.w3.org/International/wiki/Case_folding) (mostly lower casing)
* **nfkc_cf**: nfkc + [Unicode case folding](https://www.w3.org/International/wiki/Case_folding).
* **identity**: no normalization

You can choose the normalization rule with `--normalization_rule_name` flag.
```
% spm_train --normalization_rule_name=identity --input=<input> --model_prefix=<model file> --vocab_size=8000                                                                                                                                                                                
```

NOTE: Due to the limitation of normalization algorithm, full NFKC normalization is not implemented. [builder.h] describes example character sequences not normalized by our NFKC implementation.

The difference between **nmt_nfkc** and **nfkc** can be found via ```diff -u data/nfkc.tsv data/nmt_nfkc.tsv``` command.

## Use custom normalization rule
The normalization is performed with user-defined string-to-string mappings and leftmost longest matching.

You can use custom normalization rule by preparing a TSV file formatted as follows:
```
41 302 300      1EA6
41 302 301      1EA4
41 302 303      1EAA
...
```
In this sample, UCS4 sequence [41 302 300] (hex) is converted into [1EA6] (hex). When there are ambiguities in the conversions, the longest rule is used.
Note that the tab is used as a delimiter for source and target sequence and space is used as a delimiter for UCS4 characters. We can make the target sequence empty to remove some specific characters from the text.
See [data/nfkc.tsv](../data/nfkc.tsv) as an example. Once a TSV file is prepared, you can specify it with `--normalization_rule_tsv` flag.
```
% spm_train --normalization_rule_tsv=<rule tsv file> --input=<input> --model_prefix=<model file> --vocab_size=8000                                                                                                                                                                             
```

`<model file>` embeds the normalization rule so the same normalization rule is applied when `<model file>` is used.


## Command line tool to perform normalization
```
% spm_normalize --model=<model_file> file1 file2.. 
% spm_normalize --normalization_rule_tsv=custom.tsv file1 file2..
```
The first command line uses the normalization rule embedded in the model file. The second command line uses the normalization rule in TSV file and is useful to make normalization rule interactively.
