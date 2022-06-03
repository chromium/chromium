# tf.Text Overview

<!--* freshness: { owner: 'broken' reviewed: '2021-01-21' } *-->

## What is tf.Text?

TF.Text is a collection of TensorFlow ops and libraries to help users work with
input in text form (meaning raw text strings or documents). The most important
functionality of tf.Text is to extract powerful syntactic and semantic text
features from inside the TF graph as input to your neural net.

<img src="include/architecture.gif" alt="Model Architectures"/>

Integrating preprocessing with the graph...

*   provides a large toolkit for working with text.
*   allows integration with a large suite of tensorflow tools to support
    projects from problem definition through training, eval, and launch.
*   reduces complexity at serving time.

## What kinds of preprocessing can you do with tf.Text?

TF.Text Ops provide extensive support for
[handling unicode](https://colab.sandbox.google.com/github/tensorflow/docs/blob/master/site/en/tutorials/representation/unicode.ipynb)
in-graph. This includes transforming text encodings and interpreting a large
variety of text encodings as individual characters. TF.Text provides most
support assuming that [Unicode](http://unicode.org/) will be used in-graph for
character representations of text.

TF.Text makes use of
[Ragged Tensor](https://www.tensorflow.org/guide/ragged_tensors) representations
internally. This is a tensor format that is efficient for managing
variable-length lists appropriate for text processing (e.g. characters may have
multi-byte encodings, words can be of varying lengths, sentences have different
numbers of tokens, documents have varying numbers of sentences, etc.).

In addition to basic text handling and tensor representations, tf.Text provides
library support for some feature engineering which includes constructing
n-grams, detecting token-level structure in text (wordshape), and projecting
features at one scale to another (e.g. constructing sentence-level
representations from token-level information). View the
[API DOCS](api_docs/python/index.md) for the full suite of available ops.
