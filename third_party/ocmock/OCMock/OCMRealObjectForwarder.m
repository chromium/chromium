/*
 *  Copyright (c) 2010-2021 Erik Doernenburg and contributors
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
#import "NSInvocation+OCMAdditions.h"
#import "OCMRealObjectForwarder.h"
#import "OCMFunctionsPrivate.h"
#import "OCPartialMockObject.h"


@implementation OCMRealObjectForwarder

- (void)handleInvocation:(NSInvocation *)anInvocation
{
    id invocationTarget = [anInvocation target];

    BOOL isInInitFamily = [anInvocation methodIsInInitFamily];
    BOOL isInCreateFamily = isInInitFamily ? NO : [anInvocation methodIsInCreateFamily];

    [anInvocation setSelector:OCMAliasForOriginalSelector([anInvocation selector])];
    if([invocationTarget isProxy])
    {
        if(!class_getInstanceMethod([invocationTarget mockObjectClass], @selector(realObject)))
            [NSException raise:NSInternalInconsistencyException format:@"Method andForwardToRealObject can only be used with partial mocks and class methods."];

        NSObject *realObject = [(OCPartialMockObject *)invocationTarget realObject];
        [anInvocation setTarget:realObject];
        if(isInInitFamily)
        {
            // The init method of the real object will "consume" self, but because the method was
            // invoked on the mock and not the real object a corresponding retain is missing; so
            // we do this here. The analyzer doesn't understand this; see #456 for details.
#ifndef __clang_analyzer__
            [realObject retain];
#endif
        }
    }

    [anInvocation invoke];

    if(isInInitFamily || isInCreateFamily)
    {
        // After invoking the method on the real object the return value's retain count is correct,
        // but because we have a chain of handlers for an invocation and we handle the retain count
        // adjustments at the end in the stub, we undo the additional retains here.
        id returnVal;
        [anInvocation getReturnValue:&returnVal];
        [returnVal autorelease];
    }
}


@end
