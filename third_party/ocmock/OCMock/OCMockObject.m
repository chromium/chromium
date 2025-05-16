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

#import "NSInvocation+OCMAdditions.h"
#import "OCMockObject.h"
#import "OCClassMockObject.h"
#import "OCMExceptionReturnValueProvider.h"
#import "OCMExpectationRecorder.h"
#import "OCMFunctionsPrivate.h"
#import "OCMInvocationExpectation.h"
#import "OCMLocation.h"
#import "OCMMacroState.h"
#import "OCMQuantifier.h"
#import "OCMVerifier.h"
#import "OCObserverMockObject.h"
#import "OCPartialMockObject.h"
#import "OCProtocolMockObject.h"


@implementation OCMockObject

#pragma mark Class initialisation

+ (void)initialize
{
    if([[NSInvocation class] instanceMethodSignatureForSelector:@selector(getArgumentAtIndexAsObject:)] == NULL)
        [NSException raise:NSInternalInconsistencyException format:@"** Expected method not present; the method getArgumentAtIndexAsObject: is not implemented by NSInvocation. If you see this exception it is likely that you are using the static library version of OCMock and your project is not configured correctly to load categories from static libraries. Did you forget to add the -ObjC linker flag?"];
}


#pragma mark Factory methods

+ (id)mockForClass:(Class)aClass
{
    return [[[OCClassMockObject alloc] initWithClass:aClass] autorelease];
}

+ (id)mockForProtocol:(Protocol *)aProtocol
{
    return [[[OCProtocolMockObject alloc] initWithProtocol:aProtocol] autorelease];
}

+ (id)partialMockForObject:(NSObject *)anObject
{
    return [[[OCPartialMockObject alloc] initWithObject:anObject] autorelease];
}


+ (id)niceMockForClass:(Class)aClass
{
    return [self _makeNice:[self mockForClass:aClass]];
}

+ (id)niceMockForProtocol:(Protocol *)aProtocol
{
    return [self _makeNice:[self mockForProtocol:aProtocol]];
}


+ (id)_makeNice:(OCMockObject *)mock
{
    mock->isNice = YES;
    return mock;
}


+ (id)observerMock
{
    return [[[OCObserverMockObject alloc] init] autorelease];
}


#pragma mark Initialisers, description, accessors, etc.

- (instancetype)init
{
    // Check whether init is called a second time, which can happen when stubbing alloc/init. Note
    // that you only really stub the alloc method. Init cannot be stubbed. Invocations of init
    // will always end up here, and we return self. If init is invoked inside a macro that's an
    // error, which will be detected in the init method of the recorder.
    if(stubs != nil)
    {
        // check if we are called from inside a macro
        OCMRecorder *recorder = [[OCMMacroState globalState] recorder];
        if(recorder != nil)
        {
            [recorder setMockObject:self];
            return (id)[[recorder retain] init];
        }
        return self;
    }

    if([self class] == [OCMockObject class])
    {
        [NSException raise:NSInternalInconsistencyException format:@"*** Cannot create instances of OCMockObject. Please use one of the subclasses."];
    }

    // no [super init], we're inheriting from NSProxy
    expectationOrderMatters = NO;
    stubs = [[NSMutableArray alloc] init];
    expectations = [[NSMutableArray alloc] init];
    exceptions = [[NSMutableArray alloc] init];
    invocations = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    [stubs release];
    [expectations release];
    [exceptions release];
    [invocations release];
    [super dealloc];
}

- (NSString *)description
{
    return @"OCMockObject";
}

- (void)addStub:(OCMInvocationStub *)aStub
{
    [self assertInvocationsArrayIsPresent];
    @synchronized(stubs)
    {
        [stubs addObject:aStub];
    }
}

- (OCMInvocationStub *)stubForInvocation:(NSInvocation *)anInvocation
{
    @synchronized(stubs)
    {
        for(OCMInvocationStub *stub in stubs)
            if([stub matchesInvocation:anInvocation])
                return stub;
        return nil;
    }
}

- (void)addExpectation:(OCMInvocationExpectation *)anExpectation
{
    @synchronized(expectations)
    {
        [expectations addObject:anExpectation];
    }
}

- (void)assertInvocationsArrayIsPresent
{
    if(invocations == nil)
    {
        [NSException raise:NSInternalInconsistencyException format:@"** Cannot use mock object %@ at %p. This error usually occurs when a mock object is used after stopMocking has been called on it. In most cases it is not necessary to call stopMocking. If you know you have to, please make sure that the mock object is not used afterwards.", [self description], (void *)self];
    }
}

- (void)addInvocation:(NSInvocation *)anInvocation
{
    @synchronized(invocations)
    {
        // We can't do a normal retain arguments on anInvocation because its target/arguments/return
        // value could be self. That would produce a retain cycle self->invocations->anInvocation->self.
        // However we need to retain everything on anInvocation that isn't self because we expect them to
        // stick around after this method returns. Use our special method to retain just what's needed.
        // This still doesn't completely prevent retain cycles since any of the arguments could have a
        // strong reference to self. Those will have to be broken with manual calls to -stopMocking.
        [anInvocation retainObjectArgumentsExcludingObject:self];
        [invocations addObject:anInvocation];
    }
}


