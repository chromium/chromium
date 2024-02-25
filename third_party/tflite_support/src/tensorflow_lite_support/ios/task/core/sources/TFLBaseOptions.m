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
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions.h"

@implementation TFLCpuSettings
@synthesize numThreads;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.numThreads = -1;
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  TFLCpuSettings *cpuSettings = [[TFLCpuSettings alloc] init];

  cpuSettings.numThreads = self.numThreads;

  return cpuSettings;
}

@end

@implementation TFLCoreMLDelegateSettings

- (instancetype)initWithCoreMLVersion:(int32_t)coreMLVersion
                       enableddevices:(CoreMLDelegateEnabledDevices)enabledDevices {
  self = [super init];
  if (self) {
    _enabledDevices = enabledDevices;
    _coreMLVersion = coreMLVersion;
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  TFLCoreMLDelegateSettings *coreMLDelegateSettings =
      [[TFLCoreMLDelegateSettings alloc] initWithCoreMLVersion:self.coreMLVersion
                                                enableddevices:self.enabledDevices];
  return coreMLDelegateSettings;
}

@end

@implementation TFLComputeSettings
@synthesize cpuSettings;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.cpuSettings = [[TFLCpuSettings alloc] init];
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  TFLComputeSettings *computeSettings = [[TFLComputeSettings alloc] init];

  computeSettings.cpuSettings = self.cpuSettings;

  return computeSettings;
}

@end

@implementation TFLBaseOptions
@synthesize modelFile;
@synthesize computeSettings;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.computeSettings = [[TFLComputeSettings alloc] init];
    self.modelFile = [[TFLExternalFile alloc] init];
    // Initialized to nil to indicate CoreML Delegate is not enabled yet.
    self.coreMLDelegateSettings = nil;
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  TFLBaseOptions *baseOptions = [[TFLBaseOptions alloc] init];

  baseOptions.modelFile = self.modelFile;
  baseOptions.computeSettings = self.computeSettings;
  baseOptions.coreMLDelegateSettings = self.coreMLDelegateSettings;

  return baseOptions;
}

@end
