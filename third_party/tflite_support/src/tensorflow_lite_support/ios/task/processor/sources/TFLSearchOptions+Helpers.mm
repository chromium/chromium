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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLSearchOptions+Helpers.h"

@implementation TFLSearchOptions (Helpers)

- (void)copyToCppOptions:(tflite::task::processor::SearchOptions *)cppSearchOptions {
  if (self.indexFile.filePath) {
    cppSearchOptions->mutable_index_file()->set_file_name(self.indexFile.filePath.UTF8String);
  }
  cppSearchOptions->set_max_results(self.maxResults);
}

@end
