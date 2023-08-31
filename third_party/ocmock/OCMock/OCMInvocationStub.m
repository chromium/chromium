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

#import "NSInvocation+OCMAdditions.h"
#import "OCMInvocationStub.h"
#import "OCMArg.h"
#import "OCMArgAction.h"

#define UNSET_RETURN_VALUE_MARKER ((id)0x01234567)


@implementation OCMInvocationStub

- (id)init
{
    self = [super init];
    invocationActions = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    [invocationActions release];
    [super dealloc];
}


- (void)addInvocationAction:(id)anAction
{
    [invocationActions addObject:anAction];
}

- (NSArray *)invocationActions
{
    return invocationActions;
}


- (void)handleInvocation:(NSInvocation *)anInvocation
{
    [self invokeArgActionsForInvocation:anInvocation];
    id target = [anInvocation target];

    BOOL isInInitFamily = [anInvocation methodIsInInitFamily];
    BOOL isInCreateFamily = isInInitFamily ? NO : [anInvocation methodIsInCreateFamily];
    if(isInInitFamily || isInCreateFamily)
    {
        id returnVal = UNSET_RETURN_VALUE_MARKER;
        [anInvocation setReturnValue:&returnVal];

        [self invokeActionsForInvocation:anInvocation];

        [anInvocation getReturnValue:&returnVal];
        if(returnVal == UNSET_RETURN_VALUE_MARKER)
            [NSException raise:NSInvalidArgumentException format:@"%@ was stubbed but no return value set. A return value is required for all alloc/copy/new/mutablecopy/init methods. If you intended to return nil, make this explicit with .andReturn(nil)", NSStringFromSelector([anInvocation selector])];

        if(isInCreateFamily)
        {
            // methods that "create" an object return it with an extra retain count
            [returnVal retain];
        }
        if(isInInitFamily)
        {
            // init family methods "consume" self and retain their return value. Do the retain
            // first in case the return value and self are the same.  The analyzer doesn't
            // understand this; see #456 for details. In this case we also need to do something
            // harmless with target or else the analyzer will complain about it not being used.
            [returnVal retain];
#ifndef __clang_analyzer__
            [target release];
#else
            [target class];
#endif
        }
    }
    else
    {
        [self invokeActionsForInvocation:anInvocation];
    }
}

- (void)invokeArgActionsForInvocation:(NSInvocation *)anInvocation
{
    NSMethodSignature *signature = [recordedInvocation methodSignature];
    NSUInteger n = [signature numberOfArguments];
    for(NSUInteger i = 2; i < n; i++)
    {
        id recordedArg = [recordedInvocation getArgumentAtIndexAsObject:i];
        id passedArg = [anInvocation getArgumentAtIndexAsObject:i];

        if([recordedArg isProxy])
            continue;

        if([recordedArg isKindOfClass:[NSValue class]])
            recordedArg = [OCMArg resolveSpecialValues:recordedArg];

        if([recordedArg isKindOfClass:[OCMArgAction class]])
            [recordedArg handleArgument:passedArg];
    }
}

- (void)invokeActionsForInvocation:(NSInvocation *)anInvocation
{
    [invocationActions makeObjectsPerformSelector:@selector(handleInvocation:) withObject:anInvocation];
}


@end
