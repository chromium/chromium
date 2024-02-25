/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions+Helpers.h"

@implementation TFLBaseOptions (Helpers)

- (void)copyToCOptions:(TfLiteBaseOptions *)cBaseOptions {
  if (self.modelFile.filePath) {
    cBaseOptions->model_file.file_path = self.modelFile.filePath.UTF8String;
    cBaseOptions->compute_settings.cpu_settings.num_threads =
        (int)self.computeSettings.cpuSettings.numThreads;
    if (self.coreMLDelegateSettings) {
      cBaseOptions->compute_settings.coreml_delegate_settings.enable_delegate = true;
      cBaseOptions->compute_settings.coreml_delegate_settings.coreml_version =
          self.coreMLDelegateSettings.coreMLVersion;
      switch (self.coreMLDelegateSettings.enabledDevices) {
        case TFLCoreMLDelegateSettings_DevicesAll:
          cBaseOptions->compute_settings.coreml_delegate_settings.enabled_devices = kDevicesAll;
          break;
        case TFLCoreMLDelegateSettings_DevicesWithNeuralEngine:
          cBaseOptions->compute_settings.coreml_delegate_settings.enabled_devices =
              kDevicesWithNeuralEngine;
          break;
      }
    }
  }
}

@end
