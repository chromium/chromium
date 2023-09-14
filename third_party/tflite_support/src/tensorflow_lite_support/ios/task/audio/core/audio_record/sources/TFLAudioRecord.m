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

#import <AVFoundation/AVFoundation.h>

#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/audio/core/audio_record/sources/TFLAudioRecord.h"
#import "tensorflow_lite_support/ios/task/audio/core/sources/TFLRingBuffer.h"

#define SUPPORTED_CHANNEL_COUNT 2

/** Error domain for TFL Audio Record Errors. */
static NSString *const TFLAudioRecordErrorDomain = @"org.tensorflow.lite.audio.record";

@implementation TFLAudioRecord {
  AVAudioEngine *_audioEngine;

  /* Specifying a custom buffer size on AVAUdioEngine while tapping does not take effect. Hence we
   * are storing the returned samples in a ring buffer to acheive the desired buffer size. If the
   * specified buffer size is shorter than the buffer size supported by `AVAudioEngine` only the
   * most recent data of the buffer of size `bufferSize` will be stored by the ring buffer. */
  TFLRingBuffer *_ringBuffer;
  dispatch_queue_t _conversionQueue;
  NSError *_globalError;
}

- (nullable instancetype)initWithAudioFormat:(TFLAudioFormat *)audioFormat
                                  bufferSize:(NSUInteger)bufferSize
                                       error:(NSError **)error {
  self = [self init];
  if (self) {
    if (audioFormat.channelCount > SUPPORTED_CHANNEL_COUNT) {
      [TFLCommonUtils
          createCustomError:error
                 withDomain:TFLAudioRecordErrorDomain
                       code:TFLAudioRecordErrorCodeInvalidArgumentError
                description:
                    [NSString
                        stringWithFormat:
                            @"The channel count provided does not match the supported "
                            @"channel count. Only up to %d audio channels are currently supported.",
                            SUPPORTED_CHANNEL_COUNT]];
      return nil;
    }

    if (bufferSize % audioFormat.channelCount != 0) {
      [TFLCommonUtils
          createCustomError:error
                 withDomain:TFLAudioRecordErrorDomain
                       code:TFLAudioRecordErrorCodeInvalidArgumentError
                description:@"The buffer size provided is not a multiple of channel count."];
      return nil;
    }

    NSError *waitError = nil;
    [TFLCommonUtils
        createCustomError:&waitError
               withDomain:TFLAudioRecordErrorDomain
                     code:TFLAudioRecordErrorCodeWaitingForNewMicInputError
              description:@"TFLAudioRecord hasn't started receiving samples from the audio "
                          @"input source. Please wait for the input."];

    _globalError = waitError;
    _audioFormat = audioFormat;
    _audioEngine = [[AVAudioEngine alloc] init];
    _bufferSize = bufferSize;

    _ringBuffer = [[TFLRingBuffer alloc] initWithBufferSize:_bufferSize];
    _conversionQueue =
        dispatch_queue_create("org.tensorflow.lite.AudioConversionQueue", NULL);  // Serial Queue
  }
  return self;
}

