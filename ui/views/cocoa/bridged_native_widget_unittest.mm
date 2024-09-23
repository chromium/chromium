// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"

#import <Cocoa/Cocoa.h>
#include <objc/runtime.h>

#include <memory>
#include <string>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/bind.h"
#import "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/views_nswindow_delegate.h"
#import "testing/gtest_mac.h"
#include "ui/base/cocoa/find_pasteboard.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#import "ui/base/test/cocoa_helper.h"
#include "ui/display/screen.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#import "ui/views/cocoa/text_input_host.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using base::ASCIIToUTF16;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;

#define EXPECT_EQ_RANGE(a, b)        \
  EXPECT_EQ(a.location, b.location); \
  EXPECT_EQ(a.length, b.length);

// Helpers to verify an expectation against both the actual toolkit-views
// behaviour and the Cocoa behaviour.

#define EXPECT_NSEQ_3(expected_literal, expected_cocoa, actual_views) \
  EXPECT_NSEQ(expected_literal, actual_views);                        \
  EXPECT_NSEQ(expected_cocoa, actual_views);

#define EXPECT_EQ_RANGE_3(expected_literal, expected_cocoa, actual_views) \
  EXPECT_EQ_RANGE(expected_literal, actual_views);                        \
  EXPECT_EQ_RANGE(expected_cocoa, actual_views);

#define EXPECT_EQ_3(expected_literal, expected_cocoa, actual_views) \
  EXPECT_EQ(expected_literal, actual_views);                        \
  EXPECT_EQ(expected_cocoa, actual_views);

namespace {

// Implemented NSResponder action messages for use in tests.
NSArray* const kMoveActions = @[
  @"moveForward:",
  @"moveRight:",
  @"moveBackward:",
  @"moveLeft:",
  @"moveUp:",
  @"moveDown:",
  @"moveWordForward:",
  @"moveWordBackward:",
  @"moveToBeginningOfLine:",
  @"moveToEndOfLine:",
  @"moveToBeginningOfParagraph:",
  @"moveToEndOfParagraph:",
  @"moveToEndOfDocument:",
  @"moveToBeginningOfDocument:",
  @"pageDown:",
  @"pageUp:",
  @"moveWordRight:",
  @"moveWordLeft:",
  @"moveToLeftEndOfLine:",
  @"moveToRightEndOfLine:"
];

NSArray* const kSelectActions = @[
  @"moveBackwardAndModifySelection:",
  @"moveForwardAndModifySelection:",
  @"moveWordForwardAndModifySelection:",
  @"moveWordBackwardAndModifySelection:",
  @"moveUpAndModifySelection:",
  @"moveDownAndModifySelection:",
  @"moveToBeginningOfLineAndModifySelection:",
  @"moveToEndOfLineAndModifySelection:",
  @"moveToBeginningOfParagraphAndModifySelection:",
  @"moveToEndOfParagraphAndModifySelection:",
  @"moveToEndOfDocumentAndModifySelection:",
  @"moveToBeginningOfDocumentAndModifySelection:",
  @"pageDownAndModifySelection:",
  @"pageUpAndModifySelection:",
  @"moveParagraphForwardAndModifySelection:",
  @"moveParagraphBackwardAndModifySelection:",
  @"moveRightAndModifySelection:",
  @"moveLeftAndModifySelection:",
  @"moveWordRightAndModifySelection:",
  @"moveWordLeftAndModifySelection:",
  @"moveToLeftEndOfLineAndModifySelection:",
  @"moveToRightEndOfLineAndModifySelection:"
];

NSArray* const kDeleteActions = @[
  @"deleteForward:", @"deleteBackward:", @"deleteWordForward:",
  @"deleteWordBackward:", @"deleteToBeginningOfLine:", @"deleteToEndOfLine:",
  @"deleteToBeginningOfParagraph:", @"deleteToEndOfParagraph:"
];

// This omits @"insertText:":. See BridgedNativeWidgetTest.NilTextInputClient.
NSArray* const kMiscActions = @[ @"cancelOperation:", @"transpose:", @"yank:" ];

// Empty range shortcut for readability.
NSRange EmptyRange() {
  return NSMakeRange(NSNotFound, 0);
}

// Sets |composition_text| as the composition text with caret placed at
// |caret_pos| and updates |caret_range|.
void SetCompositionText(ui::TextInputClient* client,
                        const std::u16string& composition_text,
                        const int caret_pos,
                        NSRange* caret_range) {
  ui::CompositionText composition;
  composition.selection = gfx::Range(caret_pos);
  composition.text = composition_text;
  client->SetCompositionText(composition);
  if (caret_range)
    *caret_range = NSMakeRange(caret_pos, 0);
}

// Returns a zero width rectangle corresponding to current caret position.
gfx::Rect GetCaretBounds(const ui::TextInputClient* client) {
  gfx::Rect caret_bounds = client->GetCaretBounds();
  caret_bounds.set_width(0);
  return caret_bounds;
}

// Returns a zero width rectangle corresponding to caret bounds when it's placed
// at |caret_pos| and updates |caret_range|.
gfx::Rect GetCaretBoundsForPosition(ui::TextInputClient* client,
                                    const std::u16string& composition_text,
                                    const int caret_pos,
                                    NSRange* caret_range) {
  SetCompositionText(client, composition_text, caret_pos, caret_range);
  return GetCaretBounds(client);
}

// Returns the expected boundary rectangle for characters of |composition_text|
// within the |query_range|.
gfx::Rect GetExpectedBoundsForRange(ui::TextInputClient* client,
                                    const std::u16string& composition_text,
                                    NSRange query_range) {
  gfx::Rect left_caret = GetCaretBoundsForPosition(
      client, composition_text, query_range.location, nullptr);
  gfx::Rect right_caret = GetCaretBoundsForPosition(
      client, composition_text, query_range.location + query_range.length,
      nullptr);

  // The expected bounds correspond to the area between the left and right caret
  // positions.
  return gfx::Rect(left_caret.x(), left_caret.y(),
                   right_caret.x() - left_caret.x(), left_caret.height());
}

// Uses the NSTextInputClient protocol to extract a substring from |view|.
NSString* GetViewStringForRange(NSView<NSTextInputClient>* view,
                                NSRange range) {
  return [[view attributedSubstringForProposedRange:range actualRange:nullptr]
      string];
}

// The behavior of NSTextView for RTL strings is buggy for some move and select
// commands, but only when the command is received when there is a selection
// active. E.g. moveRight: moves a cursor right in an RTL string, but it moves
// to the left-end of a selection. See TestEditingCommands() for specifics.
// This is filed as rdar://27863290.
bool IsRTLMoveBuggy(SEL sel) {
  return sel == @selector(moveWordRight:) || sel == @selector(moveWordLeft:) ||
         sel == @selector(moveRight:) || sel == @selector(moveLeft:);
}
bool IsRTLSelectBuggy(SEL sel) {
  return sel == @selector(moveWordRightAndModifySelection:) ||
         sel == @selector(moveWordLeftAndModifySelection:) ||
         sel == @selector(moveRightAndModifySelection:) ||
         sel == @selector(moveLeftAndModifySelection:);
}

// Used by InterpretKeyEventsDonorForNSView to simulate IME behavior.
using InterpretKeyEventsCallback = base::RepeatingCallback<void(id)>;
InterpretKeyEventsCallback* g_fake_interpret_key_events = nullptr;

// Used by UpdateWindowsDonorForNSApp to hook -[NSApp updateWindows].
base::RepeatingClosure* g_update_windows_closure = nullptr;

// Used to provide a return value for +[NSTextInputContext currentInputContext].
NSTextInputContext* g_fake_current_input_context = nullptr;

}  // namespace

// Subclass of BridgedContentView with an override of interpretKeyEvents:. Note
// the size of the class must match BridgedContentView since the method table
// is swapped out at runtime. This is basically a mock, but mocks are banned
// under ui/views. Method swizzling causes these tests to flake when
// parallelized in the same process.
@interface InterpretKeyEventMockedBridgedContentView : BridgedContentView
@end

@implementation InterpretKeyEventMockedBridgedContentView

- (void)interpretKeyEvents:(NSArray<NSEvent*>*)eventArray {
  ASSERT_TRUE(g_fake_interpret_key_events);
  g_fake_interpret_key_events->Run(self);
}

@end

@interface UpdateWindowsDonorForNSApp : NSApplication
@end

@implementation UpdateWindowsDonorForNSApp

- (void)updateWindows {
  ASSERT_TRUE(g_update_windows_closure);
  g_update_windows_closure->Run();
}

@end
@interface CurrentInputContextDonorForNSTextInputContext : NSTextInputContext
@end

@implementation CurrentInputContextDonorForNSTextInputContext

+ (NSTextInputContext*)currentInputContext {
  return g_fake_current_input_context;
}

@end

// Let's not mess with the machine's actual find pasteboard!
@interface MockFindPasteboard : FindPasteboard
@end

@implementation MockFindPasteboard {
  NSString* __strong _text;
}

+ (FindPasteboard*)sharedInstance {
  static MockFindPasteboard* instance = nil;
  if (!instance) {
    instance = [[MockFindPasteboard alloc] init];
  }
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _text = @"";
  }
  return self;
}

- (void)loadTextFromPasteboard:(NSNotification*)notification {
  // No-op
}

- (void)setFindText:(NSString*)newText {
  _text = [newText copy];
}

- (NSString*)findText {
  return _text;
}
@end

@interface NativeWidgetMacNSWindowForTesting : NativeWidgetMacNSWindow {
  BOOL hasShadowForTesting;
}
@end

// An NSTextStorage subclass for our DummyTextView, to work around test
// failures with macOS 13. See crbug.com/1446817 .
@interface DummyTextStorage : NSTextStorage {
  NSMutableAttributedString* __strong _backingStore;
}
@end

@implementation DummyTextStorage

- (id)init {
  self = [super init];
  if (self) {
    _backingStore = [[NSMutableAttributedString alloc] init];
  }
  return self;
}

