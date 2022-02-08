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

  [cpuSettings setNumThreads:self.numThreads];

  return cpuSettings;
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

  [computeSettings setCpuSettings:self.cpuSettings];

  return computeSettings;
}

@end

@implementation TFLExternalFile
@synthesize filePath;

- (id)copyWithZone:(NSZone *)zone {
  TFLExternalFile *externalFile = [[TFLExternalFile alloc] init];

  [externalFile setFilePath:self.filePath];

  return externalFile;
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
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  TFLBaseOptions *baseOptions = [[TFLBaseOptions alloc] init];

  [baseOptions setModelFile:self.modelFile];
  [baseOptions setComputeSettings:self.computeSettings];

  return baseOptions;
}

@end
