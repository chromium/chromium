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

#import "OCMExpectationRecorder.h"
#import "OCMInvocationExpectation.h"
#import "OCMockObject.h"


@implementation OCMExpectationRecorder

#pragma mark Initialisers, description, accessors, etc.

- (id)init
{
    self = [super init];
    [invocationMatcher release];
    invocationMatcher = [[OCMInvocationExpectation alloc] init];
    return self;
}

- (OCMInvocationExpectation *)expectation
{
    return (OCMInvocationExpectation *)invocationMatcher;
}


#pragma mark Modifying the expectation

- (id)never
{
    [[self expectation] setMatchAndReject:YES];
    return self;
}


#pragma mark Finishing recording

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    [super forwardInvocation:anInvocation];
    [mockObject addExpectation:[self expectation]];
}


@end