- (NSString*)string {
  return [_backingStore string];
}

- (NSDictionary*)attributesAtIndex:(NSUInteger)location
                    effectiveRange:(NSRangePointer)range {
  return [_backingStore attributesAtIndex:location effectiveRange:range];
}

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString*)str {
  [self beginEditing];
  [_backingStore replaceCharactersInRange:range withString:str];
  [self edited:NSTextStorageEditedCharacters
               range:range
      changeInLength:str.length - range.length];
  [self endEditing];
}

- (void)setAttributes:(NSDictionary<NSAttributedStringKey, id>*)attrs
                range:(NSRange)range {
  [self beginEditing];
  [_backingStore setAttributes:attrs range:range];
  [self edited:NSTextStorageEditedAttributes range:range changeInLength:0];
  [self endEditing];
}

@end

// An NSTextView subclass that uses its own NSTextStorage subclass, to work
// around test failures with macOS 13. See crbug.com/1446817 .
@interface DummyTextView : NSTextView {
}
@end

@implementation DummyTextView

- (instancetype)initWithFrame:(NSRect)frameRect
                textContainer:(NSTextContainer*)container {
  DummyTextStorage* textStorage = [[DummyTextStorage alloc] init];
  NSTextContainer* textContainer = [[NSTextContainer alloc]
      initWithSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
  NSLayoutManager* layoutManager = [[NSLayoutManager alloc] init];

  [layoutManager addTextContainer:textContainer];
  [textStorage addLayoutManager:layoutManager];

  self = [super initWithFrame:frameRect textContainer:textContainer];

  return self;
}

@end

@implementation NativeWidgetMacNSWindowForTesting

// Preserves the value of the hasShadow flag. During testing, -hasShadow will
// always return NO because shadows are disabled on the bots.
- (void)setHasShadow:(BOOL)flag {
  hasShadowForTesting = flag;
  [super setHasShadow:flag];
}

// Returns the value of the hasShadow flag during tests.
- (BOOL)hasShadowForTesting {
  return hasShadowForTesting;
}

@end

namespace views::test {

// Provides the |parent| argument to construct a NativeWidgetNSWindowBridge.
class MockNativeWidgetMac : public NativeWidgetMac {
 public:
  explicit MockNativeWidgetMac(internal::NativeWidgetDelegate* delegate)
      : NativeWidgetMac(delegate) {}

  MockNativeWidgetMac(const MockNativeWidgetMac&) = delete;
  MockNativeWidgetMac& operator=(const MockNativeWidgetMac&) = delete;

  using NativeWidgetMac::GetInProcessNSWindowBridge;
  using NativeWidgetMac::GetNSWindowHost;

  // internal::NativeWidgetPrivate:
  void InitNativeWidget(Widget::InitParams params) override {
    ownership_ = params.ownership;

    NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindowForTesting alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    GetNSWindowHost()->CreateInProcessNSWindowBridge(window);
    if (auto* parent =
            NativeWidgetMacNSWindowHost::GetFromNativeView(params.parent)) {
      GetNSWindowHost()->SetParent(parent);
    }
    GetNSWindowHost()->InitWindow(params,
                                  ConvertBoundsToScreenIfNeeded(params.bounds));

    // Usually the bridge gets initialized here. It is skipped to run extra
    // checks in tests, and so that a second window isn't created.
    delegate_for_testing()->OnNativeWidgetCreated();

    // To allow events to dispatch to a view, it needs a way to get focus.
    SetFocusManager(GetWidget()->GetFocusManager());
  }

  void ReorderNativeViews() override {
    // Called via Widget::Init to set the content view. No-op in these tests.
  }
};

// Helper test base to construct a NativeWidgetNSWindowBridge with a valid
// parent.
class BridgedNativeWidgetTestBase : public ui::CocoaTest,
                                    public WidgetObserver {
 public:
  struct SkipInitialization {};

  BridgedNativeWidgetTestBase()
      : widget_(new Widget),
        native_widget_mac_(new MockNativeWidgetMac(widget_.get())) {
    observation_.Observe(widget_.get());
  }

  explicit BridgedNativeWidgetTestBase(SkipInitialization tag)
      : native_widget_mac_(nullptr) {}

  BridgedNativeWidgetTestBase(const BridgedNativeWidgetTestBase&) = delete;
  BridgedNativeWidgetTestBase& operator=(const BridgedNativeWidgetTestBase&) =
      delete;

  remote_cocoa::NativeWidgetNSWindowBridge* bridge() {
    return native_widget_mac_ ? native_widget_mac_->GetInProcessNSWindowBridge()
                              : nullptr;
  }
  NativeWidgetMacNSWindowHost* GetNSWindowHost() {
    return native_widget_mac_ ? native_widget_mac_->GetNSWindowHost() : nullptr;
  }

  // Generate an autoreleased KeyDown NSEvent* in |widget_| for pressing the
  // corresponding |key_code|.
  NSEvent* VkeyKeyDown(ui::KeyboardCode key_code) {
    return cocoa_test_event_utils::SynthesizeKeyEvent(
        widget_->GetNativeWindow().GetNativeNSWindow(), true /* keyDown */,
        key_code, 0);
  }

  // Generate an autoreleased KeyDown NSEvent* using the given keycode, and
  // representing the first unicode character of |chars|.
  NSEvent* UnicodeKeyDown(int key_code, NSString* chars) {
    return cocoa_test_event_utils::KeyEventWithKeyCode(
        key_code, [chars characterAtIndex:0], NSEventTypeKeyDown, 0);
  }

  // Overridden from testing::Test:
  void SetUp() override {
    ui::CocoaTest::SetUp();

    Widget::InitParams init_params(ownership_);
    init_params.native_widget = native_widget_mac_.get();
    init_params.type = type_;
    init_params.opacity = opacity_;
    init_params.bounds = bounds_;
    init_params.shadow_type = shadow_type_;

    if (native_widget_mac_)
      native_widget_mac_->GetWidget()->Init(std::move(init_params));
  }

  void TearDown() override {
    // ui::CocoaTest::TearDown will wait until all NSWindows are destroyed, so
    // be sure to destroy the widget (which will destroy its NSWindow)
    // beforehand.
    native_widget_mac_ = nullptr;
    if (widget_) {
      observation_.Reset();
      widget_->CloseNow();
    }
    ui::CocoaTest::TearDown();
  }

  NSWindow* bridge_window() const {
    if (auto* bridge = native_widget_mac_->GetInProcessNSWindowBridge())
      return bridge->ns_window();
    return nil;
  }

  bool BridgeWindowHasShadow() {
    return [base::apple::ObjCCast<NativeWidgetMacNSWindowForTesting>(
        bridge_window()) hasShadowForTesting];
  }

  // Overridden from WidgetObserver:
  void OnWidgetDestroyed(Widget* widget) override {
    native_widget_mac_ = nullptr;
    if (observation_.IsObservingSource(widget)) {
      observation_.Reset();
    }
  }

 protected:
  std::unique_ptr<Widget> widget_;
  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
  raw_ptr<MockNativeWidgetMac> native_widget_mac_;  // Owned by `widget_`.

  // Use a frameless window, otherwise Widget will try to center the window
  // before the tests covering the Init() flow are ready to do that.
  Widget::InitParams::Type type_ = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  // To control the lifetime without an actual window that must be closed,
  // tests in this file use CLIENT_OWNS_WIDGET.
  Widget::InitParams::Ownership ownership_ =
      Widget::InitParams::CLIENT_OWNS_WIDGET;
  // Opacity defaults to "infer" which is usually updated by ViewsDelegate.
  Widget::InitParams::WindowOpacity opacity_ =
      Widget::InitParams::WindowOpacity::kOpaque;
  gfx::Rect bounds_ = gfx::Rect(100, 100, 100, 100);
  Widget::InitParams::ShadowType shadow_type_ =
      Widget::InitParams::ShadowType::kDefault;

 private:
  TestViewsDelegate test_views_delegate_;

  display::ScopedNativeScreen screen_;
};

class BridgedNativeWidgetTest : public BridgedNativeWidgetTestBase,
                                public TextfieldController {
 public:
  using HandleKeyEventCallback =
      base::RepeatingCallback<bool(Textfield*, const ui::KeyEvent& key_event)>;

  BridgedNativeWidgetTest();

  BridgedNativeWidgetTest(const BridgedNativeWidgetTest&) = delete;
  BridgedNativeWidgetTest& operator=(const BridgedNativeWidgetTest&) = delete;

  ~BridgedNativeWidgetTest() override;

  // Install a textfield with input type |text_input_type| in the view hierarchy
  // and make it the text input client. Also initializes |dummy_text_view_|.
  Textfield* InstallTextField(
      const std::u16string& text,
      ui::TextInputType text_input_type = ui::TEXT_INPUT_TYPE_TEXT);
  Textfield* InstallTextField(const std::string& text);

  // Returns the actual current text for |ns_view_|, or the selected substring.
  NSString* GetActualText();
  NSString* GetActualSelectedText();

  // Returns the expected current text from |dummy_text_view_|, or the selected
  // substring.
  NSString* GetExpectedText();
  NSString* GetExpectedSelectedText();

  // Returns the actual selection range for |ns_view_|.
  NSRange GetActualSelectionRange();

  // Returns the expected selection range from |dummy_text_view_|.
  NSRange GetExpectedSelectionRange();

  // Set the selection range for the installed textfield and |dummy_text_view_|.
  void SetSelectionRange(NSRange range);

  // Perform command |sel| on |ns_view_| and |dummy_text_view_|.
  void PerformCommand(SEL sel);

  // Make selection from |start| to |end| on installed views::Textfield and
  // |dummy_text_view_|. If |start| > |end|, extend selection to left from
  // |start|.
  void MakeSelection(int start, int end);

  // Helper method to set the private |keyDownEvent_| field on |ns_view_|.
  void SetKeyDownEvent(NSEvent* event);

  // Sets a callback to run on the next HandleKeyEvent().
  void SetHandleKeyEventCallback(HandleKeyEventCallback callback);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // TextfieldController:
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 protected:
  // Test delete to beginning of line or paragraph based on |sel|. |sel| can be
  // either deleteToBeginningOfLine: or deleteToBeginningOfParagraph:.
  void TestDeleteBeginning(SEL sel);

  // Test delete to end of line or paragraph based on |sel|. |sel| can be
  // either deleteToEndOfLine: or deleteToEndOfParagraph:.
  void TestDeleteEnd(SEL sel);

  // Test editing commands in |selectors| against the expectations set by
  // |dummy_text_view_|. This is done by selecting every substring within a set
  // of test strings (both RTL and non-RTL by default) and performing every
  // selector on both the NSTextView and the BridgedContentView hosting a
  // focused views::TextField to ensure the resulting text and selection ranges
  // match. |selectors| is an NSArray of NSStrings. |cases| determines whether
  // RTL strings are to be tested.
  void TestEditingCommands(NSArray* selectors);

  std::unique_ptr<views::View> view_;

  // Owned by bridge().
  BridgedContentView* __weak ns_view_;

  // An NSTextView which helps set the expectations for our tests.
  NSTextView* __strong dummy_text_view_;

  HandleKeyEventCallback handle_key_event_callback_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

// Class that counts occurrences of a VKEY_RETURN accelerator, marking them
// processed.
class EnterAcceleratorView : public View {
  METADATA_HEADER(EnterAcceleratorView, View)

 public:
  EnterAcceleratorView() { AddAccelerator({ui::VKEY_RETURN, 0}); }
  int count() const { return count_; }

  // View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    ++count_;
    return true;
  }

 private:
  int count_ = 0;
};

BEGIN_METADATA(EnterAcceleratorView)
END_METADATA

BridgedNativeWidgetTest::BridgedNativeWidgetTest() = default;

BridgedNativeWidgetTest::~BridgedNativeWidgetTest() = default;

Textfield* BridgedNativeWidgetTest::InstallTextField(
    const std::u16string& text,
    ui::TextInputType text_input_type) {
  Textfield* textfield = new Textfield();
  textfield->SetText(text);
  textfield->SetTextInputType(text_input_type);
  textfield->set_controller(this);
  view_->RemoveAllChildViews();
  view_->AddChildView(textfield);
  textfield->SetBoundsRect(bounds_);

  // Request focus so the InputMethod can dispatch events to the RootView, and
  // have them delivered to the textfield. Note that focusing a textfield
  // schedules a task to flash the cursor, so this requires |message_loop_|.
  textfield->RequestFocus();

  GetNSWindowHost()->text_input_host()->SetTextInputClient(textfield);

  // Initialize the dummy text view. Initializing this with NSZeroRect causes
  // weird NSTextView behavior on OSX 10.9.
  dummy_text_view_ =
      [[DummyTextView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
  [dummy_text_view_ setString:SysUTF16ToNSString(text)];
  return textfield;
}

Textfield* BridgedNativeWidgetTest::InstallTextField(const std::string& text) {
  return InstallTextField(base::ASCIIToUTF16(text));
}

NSString* BridgedNativeWidgetTest::GetActualText() {
  return GetViewStringForRange(ns_view_, EmptyRange());
}

NSString* BridgedNativeWidgetTest::GetActualSelectedText() {
  return GetViewStringForRange(ns_view_, [ns_view_ selectedRange]);
}

NSString* BridgedNativeWidgetTest::GetExpectedText() {
  return GetViewStringForRange(dummy_text_view_, EmptyRange());
}

NSString* BridgedNativeWidgetTest::GetExpectedSelectedText() {
  return GetViewStringForRange(dummy_text_view_,
                               [dummy_text_view_ selectedRange]);
}

NSRange BridgedNativeWidgetTest::GetActualSelectionRange() {
  return [ns_view_ selectedRange];
}

NSRange BridgedNativeWidgetTest::GetExpectedSelectionRange() {
  return [dummy_text_view_ selectedRange];
}

void BridgedNativeWidgetTest::SetSelectionRange(NSRange range) {
  ui::TextInputClient* client = [ns_view_ textInputClientForTesting];
  client->SetEditableSelectionRange(gfx::Range(range));

  [dummy_text_view_ setSelectedRange:range];
}

void BridgedNativeWidgetTest::PerformCommand(SEL sel) {
  [ns_view_ doCommandBySelector:sel];
  [dummy_text_view_ doCommandBySelector:sel];
}

void BridgedNativeWidgetTest::MakeSelection(int start, int end) {
  ui::TextInputClient* client = [ns_view_ textInputClientForTesting];
  const gfx::Range range(start, end);

  // Although a gfx::Range is directed, the underlying model will not choose an
  // affinity until the cursor is moved.
  client->SetEditableSelectionRange(range);

  // Set the range without an affinity. The first @selector sent to the text
  // field determines the affinity. Note that Range::ToNSRange() may discard
  // the direction since NSRange has no direction.
  [dummy_text_view_ setSelectedRange:range.ToNSRange()];
}

void BridgedNativeWidgetTest::SetKeyDownEvent(NSEvent* event) {
  ns_view_.keyDownEventForTesting = event;
}

void BridgedNativeWidgetTest::SetHandleKeyEventCallback(
    HandleKeyEventCallback callback) {
  handle_key_event_callback_ = std::move(callback);
}

void BridgedNativeWidgetTest::SetUp() {
  BridgedNativeWidgetTestBase::SetUp();

  view_ = std::make_unique<views::internal::RootView>(widget_.get());
  NSWindow* window = bridge_window();

  // The delegate should exist before setting the root view.
  EXPECT_TRUE([window delegate]);
  GetNSWindowHost()->SetRootView(view_.get());
  bridge()->CreateContentView(GetNSWindowHost()->GetRootViewNSViewId(),
                              view_->bounds());
  ns_view_ = bridge()->ns_view();

  // Pretend it has been shown via NativeWidgetMac::Show().
  [window orderFront:nil];
  [window makeFirstResponder:bridge()->ns_view()];
}

void BridgedNativeWidgetTest::TearDown() {
  // Clear kill buffer so that no state persists between tests.
  TextfieldModel::ClearKillBuffer();

  if (GetNSWindowHost()) {
    GetNSWindowHost()->SetRootView(nullptr);
    bridge()->DestroyContentView();
  }
  view_.reset();
  BridgedNativeWidgetTestBase::TearDown();
}

bool BridgedNativeWidgetTest::HandleKeyEvent(Textfield* sender,
                                             const ui::KeyEvent& key_event) {
  if (handle_key_event_callback_)
    return handle_key_event_callback_.Run(sender, key_event);
  return false;
}

void BridgedNativeWidgetTest::TestDeleteBeginning(SEL sel) {
  InstallTextField("foo bar baz");
  EXPECT_EQ_RANGE_3(NSMakeRange(11, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move the caret to the beginning of the line.
  SetSelectionRange(NSMakeRange(0, 0));
  // Verify no deletion takes place.
  PerformCommand(sel);
  EXPECT_NSEQ_3(@"foo bar baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move the caret as- "foo| bar baz".
  SetSelectionRange(NSMakeRange(3, 0));
  PerformCommand(sel);
  // Verify state is "| bar baz".
  EXPECT_NSEQ_3(@" bar baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Make a selection as- " bar |baz|".
  SetSelectionRange(NSMakeRange(5, 3));
  PerformCommand(sel);
  // Verify only the selection is deleted so that the state is " bar |".
  EXPECT_NSEQ_3(@" bar ", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(5, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Verify yanking inserts the deleted text.
  PerformCommand(@selector(yank:));
  EXPECT_NSEQ_3(@" bar baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(8, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

void BridgedNativeWidgetTest::TestDeleteEnd(SEL sel) {
  InstallTextField("foo bar baz");
  EXPECT_EQ_RANGE_3(NSMakeRange(11, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Caret is at the end of the line. Verify no deletion takes place.
  PerformCommand(sel);
  EXPECT_NSEQ_3(@"foo bar baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(11, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move the caret as- "foo bar| baz".
  SetSelectionRange(NSMakeRange(7, 0));
  PerformCommand(sel);
  // Verify state is "foo bar|".
  EXPECT_NSEQ_3(@"foo bar", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(7, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Make a selection as- "|foo |bar".
  SetSelectionRange(NSMakeRange(0, 4));
  PerformCommand(sel);
  // Verify only the selection is deleted so that the state is "|bar".
  EXPECT_NSEQ_3(@"bar", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Verify yanking inserts the deleted text.
  PerformCommand(@selector(yank:));
  EXPECT_NSEQ_3(@"foo bar", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(4, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

void BridgedNativeWidgetTest::TestEditingCommands(NSArray* selectors) {
  struct {
    std::u16string test_string;
    bool is_rtl;
  } test_cases[] = {
      {u"ab c", false},
      {u"\x0634\x0632 \x064A", true},
  };

  for (const auto& test_case : test_cases) {
    for (NSString* selector_string in selectors) {
      SEL sel = NSSelectorFromString(selector_string);
      const int len = test_case.test_string.length();
      for (int i = 0; i <= len; i++) {
        for (int j = 0; j <= len; j++) {
          SCOPED_TRACE(base::StringPrintf(
              "Testing range [%d-%d] for case %s and selector %s\n", i, j,
              base::UTF16ToUTF8(test_case.test_string).c_str(),
              base::SysNSStringToUTF8(selector_string).c_str()));

          InstallTextField(test_case.test_string);
          MakeSelection(i, j);

          // Sanity checks for MakeSelection().
          EXPECT_NSEQ(GetExpectedSelectedText(), GetActualSelectedText());
          EXPECT_EQ_RANGE_3(NSMakeRange(std::min(i, j), std::abs(i - j)),
                            GetExpectedSelectionRange(),
                            GetActualSelectionRange());

          // Bail out early for selection-modifying commands that are buggy in
          // Cocoa, since the selected text will not match.
          if (test_case.is_rtl && i != j && IsRTLSelectBuggy(sel))
            continue;

          PerformCommand(sel);
          EXPECT_NSEQ(GetExpectedSelectedText(), GetActualSelectedText());
          EXPECT_NSEQ(GetExpectedText(), GetActualText());

          // Spot-check some Cocoa RTL bugs. These only manifest when there is a
          // selection (i != j), not for regular cursor moves.
          if (test_case.is_rtl && i != j && IsRTLMoveBuggy(sel)) {
            if (sel == @selector(moveRight:)) {
              // Surely moving right with an rtl string moves to the start of a
              // range (i.e. min). But Cocoa moves to the end.
              EXPECT_EQ_RANGE(NSMakeRange(std::max(i, j), 0),
                              GetExpectedSelectionRange());
              EXPECT_EQ_RANGE(NSMakeRange(std::min(i, j), 0),
                              GetActualSelectionRange());
            } else if (sel == @selector(moveLeft:)) {
              EXPECT_EQ_RANGE(NSMakeRange(std::min(i, j), 0),
                              GetExpectedSelectionRange());
              EXPECT_EQ_RANGE(NSMakeRange(std::max(i, j), 0),
                              GetActualSelectionRange());
            }
            continue;
          }

          EXPECT_EQ_RANGE(GetExpectedSelectionRange(),
                          GetActualSelectionRange());
        }
      }
    }
  }
}

// The TEST_VIEW macro expects the view it's testing to have a superview. In
// these tests, the NSView bridge is a contentView, at the root. These mimic
// what TEST_VIEW usually does.
TEST_F(BridgedNativeWidgetTest, BridgedNativeWidgetTest_TestViewAddRemove) {
  BridgedContentView* view = bridge()->ns_view();
  NSWindow* window = bridge_window();
  EXPECT_NSEQ([window contentView], view);
  EXPECT_NSEQ(window, [view window]);

  // The superview of a contentView is an NSNextStepFrame.
  EXPECT_TRUE([view superview]);
  EXPECT_TRUE([view bridge]);

  // Ensure the tracking area to propagate mouseMoved: events to the RootView is
  // installed.
  EXPECT_EQ(1u, [[view trackingAreas] count]);

  // Closing the window should tear down the C++ bridge, remove references to
  // any C++ objects in the ObjectiveC object, and remove it from the hierarchy.
  [window close];
  EXPECT_FALSE([view bridge]);
  EXPECT_FALSE([view superview]);
  EXPECT_FALSE([view window]);
  EXPECT_EQ(0u, [[view trackingAreas] count]);
  EXPECT_FALSE([window contentView]);
  EXPECT_FALSE([window delegate]);
}

TEST_F(BridgedNativeWidgetTest, BridgedNativeWidgetTest_TestViewDisplay) {
  [bridge()->ns_view() display];
}

// Test that resizing the window resizes the root view appropriately.
TEST_F(BridgedNativeWidgetTest, ViewSizeTracksWindow) {
  const int kTestNewWidth = 400;
  const int kTestNewHeight = 300;

  // |bridge_window()| is borderless, so these should align.
  NSSize window_size = [bridge_window() frame].size;
  EXPECT_EQ(view_->width(), static_cast<int>(window_size.width));
  EXPECT_EQ(view_->height(), static_cast<int>(window_size.height));

  // Make sure a resize actually occurs.
  EXPECT_NE(kTestNewWidth, view_->width());
  EXPECT_NE(kTestNewHeight, view_->height());

  [bridge_window() setFrame:NSMakeRect(0, 0, kTestNewWidth, kTestNewHeight)
                    display:NO];
  EXPECT_EQ(kTestNewWidth, view_->width());
  EXPECT_EQ(kTestNewHeight, view_->height());
}

TEST_F(BridgedNativeWidgetTest, GetInputMethodShouldNotReturnNull) {
  EXPECT_TRUE(native_widget_mac_->GetInputMethod());
}

// A simpler test harness for testing initialization flows.
class BridgedNativeWidgetInitTest : public BridgedNativeWidgetTestBase {
 public:
  BridgedNativeWidgetInitTest()
      : BridgedNativeWidgetTestBase(SkipInitialization()) {}

  BridgedNativeWidgetInitTest(const BridgedNativeWidgetInitTest&) = delete;
  BridgedNativeWidgetInitTest& operator=(const BridgedNativeWidgetInitTest&) =
      delete;

  // Prepares a new |window_| and |widget_| for a call to PerformInit().
  void CreateNewWidgetToInit() {
    native_widget_mac_ = nullptr;
    if (widget_) {
      observation_.Reset();
      widget_->CloseNow();
    }
    widget_ = std::make_unique<Widget>();
    observation_.Observe(widget_.get());
    native_widget_mac_ = new MockNativeWidgetMac(widget_.get());
  }

  void PerformInit() {
    Widget::InitParams init_params(ownership_);
    init_params.native_widget = native_widget_mac_.get();
    init_params.type = type_;
    init_params.opacity = opacity_;
    init_params.bounds = bounds_;
    init_params.shadow_type = shadow_type_;
    widget_->Init(std::move(init_params));
  }
};

// Test that NativeWidgetNSWindowBridge remains sane if Init() is never called.
TEST_F(BridgedNativeWidgetInitTest, InitNotCalled) {
  // Don't use a Widget* as the delegate. ~Widget() checks for Widget::
  // |native_widget_destroyed_| being set to true. That can only happen with a
  // non-null WidgetDelegate, which is only set in Widget::Init(). Then, since
  // neither Widget nor NativeWidget take ownership, use a unique_ptr.
  std::unique_ptr<MockNativeWidgetMac> native_widget(
      new MockNativeWidgetMac(nullptr));
  native_widget_mac_ = native_widget.get();
  EXPECT_FALSE(bridge());
  EXPECT_FALSE(GetNSWindowHost()->GetInProcessNSWindow());
  native_widget_mac_ = nullptr;
}

// Tests the shadow type given in InitParams.
TEST_F(BridgedNativeWidgetInitTest, ShadowType) {
  // Verify Widget::InitParam defaults and arguments added from SetUp().
  EXPECT_EQ(Widget::InitParams::TYPE_WINDOW_FRAMELESS, type_);
  EXPECT_EQ(Widget::InitParams::WindowOpacity::kOpaque, opacity_);
  EXPECT_EQ(Widget::InitParams::ShadowType::kDefault, shadow_type_);

  CreateNewWidgetToInit();
  EXPECT_FALSE(
      BridgeWindowHasShadow());  // Default for NSWindowStyleMaskBorderless.
  PerformInit();

  // Borderless is 0, so isn't really a mask. Check that nothing is set.
  EXPECT_EQ(NSWindowStyleMaskBorderless, [bridge_window() styleMask]);
  EXPECT_TRUE(BridgeWindowHasShadow());  // ShadowType::kDefault means a shadow.

  CreateNewWidgetToInit();
  shadow_type_ = Widget::InitParams::ShadowType::kNone;
  PerformInit();
  EXPECT_FALSE(BridgeWindowHasShadow());  // Preserves lack of shadow.

  // Default for Widget::InitParams::TYPE_WINDOW.
  CreateNewWidgetToInit();
  PerformInit();
  EXPECT_FALSE(BridgeWindowHasShadow());  // ShadowType::kNone removes shadow.

  shadow_type_ = Widget::InitParams::ShadowType::kDefault;
  CreateNewWidgetToInit();
  PerformInit();
  EXPECT_TRUE(BridgeWindowHasShadow());  // Preserves shadow.
}

// Ensure a nil NSTextInputContext is returned when the ui::TextInputClient is
// not editable, a password field, or unset.
TEST_F(BridgedNativeWidgetTest, InputContext) {
  const std::u16string test_string = u"test_str";
  InstallTextField(test_string, ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_FALSE([ns_view_ inputContext]);
  InstallTextField(test_string, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_TRUE([ns_view_ inputContext]);
  GetNSWindowHost()->text_input_host()->SetTextInputClient(nullptr);
  EXPECT_FALSE([ns_view_ inputContext]);
  InstallTextField(test_string, ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_FALSE([ns_view_ inputContext]);
}

// Test getting complete string using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetCompleteString) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  NSRange range = NSMakeRange(0, test_string.size());
  NSRange actual_range;

  NSAttributedString* actual_text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];

  NSRange expected_range;
  NSAttributedString* expected_text =
      [dummy_text_view_ attributedSubstringForProposedRange:range
                                                actualRange:&expected_range];

  EXPECT_NSEQ_3(@"foo bar baz", [expected_text string], [actual_text string]);
  EXPECT_EQ_RANGE_3(range, expected_range, actual_range);
}

// Test getting middle substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetMiddleSubstring) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  NSRange range = NSMakeRange(4, 3);
  NSRange actual_range;
  NSAttributedString* actual_text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];

  NSRange expected_range;
  NSAttributedString* expected_text =
      [dummy_text_view_ attributedSubstringForProposedRange:range
                                                actualRange:&expected_range];

  EXPECT_NSEQ_3(@"bar", [expected_text string], [actual_text string]);
  EXPECT_EQ_RANGE_3(range, expected_range, actual_range);
}

// Test getting ending substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetEndingSubstring) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  NSRange range = NSMakeRange(8, 100);
  NSRange actual_range;
  NSAttributedString* actual_text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  NSRange expected_range;
  NSAttributedString* expected_text =
      [dummy_text_view_ attributedSubstringForProposedRange:range
                                                actualRange:&expected_range];

  EXPECT_NSEQ_3(@"baz", [expected_text string], [actual_text string]);
  EXPECT_EQ_RANGE_3(NSMakeRange(8, 3), expected_range, actual_range);
}

// Test getting empty substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetEmptySubstring) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  // Try with EmptyRange(). This behaves specially and should return the
  // complete string and the corresponding text range.
  NSRange range = EmptyRange();
  NSRange actual_range = EmptyRange();
  NSRange expected_range = EmptyRange();
  NSAttributedString* actual_text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  NSAttributedString* expected_text =
      [dummy_text_view_ attributedSubstringForProposedRange:range
                                                actualRange:&expected_range];
  EXPECT_NSEQ_3(@"foo bar baz", [expected_text string], [actual_text string]);
  EXPECT_EQ_RANGE_3(NSMakeRange(0, test_string.length()), expected_range,
                    actual_range);

  // Try with a valid empty range.
  range = NSMakeRange(2, 0);
  actual_range = EmptyRange();
  expected_range = EmptyRange();
  actual_text = [ns_view_ attributedSubstringForProposedRange:range
                                                  actualRange:&actual_range];
  expected_text =
      [dummy_text_view_ attributedSubstringForProposedRange:range
                                                actualRange:&expected_range];
  EXPECT_NSEQ_3(nil, [expected_text string], [actual_text string]);
  EXPECT_EQ_RANGE_3(range, expected_range, actual_range);

  // Try with an out of bounds empty range.
  range = NSMakeRange(20, 0);
  actual_range = EmptyRange();
  expected_range = EmptyRange();
  actual_text = [ns_view_ attributedSubstringForProposedRange:range
                                                  actualRange:&actual_range];
  expected_text =
      [dummy_text_view_ attributedSubstringForProposedRange:range
                                                actualRange:&expected_range];

  EXPECT_NSEQ_3(nil, [expected_text string], [actual_text string]);
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), expected_range, actual_range);
}

// Test inserting text using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_InsertText) {
  const std::string test_string = "foo";
  InstallTextField(test_string);

  [ns_view_ insertText:SysUTF8ToNSString(test_string)
      replacementRange:EmptyRange()];
  [dummy_text_view_ insertText:SysUTF8ToNSString(test_string)
              replacementRange:EmptyRange()];

  EXPECT_NSEQ_3(@"foofoo", GetExpectedText(), GetActualText());
}

// Test replacing text using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_ReplaceText) {
  const std::string test_string = "foo bar";
  InstallTextField(test_string);

  [ns_view_ insertText:@"baz" replacementRange:NSMakeRange(4, 3)];
  [dummy_text_view_ insertText:@"baz" replacementRange:NSMakeRange(4, 3)];

  EXPECT_NSEQ_3(@"foo baz", GetExpectedText(), GetActualText());
}

// Test IME composition using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_Compose) {
  const std::string test_string = "foo ";
  InstallTextField(test_string);

  EXPECT_EQ_3(NO, [dummy_text_view_ hasMarkedText], [ns_view_ hasMarkedText]);

  // As per NSTextInputClient documentation, markedRange should return
  // {NSNotFound, 0} iff hasMarked returns false. However, NSTextView returns
  // {text_length, text_length} for this case. We maintain consistency with the
  // documentation, hence the EXPECT_FALSE check.
  EXPECT_FALSE(
      NSEqualRanges([dummy_text_view_ markedRange], [ns_view_ markedRange]));
  EXPECT_EQ_RANGE(EmptyRange(), [ns_view_ markedRange]);

  // Start composition.
  NSString* compositionText = @"bar";
  NSUInteger compositionLength = [compositionText length];
  [ns_view_ setMarkedText:compositionText
            selectedRange:NSMakeRange(0, 2)
         replacementRange:EmptyRange()];
  [dummy_text_view_ setMarkedText:compositionText
                    selectedRange:NSMakeRange(0, 2)
                 replacementRange:EmptyRange()];
  EXPECT_EQ_3(YES, [dummy_text_view_ hasMarkedText], [ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE_3(NSMakeRange(test_string.size(), compositionLength),
                    [dummy_text_view_ markedRange], [ns_view_ markedRange]);
  EXPECT_EQ_RANGE_3(NSMakeRange(test_string.size(), 2),
                    GetExpectedSelectionRange(), GetActualSelectionRange());

  // Confirm composition.
  [ns_view_ unmarkText];
  [dummy_text_view_ unmarkText];

  EXPECT_EQ_3(NO, [dummy_text_view_ hasMarkedText], [ns_view_ hasMarkedText]);
  EXPECT_FALSE(
      NSEqualRanges([dummy_text_view_ markedRange], [ns_view_ markedRange]));
  EXPECT_EQ_RANGE(EmptyRange(), [ns_view_ markedRange]);
  EXPECT_NSEQ_3(@"foo bar", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange([GetActualText() length], 0),
                    GetExpectedSelectionRange(), GetActualSelectionRange());
}

// Test IME composition for accented characters.
TEST_F(BridgedNativeWidgetTest, TextInput_AccentedCharacter) {
  InstallTextField("abc");

  // Simulate action messages generated when the key 'a' is pressed repeatedly
  // and leads to the showing of an IME candidate window. To simulate an event,
  // set the private keyDownEvent field on the BridgedContentView.

  // First an insertText: message with key 'a' is generated.
  SetKeyDownEvent(cocoa_test_event_utils::SynthesizeKeyEvent(
      widget_->GetNativeWindow().GetNativeNSWindow(), true, ui::VKEY_A, 0));
  [ns_view_ insertText:@"a" replacementRange:EmptyRange()];
  [dummy_text_view_ insertText:@"a" replacementRange:EmptyRange()];
  SetKeyDownEvent(nil);
  EXPECT_EQ_3(NO, [dummy_text_view_ hasMarkedText], [ns_view_ hasMarkedText]);
  EXPECT_NSEQ_3(@"abca", GetExpectedText(), GetActualText());

  // Next the IME popup appears. On selecting the accented character using arrow
  // keys, setMarkedText action message is generated which replaces the earlier
  // inserted 'a'.
  SetKeyDownEvent(cocoa_test_event_utils::SynthesizeKeyEvent(
      widget_->GetNativeWindow().GetNativeNSWindow(), true, ui::VKEY_RIGHT, 0));
  [ns_view_ setMarkedText:@"à"
            selectedRange:NSMakeRange(0, 1)
         replacementRange:NSMakeRange(3, 1)];
  [dummy_text_view_ setMarkedText:@"à"
                    selectedRange:NSMakeRange(0, 1)
                 replacementRange:NSMakeRange(3, 1)];
  SetKeyDownEvent(nil);
  EXPECT_EQ_3(YES, [dummy_text_view_ hasMarkedText], [ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE_3(NSMakeRange(3, 1), [dummy_text_view_ markedRange],
                    [ns_view_ markedRange]);
  EXPECT_EQ_RANGE_3(NSMakeRange(3, 1), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
  EXPECT_NSEQ_3(@"abcà", GetExpectedText(), GetActualText());

  // On pressing enter, the marked text is confirmed.
  SetKeyDownEvent(cocoa_test_event_utils::SynthesizeKeyEvent(
      widget_->GetNativeWindow().GetNativeNSWindow(), true, ui::VKEY_RETURN,
      0));
  [ns_view_ insertText:@"à" replacementRange:EmptyRange()];
  [dummy_text_view_ insertText:@"à" replacementRange:EmptyRange()];
  SetKeyDownEvent(nil);
  EXPECT_EQ_3(NO, [dummy_text_view_ hasMarkedText], [ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE_3(NSMakeRange(4, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
  EXPECT_NSEQ_3(@"abcà", GetExpectedText(), GetActualText());
}

// Test moving the caret left and right using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_MoveLeftRight) {
  InstallTextField("foo");
  EXPECT_EQ_RANGE_3(NSMakeRange(3, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move right not allowed, out of range.
  PerformCommand(@selector(moveRight:));
  EXPECT_EQ_RANGE_3(NSMakeRange(3, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move left.
  PerformCommand(@selector(moveLeft:));
  EXPECT_EQ_RANGE_3(NSMakeRange(2, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move right.
  PerformCommand(@selector(moveRight:));
  EXPECT_EQ_RANGE_3(NSMakeRange(3, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

// Test backward delete using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteBackward) {
  InstallTextField("a");
  EXPECT_EQ_RANGE_3(NSMakeRange(1, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Delete one character.
  PerformCommand(@selector(deleteBackward:));
  EXPECT_NSEQ_3(nil, GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Verify that deletion did not modify the kill buffer.
  PerformCommand(@selector(yank:));
  EXPECT_NSEQ_3(nil, GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Try to delete again on an empty string.
  PerformCommand(@selector(deleteBackward:));
  EXPECT_NSEQ_3(nil, GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

// Test forward delete using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteForward) {
  InstallTextField("a");
  EXPECT_EQ_RANGE_3(NSMakeRange(1, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // At the end of the string, can't delete forward.
  PerformCommand(@selector(deleteForward:));
  EXPECT_NSEQ_3(@"a", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(1, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Should succeed after moving left first.
  PerformCommand(@selector(moveLeft:));
  PerformCommand(@selector(deleteForward:));
  EXPECT_NSEQ_3(nil, GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Verify that deletion did not modify the kill buffer.
  PerformCommand(@selector(yank:));
  EXPECT_NSEQ_3(nil, GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

// Test forward word deletion using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteWordForward) {
  InstallTextField("foo bar baz");
  EXPECT_EQ_RANGE_3(NSMakeRange(11, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Caret is at the end of the line. Verify no deletion takes place.
  PerformCommand(@selector(deleteWordForward:));
  EXPECT_NSEQ_3(@"foo bar baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(11, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move the caret as- "foo b|ar baz".
  SetSelectionRange(NSMakeRange(5, 0));
  PerformCommand(@selector(deleteWordForward:));
  // Verify state is "foo b| baz"
  EXPECT_NSEQ_3(@"foo b baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(5, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Make a selection as- "|fo|o b baz".
  SetSelectionRange(NSMakeRange(0, 2));
  PerformCommand(@selector(deleteWordForward:));
  // Verify only the selection is deleted and state is "|o b baz".
  EXPECT_NSEQ_3(@"o b baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Verify that deletion did not modify the kill buffer.
  PerformCommand(@selector(yank:));
  EXPECT_NSEQ_3(@"o b baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

// Test backward word deletion using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteWordBackward) {
  InstallTextField("foo bar baz");
  EXPECT_EQ_RANGE_3(NSMakeRange(11, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move the caret to the beginning of the line.
  SetSelectionRange(NSMakeRange(0, 0));
  // Verify no deletion takes place.
  PerformCommand(@selector(deleteWordBackward:));
  EXPECT_NSEQ_3(@"foo bar baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(0, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Move the caret as- "foo ba|r baz".
  SetSelectionRange(NSMakeRange(6, 0));
  PerformCommand(@selector(deleteWordBackward:));
  // Verify state is "foo |r baz".
  EXPECT_NSEQ_3(@"foo r baz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(4, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Make a selection as- "f|oo r b|az".
  SetSelectionRange(NSMakeRange(1, 6));
  PerformCommand(@selector(deleteWordBackward:));
  // Verify only the selection is deleted and state is "f|az"
  EXPECT_NSEQ_3(@"faz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(1, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());

  // Verify that deletion did not modify the kill buffer.
  PerformCommand(@selector(yank:));
  EXPECT_NSEQ_3(@"faz", GetExpectedText(), GetActualText());
  EXPECT_EQ_RANGE_3(NSMakeRange(1, 0), GetExpectedSelectionRange(),
                    GetActualSelectionRange());
}

// Test deleting to beginning/end of line/paragraph using text input protocol.

TEST_F(BridgedNativeWidgetTest, TextInput_DeleteToBeginningOfLine) {
  TestDeleteBeginning(@selector(deleteToBeginningOfLine:));
}

TEST_F(BridgedNativeWidgetTest, TextInput_DeleteToEndOfLine) {
  TestDeleteEnd(@selector(deleteToEndOfLine:));
}

TEST_F(BridgedNativeWidgetTest, TextInput_DeleteToBeginningOfParagraph) {
  TestDeleteBeginning(@selector(deleteToBeginningOfParagraph:));
}

TEST_F(BridgedNativeWidgetTest, TextInput_DeleteToEndOfParagraph) {
  TestDeleteEnd(@selector(deleteToEndOfParagraph:));
}

// Test move commands against expectations set by |dummy_text_view_|.
TEST_F(BridgedNativeWidgetTest, TextInput_MoveEditingCommands) {
  TestEditingCommands(kMoveActions);
}

// Test move and select commands against expectations set by |dummy_text_view_|.
TEST_F(BridgedNativeWidgetTest, TextInput_MoveAndSelectEditingCommands) {
  TestEditingCommands(kSelectActions);
}

// Test delete commands against expectations set by |dummy_text_view_|.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteCommands) {
  TestEditingCommands(kDeleteActions);
}

// Test that we don't crash during an action message even if the TextInputClient
// is nil. Regression test for crbug.com/615745.
TEST_F(BridgedNativeWidgetTest, NilTextInputClient) {
  GetNSWindowHost()->text_input_host()->SetTextInputClient(nullptr);
  NSMutableArray* selectors = [NSMutableArray array];
  [selectors addObjectsFromArray:kMoveActions];
  [selectors addObjectsFromArray:kSelectActions];
  [selectors addObjectsFromArray:kDeleteActions];

  // -insertText: is omitted from this list to avoid a DCHECK in
  // doCommandBySelector:. AppKit never passes -insertText: to
  // doCommandBySelector: (it calls -insertText: directly instead).
  [selectors addObjectsFromArray:kMiscActions];

  for (NSString* selector in selectors)
    [ns_view_ doCommandBySelector:NSSelectorFromString(selector)];

  [ns_view_ insertText:@""];
}

// Test transpose command against expectations set by |dummy_text_view_|.
TEST_F(BridgedNativeWidgetTest, TextInput_Transpose) {
  TestEditingCommands(@[ @"transpose:" ]);
}

// Test firstRectForCharacterRange:actualRange for cases where query range is
// empty or outside composition range.
TEST_F(BridgedNativeWidgetTest, TextInput_FirstRectForCharacterRange_Caret) {
  InstallTextField("");
  ui::TextInputClient* client = [ns_view_ textInputClientForTesting];

  // No composition. Ensure bounds and range corresponding to the current caret
  // position are returned.
  // Initially selection range will be [0, 0].
  NSRange caret_range = NSMakeRange(0, 0);
  NSRange query_range = NSMakeRange(1, 1);
  NSRange actual_range;
  NSRect rect = [ns_view_ firstRectForCharacterRange:query_range
                                         actualRange:&actual_range];
  EXPECT_EQ(GetCaretBounds(client), gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(caret_range, actual_range);

  // Set composition with caret before second character ('e').
  const std::u16string test_string = u"test_str";
  const size_t kTextLength = 8;
  SetCompositionText(client, test_string, 1, &caret_range);

  // Test bounds returned for empty ranges within composition range. As per
  // Apple's documentation for firstRectForCharacterRange:actualRange:, for an
  // empty query range, the returned rectangle should coincide with the
  // insertion point and have zero width. However in our implementation, if the
  // empty query range lies within the composition range, we return a zero width
  // rectangle corresponding to the query range location.

  // Test bounds returned for empty range before second character ('e') are same
  // as caret bounds when placed before second character.
  query_range = NSMakeRange(1, 0);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_EQ(GetCaretBoundsForPosition(client, test_string, 1, &caret_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);

  // Test bounds returned for empty range after the composition text are same as
  // caret bounds when placed after the composition text.
  query_range = NSMakeRange(kTextLength, 0);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_NE(GetCaretBoundsForPosition(client, test_string, 1, &caret_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ(
      GetCaretBoundsForPosition(client, test_string, kTextLength, &caret_range),
      gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);

  // Query outside composition range. Ensure bounds and range corresponding to
  // the current caret position are returned.
  query_range = NSMakeRange(kTextLength + 1, 0);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_EQ(GetCaretBounds(client), gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(caret_range, actual_range);

  // Make sure not crashing by passing null pointer instead of actualRange.
  rect = [ns_view_ firstRectForCharacterRange:query_range actualRange:nullptr];
}

// Test firstRectForCharacterRange:actualRange for non-empty query ranges within
// the composition range.
TEST_F(BridgedNativeWidgetTest, TextInput_FirstRectForCharacterRange) {
  InstallTextField("");
  ui::TextInputClient* client = [ns_view_ textInputClientForTesting];

  const std::u16string test_string = u"test_str";
  const size_t kTextLength = 8;
  SetCompositionText(client, test_string, 1, nullptr);

  // Query bounds for the whole composition string.
  NSRange query_range = NSMakeRange(0, kTextLength);
  NSRange actual_range;
  NSRect rect = [ns_view_ firstRectForCharacterRange:query_range
                                         actualRange:&actual_range];
  EXPECT_EQ(GetExpectedBoundsForRange(client, test_string, query_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);

  // Query bounds for the substring "est_".
  query_range = NSMakeRange(1, 4);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_EQ(GetExpectedBoundsForRange(client, test_string, query_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);
}

// Test simulated codepaths for IMEs that do not always "mark" text. E.g.
// phonetic languages such as Korean and Vietnamese.
TEST_F(BridgedNativeWidgetTest, TextInput_SimulatePhoneticIme) {
  Textfield* textfield = InstallTextField("");
  EXPECT_TRUE([ns_view_ textInputClientForTesting]);

  object_setClass(ns_view_, [InterpretKeyEventMockedBridgedContentView class]);

  // Sequence of calls (and corresponding keyDown events) obtained via tracing
  // with 2-Set Korean IME and pressing q, o, then Enter on the keyboard.
  NSEvent* q_in_ime = UnicodeKeyDown(12, @"ㅂ");
  InterpretKeyEventsCallback handle_q_in_ime = base::BindRepeating([](id view) {
    [view insertText:@"ㅂ" replacementRange:NSMakeRange(NSNotFound, 0)];
  });

  NSEvent* o_in_ime = UnicodeKeyDown(31, @"ㅐ");
  InterpretKeyEventsCallback handle_o_in_ime = base::BindRepeating([](id view) {
    [view insertText:@"배" replacementRange:NSMakeRange(0, 1)];
  });

  InterpretKeyEventsCallback handle_return_in_ime =
      base::BindRepeating([](id view) {
        // When confirming the composition, AppKit repeats itself.
        [view insertText:@"배" replacementRange:NSMakeRange(0, 1)];
        [view insertText:@"배" replacementRange:NSMakeRange(0, 1)];
        [view doCommandBySelector:@selector(insertNewLine:)];
      });

  // Add a hook for the KeyEvent being received by the TextfieldController. E.g.
  // this is where the Omnibox would start to search when Return is pressed.
  bool saw_vkey_return = false;
  SetHandleKeyEventCallback(base::BindRepeating(
      [](bool* saw_return, Textfield* textfield, const ui::KeyEvent& event) {
        if (event.key_code() == ui::VKEY_RETURN) {
          EXPECT_FALSE(*saw_return);
          *saw_return = true;
          EXPECT_EQ(base::SysNSStringToUTF16(@"배"), textfield->GetText());
        }
        return false;
      },
      &saw_vkey_return));

  EXPECT_EQ(u"", textfield->GetText());

  g_fake_interpret_key_events = &handle_q_in_ime;
  [ns_view_ keyDown:q_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"ㅂ"), textfield->GetText());
  EXPECT_FALSE(saw_vkey_return);

  g_fake_interpret_key_events = &handle_o_in_ime;
  [ns_view_ keyDown:o_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"배"), textfield->GetText());
  EXPECT_FALSE(saw_vkey_return);

  // Note the "Enter" should not replace the replacement range, even though a
  // replacement range was set.
  g_fake_interpret_key_events = &handle_return_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  EXPECT_EQ(base::SysNSStringToUTF16(@"배"), textfield->GetText());

  // VKEY_RETURN should be seen by via the unhandled key event handler (but not
  // via -insertText:.
  EXPECT_TRUE(saw_vkey_return);

  g_fake_interpret_key_events = nullptr;
}

// Test simulated codepaths for typing 'm', 'o', 'o', Enter in the Telex IME.
// This IME does not mark text, but, unlike 2-set Korean, it re-inserts the
// entire word on each keypress, even though only the last character in the word
// can be modified. This prevents the keypress being treated as a "character"
// event (which is unavoidably unfortunate for the Undo buffer), but also led to
// a codepath that suppressed a VKEY_RETURN when it should not, since there is
// no candidate IME window to dismiss for this IME.
TEST_F(BridgedNativeWidgetTest, TextInput_SimulateTelexMoo) {
  Textfield* textfield = InstallTextField("");
  EXPECT_TRUE([ns_view_ textInputClientForTesting]);

  EnterAcceleratorView* enter_view = new EnterAcceleratorView();
  textfield->parent()->AddChildView(enter_view);

  // Sequence of calls (and corresponding keyDown events) obtained via tracing
  // with Telex IME and pressing 'm', 'o', 'o', then Enter on the keyboard.
  // Note that without the leading 'm', only one character changes, which could
  // allow the keypress to be treated as a character event, which would not
  // produce the bug.
  NSEvent* m_in_ime = UnicodeKeyDown(46, @"m");
  InterpretKeyEventsCallback handle_m_in_ime = base::BindRepeating([](id view) {
    [view insertText:@"m" replacementRange:NSMakeRange(NSNotFound, 0)];
  });

  // Note that (unlike Korean IME), Telex generates a latin "o" for both events:
  // it doesn't associate a unicode character on the second NSEvent.
  NSEvent* o_in_ime = UnicodeKeyDown(31, @"o");
  InterpretKeyEventsCallback handle_first_o_in_ime =
      base::BindRepeating([](id view) {
        // Note the whole word is replaced, not just the last character.
        [view insertText:@"mo" replacementRange:NSMakeRange(0, 1)];
      });
  InterpretKeyEventsCallback handle_second_o_in_ime =
      base::BindRepeating([](id view) {
        [view insertText:@"mô" replacementRange:NSMakeRange(0, 2)];
      });

  InterpretKeyEventsCallback handle_return_in_ime =
      base::BindRepeating([](id view) {
        // Note the previous -insertText: repeats, even though it is unchanged.
        // But the IME also follows with an -insertNewLine:.
        [view insertText:@"mô" replacementRange:NSMakeRange(0, 2)];
        [view doCommandBySelector:@selector(insertNewLine:)];
      });

  EXPECT_EQ(u"", textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  object_setClass(ns_view_, [InterpretKeyEventMockedBridgedContentView class]);
  g_fake_interpret_key_events = &handle_m_in_ime;
  [ns_view_ keyDown:m_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"m"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_first_o_in_ime;
  [ns_view_ keyDown:o_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"mo"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_second_o_in_ime;
  [ns_view_ keyDown:o_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"mô"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_return_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  EXPECT_EQ(base::SysNSStringToUTF16(@"mô"),
            textfield->GetText());    // No change.
  EXPECT_EQ(1, enter_view->count());  // Now we see the accelerator.
}

// Simulate 'a' and candidate selection keys. This should just insert "啊",
// suppressing accelerators.
TEST_F(BridgedNativeWidgetTest, TextInput_NoAcceleratorPinyinSelectWord) {
  Textfield* textfield = InstallTextField("");
  EXPECT_TRUE([ns_view_ textInputClientForTesting]);

  EnterAcceleratorView* enter_view = new EnterAcceleratorView();
  textfield->parent()->AddChildView(enter_view);

  // Sequence of calls (and corresponding keyDown events) obtained via tracing
  // with Pinyin IME and pressing 'a', Tab, PageDown, PageUp, Right, Down, Left,
  // and finally Up on the keyboard.
  // Note 0 is the actual keyCode for 'a', not a placeholder.
  NSEvent* a_in_ime = UnicodeKeyDown(0, @"a");
  InterpretKeyEventsCallback handle_a_in_ime = base::BindRepeating([](id view) {
    // Pinyin does not change composition text while selecting candidate words.
    [view setMarkedText:@"a"
           selectedRange:NSMakeRange(1, 0)
        replacementRange:NSMakeRange(NSNotFound, 0)];
  });

  InterpretKeyEventsCallback handle_tab_in_ime =
      base::BindRepeating([](id view) {
        [view setMarkedText:@"ā"
               selectedRange:NSMakeRange(0, 1)
            replacementRange:NSMakeRange(NSNotFound, 0)];
      });

  // Composition text will not change in candidate selection.
  InterpretKeyEventsCallback handle_candidate_select_in_ime =
      base::BindRepeating([](id view) {});

  InterpretKeyEventsCallback handle_space_in_ime =
      base::BindRepeating([](id view) {
        // Space will confirm the composition.
        [view insertText:@"啊" replacementRange:NSMakeRange(NSNotFound, 0)];
      });

  InterpretKeyEventsCallback handle_enter_in_ime =
      base::BindRepeating([](id view) {
        // Space after Space will generate -insertNewLine:.
        [view doCommandBySelector:@selector(insertNewLine:)];
      });

  EXPECT_EQ(u"", textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  object_setClass(ns_view_, [InterpretKeyEventMockedBridgedContentView class]);
  g_fake_interpret_key_events = &handle_a_in_ime;
  [ns_view_ keyDown:a_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"a"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_tab_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  EXPECT_EQ(base::SysNSStringToUTF16(@"ā"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());  // Not seen as an accelerator.

  // Pinyin changes candidate word on this sequence of keys without changing the
  // composition text. At the end of this sequence, the word "啊" should be
  // selected.
  const ui::KeyboardCode key_sequence[] = {ui::VKEY_NEXT,  ui::VKEY_PRIOR,
                                           ui::VKEY_RIGHT, ui::VKEY_DOWN,
                                           ui::VKEY_LEFT,  ui::VKEY_UP};

  g_fake_interpret_key_events = &handle_candidate_select_in_ime;
  for (auto key : key_sequence) {
    [ns_view_ keyDown:VkeyKeyDown(key)];
    EXPECT_EQ(base::SysNSStringToUTF16(@"ā"),
              textfield->GetText());  // No change.
    EXPECT_EQ(0, enter_view->count());
  }

  // Space to confirm composition
  g_fake_interpret_key_events = &handle_space_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_SPACE)];
  EXPECT_EQ(base::SysNSStringToUTF16(@"啊"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  // The next Enter should be processed as accelerator.
  g_fake_interpret_key_events = &handle_enter_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  EXPECT_EQ(base::SysNSStringToUTF16(@"啊"), textfield->GetText());
  EXPECT_EQ(1, enter_view->count());
}

// Simulate 'a', Enter in Hiragana. This should just insert "あ", suppressing
// accelerators.
TEST_F(BridgedNativeWidgetTest, TextInput_NoAcceleratorEnterComposition) {
  Textfield* textfield = InstallTextField("");
  EXPECT_TRUE([ns_view_ textInputClientForTesting]);

  EnterAcceleratorView* enter_view = new EnterAcceleratorView();
  textfield->parent()->AddChildView(enter_view);

  // Sequence of calls (and corresponding keyDown events) obtained via tracing
  // with Hiragana IME and pressing 'a', then Enter on the keyboard.
  // Note 0 is the actual keyCode for 'a', not a placeholder.
  NSEvent* a_in_ime = UnicodeKeyDown(0, @"a");
  InterpretKeyEventsCallback handle_a_in_ime = base::BindRepeating([](id view) {
    // TODO(crbug.com/41254370): |text| should be an NSAttributedString.
    [view setMarkedText:@"あ"
           selectedRange:NSMakeRange(1, 0)
        replacementRange:NSMakeRange(NSNotFound, 0)];
  });

  InterpretKeyEventsCallback handle_first_return_in_ime =
      base::BindRepeating([](id view) {
        [view insertText:@"あ" replacementRange:NSMakeRange(NSNotFound, 0)];
        // Note there is no call to -insertNewLine: here.
      });
  InterpretKeyEventsCallback handle_second_return_in_ime = base::BindRepeating(
      [](id view) { [view doCommandBySelector:@selector(insertNewLine:)]; });

  EXPECT_EQ(u"", textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  object_setClass(ns_view_, [InterpretKeyEventMockedBridgedContentView class]);
  g_fake_interpret_key_events = &handle_a_in_ime;
  [ns_view_ keyDown:a_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"あ"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_first_return_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  EXPECT_EQ(base::SysNSStringToUTF16(@"あ"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());  // Not seen as an accelerator.

  g_fake_interpret_key_events = &handle_second_return_in_ime;
  [ns_view_
      keyDown:VkeyKeyDown(ui::VKEY_RETURN)];  // Sanity check: send Enter again.
  EXPECT_EQ(base::SysNSStringToUTF16(@"あ"),
            textfield->GetText());    // No change.
  EXPECT_EQ(1, enter_view->count());  // Now we see the accelerator.
}

// Simulate 'a', Tab, Enter, Enter in Hiragana. This should just insert "a",
// suppressing accelerators.
TEST_F(BridgedNativeWidgetTest, TextInput_NoAcceleratorTabEnterComposition) {
  Textfield* textfield = InstallTextField("");
  EXPECT_TRUE([ns_view_ textInputClientForTesting]);

  EnterAcceleratorView* enter_view = new EnterAcceleratorView();
  textfield->parent()->AddChildView(enter_view);

  // Sequence of calls (and corresponding keyDown events) obtained via tracing
  // with Hiragana IME and pressing 'a', Tab, then Enter on the keyboard.
  NSEvent* a_in_ime = UnicodeKeyDown(0, @"a");
  InterpretKeyEventsCallback handle_a_in_ime = base::BindRepeating([](id view) {
    // TODO(crbug.com/41254370): |text| should have an underline.
    [view setMarkedText:@"あ"
           selectedRange:NSMakeRange(1, 0)
        replacementRange:NSMakeRange(NSNotFound, 0)];
  });

  InterpretKeyEventsCallback handle_tab_in_ime =
      base::BindRepeating([](id view) {
        // TODO(crbug.com/41254370): |text| should be an NSAttributedString (now
        // with a different underline color).
        [view setMarkedText:@"a"
               selectedRange:NSMakeRange(0, 1)
            replacementRange:NSMakeRange(NSNotFound, 0)];
        // Note there is no -insertTab: generated.
      });

  InterpretKeyEventsCallback handle_first_return_in_ime =
      base::BindRepeating([](id view) {
        // Do *nothing*. Enter does not confirm nor change the composition, it
        // just dismisses the IME window, leaving the text marked.
      });
  InterpretKeyEventsCallback handle_second_return_in_ime =
      base::BindRepeating([](id view) {
        // The second return will confirm the composition.
        [view insertText:@"a" replacementRange:NSMakeRange(NSNotFound, 0)];
      });
  InterpretKeyEventsCallback handle_third_return_in_ime =
      base::BindRepeating([](id view) {
        // Only the third return will generate -insertNewLine:.
        [view doCommandBySelector:@selector(insertNewLine:)];
      });

  EXPECT_EQ(u"", textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  object_setClass(ns_view_, [InterpretKeyEventMockedBridgedContentView class]);
  g_fake_interpret_key_events = &handle_a_in_ime;
  [ns_view_ keyDown:a_in_ime];
  EXPECT_EQ(base::SysNSStringToUTF16(@"あ"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_tab_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_TAB)];
  // Tab will switch to a Romanji (Latin) character.
  EXPECT_EQ(base::SysNSStringToUTF16(@"a"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());

  g_fake_interpret_key_events = &handle_first_return_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  // Enter just dismisses the IME window. The composition is still active.
  EXPECT_EQ(base::SysNSStringToUTF16(@"a"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());  // Not seen as an accelerator.

  g_fake_interpret_key_events = &handle_second_return_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];
  // Enter now confirms the composition (unmarks text). Note there is still no
  // IME window visible but, since there is marked text, IME is still active.
  EXPECT_EQ(base::SysNSStringToUTF16(@"a"), textfield->GetText());
  EXPECT_EQ(0, enter_view->count());  // Not seen as an accelerator.

  g_fake_interpret_key_events = &handle_third_return_in_ime;
  [ns_view_ keyDown:VkeyKeyDown(ui::VKEY_RETURN)];  // Send Enter a third time.
  EXPECT_EQ(base::SysNSStringToUTF16(@"a"),
            textfield->GetText());    // No change.
  EXPECT_EQ(1, enter_view->count());  // Now we see the accelerator.
}

// Test a codepath that could hypothetically cause [NSApp updateWindows] to be
// called recursively due to IME dismissal during teardown triggering a focus
// change. Twice.
TEST_F(BridgedNativeWidgetTest, TextInput_RecursiveUpdateWindows) {
  Textfield* textfield = InstallTextField("");
  EXPECT_TRUE([ns_view_ textInputClientForTesting]);

  object_setClass(ns_view_, [InterpretKeyEventMockedBridgedContentView class]);
  base::apple::ScopedObjCClassSwizzler update_windows_swizzler(
      [NSApplication class], [UpdateWindowsDonorForNSApp class],
      @selector(updateWindows));
  base::apple::ScopedObjCClassSwizzler current_input_context_swizzler(
      [NSTextInputContext class],
      [CurrentInputContextDonorForNSTextInputContext class],
      @selector(currentInputContext));

  int vkey_return_count = 0;

  // Everything happens with this one event.
  NSEvent* return_with_fake_ime = cocoa_test_event_utils::SynthesizeKeyEvent(
      widget_->GetNativeWindow().GetNativeNSWindow(), true, ui::VKEY_RETURN, 0);

  InterpretKeyEventsCallback generate_return_and_fake_ime = base::BindRepeating(
      [](int* saw_return_count, id view) {
        EXPECT_EQ(0, *saw_return_count);
        // First generate the return to simulate an input context change.
        [view insertText:@"\r" replacementRange:NSMakeRange(NSNotFound, 0)];

        EXPECT_EQ(1, *saw_return_count);
      },
      &vkey_return_count);

  bool saw_update_windows = false;
  base::RepeatingClosure update_windows_closure = base::BindRepeating(
      [](bool* saw_update_windows, BridgedContentView* view,
         NativeWidgetMacNSWindowHost* host, Textfield* textfield) {
        // Ensure updateWindows is not invoked recursively.
        EXPECT_FALSE(*saw_update_windows);
        *saw_update_windows = true;

        // Inside updateWindows, assume the IME got dismissed and wants to
        // insert its last bit of text for the old input context.
        [view insertText:@"배" replacementRange:NSMakeRange(0, 1)];

        // This is triggered by the setTextInputClient:nullptr in
        // SetHandleKeyEventCallback(), so -inputContext should also be nil.
        EXPECT_FALSE([view inputContext]);

        // Ensure we can't recursively call updateWindows. A TextInputClient
        // reacting to InsertChar could theoretically do this, but toolkit-views
        // DCHECKs if there is recursive event dispatch, so call
        // setTextInputClient directly.
        host->text_input_host()->SetTextInputClient(textfield);

        // Finally simulate what -[NSApp updateWindows] should _actually_ do,
        // which is to update the input context (from the first responder).
        g_fake_current_input_context = [view inputContext];

        // Now, the |textfield| set above should have been set again.
        EXPECT_TRUE(g_fake_current_input_context);
      },
      &saw_update_windows, ns_view_, GetNSWindowHost(), textfield);

  SetHandleKeyEventCallback(base::BindRepeating(
      [](int* saw_return_count, BridgedContentView* view,
         NativeWidgetMacNSWindowHost* host, Textfield* textfield,
         const ui::KeyEvent& event) {
        if (event.key_code() == ui::VKEY_RETURN) {
          *saw_return_count += 1;
          // Simulate Textfield::OnBlur() by clearing the input method.
          // Textfield needs to be in a Widget to do this normally.
          host->text_input_host()->SetTextInputClient(nullptr);
        }
        return false;
      },
      &vkey_return_count, ns_view_, GetNSWindowHost()));

  // Starting text (just insert it).
  [ns_view_ insertText:@"ㅂ" replacementRange:NSMakeRange(NSNotFound, 0)];

  EXPECT_EQ(base::SysNSStringToUTF16(@"ㅂ"), textfield->GetText());

  g_fake_interpret_key_events = &generate_return_and_fake_ime;
  g_update_windows_closure = &update_windows_closure;
  g_fake_current_input_context = [ns_view_ inputContext];
  EXPECT_TRUE(g_fake_current_input_context);
  [ns_view_ keyDown:return_with_fake_ime];

  // We should see one VKEY_RETURNs and one updateWindows. In particular, note
  // that there is not a second VKEY_RETURN seen generated by keyDown: thinking
  // the event has been unhandled. This is because it was handled when the fake
  // IME sent \r.
  EXPECT_TRUE(saw_update_windows);
  EXPECT_EQ(1, vkey_return_count);

  // The text inserted during updateWindows should have been inserted, even
  // though we were trying to change the input context.
  EXPECT_EQ(base::SysNSStringToUTF16(@"배"), textfield->GetText());

  EXPECT_TRUE(g_fake_current_input_context);

  g_fake_current_input_context = nullptr;
  g_fake_interpret_key_events = nullptr;
  g_update_windows_closure = nullptr;
}

// Write selection text to the pasteboard.
TEST_F(BridgedNativeWidgetTest, TextInput_WriteToPasteboard) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  NSArray* types = @[ NSPasteboardTypeString ];

  // Try to write with no selection. This will succeed, but the string will be
  // empty.
  {
    NSPasteboard* pboard = [NSPasteboard pasteboardWithUniqueName];
    BOOL wrote_to_pboard = [ns_view_ writeSelectionToPasteboard:pboard
                                                          types:types];
    EXPECT_TRUE(wrote_to_pboard);
    NSArray* objects = [pboard readObjectsForClasses:@[ [NSString class] ]
                                             options:nullptr];
    EXPECT_EQ(1u, [objects count]);
    EXPECT_NSEQ(@"", [objects lastObject]);
  }

  // Write a selection successfully.
  {
    SetSelectionRange(NSMakeRange(4, 7));
    NSPasteboard* pboard = [NSPasteboard pasteboardWithUniqueName];
    BOOL wrote_to_pboard = [ns_view_ writeSelectionToPasteboard:pboard
                                                          types:types];
    EXPECT_TRUE(wrote_to_pboard);
    NSArray* objects = [pboard readObjectsForClasses:@[ [NSString class] ]
                                             options:nullptr];
    EXPECT_EQ(1u, [objects count]);
    EXPECT_NSEQ(@"bar baz", [objects lastObject]);
  }
}

TEST_F(BridgedNativeWidgetTest, WriteToFindPasteboard) {
  base::apple::ScopedObjCClassSwizzler swizzler([FindPasteboard class],
                                                [MockFindPasteboard class],
                                                @selector(sharedInstance));
  EXPECT_NSEQ(@"", [[FindPasteboard sharedInstance] findText]);

  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  SetSelectionRange(NSMakeRange(4, 7));
  [ns_view_ copyToFindPboard:nil];
  EXPECT_NSEQ(@"bar baz", [[FindPasteboard sharedInstance] findText]);

  // Don't overwrite with empty selection
  SetSelectionRange(NSMakeRange(0, 0));
  [ns_view_ copyToFindPboard:nil];
  EXPECT_NSEQ(@"bar baz", [[FindPasteboard sharedInstance] findText]);
}

}  // namespace views::test
