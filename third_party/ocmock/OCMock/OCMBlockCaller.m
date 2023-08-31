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

#import "OCMBlockCaller.h"


@implementation OCMBlockCaller

- (id)initWithCallBlock:(void (^)(NSInvocation *))theBlock
{
    if((self = [super init]))
    {
        block = [theBlock copy];
    }

    return self;
}

- (void)dealloc
{
    [block release];
    [super dealloc];
}

- (void)handleInvocation:(NSInvocation *)anInvocation
{
    if(block != nil)
    {
        block(anInvocation);
    }
}

@end
