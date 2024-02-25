/*
 *  Copyright (c) 2006-2021 Erik Doernenburg and contributors
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
#import "NSMethodSignature+OCMAdditions.h"
#import "OCMArg.h"
#import "OCMFunctionsPrivate.h"

#if(TARGET_OS_OSX && (!defined(__MAC_10_10) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_10)) ||                                  \
    (TARGET_OS_IPHONE && (!defined(__IPHONE_8_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_8_0))
static BOOL OCMObjectIsClass(id object)
{
    return class_isMetaClass(object_getClass(object));
}
#define object_isClass OCMObjectIsClass
#endif

static NSString *const OCMArgAnyPointerDescription = @"<[OCMArg anyPointer]>";


@implementation NSInvocation(OCMAdditions)

+ (NSInvocation *)invocationForBlock:(id)block withArguments:(NSArray *)arguments
{
    NSMethodSignature *sig = [NSMethodSignature signatureForBlock:block];
    NSInvocation *inv = [self invocationWithMethodSignature:sig];

    NSUInteger numArgsRequired = sig.numberOfArguments - 1;
    if((arguments != nil) && ([arguments count] != numArgsRequired))
        [NSException raise:NSInvalidArgumentException format:@"Specified too few arguments for block; expected %lu arguments.", (unsigned long)numArgsRequired];

    for(NSUInteger i = 0, j = 1; i < numArgsRequired; ++i, ++j)
    {
        id arg = [arguments objectAtIndex:i];
        [inv setArgumentWithObject:arg atIndex:j];
    }

    return inv;
}


static NSString *const OCMRetainedObjectArgumentsKey = @"OCMRetainedObjectArgumentsKey";

- (void)retainObjectArgumentsExcludingObject:(id)objectToExclude
{
    if(objc_getAssociatedObject(self, OCMRetainedObjectArgumentsKey) != nil)
    {
        // looks like we've retained the arguments already; do nothing else
        return;
    }

    NSMutableArray *retainedArguments = [[NSMutableArray alloc] init];

    id target = [self target];
    if((target != nil) && (target != objectToExclude) && !object_isClass(target))
    {
        // Bad things will happen if the target is a block since it's not being
        // copied. There isn't a very good way to tell if an invocation's target
        // is a block though (the argument type at index 0 is always "@" even if
        // the target is a Class or block), and in practice it's OK since you
        // can't mock a block.
        [retainedArguments addObject:target];
    }

    NSUInteger numberOfArguments = [[self methodSignature] numberOfArguments];
    for(NSUInteger index = 2; index < numberOfArguments; index++)
    {
        const char *argumentType = [[self methodSignature] getArgumentTypeAtIndex:index];
        if(OCMIsObjectType(argumentType))
        {
            id argument;
            [self getArgument:&argument atIndex:index];
            if((argument != nil) && (argument != objectToExclude))
            {
                if(OCMIsBlockType(argumentType) && OCMIsBlock(argument))
                {
                    // The argument's type is block and the passed argument is a block. In this
                    // case we can't retain the argument because it might be stack block, which
                    // must be copied. Further, non-escaping blocks have a lifetime that is stack-
                    // based and they treat copy/release as a no-op. Keeping a reference to these
                    // would result in a dangling pointer, which is why they are ignored here.
                    // Note: even when the argument's type is block the argument could be
                    // something else, e.g. an instance of OCMConstraint. Such cases are handled
                    // like regular objects in the last else branch below.
                    if(OCMIsNonEscapingBlock(argument) == NO)
                    {
                        id blockArgument = [argument copy];
                        [retainedArguments addObject:blockArgument];
                        [blockArgument release];
                    }
                }
                else if(OCMIsClassType(argumentType) && object_isClass(argument))
                {
                    // The argument's type is class and the passed argument is a class. In this
                    // case do not retain the argument. Note: Even though the type is class the
                    // argument could be a non-class, e.g. an instance of OCMArg.
                }
                else
                {
                    [retainedArguments addObject:argument];
                }
            }
        }
    }

    objc_setAssociatedObject(self, OCMRetainedObjectArgumentsKey, retainedArguments, OBJC_ASSOCIATION_RETAIN);
    [retainedArguments release];
}


- (void)setArgumentWithObject:(id)arg atIndex:(NSInteger)idx
{
    const char *typeEncoding = [[self methodSignature] getArgumentTypeAtIndex:idx];
    if((arg == nil) || ([arg respondsToSelector:@selector(isKindOfClass:)] && [arg isKindOfClass:[NSNull class]]))
    {
        if(typeEncoding[0] == '^')
        {
            void *nullPtr = NULL;
            [self setArgument:&nullPtr atIndex:idx];
        }
        else if(typeEncoding[0] == '@')
        {
            id nilObj = nil;
            [self setArgument:&nilObj atIndex:idx];
        }
        else if(OCMNumberTypeForObjCType(typeEncoding))
        {
            NSUInteger argSize;
            NSGetSizeAndAlignment(typeEncoding, &argSize, NULL);
            void *argBuffer = calloc(1, argSize);
            [self setArgument:argBuffer atIndex:idx];
            free(argBuffer);
        }
        else
        {
            [NSException raise:NSInvalidArgumentException format:@"Unable to create default value for type '%s'.", typeEncoding];
        }
    }
    else if(OCMIsObjectType(typeEncoding))
    {
        [self setArgument:&arg atIndex:idx];
    }
    else
    {
        if(![arg isKindOfClass:[NSValue class]])
            [NSException raise:NSInvalidArgumentException format:@"Argument '%@' should be boxed in NSValue.", arg];

        char const *valEncoding = [arg objCType];

        /// @note Here we allow any data pointer to be passed as a void pointer and
        /// any numerical types to be passed as arguments to the block.
        BOOL takesVoidPtr = !strcmp(typeEncoding, "^v") && valEncoding[0] == '^';
        BOOL takesNumber = OCMNumberTypeForObjCType(typeEncoding) && OCMNumberTypeForObjCType(valEncoding);

        if(!takesVoidPtr && !takesNumber && !OCMEqualTypesAllowingOpaqueStructs(typeEncoding, valEncoding))
            [NSException raise:NSInvalidArgumentException
                        format:@"Argument type mismatch; type of argument required is '%s' but type of value provided is '%s'",
                        typeEncoding, valEncoding];

        NSUInteger argSize;
        NSGetSizeAndAlignment(typeEncoding, &argSize, NULL);
        void *argBuffer = malloc(argSize);
        [arg getValue:argBuffer];
        [self setArgument:argBuffer atIndex:idx];
        free(argBuffer);
    }
}


- (id)getArgumentAtIndexAsObject:(NSInteger)argIndex
{
    const char *argType = OCMTypeWithoutQualifiers([[self methodSignature] getArgumentTypeAtIndex:(NSUInteger)argIndex]);

    if((strlen(argType) > 1) && (strchr("{^", argType[0]) == NULL) && (strcmp("@?", argType) != 0))
        [NSException raise:NSInvalidArgumentException format:@"Cannot handle argument type '%s'.", argType];

    if(OCMIsObjectType(argType))
    {
        id value;
        [self getArgument:&value atIndex:argIndex];
        return value;
    }

    switch(argType[0])
    {
        case ':':
        {
            SEL s = (SEL)0;
            [self getArgument:&s atIndex:argIndex];
            return [NSValue valueWithBytes:&s objCType:":"];
        }
        case 'i':
        {
            int value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 's':
        {
            short value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'l':
        {
            long value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'q':
        {
            long long value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'c':
        {
            char value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'C':
        {
            unsigned char value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'I':
        {
            unsigned int value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'S':
        {
            unsigned short value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'L':
        {
            unsigned long value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'Q':
        {
            unsigned long long value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'f':
        {
            float value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'd':
        {
            double value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case 'D':
        {
            long double value;
            [self getArgument:&value atIndex:argIndex];
            return [NSValue valueWithBytes:&value objCType:@encode(__typeof__(value))];
        }
        case 'B':
        {
            bool value;
            [self getArgument:&value atIndex:argIndex];
            return @(value);
        }
        case '^':
        case '*':
        {
            void *value = NULL;
            [self getArgument:&value atIndex:argIndex];
            return [NSValue valueWithPointer:value];
        }
        case '{': // structure
        {
            NSUInteger argSize;
            NSGetSizeAndAlignment([[self methodSignature] getArgumentTypeAtIndex:(NSUInteger)argIndex], &argSize, NULL);
            if(argSize == 0) // TODO: Can this happen? Is frameLength a good choice in that case?
                argSize = [[self methodSignature] frameLength];
            NSMutableData *argumentData = [[[NSMutableData alloc] initWithLength:argSize] autorelease];
            [self getArgument:[argumentData mutableBytes] atIndex:argIndex];
            return [NSValue valueWithBytes:[argumentData bytes] objCType:argType];
        }
    }
    [NSException raise:NSInvalidArgumentException format:@"Argument type '%s' not supported", argType];
    return nil;
}


- (NSString *)invocationDescription
{
    NSMethodSignature *methodSignature = [self methodSignature];
    NSUInteger numberOfArgs = [methodSignature numberOfArguments];

    if(numberOfArgs == 2)
        return NSStringFromSelector([self selector]);

    NSArray *selectorParts = [NSStringFromSelector([self selector]) componentsSeparatedByString:@":"];
    NSMutableString *description = [[NSMutableString alloc] init];
    NSUInteger i;
    for(i = 2; i < numberOfArgs; i++)
    {
        [description appendFormat:@"%@%@:", (i > 2 ? @" " : @""), [selectorParts objectAtIndex:(i - 2)]];
        [description appendString:[self argumentDescriptionAtIndex:(NSInteger)i]];
    }

    return [description autorelease];
}

- (NSString *)argumentDescriptionAtIndex:(NSInteger)argIndex
{
    const char *argType = OCMTypeWithoutQualifiers([[self methodSignature] getArgumentTypeAtIndex:(NSUInteger)argIndex]);

    switch(*argType)
    {
        case '@': return [self objectDescriptionAtIndex:argIndex];
        case 'B': return [self boolDescriptionAtIndex:argIndex];
        case 'c': return [self charDescriptionAtIndex:argIndex];
        case 'C': return [self unsignedCharDescriptionAtIndex:argIndex];
        case 'i': return [self intDescriptionAtIndex:argIndex];
        case 'I': return [self unsignedIntDescriptionAtIndex:argIndex];
        case 's': return [self shortDescriptionAtIndex:argIndex];
        case 'S': return [self unsignedShortDescriptionAtIndex:argIndex];
        case 'l': return [self longDescriptionAtIndex:argIndex];
        case 'L': return [self unsignedLongDescriptionAtIndex:argIndex];
        case 'q': return [self longLongDescriptionAtIndex:argIndex];
        case 'Q': return [self unsignedLongLongDescriptionAtIndex:argIndex];
        case 'd': return [self doubleDescriptionAtIndex:argIndex];
        case 'f': return [self floatDescriptionAtIndex:argIndex];
        case 'D': return [self longDoubleDescriptionAtIndex:argIndex];
        case '{': return [self structDescriptionAtIndex:argIndex];
        case '^': return [self pointerDescriptionAtIndex:argIndex];
        case '*': return [self cStringDescriptionAtIndex:argIndex];
        case ':': return [self selectorDescriptionAtIndex:argIndex];
        default: return [@"<??" stringByAppendingString:@">"]; // avoid confusion with trigraphs...
    }
}

- (NSString *)objectDescriptionAtIndex:(NSInteger)anInt
{
    id object;

    [self getArgument:&object atIndex:anInt];
    if(object == nil)
        return @"nil";
    else if(![object isProxy] && [object isKindOfClass:[NSString class]])
        return [NSString stringWithFormat:@"@\"%@\"", [object description]];
    else
        // The description cannot be nil, if it is then replace it
        return [object description] ?: @"<nil description>";
}

- (NSString *)boolDescriptionAtIndex:(NSInteger)anInt
{
    bool value;
    [self getArgument:&value atIndex:anInt];
    return value ? @"YES" : @"NO";
}

- (NSString *)charDescriptionAtIndex:(NSInteger)anInt
{
    unsigned char buffer[128];
    memset(buffer, 0x0, 128);

    [self getArgument:&buffer atIndex:anInt];

    // If there's only one character in the buffer, and it's 0 or 1, then we have a BOOL
    if(buffer[1] == '\0' && (buffer[0] == 0 || buffer[0] == 1))
        return (buffer[0] == 1 ? @"YES" : @"NO");
    else
        return [NSString stringWithFormat:@"'%c'", *buffer];
}

- (NSString *)unsignedCharDescriptionAtIndex:(NSInteger)anInt
{
    unsigned char buffer[128];
    memset(buffer, 0x0, 128);

    [self getArgument:&buffer atIndex:anInt];
    return [NSString stringWithFormat:@"'%c'", *buffer];
}

- (NSString *)intDescriptionAtIndex:(NSInteger)anInt
{
    int intValue;

    [self getArgument:&intValue atIndex:anInt];
    return [NSString stringWithFormat:@"%d", intValue];
}

- (NSString *)unsignedIntDescriptionAtIndex:(NSInteger)anInt
{
    unsigned int intValue;

    [self getArgument:&intValue atIndex:anInt];
    return [NSString stringWithFormat:@"%d", intValue];
}

- (NSString *)shortDescriptionAtIndex:(NSInteger)anInt
{
    short shortValue;

    [self getArgument:&shortValue atIndex:anInt];
    return [NSString stringWithFormat:@"%hi", shortValue];
}

- (NSString *)unsignedShortDescriptionAtIndex:(NSInteger)anInt
{
    unsigned short shortValue;

    [self getArgument:&shortValue atIndex:anInt];
    return [NSString stringWithFormat:@"%hu", shortValue];
}

- (NSString *)longDescriptionAtIndex:(NSInteger)anInt
{
    long longValue;

    [self getArgument:&longValue atIndex:anInt];
    return [NSString stringWithFormat:@"%ld", longValue];
}

- (NSString *)unsignedLongDescriptionAtIndex:(NSInteger)anInt
{
    unsigned long longValue;

    [self getArgument:&longValue atIndex:anInt];
    return [NSString stringWithFormat:@"%lu", longValue];
}

- (NSString *)longLongDescriptionAtIndex:(NSInteger)anInt
{
    long long longLongValue;

    [self getArgument:&longLongValue atIndex:anInt];
    return [NSString stringWithFormat:@"%qi", longLongValue];
}

- (NSString *)unsignedLongLongDescriptionAtIndex:(NSInteger)anInt
{
    unsigned long long longLongValue;

    [self getArgument:&longLongValue atIndex:anInt];
    return [NSString stringWithFormat:@"%qu", longLongValue];
}

- (NSString *)doubleDescriptionAtIndex:(NSInteger)anInt
{
    double doubleValue;

    [self getArgument:&doubleValue atIndex:anInt];
    return [NSString stringWithFormat:@"%f", doubleValue];
}

- (NSString *)floatDescriptionAtIndex:(NSInteger)anInt
{
    float floatValue;

    [self getArgument:&floatValue atIndex:anInt];
    return [NSString stringWithFormat:@"%f", floatValue];
}

- (NSString *)longDoubleDescriptionAtIndex:(NSInteger)anInt
{
    long double longDoubleValue;

    [self getArgument:&longDoubleValue atIndex:anInt];
    return [NSString stringWithFormat:@"%Lf", longDoubleValue];
}

- (NSString *)structDescriptionAtIndex:(NSInteger)anInt
{
    return [NSString stringWithFormat:@"(%@)", [[self getArgumentAtIndexAsObject:anInt] description]];
}

- (NSString *)pointerDescriptionAtIndex:(NSInteger)anInt
{
    void *buffer;

    [self getArgument:&buffer atIndex:anInt];

    if(buffer == [OCMArg anyPointer])
        return OCMArgAnyPointerDescription;
    else
        return [NSString stringWithFormat:@"%p", buffer];
}

- (NSString *)cStringDescriptionAtIndex:(NSInteger)anInt
{
    char *cStringPtr;

    [self getArgument:&cStringPtr atIndex:anInt];

    if(cStringPtr == [OCMArg anyPointer])
    {
        return OCMArgAnyPointerDescription;
    }
    else
    {
        char buffer[104];
        strlcpy(buffer, cStringPtr, sizeof(buffer));
        strlcpy(buffer + 100, "...", (sizeof(buffer) - 100));
        return [NSString stringWithFormat:@"\"%s\"", buffer];
    }
}

- (NSString *)selectorDescriptionAtIndex:(NSInteger)anInt
{
    SEL selectorValue;

    [self getArgument:&selectorValue atIndex:anInt];
    return [NSString stringWithFormat:@"@selector(%@)", NSStringFromSelector(selectorValue)];
}


- (BOOL)isMethodFamily:(NSString *)family
{
    // Definitions here: https://clang.llvm.org/docs/AutomaticReferenceCounting.html#method-families

    NSMethodSignature *signature = [self methodSignature];
    if(OCMIsObjectType(signature.methodReturnType) == NO)
    {
        return NO;
    }

    NSString *selString = NSStringFromSelector([self selector]);
    NSRange underscoreRange = [selString rangeOfString:@"^_*" options:NSRegularExpressionSearch];
    selString = [selString substringFromIndex:NSMaxRange(underscoreRange)];

    if([selString hasPrefix:family] == NO)
    {
        return NO;
    }
    NSUInteger familyLength = [family length];
    if(([selString length] > familyLength) &&
        ([[NSCharacterSet lowercaseLetterCharacterSet] characterIsMember:[selString characterAtIndex:familyLength]]))
    {
        return NO;
    }
    return YES;
}


- (BOOL)methodIsInInitFamily
{
    return [self isMethodFamily:@"init"];
}

- (BOOL)methodIsInCreateFamily
{
    return [self isMethodFamily:@"alloc"] 
            || [self isMethodFamily:@"copy"] 
            || [self isMethodFamily:@"mutableCopy"] 
            || [self isMethodFamily:@"new"];
}

@end
