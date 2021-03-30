description: Split input by delimiters that match a regex pattern; returns
offsets.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.regex_split_with_offsets" />
<meta itemprop="path" content="Stable" />
</div>

# text.regex_split_with_offsets

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/regex_split_ops.py">View source</a>



Split `input` by delimiters that match a regex pattern; returns offsets.

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.regex_split_with_offsets(
    input, delim_regex_pattern, keep_delim_regex_pattern=&#x27;&#x27;, name=None
)
</code></pre>

<!-- Placeholder for "Used in" -->

`regex_split_with_offsets` will split `input` using delimiters that match a
regex pattern in `delim_regex_pattern`. Here is an example:

```
text_input=["hello there"]
# split by whitespace
result, begin, end = regex_split_with_offsets(text_input, "\s")
# result = [["hello", "there"]]
# begin = [[0, 7]]
# end = [[5, 11]]
```

By default, delimiters are not included in the split string results.
Delimiters may be included by specifying a regex pattern
`keep_delim_regex_pattern`. For example:

```
text_input=["hello there"]
# split by whitespace
result, begin, end = regex_split_with_offsets(text_input, "\s", "\s")
# result = [["hello", " ", "there"]]
# begin = [[0, 5, 7]]
# end = [[5, 6, 11]]
```

If there are multiple delimiters in a row, there are no empty splits emitted.
For example:

```
text_input=["hello  there"]  # two continuous whitespace characters
# split by whitespace
result, begin, end = regex_split_with_offsets(text_input, "\s")
# result = [["hello", "there"]]
```

See https://github.com/google/re2/wiki/Syntax for the full list of supported
expressions.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`input`
</td>
<td>
A Tensor or RaggedTensor of string input.
</td>
</tr><tr>
<td>
`delim_regex_pattern`
</td>
<td>
A string containing the regex pattern of a delimiter.
</td>
</tr><tr>
<td>
`keep_delim_regex_pattern`
</td>
<td>
(optional) Regex pattern of delimiters that should
be kept in the result.
</td>
</tr><tr>
<td>
`name`
</td>
<td>
(optional) Name of the op.
</td>
</tr>
</table>

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Returns</h2></th></tr>
<tr class="alt">
<td colspan="2">
A tuple of RaggedTensors containing:
(split_results, begin_offsets, end_offsets)
where tokens is of type string, begin_offsets and end_offsets are of type
int64.
</td>
</tr>

</table>
