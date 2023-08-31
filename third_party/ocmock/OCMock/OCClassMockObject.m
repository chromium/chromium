/*
 *  Copyright (c) 2005-2021 Erik Doernenburg and contributors
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
#import "NSObject+OCMAdditions.h"
#import "OCClassMockObject.h"
#import "OCMFunctionsPrivate.h"
#import "OCMInvocationStub.h"

@interface NSObject (OCMClassMockingSupport)
+ (BOOL)supportsMocking:(NSString **)reason;
@end


@implementation OCClassMockObject

#pragma mark Initialisers, description, accessors, etc.

- (id)initWithClass:(Class)aClass
{
    [self assertClassIsSupported:aClass];
    [super init];
    mockedClass = aClass;
    [self prepareClassForClassMethodMocking];
    return self;
}

- (void)dealloc
{
    [self stopMocking];
    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"OCClassMockObject(%@)", NSStringFromClass(mockedClass)];
}

- (Class)mockedClass
{
    return mockedClass;
}

- (void)assertClassIsSupported:(Class)aClass
{
    if(aClass == Nil)
        [NSException raise:NSInvalidArgumentException format:@"Class cannot be Nil."];

    if([aClass respondsToSelector:@selector(supportsMocking:)])
    {
        NSString *reason = nil;
        if(![aClass supportsMocking:&reason])
            [NSException raise:NSInvalidArgumentException format:@"Class %@ does not support mocking: %@", aClass, reason];
    }
}

#pragma mark Extending/overriding superclass behaviour

- (void)stopMocking
{
    if(originalMetaClass != nil)
    {
        [self stopMockingClassMethods];
    }
    if(classCreatedForNewMetaClass != nil)
    {
        OCMDisposeSubclass(classCreatedForNewMetaClass);
        classCreatedForNewMetaClass = nil;
    }
    [super stopMocking];
}


- (void)stopMockingClassMethods
{
    OCMSetAssociatedMockForClass(nil, mockedClass);
    object_setClass(mockedClass, originalMetaClass);
    originalMetaClass = nil;
    /* created meta class will be disposed later because partial mocks create another subclass depending on it */
}


- (void)addStub:(OCMInvocationStub *)aStub
{
    [super addStub:aStub];
    if([aStub recordedAsClassMethod])
        [self setupForwarderForClassMethodSelector:[[aStub recordedInvocation] selector]];
}


#pragma mark Class method mocking

- (void)prepareClassForClassMethodMocking
{
    /* the runtime and OCMock depend on string and array; we don't intercept methods on them to avoid endless loops */
    if([[mockedClass class] isSubclassOfClass:[NSString class]] || [[mockedClass class] isSubclassOfClass:[NSArray class]])
        return;

    /* trying to replace class methods on NSManagedObject and subclasses of it doesn't work; see #339 */
    if([mockedClass isSubclassOfClass:objc_getClass("NSManagedObject")])
        return;

    /* if there is another mock for this exact class, stop it */
    id otherMock = OCMGetAssociatedMockForClass(mockedClass, NO);
    if(otherMock != nil)
        [otherMock stopMockingClassMethods];

    OCMSetAssociatedMockForClass(self, mockedClass);

    /* dynamically create a subclass and use its meta class as the meta class for the mocked class */
    classCreatedForNewMetaClass = OCMCreateSubclass(mockedClass, mockedClass);
    originalMetaClass = object_getClass(mockedClass);
    id newMetaClass = object_getClass(classCreatedForNewMetaClass);

    /* create a dummy initialize method */
    Method myDummyInitializeMethod = class_getInstanceMethod([self mockObjectClass], @selector(initializeForClassObject));
    const char *initializeTypes = method_getTypeEncoding(myDummyInitializeMethod);
    IMP myDummyInitializeIMP = method_getImplementation(myDummyInitializeMethod);
    class_addMethod(newMetaClass, @selector(initialize), myDummyInitializeIMP, initializeTypes);

    object_setClass(mockedClass, newMetaClass); // only after dummy initialize is installed (iOS9)

    /* point forwardInvocation: of the object to the implementation in the mock */
    Method myForwardMethod = class_getInstanceMethod([self mockObjectClass], @selector(forwardInvocationForClassObject:));
    IMP myForwardIMP = method_getImplementation(myForwardMethod);
    class_addMethod(newMetaClass, @selector(forwardInvocation:), myForwardIMP, method_getTypeEncoding(myForwardMethod));

    /* adding forwarder for most class methods (instance methods on meta class) to allow for verify after run */
    NSArray *methodsNotToForward = @[
        @"class", @"forwardingTargetForSelector:", @"methodSignatureForSelector:", @"forwardInvocation:", @"isBlock",
        @"instanceMethodForwarderForSelector:", @"instanceMethodSignatureForSelector:", @"resolveClassMethod:"
    ];
    void (^setupForwarderFiltered)(Class, SEL) = ^(Class cls, SEL sel) {
        if((cls == object_getClass([NSObject class])) || (cls == [NSObject class]) || (cls == object_getClass(cls)))
            return;
        if(OCMIsApplePrivateMethod(cls, sel))
            return;
        if([methodsNotToForward containsObject:NSStringFromSelector(sel)])
            return;
        @try
        {
            [self setupForwarderForClassMethodSelector:sel];
        }
        @catch(NSException *e)
        {
            // ignore for now
        }
    };
    [NSObject enumerateMethodsInClass:originalMetaClass usingBlock:setupForwarderFiltered];
}