- (AVAudioPCMBuffer *)bufferFromInputBuffer:(AVAudioPCMBuffer *)pcmBuffer
                        usingAudioConverter:(AVAudioConverter *)audioConverter
                                      error:(NSError **)error {
  // Capacity of converted PCM buffer is calculated in order to maintain the same
  // latency as the input pcmBuffer.
  AVAudioFrameCount capacity = ceil(pcmBuffer.frameLength * audioConverter.outputFormat.sampleRate /
                                    audioConverter.inputFormat.sampleRate);
  AVAudioPCMBuffer *outPCMBuffer = [[AVAudioPCMBuffer alloc]
      initWithPCMFormat:audioConverter.outputFormat
          frameCapacity:capacity * (AVAudioFrameCount)audioConverter.outputFormat.channelCount];

  AVAudioConverterInputBlock inputBlock = ^AVAudioBuffer *_Nullable(
      AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *_Nonnull outStatus) {
    *outStatus = AVAudioConverterInputStatus_HaveData;
    return pcmBuffer;
  };

  NSError *conversionError = nil;
  AVAudioConverterOutputStatus converterStatus = [audioConverter convertToBuffer:outPCMBuffer
                                                                           error:&conversionError
                                                              withInputFromBlock:inputBlock];

  switch (converterStatus) {
    case AVAudioConverterOutputStatus_HaveData: {
      return outPCMBuffer;
    }
    case AVAudioConverterOutputStatus_Error: {
      NSString *errorDescription = conversionError.localizedDescription
                                       ? conversionError.localizedDescription
                                       : @"Some error occured while processing incoming audio "
                                         @"frames.";
      [TFLCommonUtils createCustomError:error
                             withDomain:TFLAudioRecordErrorDomain
                                   code:TFLAudioRecordErrorCodeProcessingError
                            description:errorDescription];
      break;
    }
    case AVAudioConverterOutputStatus_EndOfStream: {
      [TFLCommonUtils createCustomError:error
                             withDomain:TFLAudioRecordErrorDomain
                                   code:TFLAudioRecordErrorCodeProcessingError
                            description:@"Reached end of input audio stream."];
      break;
    }
    case AVAudioConverterOutputStatus_InputRanDry: {
      [TFLCommonUtils createCustomError:error
                             withDomain:TFLAudioRecordErrorDomain
                                   code:TFLAudioRecordErrorCodeProcessingError
                            description:@"Not enough input is available to satisfy the request."];
      break;
    }
  }
  return nil;
}

- (BOOL)loadAudioPCMBuffer:(AVAudioPCMBuffer *)pcmBuffer error:(NSError **)error {
  if (pcmBuffer.frameLength == 0) {
    [TFLCommonUtils createCustomError:error
                           withDomain:TFLAudioRecordErrorDomain
                                 code:TFLAudioRecordErrorCodeInvalidArgumentError
                          description:@"You may have to try with a different "
                                      @"channel count or sample rate"];
  } else if (pcmBuffer.format.commonFormat != AVAudioPCMFormatFloat32) {
    [TFLCommonUtils createCustomError:error
                           withDomain:TFLAudioRecordErrorDomain
                                 code:TFLAudioRecordErrorCodeProcessingError
                          description:@"An error occured while processing mic input."];
  } else {
    // `pcmBuffer` is already converted to an interleaved format since this method is called after
    // -[self bufferFromInputBuffer:usingAudioConverter:error:].
    // If an `AVAudioPCMBuffer` is interleaved, both floatChannelData[0] and floatChannelData[1]
    // point to the same 1d array with both channels in an interleaved format according to:
    // https://developer.apple.com/documentation/avfaudio/avaudiopcmbuffer/1386212-floatchanneldata
    // Hence we can safely access floatChannelData[0] to get the 1D data in interleaved fashion.
    if ([self->_ringBuffer loadFloatData:pcmBuffer.floatChannelData[0]
                                dataSize:pcmBuffer.frameLength
                                  offset:0
                                    size:pcmBuffer.frameLength
                                   error:error]) {
      return YES;
    }
  }
  return NO;
}

- (void)convertAndLoadBuffer:(AVAudioPCMBuffer *)buffer
         usingAudioConverter:(AVAudioConverter *)audioConverter {
  __weak TFLAudioRecord *weakSelf = self;
  dispatch_sync(self->_conversionQueue, ^{
    __strong TFLAudioRecord *strongSelf = weakSelf;
    if (!strongSelf) return;

    NSError *conversionError = nil;
    AVAudioPCMBuffer *convertedPCMBuffer = [strongSelf bufferFromInputBuffer:buffer
                                                         usingAudioConverter:audioConverter
                                                                       error:&conversionError];

    if (!(convertedPCMBuffer && [strongSelf loadAudioPCMBuffer:convertedPCMBuffer
                                                         error:&conversionError])) {
      strongSelf->_globalError = conversionError;
    } else {
      strongSelf->_globalError = nil;
    }
  });
}

