// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/tracking_area.h"

#include "base/check.h"

// NSTrackingArea does not retain its |owner| so CrTrackingArea wraps the real
// owner in this proxy, which can stop forwarding messages to the owner when
// it is no longer |alive_|.
@interface CrTrackingAreaOwnerProxy : NSObject {
 @private
  // Whether or not the owner is "alive" and should forward calls to the real
  // owner object.
  BOOL _alive;

  // The real object for which this is a proxy. Weak.
  id _owner;

  // The Class of |owner_|. When the actual object is no longer alive (and could
  // be zombie), this allows for introspection.
  Class _ownerClass;
}
@property(nonatomic, assign) BOOL alive;
- (instancetype)initWithOwner:(id)owner;
@end

@implementation CrTrackingAreaOwnerProxy

@synthesize alive = _alive;

- (instancetype)initWithOwner:(id)owner {
  if ((self = [super init])) {
    _alive = YES;
    _owner = owner;
    _ownerClass = [owner class];
  }
  return self;
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  if (!_alive)
    return;
  [invocation invokeWithTarget:_owner];
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  // This can be called if |owner_| is not |alive_|, so use the Class to
  // generate the signature. |-forwardInvocation:| will block the actual call.
  return [_ownerClass instanceMethodSignatureForSelector:sel];
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  return [_ownerClass instancesRespondToSelector:aSelector];
}

@end

// Private Interface ///////////////////////////////////////////////////////////

@interface CrTrackingArea (Private)
- (void)windowWillClose:(NSNotification*)notif;
@end

////////////////////////////////////////////////////////////////////////////////

@implementation CrTrackingArea

- (instancetype)initWithRect:(NSRect)rect
           options:(NSTrackingAreaOptions)options
             owner:(id)owner
          userInfo:(NSDictionary*)userInfo{
  base::scoped_nsobject<CrTrackingAreaOwnerProxy> ownerProxy(
      [[CrTrackingAreaOwnerProxy alloc] initWithOwner:owner]);
  if ((self = [super initWithRect:rect
                          options:options
                            owner:ownerProxy.get()
                         userInfo:userInfo])) {
    _ownerProxy.swap(ownerProxy);
  }
  return self;
}

- (void)dealloc {
  [self clearOwner];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)clearOwner {
  [_ownerProxy setAlive:NO];
}

@end

// Scoper //////////////////////////////////////////////////////////////////////

namespace ui {

ScopedCrTrackingArea::ScopedCrTrackingArea(CrTrackingArea* tracking_area)
    : tracking_area_(tracking_area) {
}

ScopedCrTrackingArea::~ScopedCrTrackingArea() {
  [tracking_area_ clearOwner];
}

void ScopedCrTrackingArea::reset(CrTrackingArea* tracking_area) {
  tracking_area_.reset(tracking_area);
}

CrTrackingArea* ScopedCrTrackingArea::get() const {
  return tracking_area_.get();
}

}  // namespace ui
