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
import AVAudioPCMBufferUtils
import XCTest

@testable import TFLAudioClassifier

class AudioClassifierTests: XCTestCase {

  static let bundle = Bundle(for: AudioClassifierTests.self)
  static let modelPath = bundle.path(
    forResource: "yamnet_audio_classifier_with_metadata",
    ofType: "tflite")

  func verifyError(
    _ error: Error,
    expectedLocalizedDescription: String
  ) {

    XCTAssertEqual(
      error.localizedDescription,
      expectedLocalizedDescription)
  }

  func verifyCategory(
    _ category: ClassificationCategory,
    expectedIndex: NSInteger,
    expectedScore: Float,
    expectedLabel: String,
    expectedDisplayName: String?
  ) {
    XCTAssertEqual(
      category.index,
      expectedIndex)
    XCTAssertEqual(
      category.score,
      expectedScore,
      accuracy: 1e-6)
    XCTAssertEqual(
      category.label,
      expectedLabel)
    XCTAssertEqual(
      category.displayName,
      expectedDisplayName)
  }

  func verifyClassifications(
    _ classifications: Classifications,
    expectedHeadIndex: NSInteger,
    expectedCategoryCount: NSInteger
  ) {
    XCTAssertEqual(
      classifications.headIndex,
      expectedHeadIndex)
    XCTAssertEqual(
      classifications.categories.count,
      expectedCategoryCount)
  }

  func verifyClassificationResult(
    _ classificationResult: ClassificationResult,
    expectedClassificationsCount: NSInteger
  ) {
    XCTAssertEqual(
      classificationResult.classifications.count,
      expectedClassificationsCount)
  }

  func bufferFromFile(
    withName name: String,
    fileExtension: String,
    audioFormat: AudioFormat
  ) -> AVAudioPCMBuffer? {
    guard
      let filePath = AudioClassifierTests.bundle.path(
        forResource: name,
        ofType: fileExtension)
    else {
      return nil
    }

    return AVAudioPCMBuffer.loadPCMBufferFromFile(
      withPath: filePath,
      audioFormat: audioFormat)
  }

  func createAudioClassifierOptions(
    modelPath: String?
  ) throws -> AudioClassifierOptions? {
    let modelPath = try XCTUnwrap(modelPath)
    let options = AudioClassifierOptions(modelPath: modelPath)

    return options
  }

  func createAudioClassifier(
    withModelPath modelPath: String?
  ) throws -> AudioClassifier? {
    let options = try XCTUnwrap(
      self.createAudioClassifierOptions(
        modelPath: AudioClassifierTests.modelPath))
    let audioClassifier = try XCTUnwrap(
      AudioClassifier.classifier(
        options: options))

    return audioClassifier
  }

  func createAudioTensor(
    withAudioClassifier audioClassifier: AudioClassifier
  ) -> AudioTensor {
    let audioTensor = audioClassifier.createInputAudioTensor()
    return audioTensor
  }

  func loadAudioTensor(
    _ audioTensor: AudioTensor,
    fromWavFileWithName fileName: String
  ) throws {
    // Load pcm buffer from file.
    let buffer = try XCTUnwrap(
      self.bufferFromFile(
        withName: fileName,
        fileExtension: "wav",
        audioFormat: audioTensor.audioFormat))

    // Get float buffer from pcm buffer.
    let floatBuffer = try XCTUnwrap(buffer.floatBuffer)

    // Load float buffer into the audio tensor.
    try audioTensor.load(
      buffer: floatBuffer,
      offset: 0,
      size: floatBuffer.size)
  }

  func classify(
    audioTensor: AudioTensor,
    usingAudioClassifier audioClassifier: AudioClassifier
  ) throws -> ClassificationResult? {
    let classificationResult = try XCTUnwrap(
      audioClassifier.classify(
        audioTensor: audioTensor))

    let expectedClassificationsCount = 1
    self.verifyClassificationResult(
      classificationResult,
      expectedClassificationsCount: expectedClassificationsCount)

    let expectedCategoryCount = 521
    let expectedHeadIndex = 0
    self.verifyClassifications(
      classificationResult.classifications[0],
      expectedHeadIndex: expectedHeadIndex,
      expectedCategoryCount: expectedCategoryCount)

    return classificationResult
  }

  func validateForInferenceWithFloatBuffer(
    categories: [ClassificationCategory]
  ) {
    self.verifyCategory(
      categories[0],
      expectedIndex: 0,
      expectedScore: 0.957031,
      expectedLabel: "Speech",
      expectedDisplayName: nil)
    self.verifyCategory(
      categories[1],
      expectedIndex: 500,
      0.019531,               // expectedScore
      expectedLabel: "Inside, small room",
      expectedDisplayName: nil)
  }

  func testInferenceWithFloatBufferSucceeds() throws {
    let audioClassifier = try XCTUnwrap(
      self.createAudioClassifier(
        withModelPath: AudioClassifierTests.modelPath))
    let audioTensor = self.createAudioTensor(
      withAudioClassifier: audioClassifier)
    try self.loadAudioTensor(
      audioTensor, fromWavFileWithName: "speech")
    let classificationResult = try XCTUnwrap(
      self.classify(
        audioTensor: audioTensor,
        usingAudioClassifier: audioClassifier))
    self.validateForInferenceWithFloatBuffer(
      categories: classificationResult.classifications[0].categories)
  }

  func testInferenceWithNoModelPathFails() throws {
    let options = AudioClassifierOptions()
    do {
      let audioClassifier = try AudioClassifier.classifier(
        options: options)
      XCTAssertNil(audioClassifier)
    } catch {
      self.verifyError(
        error,
        expectedLocalizedDescription:
          "INVALID_ARGUMENT: Missing mandatory `model_file` field in `base_options`")
    }
  }
}
