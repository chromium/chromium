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
#import "NSMethodSignature+OCMAdditions.h"
#import "OCMFunctionsPrivate.h"


@implementation NSMethodSignature(OCMAdditions)

#pragma mark Signatures for dynamic properties

+ (NSMethodSignature *)signatureForDynamicPropertyAccessedWithSelector:(SEL)selector inClass:(Class)aClass
{
    BOOL isGetter = YES;
    objc_property_t property = [self propertyMatchingSelector:selector inClass:aClass isGetter:&isGetter];
    if(property == NULL)
        return nil;

    const char *propertyAttributesString = property_getAttributes(property);
    NSArray *propertyAttributes = [[NSString stringWithCString:propertyAttributesString
                                                      encoding:NSASCIIStringEncoding] componentsSeparatedByString:@","];
    NSString *typeStr = nil;
    BOOL isDynamic = NO;
    for(NSString *attribute in propertyAttributes)
    {
        if([attribute isEqualToString:@"D"])
            isDynamic = YES;
        else if([attribute hasPrefix:@"T"])
            typeStr = [attribute substringFromIndex:1];
    }

    if(!isDynamic)
        return nil;

    NSRange r = [typeStr rangeOfString:@"\""]; // incomplete workaround to deal with structs
    if(r.location != NSNotFound)
        typeStr = [typeStr substringToIndex:r.location];

    NSString *sigStringFormat = isGetter ? @"%@@:" : @"v@:%@";
    const char *sigCString = [[NSString stringWithFormat:sigStringFormat, typeStr] cStringUsingEncoding:NSASCIIStringEncoding];
    return [NSMethodSignature signatureWithObjCTypes:sigCString];
}


+ (objc_property_t)propertyMatchingSelector:(SEL)selector inClass:(Class)aClass isGetter:(BOOL *)isGetterPtr
{
    NSString *propertyName = NSStringFromSelector(selector);

    // first try selector as is aassuming it's a getter
    objc_property_t property = class_getProperty(aClass, [propertyName cStringUsingEncoding:NSASCIIStringEncoding]);
    if(property != NULL)
    {
        *isGetterPtr = YES;
        return property;
    }

    // try setter next if selector starts with "set"
    if([propertyName hasPrefix:@"set"])
    {
        propertyName = [propertyName substringFromIndex:@"set".length];
        propertyName = [propertyName stringByReplacingCharactersInRange:NSMakeRange(0, 1) withString:[[propertyName substringToIndex:1] lowercaseString]];
        if([propertyName hasSuffix:@":"])
            propertyName = [propertyName substringToIndex:[propertyName length] - 1];

        property = class_getProperty(aClass, [propertyName cStringUsingEncoding:NSASCIIStringEncoding]);
        if(property != NULL)
        {
            *isGetterPtr = NO;
            return property;
        }
    }

    // search through properties with custom getter/setter that corresponds to selector
    unsigned int propertiesCount = 0;
    objc_property_t *allProperties = class_copyPropertyList(aClass, &propertiesCount);
    for(unsigned int i = 0; i < propertiesCount; i++)
    {
        NSArray *propertyAttributes = [[NSString stringWithCString:property_getAttributes(allProperties[i])
                                                          encoding:NSASCIIStringEncoding] componentsSeparatedByString:@","];
        for(NSString *attribute in propertyAttributes)
        {
            if(([attribute hasPrefix:@"G"] || [attribute hasPrefix:@"S"]) &&
                [[attribute substringFromIndex:1] isEqualToString:propertyName])
            {
                *isGetterPtr = ![attribute hasPrefix:@"S"];
                property = allProperties[i];
                i = propertiesCount;
                break;
            }
        }
    }
    free(allProperties);

    return property;
}


#pragma mark Signatures for blocks

+ (NSMethodSignature *)signatureForBlock:(id)block
{
    /* For a more complete implementation of parsing the block data structure see:
     *
     * https://github.com/ebf/CTObjectiveCRuntimeAdditions/tree/master/CTObjectiveCRuntimeAdditions/CTObjectiveCRuntimeAdditions
     */

    struct OCMBlockDef *blockRef = (__bridge struct OCMBlockDef *)block;

    if(!(blockRef->flags & OCMBlockDescriptionFlagsHasSignature))
        return nil;

    void *signatureLocation = blockRef->descriptor;
    signatureLocation += sizeof(unsigned long int);
    signatureLocation += sizeof(unsigned long int);
    if(blockRef->flags & OCMBlockDescriptionFlagsHasCopyDispose)
    {
        signatureLocation += sizeof(void (*)(void *dst, void *src));
        signatureLocation += sizeof(void (*)(void *src));
    }

    const char *signature = (*(const char **)signatureLocation);
    return [NSMethodSignature signatureWithObjCTypes:signature];
}


#pragma mark Extended attributes

- (BOOL)usesSpecialStructureReturn
{
    const char *types = OCMTypeWithoutQualifiers([self methodReturnType]);

    if((types == NULL) || (types[0] != '{'))
        return NO;

    /* In some cases structures are returned by ref. The rules are complex and depend on the
       architecture, see:

       http://sealiesoftware.com/blog/archive/2008/10/30/objc_explain_objc_msgSend_stret.html
       http://developer.apple.com/library/mac/#documentation/DeveloperTools/Conceptual/LowLevelABI/000-Introduction/introduction.html
       https://github.com/atgreen/libffi/blob/master/src/x86/ffi64.c
       http://www.uclibc.org/docs/psABI-x86_64.pdf
       http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042e/IHI0042E_aapcs.pdf

       NSMethodSignature knows the details but has no API to return it, though it is in
       the debugDescription. Horribly kludgy.
    */
    NSRange range = [[self debugDescription] rangeOfString:@"is special struct return? YES"];
    return range.length > 0;
}


- (NSString *)fullTypeString
{
    NSMutableString *typeString = [NSMutableString string];
    [typeString appendFormat:@"%s", [self methodReturnType]];
    for(NSUInteger i = 0; i < [self numberOfArguments]; i++)
        [typeString appendFormat:@"%s", [self getArgumentTypeAtIndex:i]];
    return typeString;
}


- (const char *)fullObjCTypes
{
    return [[self fullTypeString] UTF8String];
}

@end
