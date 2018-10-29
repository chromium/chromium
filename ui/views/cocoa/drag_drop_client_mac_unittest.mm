// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/drag_drop_client_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/availability.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/gfx/image/image_unittest_util.h"
#import "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"
#import "ui/views_bridge_mac/bridged_native_widget_impl.h"

using base::ASCIIToUTF16;

@interface NSView (DragSessionTestingDonor)
@end

@implementation NSView (DragSessionTestingDonor)
- (NSDraggingSession*)cr_beginDraggingSessionWithItems:(NSArray*)items
                                                 event:(NSEvent*)event
                                                source:(id<NSDraggingSource>)
                                                           source {
  return nil;
}
@end

// Mocks the NSDraggingInfo sent to the DragDropClientMac's DragUpdate() and
// Drop() methods. Out of the required methods of the protocol, only
// draggingLocation and draggingPasteboard are used.
@interface MockDraggingInfo : NSObject<NSDraggingInfo> {
  NSPasteboard* pasteboard_;
}

@property BOOL animatesToDestination;
@property NSInteger numberOfValidItemsForDrop;
@property NSDraggingFormation draggingFormation;
@property(readonly)
    NSSpringLoadingHighlight springLoadingHighlight API_AVAILABLE(macos(10.11));

@end

@implementation MockDraggingInfo

@synthesize animatesToDestination;
@synthesize numberOfValidItemsForDrop;
@synthesize draggingFormation;
@synthesize springLoadingHighlight;

- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard {
  if ((self = [super init])) {
    pasteboard_ = pasteboard;
  }
  return self;
}

- (NSPoint)draggingLocation {
  return NSMakePoint(50, 50);
}

- (NSPasteboard*)draggingPasteboard {
  return pasteboard_;
}

- (NSInteger)draggingSequenceNumber {
  return 0;
}

- (id)draggingSource {
  return nil;
}

- (NSDragOperation)draggingSourceOperationMask {
  return NSDragOperationEvery;
}

- (NSWindow*)draggingDestinationWindow {
  return nil;
}

- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDestination {
  return nil;
}

- (NSImage*)draggedImage {
  return nil;
}

- (NSPoint)draggedImageLocation {
  return NSZeroPoint;
}

- (void)slideDraggedImageTo:(NSPoint)aPoint {
}

- (void)
enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)enumOpts
                          forView:(NSView*)view
                          classes:(NSArray*)classArray
                    searchOptions:(NSDictionary*)searchOptions
                       usingBlock:(void (^)(NSDraggingItem* draggingItem,
                                            NSInteger idx,
                                            BOOL* stop))block {
}

- (void)resetSpringLoading {
}

@end

namespace views {
namespace test {

// View object that will receive and process dropped data from the test.
class DragDropView : public View {
 public:
  DragDropView() {}

  void set_formats(int formats) { formats_ = formats; }

  // View:
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override {
    *formats |= formats_;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) override { return true; }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_COPY;
  }

  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }

 private:
  // Drop formats accepted by this View object.
  int formats_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DragDropView);
};

class DragDropClientMacTest : public WidgetTest {
 public:
  DragDropClientMacTest() : widget_(new Widget) {}

  DragDropClientMac* drag_drop_client() {
    return bridge_host_->drag_drop_client();
  }

  NSDragOperation DragUpdate(NSPasteboard* pasteboard) {
    DragDropClientMac* client = drag_drop_client();
    dragging_info_.reset(
        [[MockDraggingInfo alloc] initWithPasteboard:pasteboard]);
    return client->DragUpdate(dragging_info_.get());
  }

  NSDragOperation Drop() {
    DragDropClientMac* client = drag_drop_client();
    DCHECK(dragging_info_.get());
    NSDragOperation operation = client->Drop(dragging_info_.get());
    dragging_info_.reset();
    return operation;
  }

  void SetData(OSExchangeData& data) {
    drag_drop_client()->data_source_.reset(
        [[CocoaDragDropDataProvider alloc] initWithData:data]);
  }

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();
    gfx::Rect bounds(0, 0, 100, 100);
    widget_->SetBounds(bounds);

    bridge_host_ = BridgedNativeWidgetHostImpl::GetFromNativeWindow(
        widget_->GetNativeWindow());
    bridge_ = bridge_host_->bridge_impl();
    widget_->Show();

    target_ = new DragDropView();
    widget_->GetContentsView()->AddChildView(target_);
    target_->SetBoundsRect(bounds);

    drag_drop_client()->operation_ = ui::DragDropTypes::DRAG_COPY;
  }

  void TearDown() override {
    if (widget_)
      widget_->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  Widget* widget_ = nullptr;
  BridgedNativeWidgetImpl* bridge_ = nullptr;
  BridgedNativeWidgetHostImpl* bridge_host_ = nullptr;
  DragDropView* target_ = nullptr;
  base::scoped_nsobject<MockDraggingInfo> dragging_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DragDropClientMacTest);
};

