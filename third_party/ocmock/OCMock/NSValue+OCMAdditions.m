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

#import "NSValue+OCMAdditions.h"
#import "OCMFunctionsPrivate.h"


@implementation NSValue(OCMAdditions)

static NSNumber *OCMNumberForValue(NSValue *value)
{
#define CREATE_NUM(_type) ({ _type _v; [value getValue:&_v]; @(_v); })
    switch([value objCType][0])
    {
        case 'c': return CREATE_NUM(char);
        case 'C': return CREATE_NUM(unsigned char);
        case 'B': return CREATE_NUM(bool);
        case 's': return CREATE_NUM(short);
        case 'S': return CREATE_NUM(unsigned short);
        case 'i': return CREATE_NUM(int);
        case 'I': return CREATE_NUM(unsigned int);
        case 'l': return CREATE_NUM(long);
        case 'L': return CREATE_NUM(unsigned long);
        case 'q': return CREATE_NUM(long long);
        case 'Q': return CREATE_NUM(unsigned long long);
        case 'f': return CREATE_NUM(float);
        case 'd': return CREATE_NUM(double);
        default:  return nil;
    }
}


- (BOOL)getBytes:(void *)outputBuf objCType:(const char *)targetType
{
    /*
     * See if they are similar number types, and if we can convert losslessly between them.
     * For the most part, we set things up to use CFNumberGetValue, which returns false if
     * conversion will be lossy.
     */
    CFNumberType inputType = OCMNumberTypeForObjCType([self objCType]);
    CFNumberType outputType = OCMNumberTypeForObjCType(targetType);

    if(inputType == 0 || outputType == 0) // one or both are non-number types
        return NO;

    NSNumber *inputNumber = [self isKindOfClass:[NSNumber class]] ? (NSNumber *)self : OCMNumberForValue(self);

    /*
     * Due to some legacy, back-compatible requirements in CFNumber.c, CFNumberGetValue can return true for
     * some conversions which should not be allowed (by reading source, conversions from integer types to
     * 8-bit or 16-bit integer types).  So, check ourselves.
     */
    long long min;
    long long max;
    long long val = [inputNumber longLongValue];
    switch(targetType[0])
    {
        case 'B':
        case 'c': min =  CHAR_MIN; max =  CHAR_MAX; break;
        case 'C': min =         0; max = UCHAR_MAX; break;
        case 's': min =  SHRT_MIN; max =  SHRT_MAX; break;
        case 'S': min =         0; max = USHRT_MAX; break;
        default:  min = LLONG_MIN; max = LLONG_MAX; break;
    }
    if(val < min || val > max)
        return NO;

    /* Get the number, and return NO if the value was out of range or conversion was lossy */
    return CFNumberGetValue((CFNumberRef)inputNumber, outputType, outputBuf);
}


@end
