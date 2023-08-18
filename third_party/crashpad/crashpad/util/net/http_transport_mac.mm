// Copyright 2014 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/net/http_transport.h"

#import <Foundation/Foundation.h>
#include <sys/utsname.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "package.h"
#include "util/file/file_io.h"
#include "util/misc/implicit_cast.h"
#include "util/misc/metrics.h"
#include "util/net/http_body.h"

// An implementation of NSInputStream that reads from a
// crashpad::HTTPBodyStream.
@interface CrashpadHTTPBodyStreamTransport : NSInputStream {
 @private
  NSStreamStatus _streamStatus;
  id<NSStreamDelegate> __strong _delegate;
  crashpad::HTTPBodyStream* _bodyStream;  // weak
}
- (instancetype)initWithBodyStream:(crashpad::HTTPBodyStream*)bodyStream;
@end

@implementation CrashpadHTTPBodyStreamTransport

- (instancetype)initWithBodyStream:(crashpad::HTTPBodyStream*)bodyStream {
  if ((self = [super init])) {
    _streamStatus = NSStreamStatusNotOpen;
    _bodyStream = bodyStream;
  }
  return self;
}

// NSInputStream:

- (BOOL)hasBytesAvailable {
  // Per Apple's documentation: "May also return YES if a read must be attempted
  // in order to determine the availability of bytes."
  switch (_streamStatus) {
    case NSStreamStatusAtEnd:
    case NSStreamStatusClosed:
    case NSStreamStatusError:
      return NO;
    default:
      return YES;
  }
}

- (NSInteger)read:(uint8_t*)buffer maxLength:(NSUInteger)maxLen {
  _streamStatus = NSStreamStatusReading;

  crashpad::FileOperationResult rv =
      _bodyStream->GetBytesBuffer(buffer, maxLen);

  if (rv == 0)
    _streamStatus = NSStreamStatusAtEnd;
  else if (rv < 0)
    _streamStatus = NSStreamStatusError;
  else
    _streamStatus = NSStreamStatusOpen;

  return rv;
}

- (BOOL)getBuffer:(uint8_t**)buffer length:(NSUInteger*)length {
  return NO;
}

// NSStream:

- (void)scheduleInRunLoop:(NSRunLoop*)runLoop forMode:(NSString*)mode {
}

- (void)removeFromRunLoop:(NSRunLoop*)runLoop forMode:(NSString*)mode {
}

- (void)open {
  _streamStatus = NSStreamStatusOpen;
}

- (void)close {
  _streamStatus = NSStreamStatusClosed;
}

- (NSStreamStatus)streamStatus {
  return _streamStatus;
}

- (id<NSStreamDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id)delegate {
  _delegate = delegate;
}

- (id)propertyForKey:(NSStreamPropertyKey)key {
  return nil;
}

- (BOOL)setProperty:(id)property forKey:(NSStreamPropertyKey)key {
  return NO;
}

@end

namespace crashpad {

namespace {

NSString* AppendEscapedFormat(NSString* base,
                              NSString* format,
                              NSString* data) {
  return [base stringByAppendingFormat:
                   format,
                   [data stringByAddingPercentEncodingWithAllowedCharacters:
                             [[NSCharacterSet
                                 characterSetWithCharactersInString:
                                     @"()<>@,;:\\\"/[]?={} \t"] invertedSet]]];
}

// This builds the same User-Agent string that CFNetwork would build internally,
// but it uses PACKAGE_NAME and PACKAGE_VERSION in place of values obtained from
// the main bundle’s Info.plist.
NSString* UserAgentString() {
  NSString* user_agent = [NSString string];

  // CFNetwork would use the main bundle’s CFBundleName, or the main
  // executable’s filename if none.
  user_agent = AppendEscapedFormat(user_agent, @"%@", @PACKAGE_NAME);

  // CFNetwork would use the main bundle’s CFBundleVersion, or the string
  // “(unknown version)” if none.
  user_agent = AppendEscapedFormat(user_agent, @"/%@", @PACKAGE_VERSION);

  // Expected to be CFNetwork.
  NSBundle* nsurl_bundle = [NSBundle bundleForClass:[NSURLRequest class]];
  NSString* bundle_name = base::apple::ObjCCast<NSString>([nsurl_bundle
      objectForInfoDictionaryKey:base::apple::CFToNSPtrCast(kCFBundleNameKey)]);
  if (bundle_name) {
    user_agent = AppendEscapedFormat(user_agent, @" %@", bundle_name);

    NSString* bundle_version = base::apple::ObjCCast<NSString>(
        [nsurl_bundle objectForInfoDictionaryKey:base::apple::CFToNSPtrCast(
                                                     kCFBundleVersionKey)]);
    if (bundle_version) {
      user_agent = AppendEscapedFormat(user_agent, @"/%@", bundle_version);
    }
  }

  utsname os;
  if (uname(&os) != 0) {
    PLOG(WARNING) << "uname";
  } else {
    user_agent = AppendEscapedFormat(user_agent, @" %@", @(os.sysname));
    user_agent = AppendEscapedFormat(user_agent, @"/%@", @(os.release));

    // CFNetwork just uses the equivalent of os.machine to obtain the native
    // (kernel) architecture. Here, give the process’ architecture as well as
    // the native architecture. Use the same strings that the kernel would, so
    // that they can be de-duplicated.
#if defined(ARCH_CPU_X86)
    NSString* arch = @"i386";
#elif defined(ARCH_CPU_X86_64)
    NSString* arch = @"x86_64";
#elif defined(ARCH_CPU_ARM64)
    NSString* arch = @"arm64";
#else
#error Port
#endif
    user_agent = AppendEscapedFormat(user_agent, @" (%@", arch);

    NSString* machine = @(os.machine);
    if (![machine isEqualToString:arch]) {
      user_agent = AppendEscapedFormat(user_agent, @"; %@", machine);
    }

    user_agent = [user_agent stringByAppendingString:@")"];
  }

  return user_agent;
}

class HTTPTransportMac final : public HTTPTransport {
 public:
  HTTPTransportMac();

