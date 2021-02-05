description: An ItemSelector implementation that randomly selects items in a batch.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.RandomItemSelector" />
<meta itemprop="path" content="Stable" />
<meta itemprop="property" content="__init__"/>
<meta itemprop="property" content="get_selectable"/>
<meta itemprop="property" content="get_selection_mask"/>
</div>

# text.RandomItemSelector

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/item_selector_ops.py">View source</a>



An `ItemSelector` implementation that randomly selects items in a batch.

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.RandomItemSelector(
    max_selections_per_batch, selection_rate, unselectable_ids=None, shuffle_fn=None
)
</code></pre>



<!-- Placeholder for "Used in" -->

`RandomItemSelector` randomly selects items in a batch subject to
restrictions given (max_selections_per_batch, selection_rate and
unselectable_ids).

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`max_selections_per_batch`
</td>
<td>
An int of the max number of items to mask out.
</td>
</tr><tr>
<td>
`selection_rate`
</td>
<td>
The rate at which items are randomly selected.
</td>
</tr><tr>
<td>
`unselectable_ids`
</td>
<td>
(optional) A list of python ints or 1D `Tensor` of ints
which are ids that will be not be masked.
</td>
</tr><tr>
<td>
`shuffle_fn`
</td>
<td>
(optional) A function that shuffles a 1D `Tensor`. Default
uses `tf.random.shuffle`.
</td>
</tr>
</table>





<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Attributes</h2></th></tr>

<tr>
<td>
`max_selections_per_batch`
</td>
<td>

</td>
</tr><tr>
<td>
`selection_rate`
</td>
<td>

</td>
</tr><tr>
<td>
`shuffle_fn`
</td>
<td>

</td>
</tr><tr>
<td>
`unselectable_ids`
</td>
<td>

</td>
</tr>
</table>



## Methods

<h3 id="get_selectable"><code>get_selectable</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/item_selector_ops.py">View source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>get_selectable(
    input_ids, axis
)
</code></pre>

Return a boolean mask of items that can be chosen for selection.


<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`input_ids`
</td>
<td>
a `RaggedTensor`.
</td>
</tr><tr>
<td>
`axis`
</td>
<td>
axis to apply selection on.
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
a `RaggedTensor` with dtype of bool and with shape
`input_ids.shape[:axis]`. Its values are True if the
corresponding item (or broadcasted subitems) should be considered for
masking. In the default implementation, all `input_ids` items that are not
listed in `unselectable_ids` (from the class arg) are considered
selectable.
</td>
</tr>

</table>



<h3 id="get_selection_mask"><code>get_selection_mask</code></h3>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/item_selector_ops.py">View source</a>

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>get_selection_mask(
    input_ids, axis
)
</code></pre>

Returns a mask of items that have been selected.

The default implementation returns all selectable items as selectable.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Args</th></tr>

<tr>
<td>
`input_ids`
</td>
<td>
A `RaggedTensor`.
</td>
</tr><tr>
<td>
`axis`
</td>
<td>
(optional) An int detailing the dimension to apply selection on.
Default is the 1st dimension.
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2">Returns</th></tr>
<tr class="alt">
<td colspan="2">
a `RaggedTensor` with shape `input_ids.shape[:axis]`. Its values are True
if the corresponding item (or broadcasted subitems) should be selected.
</td>
</tr>

</table>





