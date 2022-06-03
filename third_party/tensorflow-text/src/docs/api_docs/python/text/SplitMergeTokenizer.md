description: Tokenizes a tensor of UTF-8 string into words according to labels.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.SplitMergeTokenizer" />
<meta itemprop="path" content="Stable" />
<meta itemprop="property" content="__init__"/>
<meta itemprop="property" content="split"/>
<meta itemprop="property" content="split_with_offsets"/>
<meta itemprop="property" content="tokenize"/>
<meta itemprop="property" content="tokenize_with_offsets"/>
</div>

# text.SplitMergeTokenizer

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/split_merge_tokenizer.py">View
source</a>

Tokenizes a tensor of UTF-8 string into words according to labels.

Inherits From: [`TokenizerWithOffsets`](../text/TokenizerWithOffsets.md),
[`Tokenizer`](../text/Tokenizer.md), [`Splitter`](../text/Splitter.md)

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.SplitMergeTokenizer()
</code></pre>

<!-- Placeholder for "Used in" -->


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

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/split_merge_tokenizer.py">View
source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>tokenize(
    input, labels, force_split_at_break_character=True
)
</code></pre>

Tokenizes a tensor of UTF-8 strings according to labels.

### Example:

```python
```

> > > strings = ["HelloMonday", "DearFriday"], labels = [[0, 1, 1, 1, 1, 0, 1,
> > > 1, 1, 1, 1], [0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0]] tokenizer =
> > > SplitMergeTokenizer() tokenizer.tokenize(strings, labels) [['Hello',
> > > 'Monday'], ['Dear', 'Friday']] ` `

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr> <td> `input` </td> <td> An N-dimensional `Tensor` or `RaggedTensor` of
UTF-8 strings. </td> </tr><tr> <td> `labels` </td> <td> An (N+1)-dimensional
`Tensor` or `RaggedTensor` of int32, with labels[i1...iN, j] being the
split(0)/merge(1) label of the j-th character for input[i1...iN]. Here split
means create a new word with this character and merge means adding this
character to the previous word. </td> </tr><tr> <td>
`force_split_at_break_character` </td> <td> bool indicates whether to force
start a new word after seeing a ICU defined whitespace character. When seeing
one or more ICU defined whitespace character: -if force_split_at_break_character
is set true, then create a new word at the first non-space character, regardless
of the label of that character, for instance:

```python
input="New York"
labels=[0, 1, 1, 0, 1, 1, 1, 1]
output tokens=["New", "York"]
```

```python
input="New York"
labels=[0, 1, 1, 1, 1, 1, 1, 1]
output tokens=["New", "York"]
```

```python
input="New York",
labels=[0, 1, 1, 1, 0, 1, 1, 1]
output tokens=["New", "York"]
```
-otherwise, whether to create a new word or not for the first non-space
character depends on the label of that character, for instance:

```python
input="New York",
labels=[0, 1, 1, 0, 1, 1, 1, 1]
output tokens=["NewYork"]
```

```python
input="New York",
labels=[0, 1, 1, 1, 1, 1, 1, 1]
output tokens=["NewYork"]
```

```python
input="New York",
labels=[0, 1, 1, 1, 0, 1, 1, 1]
output tokens=["New", "York"]
```
</td>
</tr>
</table>

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
A `RaggedTensor` of strings where `tokens[i1...iN, j]` is the string
content of the `j-th` token in `input[i1...iN]`
</td>
</tr>

</table>

<h3 id="tokenize_with_offsets"><code>tokenize_with_offsets</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/split_merge_tokenizer.py">View
source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>tokenize_with_offsets(
    input, labels, force_split_at_break_character=True
)
</code></pre>

Tokenizes a tensor of UTF-8 strings into tokens with [start,end) offsets.

### Example:

```python
```

> > > strings = ["HelloMonday", "DearFriday"], labels = [[0, 1, 1, 1, 1, 0, 1,
> > > 1, 1, 1, 1], [0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0]] tokenizer =
> > > SplitMergeTokenizer() result = tokenizer.tokenize_with_offsets(strings,
> > > labels) result[0].to_list() [['Hello', 'Monday'], ['Dear', 'Friday']]
> > > result[1].to_list() [[0, 5], [0, 4]] result[2].to_list() [[5, 11], [4,
> > > 10]] ` `

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr> <td> `input` </td> <td> An N-dimensional `Tensor` or `RaggedTensor` of
UTF-8 strings. </td> </tr><tr> <td> `labels` </td> <td> An (N+1)-dimensional
`Tensor` or `RaggedTensor` of int32, with labels[i1...iN, j] being the
split(0)/merge(1) label of the j-th character for input[i1...iN]. Here split
means create a new word with this character and merge means adding this
character to the previous word. </td> </tr><tr> <td>
`force_split_at_break_character` </td> <td> bool indicates whether to force
start a new word after seeing a ICU defined whitespace character. When seeing
one or more ICU defined whitespace character: -if force_split_at_break_character
is set true, then create a new word at the first non-space character, regardless
of the label of that character, for instance:

```python
input="New York"
labels=[0, 1, 1, 0, 1, 1, 1, 1]
output tokens=["New", "York"]
```

```python
input="New York"
labels=[0, 1, 1, 1, 1, 1, 1, 1]
output tokens=["New", "York"]
```

```python
input="New York",
labels=[0, 1, 1, 1, 0, 1, 1, 1]
output tokens=["New", "York"]
```
-otherwise, whether to create a new word or not for the first non-space
character depends on the label of that character, for instance:

```python
input="New York",
labels=[0, 1, 1, 0, 1, 1, 1, 1]
output tokens=["NewYork"]
```

```python
input="New York",
labels=[0, 1, 1, 1, 1, 1, 1, 1]
output tokens=["NewYork"]
```

```python
input="New York",
labels=[0, 1, 1, 1, 0, 1, 1, 1]
output tokens=["New", "York"]
```
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
* `tokens` is a `RaggedTensor` of strings where `tokens[i1...iN, j]` is
the string content of the `j-th` token in `input[i1...iN]`
* `start_offsets` is a `RaggedTensor` of int64s where
`start_offsets[i1...iN, j]` is the byte offset for the start of the
`j-th` token in `input[i1...iN]`.
* `end_offsets` is a `RaggedTensor` of int64s where
`end_offsets[i1...iN, j]` is the byte offset immediately after the
end of the `j-th` token in `input[i...iN]`.
</td>
</tr>

</table>
