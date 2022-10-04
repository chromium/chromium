// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_OCMOCK_OCMOCK_EXTENSIONS_H_
#define THIRD_PARTY_OCMOCK_OCMOCK_EXTENSIONS_H_

#import <Foundation/Foundation.h>

#import "third_party/ocmock/OCMock/OCMock.h"

// Some enhancements to OCMock to make it easier to write mocks.
// Pointers to objects still have to be handled with
// - (id)andReturnValue:OCMOCK_VALUE(blah)
// to keep the types working correctly.
@interface OCMStubRecorder(CrExtensions)
- (id)andReturnChar:(char)value;
- (id)andReturnUnsignedChar:(unsigned char)value;
- (id)andReturnShort:(short)value;
- (id)andReturnUnsignedShort:(unsigned short)value;
- (id)andReturnInt:(int)value;
- (id)andReturnUnsignedInt:(unsigned int)value;
- (id)andReturnLong:(long)value;
- (id)andReturnUnsignedLong:(unsigned long)value;
- (id)andReturnLongLong:(long long)value;
- (id)andReturnUnsignedLongLong:(unsigned long long)value;
- (id)andReturnFloat:(float)value;
- (id)andReturnDouble:(double)value;
- (id)andReturnBool:(BOOL)value;
- (id)andReturnInteger:(NSInteger)value;
- (id)andReturnUnsignedInteger:(NSUInteger)value;
#if !TARGET_OS_IPHONE
- (id)andReturnCGFloat:(CGFloat)value;
- (id)andReturnNSRect:(NSRect)rect;
- (id)andReturnCGRect:(CGRect)rect;
- (id)andReturnNSPoint:(NSPoint)point;
- (id)andReturnCGPoint:(CGPoint)point;
#endif
@end

// A constraint for verifying that something conforms to a protocol.
@interface cr_OCMConformToProtocolConstraint : OCMConstraint {
 @private
  Protocol* protocol_;
}
- (id)initWithProtocol:(Protocol*)protocol;
@end

@interface OCMArg(CrExtensions)
+ (id)conformsToProtocol:(Protocol*)protocol;
@end

#endif  // THIRD_PARTY_OCMOCK_OCMOCK_EXTENSIONS_H_
