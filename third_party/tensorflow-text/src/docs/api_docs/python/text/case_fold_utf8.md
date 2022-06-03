description: Applies case folding to every UTF-8 string in the input.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.case_fold_utf8" />
<meta itemprop="path" content="Stable" />
</div>

# text.case_fold_utf8

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/normalize_ops.py">View
source</a>

Applies case folding to every UTF-8 string in the input.

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.case_fold_utf8(
    input, name=None
)
</code></pre>

<!-- Placeholder for "Used in" -->

The input is a `Tensor` or `RaggedTensor` of any shape, and the resulting output
has the same shape as the input. Note that NFKC normalization is implicitly
applied to the strings.

#### For example:

```python
```

> > > case_fold_utf8(['The Quick-Brown', ... 'CAT jumped over', ... 'the lazy
> > > dog !! '] tf.Tensor(['the quick-brown' 'cat jumped over' 'the lazy dog !!
> > > '], shape=(3,), dtype=string) ` `

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`input`
</td>
<td>
A `Tensor` or `RaggedTensor` of UTF-8 encoded strings.
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
A `Tensor` or `RaggedTensor` of type string, with case-folded contents.
</td>
</tr>

</table>
