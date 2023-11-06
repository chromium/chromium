// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objc/runtime.h>

#import "third_party/ocmock/OCMock/NSInvocation+OCMAdditions.h"
#import "third_party/ocmock/OCMock/OCMArgAction.h"
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

@interface cr_OCMAsyncBlockArgCaller : OCMArgAction {
  dispatch_queue_t _queue;
  NSArray* _arguments;
}

- (instancetype)initWithQueue:(dispatch_queue_t)queue
            andBlockArguments:(NSArray*)args;

@end

@implementation cr_OCMAsyncBlockArgCaller

- (instancetype)initWithQueue:(dispatch_queue_t)queue
            andBlockArguments:(NSArray*)args {
  self = [super init];
  if (self) {
    _queue = queue;
    dispatch_retain(_queue);
    _arguments = [args copy];
  }
  return self;
}

- (void)dealloc {
  [_arguments release];
  dispatch_release(_queue);
  [super dealloc];
}

- (void)handleArgument:(id)aBlock {
  if (aBlock) {
    id copiedBlock = Block_copy(aBlock);
    NSArray* retainedArgs = [_arguments retain];
    dispatch_async(_queue, ^{
      NSInvocation* inv = [NSInvocation invocationForBlock:copiedBlock
                                             withArguments:retainedArgs];
      [inv invokeWithTarget:copiedBlock];
      [retainedArgs release];
      Block_release(copiedBlock);
    });
  }
}

@end

@implementation OCMArg(CrExtensions)

+ (id)conformsToProtocol:(Protocol*)protocol {
  return [[[cr_OCMConformToProtocolConstraint alloc] initWithProtocol:protocol]
          autorelease];
}

+ (id)invokeBlockOnQueue:(dispatch_queue_t)queue
                withArgs:(id)first, ... NS_REQUIRES_NIL_TERMINATION {
  NSMutableArray* params = [NSMutableArray array];
  va_list args;
  if (first) {
    [params addObject:first];
    va_start(args, first);
    id obj;
    while ((obj = va_arg(args, id))) {
      [params addObject:obj];
    }
    va_end(args);
  }
  return [[[cr_OCMAsyncBlockArgCaller alloc] initWithQueue:queue
                                         andBlockArguments:params] autorelease];
}

@end

@implementation OCMockObject (CrExtensions)

- (void)clearInvocations {
  @synchronized(invocations) {
    [invocations removeAllObjects];
  }
}

@end
