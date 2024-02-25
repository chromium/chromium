/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
import GMLImageUtils
import XCTest

@testable import TFLObjectDetector

class ObjectDetectorTests: XCTestCase {

  static let bundle = Bundle(for: ObjectDetectorTests.self)
  static let modelPath = bundle.path(
    forResource: "coco_ssd_mobilenet_v1_1.0_quant_2018_06_29",
    ofType: "tflite")
  let kMaxPixelOffset: CGFloat = 5

  func verifyDetectionResult(_ detectionResult: DetectionResult, precision: Float) {
    XCTAssertGreaterThan(detectionResult.detections.count, 0)

    self.verifyDetection(
      detectionResult.detections[0],
      expectedBoundingBox: CGRect(x: 54, y: 396, width: 393, height: 199),
      expectedFirstScore: 0.63,
      expectedFirstLabel: "cat", precision: precision)

    self.verifyDetection(
      detectionResult.detections[1],
      expectedBoundingBox: CGRect(x: 602, y: 157, width: 390, height: 447),
      expectedFirstScore: 0.6,
      expectedFirstLabel: "cat", precision: precision)

    self.verifyDetection(
      detectionResult.detections[2],
      expectedBoundingBox: CGRect(x: 260, y: 390, width: 179, height: 209),
      expectedFirstScore: 0.56,
      expectedFirstLabel: "cat", precision: precision)

    self.verifyDetection(
      detectionResult.detections[3],
      expectedBoundingBox: CGRect(x: 390, y: 197, width: 281, height: 409),
      expectedFirstScore: 0.5,
      expectedFirstLabel: "dog", precision: precision)
  }

  func verifyDetection(
    _ detection: Detection, expectedBoundingBox: CGRect,
    expectedFirstScore: Float,
    expectedFirstLabel: String,
    precision: Float
  ) {
    XCTAssertGreaterThan(detection.categories.count, 0)
    XCTAssertEqual(
      detection.boundingBox.origin.x,
      expectedBoundingBox.origin.x, accuracy: kMaxPixelOffset)
    XCTAssertEqual(
      detection.boundingBox.origin.y,
      expectedBoundingBox.origin.y, accuracy: kMaxPixelOffset)
    XCTAssertEqual(
      detection.boundingBox.size.width,
      expectedBoundingBox.size.width, accuracy: kMaxPixelOffset)
    XCTAssertEqual(
      detection.boundingBox.size.height,
      expectedBoundingBox.size.height, accuracy: kMaxPixelOffset)
    XCTAssertEqual(
      detection.categories[0].label,
      expectedFirstLabel)
    XCTAssertEqual(
      detection.categories[0].score,
      expectedFirstScore, accuracy: precision)
  }

  func testSuccessfulInferenceOnMLImageWithUIImage() throws {

    let modelPath = try XCTUnwrap(ObjectDetectorTests.modelPath)

    let objectDetectorOptions = ObjectDetectorOptions(modelPath: modelPath)

    let objectDetector =
      try ObjectDetector.detector(options: objectDetectorOptions)

    let gmlImage = try XCTUnwrap(
      MLImage.imageFromBundle(
        class: type(of: self),
        filename: "cats_and_dogs",
        type: "jpg"))
    let detectionResults: DetectionResult =
      try objectDetector.detect(mlImage: gmlImage)

    self.verifyDetectionResult(detectionResults, precision: 0.05)
  }

  func testModelOptionsWithMaxResults() throws {

    let modelPath = try XCTUnwrap(ObjectDetectorTests.modelPath)

    let objectDetectorOptions = ObjectDetectorOptions(modelPath: modelPath)

    let maxResults = 3
    objectDetectorOptions.classificationOptions.maxResults = maxResults

    let objectDetector =
      try ObjectDetector.detector(options: objectDetectorOptions)

    let gmlImage = try XCTUnwrap(
      MLImage.imageFromBundle(
        class: type(of: self),
        filename: "cats_and_dogs",
        type: "jpg"))
    let detectionResult: DetectionResult = try objectDetector.detect(
      mlImage: gmlImage)

    XCTAssertLessThanOrEqual(detectionResult.detections.count, maxResults)

    self.verifyDetection(
      detectionResult.detections[0],
      expectedBoundingBox: CGRect(x: 54, y: 396, width: 393, height: 199),
      expectedFirstScore: 0.63,
      expectedFirstLabel: "cat", precision: 0.05)

    self.verifyDetection(
      detectionResult.detections[1],
      expectedBoundingBox: CGRect(x: 602, y: 157, width: 390, height: 447),
      expectedFirstScore: 0.60,
      expectedFirstLabel: "cat", precision: 0.05)

    self.verifyDetection(
      detectionResult.detections[2],
      expectedBoundingBox: CGRect(x: 260, y: 394, width: 179, height: 209),
      expectedFirstScore: 0.5625,
      expectedFirstLabel: "cat", precision: 0.05)
  }
}