- (void)startTappingMicrophoneWithError:(NSError **)error {
  AVAudioNode *inputNode = [_audioEngine inputNode];
  AVAudioFormat *format = [inputNode outputFormatForBus:0];

  AVAudioFormat *recordingFormat =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                       sampleRate:self.audioFormat.sampleRate
                                         channels:(AVAudioChannelCount)self.audioFormat.channelCount
                                      interleaved:YES];

  AVAudioConverter *audioConverter = [[AVAudioConverter alloc] initFromFormat:format
                                                                     toFormat:recordingFormat];

  // Setting buffer size takes no effect on the input node. This class uses a ring buffer internally
  // to ensure the requested buffer size.
  __weak TFLAudioRecord *weakSelf = self;
  [inputNode installTapOnBus:0
                  bufferSize:(AVAudioFrameCount)self.bufferSize
                      format:format
                       block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {
                         [weakSelf convertAndLoadBuffer:buffer usingAudioConverter:audioConverter];
                       }];

  [_audioEngine prepare];
  [_audioEngine startAndReturnError:error];
}

- (BOOL)startRecordingWithError:(NSError **)error {
  switch ([AVAudioSession sharedInstance].recordPermission) {
    case AVAudioSessionRecordPermissionDenied: {
      [TFLCommonUtils createCustomError:error
                             withDomain:TFLAudioRecordErrorDomain
                                   code:TFLAudioRecordErrorCodeRecordPermissionDeniedError
                            description:@"Record permissions were denied by the user. "];
      return NO;
    }

    case AVAudioSessionRecordPermissionGranted: {
      [self startTappingMicrophoneWithError:error];
      return YES;
    }

    case AVAudioSessionRecordPermissionUndetermined: {
      [TFLCommonUtils
          createCustomError:error
                 withDomain:TFLAudioRecordErrorDomain
                       code:TFLAudioRecordErrorCodeRecordPermissionUndeterminedError
                description:@"Record permissions are undertermined. Yo must use AVAudioSession's "
                            @"requestRecordPermission() to request audio record permission from "
                            @"the user. Please read Apple's documentation for further details"
                            @"If record permissions are granted, you can call this "
                            @"method in the completion handler of requestRecordPermission()."];
      return NO;
    }
  }
}

- (void)stop {
  [[_audioEngine inputNode] removeTapOnBus:0];
  [_audioEngine stop];
  __weak TFLRingBuffer *weakRingBuffer = _ringBuffer;
  dispatch_sync(self->_conversionQueue, ^{
    [weakRingBuffer clear];
  });
}

- (nullable TFLFloatBuffer *)readAtOffset:(NSUInteger)offset
                                 withSize:(NSUInteger)size
                                    error:(NSError **)error {
  __block TFLFloatBuffer *bufferToReturn = nil;
  __block NSError *readError = nil;
  __weak TFLAudioRecord *weakSelf = self;

  dispatch_sync(_conversionQueue, ^{
    __strong TFLAudioRecord *strongSelf = weakSelf;
    if (!strongSelf) return;

    if (strongSelf->_globalError) {
      readError = [strongSelf->_globalError copy];
    } else if (offset + size > [strongSelf->_ringBuffer size]) {
      [TFLCommonUtils
          createCustomError:&readError
                 withDomain:TFLAudioRecordErrorDomain
                       code:TFLAudioRecordErrorCodeInvalidArgumentError
                description:@"Index out of bounds: offset + size should be <= to the size of "
                            @"TFLAudioRecord's internal buffer."];
    } else {
      bufferToReturn = [strongSelf->_ringBuffer floatBufferWithOffset:offset size:size];
    }
  });

  if (!bufferToReturn && error) {
    *error = readError;
  }

  return bufferToReturn;
}

@end
