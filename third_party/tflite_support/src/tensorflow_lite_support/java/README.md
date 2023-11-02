# TensorFlow Lite Support

TensorFlow Lite Support contains a set of tools and libraries that help
developing ML with TFLite for mobile apps. See the [documentation on
tensorflow.org](https://www.tensorflow.org/lite/inference_with_metadata/overview)
for more information about all the efforts under TensorFlow Lite Support.

This directory contains the Java code for the TensorFlow Lite Support Library
and TensorFlow Lite Task Library.

## TensorFlow Lite Android Support Library

Mobile application developers typically interact with typed objects such as
bitmaps or primitives such as integers. However, the TensorFlow Lite Interpreter
that runs the on-device machine learning model uses tensors in the form of
ByteBuffer, which can be difficult to debug and manipulate. The TensorFlow Lite
Android Support Library is designed to help process the input and output of
TensorFlow Lite models, and make the TensorFlow Lite interpreter easier to use.

We welcome feedback from the community as we develop this support library,
especially around:

*   Use-cases we should support including data types and operations
*   Ease of use - does the APIs make sense to the community

See the [documentation](https://www.tensorflow.org/lite/inference_with_metadata/lite_support)
for more instruction and examples.


## TensorFlow Lite Android Task Library

TensorFlow Lite Task Library provides optimized ready-to-use model interfaces
for popular machine learning tasks, such as image classification, question and
answer, etc. The model interfaces are specifically designed for each task to
achieve the best performance and usability. Task Library works cross-platform
and is supported on Java, C++, and Swift.

See the [documentation](https://www.tensorflow.org/lite/inference_with_metadata/task_library/overview)
for more instruction and examples.
