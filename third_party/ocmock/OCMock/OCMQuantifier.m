/*
 *  Copyright (c) 2016-2021 Erik Doernenburg and contributors
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

#import "OCMQuantifier.h"
#import "OCMMacroState.h"
#import "OCMVerifier.h"


@interface OCMExactCountQuantifier : OCMQuantifier

@end

@interface OCMAtLeastQuantifier : OCMQuantifier

@end

@interface OCMAtMostQuantifier : OCMQuantifier

@end


@implementation OCMQuantifier

+ (instancetype)exactly:(NSUInteger)count
{
    return [[[OCMExactCountQuantifier alloc] initWithCount:count] autorelease];
}

+ (instancetype)never
{
    return [self exactly:0];
}

+ (instancetype)atLeast:(NSUInteger)count
{
    return [[[OCMAtLeastQuantifier alloc] initWithCount:count] autorelease];
}

+ (instancetype)atMost:(NSUInteger)count
{
    return [[[OCMAtMostQuantifier alloc] initWithCount:count] autorelease];
}


- (instancetype)initWithCount:(NSUInteger)count
{
    if((self = [super init]) != nil)
    {
        expectedCount = count;
        [(OCMVerifier *)[[OCMMacroState globalState] recorder] setQuantifier:self];
    }
    return self;
}


- (BOOL)isValidCount:(NSUInteger)count
{
    return NO;
}

- (NSString *)description
{
    switch(expectedCount)
    {
        case 0:  return @"never";
        case 1:  return @"once";
        default: return [NSString stringWithFormat:@"%lu times", (unsigned long)expectedCount];
    }
}

@end


@implementation OCMExactCountQuantifier

- (BOOL)isValidCount:(NSUInteger)count
{
    return count == expectedCount;
}

@end


@implementation OCMAtLeastQuantifier

- (instancetype)initWithCount:(NSUInteger)count
{
    if(count == 0)
        @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"Count for an at-least quantifier cannot be zero." userInfo:nil];
    return [super initWithCount:count];
}

- (BOOL)isValidCount:(NSUInteger)count
{
    return count >= expectedCount;
}

- (NSString *)description
{
    return [@"at least " stringByAppendingString:[super description]];
}

@end


@implementation OCMAtMostQuantifier

- (instancetype)initWithCount:(NSUInteger)count
{
    if(count == 0)
        @throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"Count for an at-most quantifier cannot be zero. Use never or exactly-zero quantifier instead." userInfo:nil];
    return [super initWithCount:count];
}

- (BOOL)isValidCount:(NSUInteger)count
{
    return count <= expectedCount;
}

- (NSString *)description
{
    return [@"at most " stringByAppendingString:[super description]];
}

@end