- (void)setupForwarderForClassMethodSelector:(SEL)selector
{
    SEL aliasSelector = OCMAliasForOriginalSelector(selector);
    if(class_getClassMethod(mockedClass, aliasSelector) != NULL)
        return;

    Method originalMethod = class_getClassMethod(mockedClass, selector);
    IMP originalIMP = method_getImplementation(originalMethod);
    const char *types = method_getTypeEncoding(originalMethod);

    Class metaClass = object_getClass(mockedClass);
    IMP forwarderIMP = [originalMetaClass instanceMethodForwarderForSelector:selector];
    class_addMethod(metaClass, aliasSelector, originalIMP, types);
    class_replaceMethod(metaClass, selector, forwarderIMP, types);
}


- (void)forwardInvocationForClassObject:(NSInvocation *)anInvocation
{
    // in here "self" is a reference to the real class, not the mock
    OCClassMockObject *mock = OCMGetAssociatedMockForClass((Class)self, YES);
    if(mock == nil)
    {
        [NSException raise:NSInternalInconsistencyException format:@"No mock for class %@", NSStringFromClass((Class)self)];
    }
    if([mock handleInvocation:anInvocation] == NO)
    {
        [anInvocation setSelector:OCMAliasForOriginalSelector([anInvocation selector])];
        [anInvocation invoke];
    }
}

- (void)initializeForClassObject
{
    // we really just want to have an implementation so that the superclass's is not called
}


#pragma mark Proxy API

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSMethodSignature *signature = [mockedClass instanceMethodSignatureForSelector:aSelector];
    if(signature == nil)
    {
        signature = [NSMethodSignature signatureForDynamicPropertyAccessedWithSelector:aSelector inClass:mockedClass];
    }
    return signature;
}

- (Class)mockObjectClass
{
    return [super class];
}

- (Class)class
{
    return mockedClass;
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return [mockedClass instancesRespondToSelector:selector];
}

- (BOOL)isKindOfClass:(Class)aClass
{
    return [mockedClass isSubclassOfClass:aClass];
}

- (BOOL)conformsToProtocol:(Protocol *)aProtocol
{
    Class clazz = mockedClass;
    while(clazz != nil)
    {
        if(class_conformsToProtocol(clazz, aProtocol))
        {
            return YES;
        }
        clazz = class_getSuperclass(clazz);
    }
    return NO;
}

@end


#pragma mark -

/*
 taken from:
 `class-dump -f isNS /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator7.0.sdk/System/Library/Frameworks/CoreFoundation.framework`
 
 @ interface NSObject (__NSIsKinds)
 - (_Bool)isNSValue__;
 - (_Bool)isNSTimeZone__;
 - (_Bool)isNSString__;
 - (_Bool)isNSSet__;
 - (_Bool)isNSOrderedSet__;
 - (_Bool)isNSNumber__;
 - (_Bool)isNSDictionary__;
 - (_Bool)isNSDate__;
 - (_Bool)isNSData__;
 - (_Bool)isNSArray__;
 */

@implementation OCClassMockObject(NSIsKindsImplementation)

- (BOOL)isNSValue__
{
    return [mockedClass isSubclassOfClass:[NSValue class]];
}

- (BOOL)isNSTimeZone__
{
    return [mockedClass isSubclassOfClass:[NSTimeZone class]];
}

- (BOOL)isNSSet__
{
    return [mockedClass isSubclassOfClass:[NSSet class]];
}

- (BOOL)isNSOrderedSet__
{
    return [mockedClass isSubclassOfClass:[NSOrderedSet class]];
}

- (BOOL)isNSNumber__
{
    return [mockedClass isSubclassOfClass:[NSNumber class]];
}

- (BOOL)isNSDate__
{
    return [mockedClass isSubclassOfClass:[NSDate class]];
}

- (BOOL)isNSString__
{
    return [mockedClass isSubclassOfClass:[NSString class]];
}

- (BOOL)isNSDictionary__
{
    return [mockedClass isSubclassOfClass:[NSDictionary class]];
}

- (BOOL)isNSData__
{
    return [mockedClass isSubclassOfClass:[NSData class]];
}

- (BOOL)isNSArray__
{
    return [mockedClass isSubclassOfClass:[NSArray class]];
}

@end
