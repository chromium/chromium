description: Pad model input and generate corresponding input masks.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.pad_model_inputs" />
<meta itemprop="path" content="Stable" />
</div>

# text.pad_model_inputs

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/pad_model_inputs_ops.py">View source</a>



Pad model input and generate corresponding input masks.

<pre class="devsite-click-to-copy prettyprint lang-py tfo-signature-link">
<code>text.pad_model_inputs(
    input, max_seq_length, pad_value=0
)
</code></pre>



<!-- Placeholder for "Used in" -->

`pad_model_inputs` performs the final packaging of a model's inputs commonly
found in text models. This includes padding out (or simply truncating) to a
fixed-size, 2-dimensional `Tensor` and generating mask `Tensor`s (of the same 2D
shape) with values of 0 if the corresponding item is a pad value and 1 if it is
part of the original input.

Note that a simple truncation strategy (drop everything after max sequence
length) is used to force the inputs to the specified shape. This may be
incorrect and users should instead apply a `Trimmer` upstream to safely truncate
large inputs.

```
>>> input_data = tf.ragged.constant([
...            [101, 1, 2, 102, 10, 20, 102],
...            [101, 3, 4, 102, 30, 40, 50, 60, 70, 80],
...            [101, 5, 6, 7, 8, 9, 102, 70],
...        ], np.int32)
>>> data, mask = pad_model_inputs(input=input_data, max_seq_length=9)
>>> print("data: %s, mask: %s" % (data, mask))
  data: tf.Tensor(
  [[101   1   2 102  10  20 102   0   0]
   [101   3   4 102  30  40  50  60  70]
   [101   5   6   7   8   9 102  70   0]], shape=(3, 9), dtype=int32),
  mask: tf.Tensor(
  [[1 1 1 1 1 1 1 0 0]
   [1 1 1 1 1 1 1 1 1]
   [1 1 1 1 1 1 1 1 0]], shape=(3, 9), dtype=int32)
```

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Args</h2></th></tr>

<tr>
<td>
`input`
</td>
<td>
A `RaggedTensor` with rank >= 2.
</td>
</tr><tr>
<td>
`max_seq_length`
</td>
<td>
An int, or scalar `Tensor`. The "input" `Tensor` will be
flattened down to 2 dimensions and then have its 2nd dimension either
padded out or truncated to this size.
</td>
</tr><tr>
<td>
`pad_value`
</td>
<td>
An int or scalar `Tensor` specifying the value used for padding.
</td>
</tr>
</table>



<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Returns</h2></th></tr>
<tr class="alt">
<td colspan="2">
A tuple of (padded_input, pad_mask) where:
</td>
</tr>
<tr>
<td>
`padded_input`
</td>
<td>
A `Tensor` corresponding to `inputs` that has been
padded/truncated out to a fixed size and flattened to 2
dimensions.
</td>
</tr><tr>
<td>
`pad_mask`
</td>
<td>
A `Tensor` corresponding to `padded_input` whose values are
0 if the corresponding item is a pad value and 1 if it is not.
</td>
</tr>
</table>

