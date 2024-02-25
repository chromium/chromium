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

#import "OCMLocation.h"


@implementation OCMLocation

+ (instancetype)locationWithTestCase:(id)aTestCase file:(NSString *)aFile line:(NSUInteger)aLine
{
    return [[[OCMLocation alloc] initWithTestCase:aTestCase file:aFile line:aLine] autorelease];
}

- (instancetype)initWithTestCase:(id)aTestCase file:(NSString *)aFile line:(NSUInteger)aLine
{
    if((self = [super init]))
    {
        testCase = aTestCase;
        file = [aFile retain];
        line = aLine;
    }

    return self;
}

- (void)dealloc
{
    [file release];
    [super dealloc];
}

- (id)testCase
{
    return testCase;
}

- (NSString *)file
{
    return file;
}

- (NSUInteger)line
{
    return line;
}

@end


OCMLocation *OCMMakeLocation(id testCase, const char *fileCString, int line)
{
    return [OCMLocation locationWithTestCase:testCase file:[NSString stringWithUTF8String:fileCString] line:line];
}