  HTTPTransportMac(const HTTPTransportMac&) = delete;
  HTTPTransportMac& operator=(const HTTPTransportMac&) = delete;

  ~HTTPTransportMac() override;

  bool ExecuteSynchronously(std::string* response_body) override;
};

HTTPTransportMac::HTTPTransportMac() : HTTPTransport() {
}

HTTPTransportMac::~HTTPTransportMac() {
}

bool HTTPTransportMac::ExecuteSynchronously(std::string* response_body) {
  DCHECK(body_stream());

  @autoreleasepool {
    NSString* url_ns_string = base::SysUTF8ToNSString(url());
    NSURL* url = [NSURL URLWithString:url_ns_string];
    NSMutableURLRequest* request =
        [NSMutableURLRequest requestWithURL:url
                                cachePolicy:NSURLRequestUseProtocolCachePolicy
                            timeoutInterval:timeout()];
    [request setHTTPMethod:base::SysUTF8ToNSString(method())];

    // If left to its own devices, CFNetwork would build a user-agent string
    // based on keys in the main bundle’s Info.plist, giving ugly results if
    // there is no Info.plist. Provide a User-Agent string similar to the one
    // that CFNetwork would use, but with appropriate values in place of the
    // Info.plist-derived strings.
    [request setValue:UserAgentString() forHTTPHeaderField:@"User-Agent"];

    for (const auto& pair : headers()) {
      [request setValue:base::SysUTF8ToNSString(pair.second)
          forHTTPHeaderField:base::SysUTF8ToNSString(pair.first)];
    }

    NSInputStream* input_stream = [[CrashpadHTTPBodyStreamTransport alloc]
        initWithBodyStream:body_stream()];
    [request setHTTPBodyStream:input_stream];

    NSURLResponse* response = nil;
    NSError* error = nil;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // Deprecated in OS X 10.11. The suggested replacement, NSURLSession, is
    // only available on 10.9 and later, and this needs to run on earlier
    // releases.
    NSData* body = [NSURLConnection sendSynchronousRequest:request
                                         returningResponse:&response
                                                     error:&error];
#pragma clang diagnostic pop

    if (error) {
      Metrics::CrashUploadErrorCode(error.code);
      LOG(ERROR) << [[error localizedDescription] UTF8String] << " ("
                 << [[error domain] UTF8String] << " " << [error code] << ")";
      return false;
    }
    if (!response) {
      LOG(ERROR) << "no response";
      return false;
    }
    NSHTTPURLResponse* http_response =
        base::apple::ObjCCast<NSHTTPURLResponse>(response);
    if (!http_response) {
      LOG(ERROR) << "no http_response";
      return false;
    }
    NSInteger http_status = [http_response statusCode];
    if (http_status < 200 || http_status > 203) {
      LOG(ERROR) << base::StringPrintf("HTTP status %ld",
                                       implicit_cast<long>(http_status));
      return false;
    }

    if (response_body) {
      response_body->assign(static_cast<const char*>([body bytes]),
                            [body length]);
    }

    return true;
  }
}

}  // namespace

// static
std::unique_ptr<HTTPTransport> HTTPTransport::Create() {
  return std::unique_ptr<HTTPTransport>(new HTTPTransportMac());
}

}  // namespace crashpad
