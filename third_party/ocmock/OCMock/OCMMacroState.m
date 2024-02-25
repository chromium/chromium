/*
 *  Copyright (c) 2014-2021 Erik Doernenburg and contributors
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

#import "OCMMacroState.h"
#import "OCMExpectationRecorder.h"
#import "OCMVerifier.h"


@implementation OCMMacroState

static NSString *const OCMGlobalStateKey = @"OCMGlobalStateKey";

#pragma mark Methods to begin/end macros

+ (void)beginStubMacro
{
    OCMStubRecorder *recorder = [[[OCMStubRecorder alloc] init] autorelease];
    OCMMacroState *macroState = [[OCMMacroState alloc] initWithRecorder:recorder];
    [NSThread currentThread].threadDictionary[OCMGlobalStateKey] = macroState;
    [macroState release];
}

+ (OCMStubRecorder *)endStubMacro
{
    NSMutableDictionary *threadDictionary = [NSThread currentThread].threadDictionary;
    OCMMacroState *globalState = threadDictionary[OCMGlobalStateKey];
    OCMStubRecorder *recorder = [[(OCMStubRecorder *)[globalState recorder] retain] autorelease];
    BOOL didThrow = [globalState invocationDidThrow];
    [threadDictionary removeObjectForKey:OCMGlobalStateKey];
    if(didThrow == NO && [recorder didRecordInvocation] == NO)
    {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Did not record an invocation in OCMStub/OCMExpect/OCMReject.\n"
                           @"Possible causes are:\n"
                           @"- The receiver is not a mock object.\n"
                           @"- The selector conflicts with a selector implemented by OCMStubRecorder/OCMExpectationRecorder."];
    }
    return recorder;
}


+ (void)beginExpectMacro
{
    OCMExpectationRecorder *recorder = [[[OCMExpectationRecorder alloc] init] autorelease];
    OCMMacroState *macroState = [[OCMMacroState alloc] initWithRecorder:recorder];
    [NSThread currentThread].threadDictionary[OCMGlobalStateKey] = macroState;
    [macroState release];
}

+ (OCMStubRecorder *)endExpectMacro
{
    return [self endStubMacro];
}


+ (void)beginRejectMacro
{
    OCMExpectationRecorder *recorder = [[[OCMExpectationRecorder alloc] init] autorelease];
    OCMMacroState *macroState = [[OCMMacroState alloc] initWithRecorder:recorder];
    [NSThread currentThread].threadDictionary[OCMGlobalStateKey] = macroState;
    [macroState release];
}

+ (OCMStubRecorder *)endRejectMacro
{
    OCMMacroState *globalState = [NSThread currentThread].threadDictionary[OCMGlobalStateKey];
    // Calling never after the invocation to avoid running afoul of ARC's expectations on
    // return values from init methods.
    [(OCMExpectationRecorder *)[globalState recorder] never];
    return [self endStubMacro];
}


+ (void)beginVerifyMacroAtLocation:(OCMLocation *)aLocation
{
    return [self beginVerifyMacroAtLocation:aLocation withQuantifier:nil];
}

+ (void)beginVerifyMacroAtLocation:(OCMLocation *)aLocation withQuantifier:(OCMQuantifier *)quantifier
{
    OCMVerifier *recorder = [[[OCMVerifier alloc] init] autorelease];
    [recorder setLocation:aLocation];
    [recorder setQuantifier:quantifier];
    OCMMacroState *macroState = [[OCMMacroState alloc] initWithRecorder:recorder];
    [NSThread currentThread].threadDictionary[OCMGlobalStateKey] = macroState;
    [macroState release];
}

+ (void)endVerifyMacro
{
    NSMutableDictionary *threadDictionary = [NSThread currentThread].threadDictionary;
    OCMMacroState *globalState = threadDictionary[OCMGlobalStateKey];
    OCMVerifier *verifier = [[(OCMVerifier *)[globalState recorder] retain] autorelease];
    BOOL didThrow = [globalState invocationDidThrow];
    [threadDictionary removeObjectForKey:OCMGlobalStateKey];
    if(didThrow == NO && [verifier didRecordInvocation] == NO)
    {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Did not record an invocation in OCMVerify.\n"
                           @"Possible causes are:\n"
                           @"- The receiver is not a mock object.\n"
                           @"- The selector conflicts with a selector implemented by OCMVerifier."];
    }
}


#pragma mark Accessing global state

+ (OCMMacroState *)globalState
{
    return [NSThread currentThread].threadDictionary[OCMGlobalStateKey];
}


#pragma mark Init, dealloc, accessors

- (id)initWithRecorder:(OCMRecorder *)aRecorder
{
    if((self = [super init]))
    {
        recorder = [aRecorder retain];
    }

    return self;
}

- (void)dealloc
{
    [recorder release];
    if([NSThread currentThread].threadDictionary[OCMGlobalStateKey] == self)
        [NSException raise:NSInternalInconsistencyException format:@"Unexpected dealloc while set as the global state"];
    [super dealloc];
}

- (void)setRecorder:(OCMRecorder *)aRecorder
{
    [recorder autorelease];
    recorder = [aRecorder retain];
}

- (OCMRecorder *)recorder
{
    return recorder;
}

- (void)setInvocationDidThrow:(BOOL)flag
{
    invocationDidThrow = flag;
}

- (BOOL)invocationDidThrow
{
    return invocationDidThrow;
}


#pragma mark Changing the recorder

- (void)switchToClassMethod
{
    [recorder classMethod];
}


@end
