# TFLite Task library - C++

A flexible and ready-to-use library for common machine learning model types,
such as classification and detection.

## Text Task Libraries

### QuestionAnswerer

`QuestionAnswerer` API is able to load
[Mobile BERT](https://tfhub.dev/tensorflow/mobilebert/1) or
[AlBert](https://tfhub.dev/tensorflow/albert_lite_base/1) TFLite models and
answer question based on context.

Use the C++ API to answer questions as follows:

```cc
using tflite::task::text::BertQuestionAnswerer;
using tflite::task::text::QaAnswer;
// Create API handler with Mobile Bert model.
auto qa_client = BertQuestionAnswerer::CreateBertQuestionAnswererFromFile("/path/to/mobileBertModel", "/path/to/vocab");
// Or create API handler with Albert model.
// auto qa_client = BertQuestionAnswerer::CreateAlbertQuestionAnswererFromFile("/path/to/alBertModel", "/path/to/sentencePieceModel");


std::string context =
    "Nikola Tesla (Serbian Cyrillic: Никола Тесла; 10 "
    "July 1856 – 7 January 1943) was a Serbian American inventor, electrical "
    "engineer, mechanical engineer, physicist, and futurist best known for his "
    "contributions to the design of the modern alternating current (AC) "
    "electricity supply system.";
std::string question = "When was Nikola Tesla born?";
// Run inference with `context` and a given `question` to the context, and get top-k
// answers ranked by logits.
const std::vector<QaAnswer> answers = qa_client->Answer(context, question);
// Access QaAnswer results.
for (const QaAnswer& item : answers) {
  std::cout << absl::StrFormat("Text: %s logit=%f start=%d end=%d", item.text,
                               item.pos.logit, item.pos.start, item.pos.end)
            << std::endl;
}
// Output:
// Text: 10 July 1856 logit=16.8527 start=17 end=19
// ... (and more)
//
// So the top-1 answer is: "10 July 1856".
```

In the above code, `item.text` is the text content of an answer. We use a span
with closed interval `[item.pos.start, item.pos.end]` to denote predicted tokens
in the answer, and `item.pos.logit` is the sum of span logits to represent the
confidence score.

### NLClassifier

`NLClassifier` API is able to load any TFLite models for natural language
classaification task such as language detection or sentiment detection.

The API expects a TFLite model with the following input/output tensor:
Input tensor0:
  (kTfLiteString) - input of the model, accepts a string.
Output tensor0:
  (kTfLiteUInt8/kTfLiteInt8/kTfLiteInt16/kTfLiteFloat32/kTfLiteFloat64)
  - output scores for each class, if type is one of the Int types,
    dequantize it to double
Output tensor1: optional
  (kTfLiteString)
  - output classname for each class, should be of the same length with
    scores. If this tensor is not present, the API uses score indices as
    classnames.
By default the API tries to find the input/output tensors with default
configurations in NLClassifierOptions, with tensor name prioritized over
tensor index. The option is configurable for different TFLite models.

Use the C++ API to perform language ID classification as follows:

```cc
using tflite::task::text::nlclassifier::NLClassifier;
using tflite::task::core::Category;
auto classifier = NLClassifier::CreateFromFileAndOptions("/path/to/model");
// Or create a customized NLClassifierOptions
// NLClassifierOptions options =
//   {
//     .output_score_tensor_name = myOutputScoreTensorName,
//     .output_label_tensor_name = myOutputLabelTensorName,
//   }
// auto classifier = NLClassifier::CreateFromFileAndOptions("/path/to/model", options);
std::string context = "What language is this?";
std::vector<Category> categories = classifier->Classify(context);
// Access category results.
for (const Categoryr& category : categories) {
  std::cout << absl::StrFormat("Language: %s Probability: %f", category.class_name, category_.score)
            << std::endl;
}
// Output:
// Language: en Probability=0.9
// ... (and more)
//
// So the top-1 answer is 'en'.
```

## Vision Task Libraries

### Image Classifier

`ImageClassifier` accepts any TFLite image classification model (with optional,
but strongly recommended, TFLite Model Metadata) that conforms to the following
spec:

Input tensor (type: `kTfLiteUInt8` / `kTfLiteFloat32`):

   - image input of size `[batch x height x width x channels]`.
   - batch inference is not supported (`batch` is required to be 1).
   - only RGB inputs are supported (`channels` is required to be 3).
   - if type is `kTfLiteFloat32`, `NormalizationOptions` are required to be
     attached to the metadata for input normalization.

At least one output tensor (type: `kTfLiteUInt8` / `kTfLiteFloat32`) with:

   -  `N` classes and either 2 or 4 dimensions, i.e. `[1 x N]` or
      `[1 x 1 x 1 x N]`
   - optional (but recommended) label map(s) as AssociatedFile-s with type
     TENSOR_AXIS_LABELS, containing one label per line. The first such
     AssociatedFile (if any) is used to fill the `class_name` field of the
     results. The `display_name` field is filled from the AssociatedFile (if
     any) whose locale matches the `display_names_locale` field of the
     `ImageClassifierOptions` used at creation time ("en" by default, i.e.
     English). If none of these are available, only the `index` field of the
     results will be filled.

An example of such model can be found at:
https://tfhub.dev/bohemian-visual-recognition-alliance/lite-model/models/mushroom-identification_v1/1

Example usage:

```cc
// More options are available (e.g. max number of results to return). At the
// very least, the model must be specified:
ImageClassifierOptions options;
options.mutable_model_file_with_metadata()->set_file_name(
    "/path/to/model.tflite");

// Create an ImageClassifier instance from the options.
StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
    ImageClassifier::CreateFromOptions(options);
// Check if an error occurred.
if (!image_classifier_or.ok()) {
  std::cerr << "An error occurred during ImageClassifier creation: "
            << image_classifier_or.status().message();
  return;
}
std::unique_ptr<ImageClassifier> image_classifier =
    std::move(image_classifier_or.value());

// Prepare FrameBuffer input from e.g. image RGBA data, width and height:
std::unique_ptr<FrameBuffer> frame_buffer =
    CreateFromRgbaRawBuffer(image_rgba_data, {image_width, image_height});

// Run inference:
StatusOr<ClassificationResult> result_or =
    image_classifier->Classify(*frame_buffer);
// Check if an error occurred.
if (!result_or.ok()) {
  std::cerr << "An error occurred during classification: "
            << result_or.status().message();
  return;
}
ClassificationResult result = result_or.value();

// Example value for 'result':
//
// classifications {
//   classes { index: 934 score: 0.95 class_name: "cat" }
//   classes { index: 948 score: 0.007 class_name: "dog" }
//   classes { index: 927 score: 0.003 class_name: "fox" }
//   head_index: 0
// }
```

A CLI demo tool is also available [here][1] for easily trying out this API.

### Object Detector

`ObjectDetector` accepts any object detection TFLite model (with mandatory
TFLite Model Metadata) that conforms to the following spec (e.g. Single Shot
Detectors):

Input tensor (type: `kTfLiteUInt8` / `kTfLiteFloat32`):

   - image input of size `[batch x height x width x channels]`.
   - batch inference is not supported (`batch` is required to be 1).
   - only RGB inputs are supported (`channels` is required to be 3).
   - if type is kTfLiteFloat32, `NormalizationOptions` are required to be
     attached to the metadata for input normalization.

Output tensors must be the 4 outputs (type: `kTfLiteFloat32`) of a
[`DetectionPostProcess`][2] op, i.e:

* Locations:

  - of size `[num_results x 4]`, the inner array
    representing bounding boxes in the form [top, left, right, bottom].
  - BoundingBoxProperties are required to be attached to the metadata
    and must specify type=BOUNDARIES and coordinate_type=RATIO.

* Classes:

  - of size `[num_results]`, each value representing the
    integer index of a class.
  - optional (but recommended) label map(s) can be attached as
    AssociatedFile-s with type TENSOR_VALUE_LABELS, containing one label per
    line. The first such AssociatedFile (if any) is used to fill the
    `class_name` field of the results. The `display_name` field is filled
    from the AssociatedFile (if any) whose locale matches the
    `display_names_locale` field of the `ObjectDetectorOptions` used at
    creation time ("en" by default, i.e. English). If none of these are
    available, only the `index` field of the results will be filled.

* Scores:

  - of size `[num_results]`, each value representing the score
    of the detected object.

* Number of results:

  - integer `num_results` as a tensor of size `[1]`

An example of such model can be found at:
https://tfhub.dev/google/lite-model/object_detection/mobile_object_localizer_v1/1/metadata/1

Example usage:

```cc
// More options are available (e.g. max number of results to return). At the
// very least, the model must be specified:
ObjectDetectorOptions options;
options.mutable_model_file_with_metadata()->set_file_name(
    "/path/to/model.tflite");

// Create an ObjectDetector instance from the options.
StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
    ObjectDetector::CreateFromOptions(options);
// Check if an error occurred.
if (!object_detector_or.ok()) {
  std::cerr << "An error occurred during ObjectDetector creation: "
            << object_detector_or.status().message();
  return;
}
std::unique_ptr<ObjectDetector> object_detector =
    std::move(object_detector_or.value());

// Prepare FrameBuffer input from e.g. image RGBA data, width and height:
std::unique_ptr<FrameBuffer> frame_buffer =
    CreateFromRgbaRawBuffer(image_rgba_data, {image_width, image_height});

// Run inference:
StatusOr<DetectionResult> result_or = object_detector->Detect(*frame_buffer);
// Check if an error occurred.
if (!result_or.ok()) {
  std::cerr << "An error occurred during detection: "
            << result_or.status().message();
  return;
}
DetectionResult result = result_or.value();

// Example value for 'result':
//
// detections {
//   bounding_box {
//     origin_x: 54
//     origin_y: 398
//     width: 393
//     height: 196
//   }
//   classes { index: 16 score: 0.65 class_name: "cat" }
// }
// detections {
//   bounding_box {
//     origin_x: 602
//     origin_y: 157
//     width: 394
//     height: 447
//   }
//   classes { index: 17 score: 0.45 class_name: "dog" }
// }
```

A CLI demo tool is available [here][3] for easily trying out this API.

### Image Segmenter

`ImageSegmenter` accepts any TFLite model (with optional, but strongly
recommended, TFLite Model Metadata) that conforms to the following spec:

Input tensor (type: `kTfLiteUInt8` / `kTfLiteFloat32`):

   - image input of size `[batch x height x width x channels]`.
   - batch inference is not supported (`batch` is required to be 1).
   - only RGB inputs are supported (`channels` is required to be 3).
   - if type is kTfLiteFloat32, `NormalizationOptions` are required to be
     attached to the metadata for input normalization.

Output tensor (type: `kTfLiteUInt8` / `kTfLiteFloat32`):

   - tensor of size `[batch x mask_height x mask_width x num_classes]`, where
     `batch` is required to be 1, `mask_width` and `mask_height` are the
     dimensions of the segmentation masks produced by the model, and
     `num_classes` is the number of classes supported by the model.
   - optional (but recommended) label map(s) can be attached as
     AssociatedFile-s with type TENSOR_AXIS_LABELS, containing one label per
     line. The first such AssociatedFile (if any) is used to fill the
     `class_name` field of the results. The `display_name` field is filled
     from the AssociatedFile (if any) whose locale matches the
     `display_names_locale` field of the `ImageSegmenterOptions` used at
     creation time ("en" by default, i.e. English). If none of these are
     available, only the `index` field of the results will be filled.

An example of such model can be found at:
https://tfhub.dev/tensorflow/lite-model/deeplabv3/1/metadata/1

Example usage:

```cc
// More options are available to select between return a single category mask
// or multiple confidence masks during post-processing.
ImageSegmenterOptions options;
options.mutable_model_file_with_metadata()->set_file_name(
    "/path/to/model.tflite");

// Create an ImageSegmenter instance from the options.
StatusOr<std::unique_ptr<ImageSegmenter>> image_segmenter_or =
    ImageSegmenter::CreateFromOptions(options);
// Check if an error occurred.
if (!image_segmenter_or.ok()) {
  std::cerr << "An error occurred during ImageSegmenter creation: "
            << image_segmenter_or.status().message();
  return;
}
std::unique_ptr<ImageSegmenter> immage_segmenter =
    std::move(image_segmenter_or.value());

// Prepare FrameBuffer input from e.g. image RGBA data, width and height:
std::unique_ptr<FrameBuffer> frame_buffer =
    CreateFromRgbaRawBuffer(image_rgba_data, {image_width, image_height});

// Run inference:
StatusOr<SegmentationResult> result_or =
    immage_segmenter->Segment(*frame_buffer);
// Check if an error occurred.
if (!result_or.ok()) {
  std::cerr << "An error occurred during segmentation: "
            << result_or.status().message();
  return;
}
SegmentationResult result = result_or.value();

// Example value for 'result':
//
// segmentation {
//   width: 257
//   height: 257
//   category_mask: "\x00\x01..."
//   colored_labels { r: 0 g: 0 b: 0 class_name: "background" }
//   colored_labels { r: 128 g: 0 b: 0 class_name: "aeroplane" }
//   ...
//   colored_labels { r: 128 g: 192 b: 0 class_name: "train" }
//   colored_labels { r: 0 g: 64 b: 128 class_name: "tv" }
// }
//
// Where 'category_mask' is a byte buffer of size 'width' x 'height', with the
// value of each pixel representing the class this pixel belongs to (e.g. '\x00'
// means "background", '\x01' means "aeroplane", etc).
// 'colored_labels' provides the label for each possible value, as well as
// suggested RGB components to optionally transform the result into a more
// human-friendly colored image.
//
```

A CLI demo tool is available [here][4] for easily trying out this API.

[1]: https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/examples/task/vision/desktop/image_classifier_demo.cc
[2]: https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/kernels/detection_postprocess.cc
[3]: https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/examples/task/vision/desktop/object_detector_demo.cc
[4]: https://github.com/tensorflow/tflite-support/blob/master/tensorflow_lite_support/examples/task/vision/desktop/image_segmenter_demo.cc
