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
#import "tensorflow_lite_support/ios/text/tokenizers/Sources/TFLSentencepieceTokenizer.h"
#include "absl/memory/memory.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/text/tokenizers/sentencepiece_tokenizer.h"
#import "tensorflow_lite_support/ios/text/tokenizers/Sources/TFLTokenizerUtil.h"
#import "tensorflow_lite_support/ios/utils/Sources/TFLStringUtil.h"

NS_ASSUME_NONNULL_BEGIN
using SentencepieceTokenizerCPP = ::tflite::support::text::tokenizer::SentencePieceTokenizer;

@implementation TFLSentencepieceTokenizer {
  std::unique_ptr<SentencepieceTokenizerCPP> _spTokenizer;
}

- (instancetype)initWithModelPath:(NSString *)modelPath {
  self = [super init];
  if (self) {
    _spTokenizer = absl::make_unique<SentencepieceTokenizerCPP>(MakeString(modelPath));
  }
  return self;
}

- (NSArray<NSString *> *)tokensFromInput:(NSString *)input {
  return Tokenize(_spTokenizer.get(), input);
}

- (NSArray<NSNumber *> *)idsFromTokens:(NSArray<NSString *> *)tokens {
  return ConvertTokensToIds(_spTokenizer.get(), tokens);
}

@end
NS_ASSUME_NONNULL_END
