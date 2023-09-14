// Copyright 2022 The TensorFlow Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "tensorflow_lite_support/ios/test/task/audio/core/audio_record/utils/sources/AVAudioPCMBuffer+Utils.h"

@implementation AVAudioPCMBuffer (Utils)

- (AVAudioPCMBuffer *)bufferUsingAudioConverter:(AVAudioConverter *)audioConverter {
  // Capacity of converted PCM buffer is calculated in order to maintain the same
  // latency as the input pcmBuffer.
  AVAudioFrameCount capacity = ceil(self.frameLength * audioConverter.outputFormat.sampleRate /
                                    audioConverter.inputFormat.sampleRate);
  AVAudioPCMBuffer *outPCMBuffer = [[AVAudioPCMBuffer alloc]
      initWithPCMFormat:audioConverter.outputFormat
          frameCapacity:capacity * (AVAudioFrameCount)audioConverter.outputFormat.channelCount];

  AVAudioConverterInputBlock inputBlock = ^AVAudioBuffer *_Nullable(
      AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *_Nonnull outStatus) {
    *outStatus = AVAudioConverterInputStatus_HaveData;
    return self;
  };

  AVAudioConverterOutputStatus converterStatus = [audioConverter convertToBuffer:outPCMBuffer
                                                                           error:nil
                                                              withInputFromBlock:inputBlock];
  switch (converterStatus) {
    case AVAudioConverterOutputStatus_HaveData: {
      return outPCMBuffer;
    }
    case AVAudioConverterOutputStatus_InputRanDry:
    case AVAudioConverterOutputStatus_EndOfStream:
    case AVAudioConverterOutputStatus_Error: {
      // Conversion failed so returning a nil. Reason of the error isn't important to the library's
      // users.
      break;
    }
  }

  return nil;
}

+ (nullable AVAudioPCMBuffer *)loadPCMBufferFromFileWithURL:(NSURL *)url {
  AVAudioFile *audioFile = [[AVAudioFile alloc] initForReading:url error:nil];
  AVAudioPCMBuffer *buffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:audioFile.processingFormat
                                    frameCapacity:(AVAudioFrameCount)audioFile.length];

  [audioFile readIntoBuffer:buffer error:nil];

  return buffer;
}

+ (nullable AVAudioPCMBuffer *)loadPCMBufferFromFileWithPath:(NSString *)path
                                            processingFormat:(AVAudioFormat *)processingFormat {
  AVAudioPCMBuffer *buffer =
      [AVAudioPCMBuffer loadPCMBufferFromFileWithURL:[NSURL fileURLWithPath:path]];

  if (!buffer) {
    return nil;
  }

  if ([buffer.format isEqual:processingFormat]) {
    return buffer;
  }

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:buffer.format
                                                                     toFormat:processingFormat];

  return [buffer bufferUsingAudioConverter:audioConverter];
}

+ (nullable AVAudioPCMBuffer *)loadPCMBufferFromFileWithPath:(NSString *)path
                                                 audioFormat:(TFLAudioFormat *)audioFormat {
  // Task library expects float data in interleaved format.
  AVAudioFormat *processingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)audioFormat.channelCount
                                      interleaved:YES];

  return [AVAudioPCMBuffer loadPCMBufferFromFileWithPath:path processingFormat:processingFormat];
}

- (nullable TFLFloatBuffer *)floatBuffer {
  if (self.format.commonFormat != AVAudioPCMFormatFloat32) {
    return nil;
  }

  return [[TFLFloatBuffer alloc] initWithData:self.floatChannelData[0] size:self.frameLength];
}

@end