#pragma mark Public API

- (void)setExpectationOrderMatters:(BOOL)flag
{
    expectationOrderMatters = flag;
}

- (void)stopMocking
{
    // invocations can contain objects that clients expect to be deallocated by now,
    // and they can also have a strong reference to self, creating a retain cycle. Get
    // rid of all of the invocations to hopefully let their objects deallocate, and to
    // break any retain cycles involving self.
    @synchronized(invocations)
    {
        [invocations removeAllObjects];
        [invocations autorelease];
        invocations = nil;
    }
}


- (id)stub
{
    return [[[OCMStubRecorder alloc] initWithMockObject:self] autorelease];
}

- (id)expect
{
    return [[[OCMExpectationRecorder alloc] initWithMockObject:self] autorelease];
}

- (id)reject
{
    return [[self expect] never];
}


- (id)verify
{
    return [self verifyAtLocation:nil];
}

- (id)verifyAtLocation:(OCMLocation *)location
{
    NSMutableArray *unsatisfiedExpectations = [NSMutableArray array];
    @synchronized(expectations)
    {
        for(OCMInvocationExpectation *e in expectations)
        {
            if(![e isSatisfied])
                [unsatisfiedExpectations addObject:e];
        }
    }

    if([unsatisfiedExpectations count] == 1)
    {
        NSString *description = [NSString stringWithFormat:@"%@: expected method was not invoked: %@",
                                          [self description], [[unsatisfiedExpectations objectAtIndex:0] description]];
        OCMReportFailure(location, description);
    }
    else if([unsatisfiedExpectations count] > 0)
    {
        NSString *description = [NSString stringWithFormat:@"%@: %@ expected methods were not invoked: %@",
                                          [self description], @([unsatisfiedExpectations count]), [self _stubDescriptions:YES]];
        OCMReportFailure(location, description);
    }

    OCMInvocationExpectation *firstException = nil;
    @synchronized(exceptions)
    {
        firstException = [exceptions.firstObject retain];
    }
    if(firstException)
    {
        NSString *description = [NSString stringWithFormat:@"%@: %@ (This is a strict mock failure that was ignored when it actually occurred.)",
                                          [self description], [firstException description]];
        OCMReportFailure(location, description);
    }
    [firstException release];

    return [[[OCMVerifier alloc] initWithMockObject:self] autorelease];
}


- (void)verifyWithDelay:(NSTimeInterval)delay
{
    [self verifyWithDelay:delay atLocation:nil];
}

- (void)verifyWithDelay:(NSTimeInterval)delay atLocation:(OCMLocation *)location
{
    NSTimeInterval step = 0.01;
    while(delay > 0)
    {
        @synchronized(expectations)
        {
            BOOL allExpectationsAreMatchAndReject = YES;
            for(OCMInvocationExpectation *expectation in expectations)
            {
                if(![expectation isMatchAndReject])
                {
                    allExpectationsAreMatchAndReject = NO;
                    break;
                }
            }
            if(allExpectationsAreMatchAndReject)
                break;
        }
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:MIN(step, delay)]];
        delay -= step;
        step *= 2;
    }
    [self verifyAtLocation:location];
}


#pragma mark Verify after running

- (void)verifyInvocation:(OCMInvocationMatcher *)matcher
{
    [self verifyInvocation:matcher atLocation:nil];
}

- (void)verifyInvocation:(OCMInvocationMatcher *)matcher atLocation:(OCMLocation *)location
{
    [self verifyInvocation:matcher withQuantifier:nil atLocation:location];
}

- (void)verifyInvocation:(OCMInvocationMatcher *)matcher withQuantifier:(OCMQuantifier *)quantifier atLocation:(OCMLocation *)location
{
    NSUInteger count = 0;
    [self assertInvocationsArrayIsPresent];
    @synchronized(invocations)
    {
        for(NSInvocation *invocation in invocations)
        {
            if([matcher matchesInvocation:invocation])
                count += 1;
        }
    }
    if(quantifier == nil)
        quantifier = [OCMQuantifier atLeast:1];
    if(![quantifier isValidCount:count])
    {
        NSString *description = [self descriptionForVerificationFailureWithMatcher:matcher quantifier:quantifier invocationCount:count];
        OCMReportFailure(location, description);
    }
}

- (NSString *)descriptionForVerificationFailureWithMatcher:(OCMInvocationMatcher *)matcher quantifier:(OCMQuantifier *)quantifier invocationCount:(NSUInteger)count
{
    NSString *actualDescription = nil;
    switch(count)
    {
        case 0:  actualDescription = @"not invoked";  break;
        case 1:  actualDescription = @"invoked once"; break;
        default: actualDescription = [NSString stringWithFormat:@"invoked %lu times", (unsigned long)count]; break;
    }

    return [NSString stringWithFormat:@"%@: Method `%@` was %@; but was expected %@.",
                     [self description], [matcher description], actualDescription, [quantifier description]];
}


