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

@testable import TFLBertNLClassifier

class TFLBertNLClassifierTest: XCTestCase {

  static let bundle = Bundle(for: TFLBertNLClassifierTest.self)
  static let bertModelPath = bundle.path(forResource: "bert_nl_classifier", ofType: "tflite")!

  func testClassifyPositiveResult() {
    let bertNLClassifier = TFLBertNLClassifier.bertNLClassifier(
      modelPath: TFLBertNLClassifierTest.bertModelPath)

    XCTAssertNotNil(bertNLClassifier)

    let categories = bertNLClassifier.classify(text: "it's a charming and often affecting journey")

    XCTAssertGreaterThan(categories["positive"]!.doubleValue, categories["negative"]!.doubleValue)
  }

  func testClassifyNegativeResult() {
    let bertNLClassifier = TFLBertNLClassifier.bertNLClassifier(
      modelPath: TFLBertNLClassifierTest.bertModelPath)

    XCTAssertNotNil(bertNLClassifier)

    let categories = bertNLClassifier.classify(text: "unflinchingly bleak and desperate")

    XCTAssertGreaterThan(categories["negative"]!.doubleValue, categories["positive"]!.doubleValue)
  }

  func testCreateFromOptionsClassifyPositiveResult() {
    let modelOptions = TFLBertNLClassifierOptions()
    modelOptions.maxSeqLen = 128
    let bertNLClassifier = TFLBertNLClassifier.bertNLClassifier(
      modelPath: TFLBertNLClassifierTest.bertModelPath,
      options: modelOptions)

    XCTAssertNotNil(bertNLClassifier)

    let categories = bertNLClassifier.classify(text: "it's a charming and often affecting journey")

    XCTAssertGreaterThan(categories["positive"]!.doubleValue, categories["negative"]!.doubleValue)
  }

  func testCreateFromOptionsClassifyNegativeResult() {
    let modelOptions = TFLBertNLClassifierOptions()
    modelOptions.maxSeqLen = 128
    let bertNLClassifier = TFLBertNLClassifier.bertNLClassifier(
      modelPath: TFLBertNLClassifierTest.bertModelPath,
      options: modelOptions)

    XCTAssertNotNil(bertNLClassifier)

    let categories = bertNLClassifier.classify(text: "unflinchingly bleak and desperate")

    XCTAssertGreaterThan(categories["negative"]!.doubleValue, categories["positive"]!.doubleValue)
  }
}
