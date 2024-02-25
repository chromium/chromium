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

- (id)initWithProtocol:(Protocol *)aProtocol
{
    if(aProtocol == nil)
        [NSException raise:NSInvalidArgumentException format:@"Protocol cannot be nil."];

    [super init];
    mockedProtocol = aProtocol;
    return self;
}

- (NSString *)description
{
    const char *name = protocol_getName(mockedProtocol);
    return [NSString stringWithFormat:@"OCProtocolMockObject(%s)", name];
}

#pragma mark Proxy API

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    struct { BOOL isRequired; BOOL isInstance; } opts[4] = { {YES, YES}, {NO, YES}, {YES, NO}, {NO, NO} };
    for(int i = 0; i < 4; i++)
    {
        struct objc_method_description methodDescription = protocol_getMethodDescription(mockedProtocol, aSelector, opts[i].isRequired, opts[i].isInstance);
        if(methodDescription.name != NULL)
            return [NSMethodSignature signatureWithObjCTypes:methodDescription.types];
    }
    return nil;
}

- (BOOL)conformsToProtocol:(Protocol *)aProtocol
{
    return protocol_conformsToProtocol(mockedProtocol, aProtocol);
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return ([self methodSignatureForSelector:selector] != nil);
}

@end
