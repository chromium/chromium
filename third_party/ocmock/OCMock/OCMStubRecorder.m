/*
 *  Copyright (c) 2004-2021 Erik Doernenburg and contributors
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

#import "OCMStubRecorder.h"
#import "OCClassMockObject.h"
#import "OCMBlockCaller.h"
#import "OCMBoxedReturnValueProvider.h"
#import "OCMExceptionReturnValueProvider.h"
#import "OCMIndirectReturnValueProvider.h"
#import "OCMInvocationStub.h"
#import "OCMNotificationPoster.h"
#import "OCMRealObjectForwarder.h"

#if !defined(OCM_DISABLE_XCTEST_FEATURES)
#import <XCTest/XCTest.h>
#endif

@implementation OCMStubRecorder

#pragma mark Initialisers, description, accessors, etc.

- (id)init
{
    if(invocationMatcher != nil)
        [NSException raise:NSInternalInconsistencyException format:@"** Method init invoked twice on stub recorder. Are you trying to mock the init method? This is currently not supported."];

    self = [super init];
    invocationMatcher = [[OCMInvocationStub alloc] init];
    return self;
}

- (OCMInvocationStub *)stub
{
    return (OCMInvocationStub *)invocationMatcher;
}


#pragma mark Recording invocation actions

- (id)andReturn:(id)anObject
{
    id action;
    if(anObject == mockObject)
    {
        action = [[[OCMNonRetainingObjectReturnValueProvider alloc] initWithValue:anObject] autorelease];
    }
    else
    {
        action = [[[OCMObjectReturnValueProvider alloc] initWithValue:anObject] autorelease];
    }
    [[self stub] addInvocationAction:action];
    return self;
}

- (id)andReturnValue:(NSValue *)aValue
{
    [[self stub] addInvocationAction:[[[OCMBoxedReturnValueProvider alloc] initWithValue:aValue] autorelease]];
    return self;
}

- (id)andThrow:(NSException *)anException
{
    [[self stub] addInvocationAction:[[[OCMExceptionReturnValueProvider alloc] initWithValue:anException] autorelease]];
    return self;
}

- (id)andPost:(NSNotification *)aNotification
{
    [[self stub] addInvocationAction:[[[OCMNotificationPoster alloc] initWithNotification:aNotification] autorelease]];
    return self;
}

- (id)andCall:(SEL)selector onObject:(id)anObject
{
    [[self stub] addInvocationAction:[[[OCMIndirectReturnValueProvider alloc] initWithProvider:anObject andSelector:selector] autorelease]];
    return self;
}

- (id)andDo:(void (^)(NSInvocation *))aBlock
{
    [[self stub] addInvocationAction:[[[OCMBlockCaller alloc] initWithCallBlock:aBlock] autorelease]];
    return self;
}

- (id)andForwardToRealObject
{
    [[self stub] addInvocationAction:[[[OCMRealObjectForwarder alloc] init] autorelease]];
    return self;
}

#if !defined(OCM_DISABLE_XCTEST_FEATURES)
- (id)andFulfill:(XCTestExpectation *)expectation
{
    return [self andDo:^(NSInvocation *invocation) {
        [expectation fulfill];
    }];
}
#endif

#pragma mark Finishing recording

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    [super forwardInvocation:anInvocation];
    [mockObject addStub:[self stub]];
}


@end


@implementation OCMStubRecorder (Properties)

@dynamic _andReturn;

- (OCMStubRecorder * (^)(NSValue *))_andReturn
{
    id (^theBlock)(id) = ^(NSValue *aValue) {
        if(OCMIsObjectType([aValue objCType]))
        {
            id objValue = nil;
            [aValue getValue:&objValue]; // TODO: deprecated but replacement available in 10.13 only
            return [self andReturn:objValue];
        }
        else
        {
            return [self andReturnValue:aValue];
        }
    };
    return (id)[[theBlock copy] autorelease];
}


@dynamic _andThrow;

- (OCMStubRecorder * (^)(NSException *))_andThrow
{
    id (^theBlock)(id) = ^(NSException *anException) {
        return [self andThrow:anException];
    };
    return (id)[[theBlock copy] autorelease];
}


@dynamic _andPost;

- (OCMStubRecorder * (^)(NSNotification *))_andPost
{
    id (^theBlock)(id) = ^(NSNotification *aNotification) {
        return [self andPost:aNotification];
    };
    return (id)[[theBlock copy] autorelease];
}


@dynamic _andCall;

- (OCMStubRecorder * (^)(id, SEL))_andCall
{
    id (^theBlock)(id, SEL) = ^(id anObject, SEL aSelector) {
        return [self andCall:aSelector onObject:anObject];
    };
    return (id)[[theBlock copy] autorelease];
}


@dynamic _andDo;

- (OCMStubRecorder * (^)(void (^)(NSInvocation *)))_andDo
{
    id (^theBlock)(void (^)(NSInvocation *)) = ^(void (^blockToCall)(NSInvocation *)) {
        return [self andDo:blockToCall];
    };
    return (id)[[theBlock copy] autorelease];
}


@dynamic _andForwardToRealObject;

- (OCMStubRecorder * (^)(void))_andForwardToRealObject
{
    id (^theBlock)(void) = ^(void) {
        return [self andForwardToRealObject];
    };
    return (id)[[theBlock copy] autorelease];
}

#if !defined(OCM_DISABLE_XCTEST_FEATURES)

@dynamic _andFulfill;

- (OCMStubRecorder * (^)(XCTestExpectation *))_andFulfill
{
    id (^theBlock)(XCTestExpectation *) = ^(XCTestExpectation *expectation) {
        return [self andFulfill:expectation];
    };
    return (id)[[theBlock copy] autorelease];
}
#endif

@end
