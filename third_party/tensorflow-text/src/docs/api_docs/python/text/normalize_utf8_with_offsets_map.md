description: Normalizes each UTF-8 string in the input tensor using the specified rule.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.normalize_utf8_with_offsets_map" />
<meta itemprop="path" content="Stable" />
</div>

# text.normalize_utf8_with_offsets_map

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/normalize_ops.py">View source</a>



Normalizes each UTF-8 string in the input tensor using the specified rule.

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.normalize_utf8_with_offsets_map(
    input, normalization_form=&#x27;NFKC&#x27;, name=None
)
</code></pre>



<!-- Placeholder for "Used in" -->

Returns normalized strings and an offset map used by another operation to map
post-normalized string offsets to pre-normalized string offsets.

See http://unicode.org/reports/tr15/

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`input`
</td>
<td>
A `Tensor` or `RaggedTensor` of type string. (Must be UTF-8.)
normalization_form: One of the following string values ('NFC', 'NFKC').
Default is 'NFKC'.
</td>
</tr><tr>
<td>
`name`
</td>
<td>
The name for this op (optional).
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Returns</h2></th></tr>
<tr class="alt">
<td colspan="2">
A tuple of (results, offsets_map) where:
</td>
</tr>
<tr>
<td>
`results`
</td>
<td>
A `Tensor` or `RaggedTensor` of type string, with normalized
contents.
</td>
</tr><tr>
<td>
`offsets_map`
</td>
<td>
A `Tensor` or `RaggedTensor` of type `variant`, used to map
the post-normalized string offsets to pre-normalized string offsets. It
has the same shape as the results tensor. offsets_map is an input to
`find_source_offsets` op.
</td>
</tr>
</table>

