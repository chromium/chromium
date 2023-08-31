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

#import <objc/message.h>
#import <objc/runtime.h>
#import "NSMethodSignature+OCMAdditions.h"
#import "NSObject+OCMAdditions.h"

@implementation NSObject(OCMAdditions)

+ (IMP)instanceMethodForwarderForSelector:(SEL)aSelector
{
#ifndef __arm64__
    static NSMutableDictionary *_OCMReturnTypeCache;

    if(_OCMReturnTypeCache == nil)
        _OCMReturnTypeCache = [[NSMutableDictionary alloc] init];

    BOOL needsStructureReturn;
    void *rawCacheKey[2] = { (void *)self, aSelector };
    NSData *cacheKey = [NSData dataWithBytes:rawCacheKey length:sizeof(rawCacheKey)];
    NSNumber *cachedValue = [_OCMReturnTypeCache objectForKey:cacheKey];

    if(cachedValue == nil)
    {
        NSMethodSignature *sig = [self instanceMethodSignatureForSelector:aSelector];
        needsStructureReturn = [sig usesSpecialStructureReturn];
        [_OCMReturnTypeCache setObject:@(needsStructureReturn) forKey:cacheKey];
    }
    else
    {
        needsStructureReturn = [cachedValue boolValue];
    }

    if(needsStructureReturn)
      return _objc_msgForward_stret;
#endif

  return _objc_msgForward;
}

+ (void)enumerateMethodsInClass:(Class)aClass usingBlock:(void (^)(Class cls, SEL sel))aBlock
{
    for(Class cls = aClass; cls != nil; cls = class_getSuperclass(cls))
    {
        Method *methodList = class_copyMethodList(cls, NULL);
        if(methodList == NULL)
            continue;

        for(Method *mPtr = methodList; *mPtr != NULL; mPtr++)
        {
            SEL sel = method_getName(*mPtr);
            aBlock(cls, sel);
        }
        free(methodList);
    }
}

@end
