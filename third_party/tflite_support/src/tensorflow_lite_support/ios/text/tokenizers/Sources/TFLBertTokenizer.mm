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
#import "tensorflow_lite_support/ios/text/tokenizers/Sources/TFLBertTokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"
#import "tensorflow_lite_support/ios/text/tokenizers/Sources/TFLTokenizerUtil.h"
#import "tensorflow_lite_support/ios/utils/Sources/TFLStringUtil.h"

NS_ASSUME_NONNULL_BEGIN
using BertTokenizerCPP = ::tflite::support::text::tokenizer::BertTokenizer;

@implementation TFLBertTokenizer {
  std::unique_ptr<BertTokenizerCPP> _bertTokenizer;
}

- (instancetype)initWithVocabPath:(NSString *)vocabPath {
  self = [super init];
  if (self) {
    _bertTokenizer = absl::make_unique<BertTokenizerCPP>(MakeString(vocabPath));
  }
  return self;
}

- (instancetype)initWithVocab:(NSArray<NSString *> *)vocab {
  self = [super init];
  if (self) {
    std::vector<std::string> vocabCpp;
    vocabCpp.reserve([vocab count]);
    for (NSString *word in vocab) {
      vocabCpp.emplace_back(MakeString(word));
    }
    _bertTokenizer = absl::make_unique<BertTokenizerCPP>(vocabCpp);
  }
  return self;
}

- (NSArray<NSString *> *)tokensFromInput:(NSString *)input {
  return Tokenize(_bertTokenizer.get(), input);
}

- (NSArray<NSNumber *> *)idsFromTokens:(NSArray<NSString *> *)tokens {
  return ConvertTokensToIds(_bertTokenizer.get(), tokens);
}

@end
NS_ASSUME_NONNULL_END
