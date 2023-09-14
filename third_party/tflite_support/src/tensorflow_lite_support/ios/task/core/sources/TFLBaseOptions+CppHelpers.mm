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
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions+CppHelpers.h"

@implementation TFLBaseOptions (CppHelpers)

- (void)copyToCppOptions:(tflite::task::core::BaseOptions *)cppOptions {
  if (self.modelFile.filePath) {
    cppOptions->mutable_model_file()->set_file_name(self.modelFile.filePath.UTF8String);
  }
  cppOptions->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads((int)self.computeSettings.cpuSettings.numThreads);
}

@end
