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

@testable import TFLSentencepieceTokenizer

class TFLSentencepieceTokenizerTest: XCTestCase {
  static let bundle = Bundle(for: TFLSentencepieceTokenizerTest.self)
  static let spModelPath = bundle.path(forResource: "30k-clean", ofType: "model")!

  func testInitSentenpieceTokenizerFromPath() {
    let spTokenizer = TFLSentencepieceTokenizer(
      modelPath: TFLSentencepieceTokenizerTest.spModelPath)

    XCTAssertNotNil(spTokenizer)

    let tokens = spTokenizer.tokens(fromInput: "good morning, i'm your teacher.\n")

    XCTAssertEqual(tokens, ["▁good", "▁morning", ",", "▁i", "'", "m", "▁your", "▁teacher", "."])

    let ids = spTokenizer.ids(fromTokens: tokens)

    XCTAssertEqual(ids, [254, 959, 15, 31, 22, 79, 154, 2197, 9])
  }
}