// Tests if the drag and drop target receives the dropped data.
TEST_F(DragDropClientMacTest, BasicDragDrop) {
  // Create the drop data
  OSExchangeData data;
  const base::string16& text = ASCIIToUTF16("text");
  data.SetString(text);
  SetData(data);

  target_->set_formats(ui::OSExchangeData::STRING | ui::OSExchangeData::URL);

  // Check if the target receives the data from a drop and returns the expected
  // operation.
  EXPECT_EQ(DragUpdate(nil), NSDragOperationCopy);
  EXPECT_EQ(Drop(), NSDragOperationMove);
}

// Ensure that capture is released before the end of a drag and drop operation.
TEST_F(DragDropClientMacTest, ReleaseCapture) {
  // DragDropView doesn't actually capture the mouse, so explicitly acquire it
  // to test that StartDragAndDrop() actually releases it.
  // Although this is not an interactive UI test, acquiring capture should be OK
  // since the runloop will exit before the system has any opportunity to
  // capture anything.
  bridge_->AcquireCapture();
  EXPECT_TRUE(bridge_host_->IsMouseCaptureActive());

  // Create the drop data
  OSExchangeData data;
  const base::string16& text = ASCIIToUTF16("text");
  data.SetString(text);
  data.provider().SetDragImage(gfx::test::CreateImageSkia(100, 100),
                               gfx::Vector2d());
  SetData(data);

  // There's no way to cleanly stop NSDraggingSession inside unit tests, so just
  // don't start it at all.
  base::mac::ScopedObjCClassSwizzler swizzle(
      [NSView class], @selector(beginDraggingSessionWithItems:event:source:),
      @selector(cr_beginDraggingSessionWithItems:event:source:));

  // Immediately quit drag'n'drop, or we'll hang.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DragDropClientMac::EndDrag,
                            base::Unretained(drag_drop_client())));

  // It will call ReleaseCapture().
  drag_drop_client()->StartDragAndDrop(
      target_, data, 0, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

  // The capture should be released.
  EXPECT_FALSE(bridge_host_->IsMouseCaptureActive());
}

// Tests if the drag and drop target rejects the dropped data with the
// incorrect format.
TEST_F(DragDropClientMacTest, InvalidFormatDragDrop) {
  OSExchangeData data;
  const base::string16& text = ASCIIToUTF16("text");
  data.SetString(text);
  SetData(data);

  target_->set_formats(ui::OSExchangeData::URL);

  // Check if the target receives the data from a drop and returns the expected
  // operation.
  EXPECT_EQ(DragUpdate(nil), NSDragOperationNone);
  EXPECT_EQ(Drop(), NSDragOperationNone);
}

// Tests if the drag and drop target can accept data without an OSExchangeData
// object.
TEST_F(DragDropClientMacTest, PasteboardToOSExchangeTest) {
  target_->set_formats(ui::OSExchangeData::STRING);

  scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;

  // The test should reject the data if the pasteboard is empty.
  EXPECT_EQ(DragUpdate(pasteboard->get()), NSDragOperationNone);
  EXPECT_EQ(Drop(), NSDragOperationNone);
  drag_drop_client()->EndDrag();

  // Add valid data to the pasteboard and check to see if the target accepts
  // it.
  [pasteboard->get() setString:@"text" forType:NSPasteboardTypeString];
  EXPECT_EQ(DragUpdate(pasteboard->get()), NSDragOperationCopy);
  EXPECT_EQ(Drop(), NSDragOperationMove);
}

// View object that will close Widget on drop.
class DragDropCloseView : public DragDropView {
 public:
  DragDropCloseView() {}

  // View:
  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    GetWidget()->CloseNow();
    return ui::DragDropTypes::DRAG_MOVE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DragDropCloseView);
};

// Tests that closing Widget on OnPerformDrop does not crash.
TEST_F(DragDropClientMacTest, CloseWidgetOnDrop) {
  OSExchangeData data;
  const base::string16& text = ASCIIToUTF16("text");
  data.SetString(text);
  SetData(data);

  target_ = new DragDropCloseView();
  widget_->GetContentsView()->AddChildView(target_);
  target_->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  target_->set_formats(ui::OSExchangeData::STRING | ui::OSExchangeData::URL);

  EXPECT_EQ(DragUpdate(nil), NSDragOperationCopy);
  EXPECT_EQ(Drop(), NSDragOperationMove);

  // OnPerformDrop() will have deleted the widget.
  widget_ = nullptr;
}

}  // namespace test
}  // namespace views
