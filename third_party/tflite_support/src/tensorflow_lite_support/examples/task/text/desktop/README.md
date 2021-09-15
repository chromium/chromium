# CLI Demos for C++ Text Task APIs

This folder contains simple command-line tools for easily trying out the C++
Text Task APIs.

## Bert Question Answerer

#### Prerequisites

You will need:

* a TFLite bert based question answerer model from model maker.
(e.g. [mobilebert][1] or [albert][2] available on TensorFlow Hub).

#### Usage

In the console, run:

```bash
# Download the model:
curl \
 -L 'https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1?lite-format=tflite' \
 -o /tmp/mobilebert.tflite

# Run the classification tool:
bazel run -c opt \
 tensorflow_lite_support/examples/task/text/desktop:bert_question_answerer_demo -- \
 --model_path=/tmp/mobilebert.tflite \
 --question="Where is Amazon rainforest?" \
 --context="The Amazon rainforest, alternatively, the Amazon Jungle, also known in \
English as Amazonia, is a moist broadleaf tropical rainforest in the Amazon \
biome that covers most of the Amazon basin of South America. This basin \
encompasses 7,000,000 km2 (2,700,000 sq mi), of which \
5,500,000 km2 (2,100,000 sq mi) are covered by the rainforest. This region \
includes territory belonging to nine nations."
```

#### Results

In the console, you should get:

```
answer[0]:  'South America.'
logit: 1.84847, start_index: 39, end_index: 40
answer[1]:  'most of the Amazon basin of South America.'
logit: 1.2921, start_index: 34, end_index: 40
answer[2]:  'the Amazon basin of South America.'
logit: -0.0959535, start_index: 36, end_index: 40
answer[3]:  'the Amazon biome that covers most of the Amazon basin of South America.'
logit: -0.498558, start_index: 28, end_index: 40
answer[4]:  'Amazon basin of South America.'
logit: -0.774266, start_index: 37, end_index: 40
```

## NLClassifier

#### Prerequisites

You will need:

* a TFLite text classification model with certain format.
(e.g. [movie_review_model][3], a model to classify movie reviews), you'll need
to configure the input tensor and out tensor for the API, see the [doc][4] for 
details.

#### Usage

In the console, run:

```bash
# Download the model:
curl \
 -L 'https://storage.googleapis.com/download.tensorflow.org/models/tflite/text_classification/text_classification_v2.tflite' \
 -o /tmp/movie_review.tflite

# Run the detection tool:
bazel run -c opt \
 tensorflow_lite_support/examples/task/text/desktop:nl_classifier_demo -- \
 --model_path=/tmp/movie_review.tflite \
 --text="What a waste of my time." \
 --input_tensor_name="input_text" \
 --output_score_tensor_name="probability"
```

#### Results

In the console, you should get:

```
category[0]: 'Negative' : '0.81313'
category[1]: 'Positive' : '0.18687'
```

## BertNLClassifier

#### Prerequisites

TODO(b/163086702): Update the links to models with metadata attached.

You will need:

* a Bert based TFLite text classification model from model maker. (e.g. [movie_review_model][5] available on TensorFlow Hub).

#### Usage

In the console, run:

```bash
# Download the model:
curl \
 -L 'https://url/to/bert/nl/classifier' \
 -o /tmp/bert_movie_review.tflite

# Run the segmentation tool:
bazel run -c opt \
 tensorflow_lite_support/examples/task/text/desktop:bert_nl_classifier_demo -- \
 --model_path=/tmp/bert_movie_review.tflite \
 --text="it's a charming and often affecting journey"
```

#### Results

In the console, you should get:

```
category[0]: 'negative' : '0.00006'
category[1]: 'positive' : '0.99994'
```

[1]: https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1
[2]: https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1
[3]: https://www.tensorflow.org/lite/models/text_classification/overview
[4]: https://github.com/tensorflow/tflite-support/blob/fe8b69002f5416900285dc69e2baa078c91bd994/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h#L55
[5]: http://bert/nl/classifier/model
