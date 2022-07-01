// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/test/virtual_display_mac_util.h"

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <IOKit/pwr_mgt/IOPMLib.h>

#include <map>

#include "base/check.h"
#include "base/check_op.h"
#include "base/mac/scoped_nsobject.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"

// These interfaces were generated from CoreGraphics binaries.
API_AVAILABLE(macos(10.14))
@interface CGVirtualDisplay : NSObject {
  unsigned int _vendorID;
  unsigned int _productID;
  unsigned int _serialNum;
  NSString* _name;
  struct CGSize _sizeInMillimeters;
  unsigned int _maxPixelsWide;
  unsigned int _maxPixelsHigh;
  struct CGPoint _redPrimary;
  struct CGPoint _greenPrimary;
  struct CGPoint _bluePrimary;
  struct CGPoint _whitePoint;
  id _queue;
  id _terminationHandler;
  unsigned int _displayID;
  unsigned int _hiDPI;
  NSArray* _modes;
}

@property(readonly, nonatomic)
    unsigned int vendorID;  // @synthesize vendorID=_vendorID;
@property(readonly, nonatomic)
    unsigned int productID;  // @synthesize productID=_productID;
@property(readonly, nonatomic)
    unsigned int serialNum;  // @synthesize serialNum=_serialNum;
@property(readonly, nonatomic) NSString* name;  // @synthesize name=_name;
@property(readonly, nonatomic) struct CGSize
    sizeInMillimeters;  // @synthesize sizeInMillimeters=_sizeInMillimeters;
@property(readonly, nonatomic)
    unsigned int maxPixelsWide;  // @synthesize maxPixelsWide=_maxPixelsWide;
@property(readonly, nonatomic)
    unsigned int maxPixelsHigh;  // @synthesize maxPixelsHigh=_maxPixelsHigh;
@property(readonly, nonatomic)
    struct CGPoint redPrimary;  // @synthesize redPrimary=_redPrimary;
@property(readonly, nonatomic)
    struct CGPoint greenPrimary;  // @synthesize greenPrimary=_greenPrimary;
@property(readonly, nonatomic)
    struct CGPoint bluePrimary;  // @synthesize bluePrimary=_bluePrimary;
@property(readonly, nonatomic)
    struct CGPoint whitePoint;            // @synthesize whitePoint=_whitePoint;
@property(readonly, nonatomic) id queue;  // @synthesize queue=_queue;
@property(readonly, nonatomic) id
    terminationHandler;  // @synthesize terminationHandler=_terminationHandler;
@property(readonly, nonatomic)
    unsigned int displayID;  // @synthesize displayID=_displayID;
@property(readonly, nonatomic) unsigned int hiDPI;  // @synthesize hiDPI=_hiDPI;
@property(readonly, nonatomic) NSArray* modes;      // @synthesize modes=_modes;
- (BOOL)applySettings:(id)arg1;
- (void)dealloc;
- (id)initWithDescriptor:(id)arg1;

@end

// These interfaces were generated from CoreGraphics binaries.
API_AVAILABLE(macos(10.14))
@interface CGVirtualDisplayDescriptor : NSObject {
  unsigned int _vendorID;
  unsigned int _productID;
  unsigned int _serialNum;
  NSString* _name;
  struct CGSize _sizeInMillimeters;
  unsigned int _maxPixelsWide;
  unsigned int _maxPixelsHigh;
  struct CGPoint _redPrimary;
  struct CGPoint _greenPrimary;
  struct CGPoint _bluePrimary;
  struct CGPoint _whitePoint;
  id _queue;
  id _terminationHandler;
}

@property(nonatomic) unsigned int vendorID;  // @synthesize vendorID=_vendorID;
@property(nonatomic)
    unsigned int productID;  // @synthesize productID=_productID;
@property(nonatomic)
    unsigned int serialNum;  // @synthesize serialNum=_serialNum;
@property(strong, nonatomic) NSString* name;  // @synthesize name=_name;
@property(nonatomic) struct CGSize
    sizeInMillimeters;  // @synthesize sizeInMillimeters=_sizeInMillimeters;
@property(nonatomic)
    unsigned int maxPixelsWide;  // @synthesize maxPixelsWide=_maxPixelsWide;
@property(nonatomic)
    unsigned int maxPixelsHigh;  // @synthesize maxPixelsHigh=_maxPixelsHigh;
@property(nonatomic)
    struct CGPoint redPrimary;  // @synthesize redPrimary=_redPrimary;
@property(nonatomic)
    struct CGPoint greenPrimary;  // @synthesize greenPrimary=_greenPrimary;
@property(nonatomic)
    struct CGPoint bluePrimary;  // @synthesize bluePrimary=_bluePrimary;
@property(nonatomic)
    struct CGPoint whitePoint;          // @synthesize whitePoint=_whitePoint;
@property(retain, nonatomic) id queue;  // @synthesize queue=_queue;
@property(copy, nonatomic) id
    terminationHandler;  // @synthesize terminationHandler=_terminationHandler;
- (void)dealloc;
- (id)init;
- (id)dispatchQueue;
- (void)setDispatchQueue:(id)arg1;

@end

// These interfaces were generated from CoreGraphics binaries.
API_AVAILABLE(macos(10.14))
@interface CGVirtualDisplayMode : NSObject {
  unsigned int _width;
  unsigned int _height;
  double _refreshRate;
}

