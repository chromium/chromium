# Acceleration allowlisting

A complementary directory for the work of
[accelerator allowlisting](https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/experimental/acceleration)
in TensorFlow Lite.

## Coral Edge TPU plugin

The Coral Edge TPU delegate plugin used in the
[acceleration library](https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/cc/port/default/tflite_wrapper.h).
See
[CoralSettings](https://github.com/tensorflow/tensorflow/blob/491681a5620e41bf079a582ac39c585cc86878b9/tensorflow/lite/acceleration/configuration/configuration.proto#L601)
about how to configure the Coral Edge TPU plugin. You can use the acceleration
library together with
[Task Library](https://github.com/tensorflow/tflite-support/tree/master/tensorflow_lite_support/cc/task).
Configure your desired accelerator, including the Coral plugin through the
options of each task, i.e.
[image_classifier_options](https://github.com/tensorflow/tflite-support/blob/43f1267b99f1dbc27c7c5b2e1111e1ff6b9121ea/tensorflow_lite_support/cc/task/vision/proto/image_classifier_options.proto#L79).
