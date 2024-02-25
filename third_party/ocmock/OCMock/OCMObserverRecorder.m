/*
 *  Copyright (c) 2009-2021 Erik Doernenburg and contributors
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
#import "OCMObserverRecorder.h"
#import "OCMConstraint.h"


@interface NSObject (HCMatcherDummy)
- (BOOL)matches:(id)item;
@end

#pragma mark -

@implementation OCMObserverRecorder

#pragma mark Initialisers, description, accessors, etc.

- (void)dealloc
{
    [recordedNotification release];
    [super dealloc];
}

- (BOOL)didRecordInvocation
{
    return YES; // Needed for macro use, and recorder can only end up in macro state if it was used.
}


#pragma mark Recording

- (NSNotification *)notificationWithName:(NSString *)name object:(id)sender
{
    recordedNotification = [[NSNotification notificationWithName:name object:sender] retain];
    return nil;
}

- (NSNotification *)notificationWithName:(NSString *)name object:(id)sender userInfo:(NSDictionary *)userInfo
{
    recordedNotification = [[NSNotification notificationWithName:name object:sender userInfo:userInfo] retain];
    return nil;
}


#pragma mark Verification

- (BOOL)matchesNotification:(NSNotification *)aNotification
{
    return [self argument:[recordedNotification name] matchesArgument:[aNotification name]] &&
           [self argument:[recordedNotification object] matchesArgument:[aNotification object]] &&
           [self argument:[recordedNotification userInfo] matchesArgument:[aNotification userInfo]];
}

- (BOOL)argument:(id)expectedArg matchesArgument:(id)observedArg
{
    if([expectedArg isKindOfClass:[OCMConstraint class]])
    {
        return [expectedArg evaluate:observedArg];
    }
    else if([expectedArg conformsToProtocol:objc_getProtocol("HCMatcher")])
    {
        return [expectedArg matches:observedArg];
    }
    else if(expectedArg == observedArg)
    {
        return YES;
    }
    else if(expectedArg == nil || observedArg == nil)
    {
        return NO;
    }
    else
    {
        return [expectedArg isEqual:observedArg];
    }
}


@end
