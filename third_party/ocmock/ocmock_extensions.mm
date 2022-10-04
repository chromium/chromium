// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objc/runtime.h>

#include "third_party/ocmock/ocmock_extensions.h"

#define CR_OCMOCK_RETURN_IMPL(type_name, type) \
  - (id)andReturn##type_name:(type)value { \
    return [self andReturnValue:OCMOCK_VALUE(value)]; \
  }

@implementation OCMStubRecorder(CrExtensions)

CR_OCMOCK_RETURN_IMPL(Char, char);
CR_OCMOCK_RETURN_IMPL(UnsignedChar, unsigned char);
CR_OCMOCK_RETURN_IMPL(Short, short);
CR_OCMOCK_RETURN_IMPL(UnsignedShort, unsigned short);
CR_OCMOCK_RETURN_IMPL(Int, int);
CR_OCMOCK_RETURN_IMPL(UnsignedInt, unsigned int);
CR_OCMOCK_RETURN_IMPL(Long, long);
CR_OCMOCK_RETURN_IMPL(UnsignedLong, unsigned long);
CR_OCMOCK_RETURN_IMPL(LongLong, long long);
CR_OCMOCK_RETURN_IMPL(UnsignedLongLong, unsigned long long);
CR_OCMOCK_RETURN_IMPL(Float, float);
CR_OCMOCK_RETURN_IMPL(Double, double);
CR_OCMOCK_RETURN_IMPL(Bool, BOOL);
CR_OCMOCK_RETURN_IMPL(Integer, NSInteger);
CR_OCMOCK_RETURN_IMPL(UnsignedInteger, NSUInteger);

#if !TARGET_OS_IPHONE
CR_OCMOCK_RETURN_IMPL(CGFloat, CGFloat);

- (id)andReturnNSRect:(NSRect)rect {
  return [self andReturnValue:[NSValue valueWithRect:rect]];
}

- (id)andReturnCGRect:(CGRect)rect {
  return [self andReturnNSRect:NSRectFromCGRect(rect)];
}

- (id)andReturnNSPoint:(NSPoint)point {
  return [self andReturnValue:[NSValue valueWithPoint:point]];
}

- (id)andReturnCGPoint:(CGPoint)point {
  return [self andReturnNSPoint:NSPointFromCGPoint(point)];
}
#endif  // !TARGET_OS_IPHONE

@end

@implementation cr_OCMConformToProtocolConstraint

- (id)initWithProtocol:(Protocol*)protocol {
  if (self == [super init]) {
    protocol_ = protocol;
  }
  return self;
}

- (BOOL)evaluate:(id)value {
  return protocol_conformsToProtocol(protocol_, value);
}

@end

@implementation OCMArg(CrExtensions)

+ (id)conformsToProtocol:(Protocol*)protocol {
  return [[[cr_OCMConformToProtocolConstraint alloc] initWithProtocol:protocol]
          autorelease];
}

@end
