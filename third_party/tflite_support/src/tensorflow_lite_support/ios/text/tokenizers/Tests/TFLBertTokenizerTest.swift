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

@testable import TFLBertTokenizer

class TFLBertTokenizerTest: XCTestCase {
  static let bundle = Bundle(for: TFLBertTokenizerTest.self)
  static let mobileBertVocabPath = bundle.path(forResource: "mobilebert_vocab", ofType: "txt")!

  func testInitBertTokenizerFromPath() {
    let bertTokenizer = TFLBertTokenizer(vocabPath: TFLBertTokenizerTest.mobileBertVocabPath)

    XCTAssertNotNil(bertTokenizer)

    let tokens = bertTokenizer.tokens(fromInput: "i'm questionansweraskask")

    XCTAssertEqual(tokens, ["i", "'", "m", "question", "##ans", "##wer", "##ask", "##ask"])

    let ids = bertTokenizer.ids(fromTokens: tokens)

    XCTAssertEqual(ids, [1045, 1005, 1049, 3160, 6962, 13777, 19895, 19895])
  }

  func testInitBertTokenizerFromVocab() {
    let bertTokenizer = TFLBertTokenizer(vocab: ["hell", "##o", "wor", "##ld", "there"])

    XCTAssertNotNil(bertTokenizer)

    let tokens = bertTokenizer.tokens(fromInput: "hello there hello world")

    XCTAssertEqual(tokens, ["hell", "##o", "there", "hell", "##o", "wor", "##ld"])

    let ids = bertTokenizer.ids(fromTokens: tokens)

    XCTAssertEqual(ids, [0, 1, 4, 0, 1, 2, 3])
  }
}
