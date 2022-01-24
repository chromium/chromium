/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
import XCTest

@testable import TFLNLClassifier

class TFLNLClassifierTest: XCTestCase {

  static let bundle = Bundle(for: TFLNLClassifierTest.self)
  static let modelPath = bundle.path(
    forResource: "test_model_nl_classifier_with_regex_tokenizer",
    ofType: "tflite")!

  var modelOptions:TFLNLClassifierOptions!;

  override func setUp() {
    modelOptions = TFLNLClassifierOptions()
    modelOptions.inputTensorName = "input_text"
    modelOptions.outputScoreTensorName = "probability"
  }

  func testClassifyPositiveResult() {
    let nlClassifier = TFLNLClassifier.nlClassifier(
      modelPath: TFLNLClassifierTest.modelPath,
      options: modelOptions)

    XCTAssertNotNil(nlClassifier)

    let categories = nlClassifier.classify(
      text: "This is the best movie I’ve seen in recent years. Strongly recommend it!")

    XCTAssertGreaterThan(categories["Positive"]!.doubleValue, categories["Negative"]!.doubleValue)
  }

   func testClassifyNegativeResult() {
     let nlClassifier = TFLNLClassifier.nlClassifier(
       modelPath: TFLNLClassifierTest.modelPath,
       options: modelOptions)

     XCTAssertNotNil(nlClassifier)

     let categories = nlClassifier.classify(text: "What a waste of my time.")

     XCTAssertGreaterThan(categories["Negative"]!.doubleValue, categories["Positive"]!.doubleValue)
   }
}
