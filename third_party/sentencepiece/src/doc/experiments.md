# SentencePiece Experiments

## Experiments 1 (subword vs word-based model)
### Experimental settings

*   Segmentation algorithms:
    *   **SentencePiece**: SentencePiece with a language-model based segmentation. (`--model_type=unigram`)
    *   **SentencePeice(BPE)**: SentencePiece with Byte Pair Encoding. [[Sennrich et al.](http://www.aclweb.org/anthology/P16-1162)]] (`--model_type=bpe`)
    *   **Moses**: [Moses tokenizer](https://github.com/moses-smt/mosesdecoder/blob/master/scripts/tokenizer/tokenizer.perl) for English.
    *   **KyTea**: [KyTea](http://www.phontron.com/kytea/) for Japanese.
    *   **MeCab**: [MeCab](http://taku910.github.io/mecab/) for Japanese.
    *   **neologd**: [MeCab with neologd](https://github.com/neologd/mecab-ipadic-neologd) for Japanese.
    *   **(Moses/KyTea)+SentencePiece**: Apply SentencePiece (Unigram) to pre-tokenized sentences. We have several variants with different tokenizers., e.g., **(Moses/MeCab)+SentencePiece**, **(MeCab/Moses)+SentencePiece**.
    *   *char**: Segments sentence by characters.

*   Data sets:
    *   [KFTT](http://www.phontron.com/kftt/index.html)

*   NMT parameters: ([Google’s Neural Machine Translation System](https://arxiv.org/pdf/1609.08144.pdf) is applied for all experiments.)
    *   Dropout prob: 0.2
    *   num nodes: 512
    *   num lstms: 6
    *   Decoder parameters (α and β) are optimized with development data.

*   Evaluation metrics:
    *   Case-sensitive BLEU on detokenized text with NIST scorer and KyTea segmenter. Used in-house rule-based detokenizer for Moses/KyTea/MeCab/neologd.


### Results (BLEU scores)
#### English to Japanese
|Setting|vocab size|BLEU(dev)|BLEU(test)|src #tokens/sent.|trg #tokens/sent.|
|:---|---:|---:|---:|---:|---:|
|SentencePiece|4k  (shared)|0.2857|0.2940|43.7478|29.6998|
|SentencePiece|8k  (shared)|0.2785|0.2955|30.9734|25.0540|
|SentencePiece|16k (shared)|0.2664|0.2862|27.1827|21.5326|
|SentencePiece|32k (shared)|0.2641|0.2849|25.0592|19.0840|
|SentencePiece(BPE)|8k  (shared)|0.2767|0.2947|31.7693|25.4331|
|(Moses/KyTea)+SentencePiece|8k (shared)|0.2900|0.2985|31.2719|29.9854|
|(Moses/MeCab)+SentencePiece|8k (shared)|0.2817|0.2950|31.4743|28.9537|
|(Moses/neologd)+SentencePiece|8k (shared)|0.2824|**0.3062**|31.2985|28.8645|
|Moses/Kytea|80k/80k|0.2576|0.2824|21.2513|23.2161|
|Moses/MeCab|80k/80k|0.2455|0.2780|21.2513|21.2033|
|Moses/neologd|80k/80k|0.2157|0.2378|21.2513|18.4768|
|Moses/SentencePiece|80k/8k|0.2475|0.2742|21.2513|22.9383|
|SentencePiece/KyTea|8k/80k|0.2778|0.2918|27.0429|23.2161|
|SentencePiece/MeCab|8k/80k|0.2673|0.2919|27.0429|21.2033|
|SentencePiece/neolgod|8k80k|0.2280|0.2494|27.0429|18.4768|
|Char|3k (shared)|0.2509|0.2679|109.8662|33.6963|

#### Japanese to English
|Setting|vocab size|BLEU(dev)|BLEU(test)|src #tokens/sent.|trg #tokens/sent.|
|:---|---:|---:|---:|---:|---:|
|SentencePiece|4k  (shared)|0.1970|**0.2179**|29.6998|43.7478|
|SentencePiece|8k  (shared)|0.1966|0.2162|25.0540|30.9734|
|SentencePiece|16k (shared)|0.1996|0.2160|21.5326|27.1827|
|SentencePiece|32k (shared)|0.1949|0.2159|19.0840|25.0592|
|SentencePiece(BPE)|8k  (shared)|0.1977|0.2173|25.4331|31.7693|
|(KyTea/Moses)+SentencePiece|8k (shared)|0.1921|0.2086|29.9854|31.2719|
|(MeCab/Moses)+SentencePiece|8k (shared)|0.1909|0.2049|28.9537|31.4743|
|(neologd/Moses)+SentencePiece|8k (shared)|0.1938|0.2137|28.8645|31.2985|
|KyTea/Moses|80k/80k|0.1707|0.2006|23.2161|21.2513|
|MeCab/Moses|80k/80k|0.1668|0.1892|21.2033|21.2513|
|neologd/Moses|80k/80k|0.1589|0.1836|18.4768|21.2513|
|SentencePiece/Moses|8k/80k|0.1727|0.1994|22.9383|21.2513|
|KyTea/SentencePiece|80k/8k|0.1939|0.2141|23.2161|27.0429|
|MeCab/SentencePiece|80k/8k|0.1892|0.2077|21.2033|27.0429|
|neologd/SentencePiece|80k/8k|0.1641|0.1804|18.4768|27.0429|
|Char|3k (shared)|0.0824|0.0918|33.6963|109.8662|

#### Discussion
* **SentencePiece (Unigram/BPE)** outperforms word-based methods **(Moses/KyTea/MeCab/neologd)** even with a smaller vocabulary (10% of word-based methods).
* The number of tokens to represent Japanese sentences is almost comparable between **SentencePiece (unigram)** and **KyTea**, though the vocabulary of **SentencePiece** is much smaller. It implies that Sentencepiece can effectively compress the sentences with a smaller vocabulary set.
* Pretokenization can slightly improve the BLEU scores in English to Japanese. In Japanese to English translation, pretokenization doesn't help to improve BLEU.
* **Neologd** shows poor BLEU score. Tokenizing sentences with a large named entity dictionary might not be effective in neural-based text processing.
* **SentencePiece(Unigram)** shows slightly better text compression ratio than **BPE**, but no significant differences in BLEU score.
* The selection of vocabulary size for SentencePiece is sensitive in English to Japanese. This is probably because the vocabulary size will drastically affect the tokenization results in Japanese which has no explicit spaces between words.

## Experiments 2 (subwording with various pre-tokenizations)
### Experimental settings
We have evaluated SentencePiece segmentation with the following configurations.

*   Segmentation algorithms:
    *   **BPE** (Byte Pair
        Encoding) [[Sennrich et al.](http://www.aclweb.org/anthology/P16-1162)]] (`--model_type=bpe`)
    *   **Unigram**. Language-model based segmentation. (`--model_type=unigram`)

*   pretokenization methods:
    *   **NoPretok**: No pretokenization. We train SentencePiece directly from
        raw sentences (`--split_by_whitespace=false`).
    *   **WsPretok**: Trains SentencePiece model from the sentences tokenized by
        whitespaces (`--split_by_whitespace=true`). When handling CJK, this setting is almost equivalent to **NoPretok**.
    *   **MosesPretok**: Trains SentencePiece model from sentences tokenized
        by [Moses tokenizer](https://github.com/moses-smt/mosesdecoder/blob/master/scripts/tokenizer/tokenizer.perl). We used [KyTea](http://www.phontron.com/kytea/) for
        Japanese and in-house segmenters for Korean and Chinese respectively.

*   NMT parameters: ([Google’s Neural Machine Translation System](https://arxiv.org/pdf/1609.08144.pdf) is applied for all experiments.)
    *   16k shared vocabulary (Shares the same vocabulary for source and
        target. We train single SentencePiece model by concatenating raw source
        and target sentences.)
    *   Dropout prob: 0.2
    *   num nodes: 512
    *   num lstms: 8

*   Evaluation metrics:
    *   Case-sensitive BLEU on detokenized text with NIST scorer.
    *   For CJK, the same word segmenters are applied prior to NIST scorer.
    *   No detokenizer is applied for **NoPretok** and **WsPretok**, which can
        directly emit detokenized sentences.
    *   Applied [Moses detokenizer](https://github.com/moses-smt/mosesdecoder/blob/master/scripts/tokenizer/detokenizer.perl) and in-house rule-based detokenizer (CJK) for **MosesPretok**.

*   Data sets:
    *   [KFTT](http://www.phontron.com/kftt/index.html)
    *   [MultiUN](http://opus.lingfil.uu.se/MultiUN.php) (First 5M and next
        5k/5k sentences are used for training and development/testing respectively.)
    *   [WMT16](https://www.statmt.org/wmt16/)
    *   In-house: (Used 5M parallel sentences for training)

**NoPretok** and **WsPretok** do not use any language-dependent resources.
**BPE**+**MosePretok** is almost the same configuration used in [[Sennrich et al.](http://www.aclweb.org/anthology/P16-1162)] and [[Wu et al.](https://arxiv.org/pdf/1609.08144.pdf)].

### Results (BLEU scores)
|Language Pair|BPE(NoPretok)|BPE(WsPretok)|BPE(MosesPretok)|Unigram(NoPretok)|Unigram(WsPretok)|Unigram(MosesPretok)
|---|---|---|---|---|---|---|
|KFTT en-ja|	0.2796|	0.281|	0.286|	0.2806|	0.280|	0.2871|
|KFTT ja-en|	0.1943|	0.208|	0.1967|	0.1985|	0.2148|	0.198|
|MultiUN ar-en|	0.5268|	0.5414|	0.5381|	0.5317|	0.5449|	0.5401|
|MultiUN en-ar|	0.4039|	0.4147|	0.4012|	0.4084|	0.4172|	0.3991|
|MultiUN en-zh|	0.4155|	0.4186|	0.395|	0.4214|	0.4165|	0.399|
|MultiUN zh-en|	0.46|	0.4716|	0.4806|	0.4644|	0.4711|	0.4759|
|In house en-ko|	0.178|	0.1851|	0.1893|	0.1846|	0.1872|	0.1890|
|In house ko-en|	0.1786|	0.1954|	0.1994|	0.1845|	0.1956|	0.2015|
|WMT16 cs-en|	0.1987|	0.2252|	0.2231|	0.2164|	0.2228|	0.2238|
|WMT16 de-en|	0.3194|	0.3348|	0.3374|	0.3261|	0.3375|	0.3398|
|WMT16 en-cs|	0.1607|	0.1827|	0.1812|	0.1722|	0.1778|	0.179|
|WMT16 en-de|	0.2847|	0.3029|	0.3013|	0.2946|	0.3000|	0.3053|
|WMT16 en-fi|	0.1434|	0.1528|	0.1499|	0.1472|	0.1568|	0.1517|
|WMT16 en-ru|	0.1884|	0.1973|	0.1989|	0.19|	0.1982|	0.1903|
|WMT16 fi-en|	0.1775|	0.1867|	0.1877|	0.182|	0.1882|	0.1865|
|WMT16 ru-en|	0.2042|	0.2229|	0.2194|	0.2087|	0.2201|	0.2155|

*   **MosesPretok** does not always improve BLEU scores. Comparable
    accuracy can be obtained without using language-dependent resources in many
    language pairs.
*   Whitespace pretokenization is a reasonable choice. It does not use language-specific resources.
*   **NoPretok** shows poor BLEU scores. Unigrams are more robust than BPE when no pretokenizer is applied.
