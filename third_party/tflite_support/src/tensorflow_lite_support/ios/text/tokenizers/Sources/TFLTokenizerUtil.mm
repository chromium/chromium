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
#import "tensorflow_lite_support/ios/text/tokenizers/Sources/TFLTokenizerUtil.h"

#import "tensorflow_lite_support/ios/utils/Sources/TFLStringUtil.h"

using ::tflite::support::text::tokenizer::TokenizerResult;

NSArray<NSString *> *Tokenize(Tokenizer *tokenizer, NSString *input) {
  TokenizerResult tokenize_result = tokenizer->Tokenize(MakeString(input));
  std::vector<std::string> subwords = tokenize_result.subwords;
  NSMutableArray<NSString *> *ret = [NSMutableArray arrayWithCapacity:subwords.size()];
  for (int i = 0; i < subwords.size(); ++i) {
    [ret addObject:MakeNSString(subwords[i])];
  }
  return ret;
}

NSArray<NSNumber *> *ConvertTokensToIds(Tokenizer *tokenizer, NSArray<NSString *> *tokens) {
  NSMutableArray<NSNumber *> *ret = [NSMutableArray arrayWithCapacity:[tokens count]];
  for (NSString *token in tokens) {
    std::string cc_token = MakeString(token);
    const char *cToken = cc_token.c_str();
    int id;
    tokenizer->LookupId(cToken, &id);
    [ret addObject:[NSNumber numberWithInt:id]];
  }
  return ret;
}