@property(readonly, nonatomic) unsigned int width;  // @synthesize width=_width;
@property(readonly, nonatomic)
    unsigned int height;  // @synthesize height=_height;
@property(readonly, nonatomic)
    double refreshRate;  // @synthesize refreshRate=_refreshRate;
- (id)initWithWidth:(unsigned int)arg1
             height:(unsigned int)arg2
        refreshRate:(double)arg3;

@end

// These interfaces were generated from CoreGraphics binaries.
API_AVAILABLE(macos(10.14))
@interface CGVirtualDisplaySettings : NSObject {
  NSArray* _modes;
  unsigned int _hiDPI;
}

@property(strong, nonatomic) NSArray* modes;  // @synthesize modes=_modes;
@property(nonatomic) unsigned int hiDPI;      // @synthesize hiDPI=_hiDPI;
- (void)dealloc;
- (id)init;

@end

namespace {

static constexpr int kRetinaPPI = 220;

// Track the AssertionID argument to IOPMAssertionCreateWithProperties and
// IOPMAssertionRelease.
static IOPMAssertionID g_assertion_id = kIOPMNullAssertionID;

// A global singleton that tracks the current set of mocked displays.
std::map<int, base::scoped_nsobject<CGVirtualDisplay>> g_display_map
    API_AVAILABLE(macos(10.14));

// A helper function for creating virtual display and return CGVirtualDisplay
// object.
base::scoped_nsobject<CGVirtualDisplay> createVirtualDisplay(int width,
                                                             int height,
                                                             int ppi,
                                                             BOOL hiDPI,
                                                             NSString* name)
    API_AVAILABLE(macos(10.14)) {
  base::scoped_nsobject<CGVirtualDisplaySettings> settings(
      [[CGVirtualDisplaySettings alloc] init]);
  [settings setHiDPI:hiDPI];

  base::scoped_nsobject<CGVirtualDisplayDescriptor> descriptor(
      [[CGVirtualDisplayDescriptor alloc] init]);
  [descriptor
      setQueue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0)];
  [descriptor setName:name];

  // See System Preferences > Displays > Color > Open Profile > Apple display
  // native information
  [descriptor setWhitePoint:CGPointMake(0.3125, 0.3291)];
  [descriptor setBluePrimary:CGPointMake(0.1494, 0.0557)];
  [descriptor setGreenPrimary:CGPointMake(0.2559, 0.6983)];
  [descriptor setRedPrimary:CGPointMake(0.6797, 0.3203)];
  [descriptor setMaxPixelsHigh:height];
  [descriptor setMaxPixelsWide:width];
  [descriptor
      setSizeInMillimeters:CGSizeMake(25.4 * width / ppi, 25.4 * height / ppi)];
  [descriptor setSerialNum:0];
  [descriptor setProductID:0];
  [descriptor setVendorID:0];

  base::scoped_nsobject<CGVirtualDisplay> display(
      [[CGVirtualDisplay alloc] initWithDescriptor:descriptor]);

  if ([settings hiDPI]) {
    width /= 2;
    height /= 2;
  }
  base::scoped_nsobject<CGVirtualDisplayMode> mode([[CGVirtualDisplayMode alloc]
      initWithWidth:width
             height:height
        refreshRate:60]);
  [settings setModes:@[ mode ]];

  if (![display applySettings:settings])
    return base::scoped_nsobject<CGVirtualDisplay>();

  return display;
}

}  // namespace

namespace display::test {

// static
void VirtualDisplayMacUtil::AddDisplay(int display_id, const gfx::Size& size) {
  if (@available(macos 10.14, *)) {
    NSString* display_name =
        [NSString stringWithFormat:@"Virtual Display #%d", display_id];
    base::scoped_nsobject<CGVirtualDisplay> display =
        createVirtualDisplay(size.width(), size.height(), kRetinaPPI,
                             /*hiDPI=*/false, display_name);

    DCHECK(!g_display_map[display_id]);
    g_display_map[display_id] = display;
  }
}

// static
void VirtualDisplayMacUtil::RemoveDisplay(int display_id) {
  if (@available(macos 10.14, *)) {
    auto it = g_display_map.find(display_id);

    DCHECK(it != g_display_map.end());
    g_display_map.erase(it);
  }
}

// static
void VirtualDisplayMacUtil::RemoveAllDisplays() {
  if (@available(macos 10.14, *)) {
    while (!g_display_map.empty()) {
      auto iter = g_display_map.begin();
      RemoveDisplay(iter->first);
    }
  }
}

// static
bool VirtualDisplayMacUtil::IsAPIAvailable() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return false;
#else
  if (@available(macos 10.14, *)) {
    return true;
  }
  return false;
#endif  // defined(ARCH_CPU_ARM_FAMILY)
}

// static
void VirtualDisplayMacUtil::PreventDisplaySleep() {
  IOReturn result = IOPMAssertionCreateWithName(
      kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
      CFSTR("Window placement Tests"), &g_assertion_id);
  DCHECK_EQ(result, kIOReturnSuccess);
}

// static
void VirtualDisplayMacUtil::AllowDisplaySleep() {
  IOReturn result = IOPMAssertionRelease(g_assertion_id);
  DCHECK_EQ(result, kIOReturnSuccess);
}

}  // namespace display::test
