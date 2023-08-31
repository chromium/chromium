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

#import "OCMVerifier.h"
#import "OCMInvocationMatcher.h"
#import "OCMLocation.h"
#import "OCMQuantifier.h"
#import "OCMockObject.h"


@implementation OCMVerifier

- (id)init
{
    if(invocationMatcher != nil)
        [NSException raise:NSInternalInconsistencyException format:@"** Method init invoked twice on verifier. Are you trying to verify the init method? This is currently not supported."];
    if((self = [super init]))
    {
        invocationMatcher = [[OCMInvocationMatcher alloc] init];
    }

    return self;
}

- (id)withQuantifier:(OCMQuantifier *)quantifier
{
    [self setQuantifier:quantifier];
    return self;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    [super forwardInvocation:anInvocation];
    [mockObject verifyInvocation:invocationMatcher withQuantifier:self.quantifier atLocation:self.location];
}

- (void)dealloc
{
    [_location release];
    [_quantifier release];
    [super dealloc];
}

@end
