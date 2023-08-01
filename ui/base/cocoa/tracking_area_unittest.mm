// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/tracking_area.h"
#import "ui/base/test/cocoa_helper.h"

// A test object that counts the number of times a message is sent to it.
@interface TestTrackingAreaOwner : NSObject {
 @private
  NSUInteger _messageCount;
}
@property(nonatomic, assign) NSUInteger messageCount;
- (void)performMessage;
@end

@implementation TestTrackingAreaOwner
@synthesize messageCount = _messageCount;
- (void)performMessage {
  ++_messageCount;
}
@end

namespace ui {

class CrTrackingAreaTest : public CocoaTest {
 public:
  CrTrackingAreaTest()
      : owner_([[TestTrackingAreaOwner alloc] init]),
        trackingArea_([[CrTrackingArea alloc]
            initWithRect:NSMakeRect(0, 0, 100, 100)
                 options:NSTrackingMouseMoved | NSTrackingActiveInKeyWindow
                   owner:owner_
                userInfo:nil]) {}

  TestTrackingAreaOwner* __strong owner_;
  CrTrackingArea* __strong trackingArea_;
};

TEST_F(CrTrackingAreaTest, OwnerForwards) {
  [[trackingArea_ owner] performMessage];
  EXPECT_EQ(1U, [owner_ messageCount]);

  [[trackingArea_ owner] performMessage];
  EXPECT_EQ(2U, [owner_ messageCount]);
}

TEST_F(CrTrackingAreaTest, OwnerStopsForwarding) {
  [[trackingArea_ owner] performMessage];
  EXPECT_EQ(1U, [owner_ messageCount]);

  [trackingArea_ clearOwner];

  [[trackingArea_ owner] performMessage];
  EXPECT_EQ(1U, [owner_ messageCount]);
}

TEST_F(CrTrackingAreaTest, ScoperInit) {
  {
    ScopedCrTrackingArea scoper(trackingArea_);
    [[scoper.get() owner] performMessage];
    EXPECT_EQ(1U, [owner_ messageCount]);
  }

  [[trackingArea_ owner] performMessage];
  EXPECT_EQ(1U, [owner_ messageCount]);
}

TEST_F(CrTrackingAreaTest, ScoperReset) {
  {
    ScopedCrTrackingArea scoper;
    EXPECT_FALSE(scoper.get());

    scoper.reset(trackingArea_);
    [[scoper.get() owner] performMessage];
    EXPECT_EQ(1U, [owner_ messageCount]);

    [[scoper.get() owner] performMessage];
    EXPECT_EQ(2U, [owner_ messageCount]);
  }

  [[trackingArea_ owner] performMessage];
  EXPECT_EQ(2U, [owner_ messageCount]);
}

}  // namespace ui
