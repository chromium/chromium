description: A Trimmer that allocates a length budget to segments in order.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.WaterfallTrimmer" />
<meta itemprop="path" content="Stable" />
<meta itemprop="property" content="__init__"/>
<meta itemprop="property" content="generate_mask"/>
<meta itemprop="property" content="generate_masks"/>
<meta itemprop="property" content="trim"/>
</div>

# text.WaterfallTrimmer

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/trimmer_ops.py">View source</a>



A `Trimmer` that allocates a length budget to segments in order.

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.WaterfallTrimmer(
    max_seq_length, axis=-1
)
</code></pre>



<!-- Placeholder for "Used in" -->

A `Trimmer` that allocates a length budget to segments in order, then
truncates large sequences using a waterfall strategy, then drops elements in a
sequence according to a max sequence length budget. See `generate_mask()`
for more details.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`max_seq_length`
</td>
<td>
a scalar `Tensor` or a 1D `Tensor` of type int32 that
describes the number max number of elements allowed in a batch. If a
scalar is provided, the value is broadcasted and applied to all values
across the batch.
</td>
</tr><tr>
<td>
`axis`
</td>
<td>
Axis to apply trimming on.
</td>
</tr>
</table>



## Methods

<h3 id="generate_mask"><code>generate_mask</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/trimmer_ops.py">View source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>generate_mask(
    segments
)
</code></pre>

Calculates a truncation mask given a per-batch budget.

Calculate a truncation mask given a budget of the max number of items for
each or all batch row. The allocation of the budget is done using a
'waterfall' algorithm. This algorithm allocates quota in a left-to-right
manner and fill up the buckets until we run out of budget.

For example if the budget of [5] and we have segments of size
[3, 4, 2], the truncate budget will be allocated as [3, 2, 0].

The budget can be a scalar, in which case the same budget is broadcasted
and applied to all batch rows. It can also be a 1D `Tensor` of size
`batch_size`, in which each batch row i will have a budget corresponding to
`per_batch_quota[i]`.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`segments`
</td>
<td>
A list of `RaggedTensor` each w/ a shape of [num_batch,
(num_items)].
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
a list with len(segments) of `RaggedTensor`s, see superclass for details.
</td>
</tr>

</table>



<h3 id="generate_masks"><code>generate_masks</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/trimmer_ops.py">View source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>@abc.abstractmethod</code>
<code>generate_masks(
    segments
)
</code></pre>

Generates a boolean mask specifying which portions of `segments` to drop.

Users should be able to use the results of generate_masks() to drop items
in segments using `tf.ragged.boolean_mask(seg, mask)`.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`segments`
</td>
<td>
A list of `RaggedTensor` each w/ a shape of [num_batch,
(num_items)].
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
a list with len(segments) number of items and where each item is a
`RaggedTensor` with the same shape as its counterpart in `segments` and
with a boolean dtype where each value is True if the corresponding
value in `segments` should be kept and False if it should be dropped
instead.
</td>
</tr>

</table>



<h3 id="trim"><code>trim</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/trimmer_ops.py">View source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>trim(
    segments
)
</code></pre>

Truncate the list of `segments`.

Truncate the list of `segments` using the truncation strategy defined by
`generate_masks`.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`segments`
</td>
<td>
A list of `RaggedTensor`s w/ shape [num_batch, (num_items)].
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
a list of `RaggedTensor`s with len(segments) number of items and where
each item has the same shape as its counterpart in `segments` and
with unwanted values dropped. The values are dropped according to the
`TruncationStrategy` defined.
</td>
</tr>

</table>





