// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/drag_drop_client_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/dragdrop/drag_drop_types.h"
#import "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/image/image_unittest_util.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

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
@interface MockDraggingInfo : NSObject <NSDraggingInfo>

@property BOOL animatesToDestination;
@property NSInteger numberOfValidItemsForDrop;
@property NSDraggingFormation draggingFormation;
@property(readonly) NSSpringLoadingHighlight springLoadingHighlight;

@end

@implementation MockDraggingInfo {
  NSPasteboard* _pasteboard;
}

@synthesize animatesToDestination;
@synthesize numberOfValidItemsForDrop;
@synthesize draggingFormation;
@synthesize springLoadingHighlight;

- (instancetype)initWithPasteboard:(NSPasteboard*)pasteboard {
  if ((self = [super init])) {
    _pasteboard = pasteboard;
  }
  return self;
}

- (NSPoint)draggingLocation {
  return NSMakePoint(50, 50);
}

- (NSPasteboard*)draggingPasteboard {
  return _pasteboard;
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

namespace views::test {

using ::ui::mojom::DragOperation;

// View object that will receive and process dropped data from the test.
class DragDropView : public View {
  METADATA_HEADER(DragDropView, View)

 public:
  DragDropView() = default;

  DragDropView(const DragDropView&) = delete;
  DragDropView& operator=(const DragDropView&) = delete;

  void set_formats(int formats) { formats_ = formats; }

  // View:
  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats |= formats_;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) override { return true; }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_COPY;
  }

  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override {
    return base::BindOnce(
        [](const ui::DropTargetEvent& event,
           ui::mojom::DragOperation& output_drag_op,
           std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
          output_drag_op = DragOperation::kMove;
        });
  }

 private:
  // Drop formats accepted by this View object.
  int formats_ = 0;
};

BEGIN_METADATA(DragDropView)
END_METADATA

class DragDropClientMacTest : public WidgetTest {
 public:
  DragDropClientMacTest() = default;

  DragDropClientMacTest(const DragDropClientMacTest&) = delete;
  DragDropClientMacTest& operator=(const DragDropClientMacTest&) = delete;

  DragDropClientMac* drag_drop_client() {
    return ns_window_host()->drag_drop_client();
  }

  NSDragOperation DragUpdate(NSPasteboard* pasteboard) {
    DragDropClientMac* client = drag_drop_client();
    dragging_info_ = [[MockDraggingInfo alloc] initWithPasteboard:pasteboard];
    return client->DragUpdate(dragging_info_);
  }

  NSDragOperation Drop() {
    DragDropClientMac* client = drag_drop_client();
    DCHECK(dragging_info_);
    NSDragOperation operation = client->Drop(dragging_info_);
    dragging_info_ = nil;
    return operation;
  }

  void SetData(OSExchangeData& data) {
    drag_drop_client()->exchange_data_ =
        std::make_unique<ui::OSExchangeData>(data.provider().Clone());
  }

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget()->GetWeakPtr();
    gfx::Rect bounds(0, 0, 100, 100);
    widget_->SetBounds(bounds);
    widget_->Show();

    target_ = widget_->non_client_view()->frame_view()->AddChildView(
        std::make_unique<DragDropView>());
    target_->SetBoundsRect(bounds);

    drag_drop_client()->source_operation_ = ui::DragDropTypes::DRAG_COPY;
  }

  void TearDown() override {
    target_ = nullptr;
    if (widget_)
      widget_->CloseNow();
    WidgetTest::TearDown();
  }

  remote_cocoa::NativeWidgetNSWindowBridge* bridge() {
    return ns_window_host()->GetInProcessNSWindowBridge();
  }

  NativeWidgetMacNSWindowHost* ns_window_host() {
    return NativeWidgetMacNSWindowHost::GetFromNativeWindow(
        widget_->GetNativeWindow());
  }

 protected:
  base::WeakPtr<Widget> widget_;
  raw_ptr<DragDropView> target_ = nullptr;

 private:
  MockDraggingInfo* __strong dragging_info_;
};

// Tests if the drag and drop target receives the dropped data.
TEST_F(DragDropClientMacTest, BasicDragDrop) {
  // Create the drop data
  OSExchangeData data;
  const std::u16string& text = u"text";
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
  bridge()->AcquireCapture();
  EXPECT_TRUE(ns_window_host()->IsMouseCaptureActive());

  // Create the drop data
  std::unique_ptr<OSExchangeData> data(std::make_unique<OSExchangeData>());
  const std::u16string& text = u"text";
  data->SetString(text);
  data->provider().SetDragImage(gfx::test::CreateImageSkia(100, 100),
                                gfx::Vector2d());
  SetData(*data.get());

  // There's no way to cleanly stop NSDraggingSession inside unit tests, so just
  // don't start it at all.
  base::apple::ScopedObjCClassSwizzler swizzle(
      [NSView class], @selector(beginDraggingSessionWithItems:event:source:),
      @selector(cr_beginDraggingSessionWithItems:event:source:));

  // Immediately quit drag'n'drop, or we'll hang.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DragDropClientMac::EndDrag,
                                base::Unretained(drag_drop_client())));

  // It will call ReleaseCapture().
  drag_drop_client()->StartDragAndDrop(std::move(data), 0,
                                       ui::mojom::DragEventSource::kMouse);

  // The capture should be released.
  EXPECT_FALSE(ns_window_host()->IsMouseCaptureActive());
}

// Tests if the drag and drop target rejects the dropped data with the
// incorrect format.
TEST_F(DragDropClientMacTest, InvalidFormatDragDrop) {
  OSExchangeData data;
  const std::u16string& text = u"text";
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
  METADATA_HEADER(DragDropCloseView, DragDropView)
 public:
  DragDropCloseView() = default;

  DragDropCloseView(const DragDropCloseView&) = delete;
  DragDropCloseView& operator=(const DragDropCloseView&) = delete;

  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override {
    // base::Unretained is safe here because in the tests the view isn't deleted
    // before the drop callback is run.
    return base::BindOnce(&DragDropCloseView::PerformDrop,
                          base::Unretained(this));
  }

 private:
  void PerformDrop(const ui::DropTargetEvent& event,
                   DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
    GetWidget()->CloseNow();
    output_drag_op = DragOperation::kMove;
  }
};

BEGIN_METADATA(DragDropCloseView)
END_METADATA

// Tests that closing Widget on drop does not crash.
TEST_F(DragDropClientMacTest, CloseWidgetOnDrop) {
  OSExchangeData data;
  const std::u16string& text = u"text";
  data.SetString(text);
  SetData(data);

  DragDropCloseView* target =
      widget_->non_client_view()->frame_view()->AddChildView(
          std::make_unique<DragDropCloseView>());
  target->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  target->set_formats(ui::OSExchangeData::STRING | ui::OSExchangeData::URL);

  // Dropping will destroy target_.
  target_ = nullptr;
  EXPECT_EQ(DragUpdate(nil), NSDragOperationCopy);
  EXPECT_EQ(Drop(), NSDragOperationMove);
}

}  // namespace views::test