#pragma mark Handling invocations

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    if([OCMMacroState globalState] != nil)
    {
        OCMRecorder *recorder = [[OCMMacroState globalState] recorder];
        [recorder setMockObject:self];
        // In order for ARC to work correctly, the recorder has to set up return values for
        // methods in the init family of methods. If the mock forwards a method to the recorder
        // that it will record, i.e. a method that the recorder does not implement, then the
        // recorder must set the mock as the return value. Otherwise it must use itself.
        [recorder setShouldReturnMockFromInit:(class_getInstanceMethod(object_getClass(recorder), aSelector) == NO)];
        return recorder;
    }
    return nil;
}


- (BOOL)handleSelector:(SEL)sel
{
    @synchronized(stubs)
    {
        for(OCMInvocationStub *recorder in stubs)
            if([recorder matchesSelector:sel])
                return YES;
    }
    return NO;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    @try
    {
        if([self handleInvocation:anInvocation] == NO)
            [self handleUnRecordedInvocation:anInvocation];
    }
    @catch(NSException *e)
    {
        if([[e name] isEqualToString:OCMStubbedException])
        {
            e = [[e userInfo] objectForKey:@"exception"];
        }
        else
        {
            // add non-stubbed method to list of exceptions to be re-raised in verify
            @synchronized(exceptions)
            {
                [exceptions addObject:e];
            }
        }
        [e raise];
    }
}

- (BOOL)handleInvocation:(NSInvocation *)anInvocation
{
    [self assertInvocationsArrayIsPresent];
    [self addInvocation:anInvocation];

    OCMInvocationStub *stub = [self stubForInvocation:anInvocation];
    if(stub == nil)
        return NO;

    // Retain the stub in case it ends up being removed because we still need it at the end for handleInvocation:
    [stub retain];

    BOOL removeStub = NO;
    @synchronized(expectations)
    {
        if([expectations containsObject:stub])
        {
            OCMInvocationExpectation *expectation = [self _nextExpectedInvocation];
            if(expectationOrderMatters && (expectation != stub))
            {
                [NSException raise:NSInternalInconsistencyException
                            format:@"%@: unexpected method invoked: %@\n\texpected:\t%@",
                            [self description], [stub description], [[expectations objectAtIndex:0] description]];
            }

            // We can't check isSatisfied yet, since the stub won't be satisfied until we call
            // handleInvocation: since we'll still have the current expectation in the expectations array, which
            // will cause an exception if expectationOrderMatters is YES and we're not ready for any future
            // expected methods to be called yet
            if(![(OCMInvocationExpectation *)stub isMatchAndReject])
            {
                [expectations removeObject:stub];
                removeStub = YES;
            }
        }
    }
    if(removeStub)
    {
        @synchronized(stubs)
        {
            [stubs removeObject:stub];
        }
    }

    @try
    {
        [stub handleInvocation:anInvocation];
    }
    @finally
    {
        [stub release];
    }

    return YES;
}

// Must be synchronized on expectations when calling this method.
- (OCMInvocationExpectation *)_nextExpectedInvocation
{
    for(OCMInvocationExpectation *expectation in expectations)
        if(![expectation isMatchAndReject])
            return expectation;
    return nil;
}

- (void)handleUnRecordedInvocation:(NSInvocation *)anInvocation
{
    if(isNice == NO)
    {
        [NSException raise:NSInternalInconsistencyException
                    format:@"%@: unexpected method invoked: %@ %@",
                    [self description], [anInvocation invocationDescription], [self _stubDescriptions:NO]];
    }
}

- (void)doesNotRecognizeSelector:(SEL)aSelector __unused
{
    if([OCMMacroState globalState] != nil)
    {
        // we can't do anything clever with the macro state because we must raise an exception here
        [NSException raise:NSInvalidArgumentException
                    format:@"%@: Cannot stub/expect/verify method '%@' because no such method exists in the mocked class.",
                    [self description], NSStringFromSelector(aSelector)];
    }
    else
    {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ %@]: unrecognized selector sent to instance %p",
                    [self description], NSStringFromSelector(aSelector), (void *)self];
    }
}


#pragma mark Helper methods

- (NSString *)_stubDescriptions:(BOOL)onlyExpectations
{
    NSMutableString *outputString = [NSMutableString string];
    NSArray *stubsCopy = nil;
    @synchronized(stubs)
    {
        stubsCopy = [stubs copy];
    }
    for(OCMStubRecorder *stub in stubsCopy)
    {
        BOOL expectationsContainStub = NO;
        @synchronized(expectations)
        {
            expectationsContainStub = [expectations containsObject:stub];
        }

        NSString *prefix = @"";

        if(onlyExpectations)
        {
            if(expectationsContainStub == NO)
                continue;
        }
        else
        {
            if(expectationsContainStub)
                prefix = @"expected:\t";
            else
                prefix = @"stubbed:\t";
        }
        [outputString appendFormat:@"\n\t%@%@", prefix, [stub description]];
    }
    [stubsCopy release];
    return outputString;
}


@end
