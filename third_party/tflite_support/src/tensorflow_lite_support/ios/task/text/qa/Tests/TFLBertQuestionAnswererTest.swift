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

@testable import TFLBertQuestionAnswerer

class TFLBertQuestionAnswererTest: XCTestCase {

  static let bundle = Bundle(for: TFLBertQuestionAnswererTest.self)
  static let mobileBertModelPath = bundle.path(forResource: "mobilebert_with_metadata", ofType: "tflite")!

  static let albertModelPath = bundle.path(forResource: "albert_with_metadata", ofType: "tflite")!

  static let context = """
    The role of teacher is often formal and ongoing, carried out at a school or other place of
    formal education. In many countries, a person who wishes to become a teacher must first obtain
     specified professional qualifications or credentials from a university or college. These
    professional qualifications may include the study of pedagogy, the science of teaching.
    Teachers, like other professionals, may have to continue their education after they qualify,
    a process known as continuing professional development. Teachers may use a lesson plan to
    facilitate student learning, providing a course of study which is called the curriculum.
    """
  static let question = "What is a course of study called?"
  static let answer = "the curriculum."

  func testInitMobileBert() {
    let mobileBertAnswerer = TFLBertQuestionAnswerer.questionAnswerer(
      modelPath: TFLBertQuestionAnswererTest.mobileBertModelPath)

    XCTAssertNotNil(mobileBertAnswerer)

    let answers = mobileBertAnswerer.answer(
      context: TFLBertQuestionAnswererTest.context, question: TFLBertQuestionAnswererTest.question)

    XCTAssertNotNil(answers)
    XCTAssertEqual(answers[0].text, TFLBertQuestionAnswererTest.answer)
  }

  func testInitAlbert() {
    let albertAnswerer = TFLBertQuestionAnswerer.questionAnswerer(
      modelPath: TFLBertQuestionAnswererTest.albertModelPath)

    XCTAssertNotNil(albertAnswerer)

    let answers = albertAnswerer.answer(
      context: TFLBertQuestionAnswererTest.context, question: TFLBertQuestionAnswererTest.question)

    XCTAssertNotNil(answers)
    XCTAssertEqual(answers[0].text, TFLBertQuestionAnswererTest.answer)
  }
}
