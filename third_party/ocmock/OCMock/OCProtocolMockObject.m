/*
 *  Copyright (c) 2005-2021 Erik Doernenburg and contributors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use these files except in compliance with the License. You may obtain
 *  a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 */

#import <objc/runtime.h>
#import "OCProtocolMockObject.h"


@implementation OCProtocolMockObject

#pragma mark Initialisers, description, accessors, etc.

- (id)initWithProtocols:(NSArray<Protocol *> *)aProtocols
{
    if(aProtocols == nil)
        [NSException raise:NSInvalidArgumentException format:@"Protocols cannot be nil."];
    if([aProtocols count] == 0)
        [NSException raise:NSInvalidArgumentException format:@"Protocols cannot be empty."];
    for (Protocol *protocol in aProtocols) {
        if(protocol == nil)
           [NSException raise:NSInvalidArgumentException format:@"Protocol cannot be nil."];
    }

    [super init];
    mockedProtocols = aProtocols;
    return self;
}

- (id)initWithProtocol:(Protocol *)aProtocol
{
    if(aProtocol == nil)
        [NSException raise:NSInvalidArgumentException format:@"Protocol cannot be nil."];

    return [self initWithProtocols:@[aProtocol]];
}

- (NSString *)description
{
    if ([mockedProtocols count] == 1) {
      const char *name = protocol_getName(mockedProtocols[0]);
      return [NSString stringWithFormat:@"OCProtocolMockObject(%s)", name];
    }

    NSMutableString* string = [[NSMutableString alloc] initWithString:@"OCProtoolMockObject(["];
    for (int i = 0; i < [mockedProtocols count]; i++) {
        if (i > 0) [string appendString:@", "];
        [string appendFormat:@"%s", protocol_getName(mockedProtocols[i])];
    }
    [string appendString:@"])"];
    return string;
}

#pragma mark Proxy API

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    struct { BOOL isRequired; BOOL isInstance; } opts[4] = { {YES, YES}, {NO, YES}, {YES, NO}, {NO, NO} };
    for(int i = 0; i < 4; i++)
    {
      for (Protocol *mockedProtocol in mockedProtocols) {
        struct objc_method_description methodDescription = protocol_getMethodDescription(mockedProtocol, aSelector, opts[i].isRequired, opts[i].isInstance);
        if(methodDescription.name != NULL)
          return [NSMethodSignature signatureWithObjCTypes:methodDescription.types];
      }
    }
    return nil;
}

- (BOOL)conformsToProtocol:(Protocol *)aProtocol
{
  for (Protocol *mockedProtocol in mockedProtocols) {

    if(protocol_conformsToProtocol(mockedProtocol, aProtocol)) return YES;
  }
  return NO;
}

- (BOOL)respondsToSelector:(SEL)selector
{
  for (Protocol *mockedProtocol in mockedProtocols) {
    if ([self methodSignatureForSelector:selector] != nil) {
      return YES;
    }
  }
  return NO;
}

@end
