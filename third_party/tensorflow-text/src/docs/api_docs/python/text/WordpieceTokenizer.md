description: Tokenizes a tensor of UTF-8 string tokens into subword pieces.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.WordpieceTokenizer" />
<meta itemprop="path" content="Stable" />
<meta itemprop="property" content="__init__"/>
<meta itemprop="property" content="split"/>
<meta itemprop="property" content="split_with_offsets"/>
<meta itemprop="property" content="tokenize"/>
<meta itemprop="property" content="tokenize_with_offsets"/>
</div>

# text.WordpieceTokenizer

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/wordpiece_tokenizer.py">View
source</a>

Tokenizes a tensor of UTF-8 string tokens into subword pieces.

Inherits From: [`TokenizerWithOffsets`](../text/TokenizerWithOffsets.md),
[`Tokenizer`](../text/Tokenizer.md), [`Splitter`](../text/Splitter.md)

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.WordpieceTokenizer(
    vocab_lookup_table, suffix_indicator=&#x27;##&#x27;, max_bytes_per_word=100,
    max_chars_per_token=None, token_out_type=dtypes.int64,
    unknown_token=&#x27;[UNK]&#x27;, split_unknown_characters=False
)
</code></pre>

<!-- Placeholder for "Used in" -->

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`vocab_lookup_table`
</td>
<td>
A lookup table implementing the LookupInterface
containing the vocabulary of subwords or a string which is the file path
to the vocab.txt file.
</td>
</tr><tr>
<td>
`suffix_indicator`
</td>
<td>
(optional) The characters prepended to a wordpiece to
indicate that it is a suffix to another subword. Default is '##'.
</td>
</tr><tr>
<td>
`max_bytes_per_word`
</td>
<td>
(optional) Max size of input token. Default is 100.
</td>
</tr><tr>
<td>
`max_chars_per_token`
</td>
<td>
(optional) Max size of subwords, excluding suffix
indicator. If known, providing this improves the efficiency of decoding
long words.
</td>
</tr><tr>
<td>
`token_out_type`
</td>
<td>
(optional) The type of the token to return. This can be
`tf.int64` or `tf.int32` IDs, or `tf.string` subwords. The default is
`tf.int64`.
</td>
</tr><tr>
<td>
`unknown_token`
</td>
<td>
(optional) The string value to substitute for an unknown
token. Default is "[UNK]". If set to `None`, no substitution occurs.
If `token_out_type` is `tf.int32`/`tf.int64`, the `vocab_lookup_table`
is used (after substitution) to convert the unknown token to an integer.
</td>
</tr><tr>
<td>
`split_unknown_characters`
</td>
<td>
(optional) Whether to split out single unknown
characters as subtokens. If False (default), words containing unknown
characters will be treated as single unknown tokens.
</td>
</tr>
</table>

## Methods

<h3 id="split"><code>split</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/tokenization.py">View
source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>split(
    input
)
</code></pre>

Splits the strings from the input tensor.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`input`
</td>
<td>
An N-dimensional UTF-8 string (or optionally integer) `Tensor` or
`RaggedTensor`.
</td>
</tr>
</table>

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
An N+1-dimensional UTF-8 string or integer `Tensor` or `RaggedTensor`.
For each string from the input tensor, the final, extra dimension contains
the pieces that string was split into.
</td>
</tr>

</table>

<h3 id="split_with_offsets"><code>split_with_offsets</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/tokenization.py">View
source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>split_with_offsets(
    input
)
</code></pre>

Splits the input tensor, returns the resulting pieces with offsets.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`input`
</td>
<td>
An N-dimensional UTF-8 string (or optionally integer) `Tensor` or
`RaggedTensor`.
</td>
</tr>
</table>

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
A tuple `(pieces, start_offsets, end_offsets)` where:

*   `pieces` is an N+1-dimensional UTF-8 string or integer `Tensor` or
    `RaggedTensor`.
*   `start_offsets` is an N+1-dimensional integer `Tensor` or `RaggedTensor`
    containing the starting indices of each piece (byte indices for input
    strings).
*   `end_offsets` is an N+1-dimensional integer `Tensor` or `RaggedTensor`
    containing the exclusive ending indices of each piece (byte indices for
    input strings). </td> </tr>

</table>

<h3 id="tokenize"><code>tokenize</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/wordpiece_tokenizer.py">View
source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>tokenize(
    input
)
</code></pre>

Tokenizes a tensor of UTF-8 string tokens further into subword tokens.

### Example:

```python
```

> > > tokens = [["they're", "the", "greatest"]], tokenizer =
> > > WordpieceTokenizer(vocab, token_out_type=tf.string)
> > > tokenizer.tokenize(tokens) [[['they', "##'", '##re'], ['the'], ['great',
> > > '##est']]] ` `

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`input`
</td>
<td>
An N-dimensional `Tensor` or `RaggedTensor` of UTF-8 strings.
</td>
</tr>
</table>

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
A `RaggedTensor` of tokens where `tokens[i1...iN, j]` is the string
contents (or ID in the vocab_lookup_table representing that string)
of the `jth` token in `input[i1...iN]`
</td>
</tr>

</table>

<h3 id="tokenize_with_offsets"><code>tokenize_with_offsets</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/wordpiece_tokenizer.py">View
source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>tokenize_with_offsets(
    input
)
</code></pre>

Tokenizes a tensor of UTF-8 string tokens further into subword tokens.

### Example:

```python
```

> > > tokens = [["they're", "the", "greatest"]], tokenizer =
> > > WordpieceTokenizer(vocab, token_out_type=tf.string) result =
> > > tokenizer.tokenize_with_offsets(tokens) result[0].to_list() # subwords
> > > [[['they', "##'", '##re'], ['the'], ['great', '##est']]]
> > > result[1].to_list() # start offsets [[[0, 4, 5], [0], [0, 5]]]
> > > result[2].to_list() # end offsets [[[4, 5, 7], [3], [5, 8]]] ` `

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`input`
</td>
<td>
An N-dimensional `Tensor` or `RaggedTensor` of UTF-8 strings.
</td>
</tr>
</table>

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
A tuple `(tokens, start_offsets, end_offsets)` where:

*   `tokens[i1...iN, j]` is a `RaggedTensor` of the string contents (or ID in
    the vocab_lookup_table representing that string) of the `jth` token in
    `input[i1...iN]`.
*   `start_offsets[i1...iN, j]` is a `RaggedTensor` of the byte offsets for the
    inclusive start of the `jth` token in `input[i1...iN]`.
*   `end_offsets[i1...iN, j]` is a `RaggedTensor` of the byte offsets for the
    exclusive end of the `jth` token in `input[i`...iN]` (exclusive, i.e., first
    byte after the end of the token). </td> </tr>

</table>
