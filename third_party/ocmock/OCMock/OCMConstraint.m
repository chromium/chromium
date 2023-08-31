/*
 *  Copyright (c) 2007-2021 Erik Doernenburg and contributors
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

#import <limits.h>
#import "OCMConstraint.h"


@implementation OCMConstraint

+ (instancetype)constraint
{
    return [[[self alloc] init] autorelease];
}

- (BOOL)evaluate:(id)value
{
    return NO;
}

- (id)copyWithZone:(struct _NSZone *)zone __unused
{
    return [self retain];
}

+ (instancetype)constraintWithSelector:(SEL)aSelector onObject:(id)anObject
{
    OCMInvocationConstraint *constraint = [OCMInvocationConstraint constraint];
    NSMethodSignature *signature = [anObject methodSignatureForSelector:aSelector];
    if(signature == nil)
        [NSException raise:NSInvalidArgumentException
                    format:@"Unknown selector %@ used in constraint.", NSStringFromSelector(aSelector)];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:anObject];
    [invocation setSelector:aSelector];
    constraint->invocation = invocation;
    return constraint;
}

+ (instancetype)constraintWithSelector:(SEL)aSelector onObject:(id)anObject withValue:(id)aValue
{
    OCMInvocationConstraint *constraint = (OCMInvocationConstraint *)[self constraintWithSelector:aSelector onObject:anObject];
    if([[constraint->invocation methodSignature] numberOfArguments] < 4)
        [NSException raise:NSInvalidArgumentException format:@"Constraint with value requires selector with two arguments."];
    [constraint->invocation setArgument:&aValue atIndex:3];
    return constraint;
}


@end


#pragma mark -

@implementation OCMAnyConstraint

- (BOOL)evaluate:(id)value
{
    return YES;
}

@end


#pragma mark -

@implementation OCMIsNilConstraint

- (BOOL)evaluate:(id)value
{
    return value == nil;
}

@end


#pragma mark -

@implementation OCMIsNotNilConstraint

- (BOOL)evaluate:(id)value
{
    return value != nil;
}

@end


#pragma mark -

@implementation OCMIsNotEqualConstraint

- (BOOL)evaluate:(id)value
{
    return ![value isEqual:testValue];
}

@end


#pragma mark -

@implementation OCMInvocationConstraint

- (BOOL)evaluate:(id)value
{
    [invocation setArgument:&value atIndex:2]; // should test if constraint takes arg
    [invocation invoke];
    BOOL returnValue;
    [invocation getReturnValue:&returnValue];
    return returnValue;
}

@end

#pragma mark -

@implementation OCMBlockConstraint

- (instancetype)initWithConstraintBlock:(BOOL (^)(id))aBlock
{
    if((self = [super init]))
    {
        block = [aBlock copy];
    }

    return self;
}

- (void)dealloc
{
    [block release];
    [super dealloc];
}

- (BOOL)evaluate:(id)value
{
    return block ? block(value) : NO;
}


@end
