// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views_bridge_mac/bridged_content_view.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/appkit_utils.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/canvas_painter.h"
#import "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/gfx/canvas_paint_mac.h"
#include "ui/gfx/decorated_text.h"
#import "ui/gfx/decorated_text_mac.h"
#include "ui/gfx/geometry/rect.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/path.h"
#import "ui/gfx/path_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/views_bridge_mac/bridged_native_widget_host_helper.h"
#import "ui/views_bridge_mac/bridged_native_widget_impl.h"
#import "ui/views_bridge_mac/drag_drop_client.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget_host.mojom.h"

namespace {

NSString* const kFullKeyboardAccessChangedNotification =
    @"com.apple.KeyboardUIModeDidChange";

// Convert a |point| in |source_window|'s AppKit coordinate system (origin at
// the bottom left of the window) to |target_window|'s content rect, with the
// origin at the top left of the content area.
// If |source_window| is nil, |point| will be treated as screen coordinates.
gfx::Point MovePointToWindow(const NSPoint& point,
                             NSWindow* source_window,
                             NSWindow* target_window) {
  NSPoint point_in_screen =
      source_window ? ui::ConvertPointFromWindowToScreen(source_window, point)
                    : point;

  NSPoint point_in_window =
      ui::ConvertPointFromScreenToWindow(target_window, point_in_screen);
  NSRect content_rect =
      [target_window contentRectForFrameRect:[target_window frame]];
  return gfx::Point(point_in_window.x,
                    NSHeight(content_rect) - point_in_window.y);
}

// Returns true if |client| has RTL text.
bool IsTextRTL(const ui::TextInputClient* client) {
  return client && client->GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
}

// Returns true if |event| may have triggered dismissal of an IME and would
// otherwise be ignored by a ui::TextInputClient when inserted.
bool IsImeTriggerEvent(NSEvent* event) {
  ui::KeyboardCode key = ui::KeyboardCodeFromNSEvent(event);
  return key == ui::VKEY_RETURN || key == ui::VKEY_TAB ||
         key == ui::VKEY_ESCAPE;
}

// Returns the boundary rectangle for composition characters in the
// |requested_range|. Sets |actual_range| corresponding to the returned
// rectangle. For cases, where there is no composition text or the
// |requested_range| lies outside the composition range, a zero width rectangle
// corresponding to the caret bounds is returned. Logic used is similar to
// RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(...).
gfx::Rect GetFirstRectForRangeHelper(const ui::TextInputClient* client,
                                     const gfx::Range& requested_range,
                                     gfx::Range* actual_range) {
  // NSRange doesn't support reversed ranges.
  DCHECK(!requested_range.is_reversed());
  DCHECK(actual_range);

  // Set up default return values, to be returned in case of unusual cases.
  gfx::Rect default_rect;
  *actual_range = gfx::Range::InvalidRange();
  if (!client)
    return default_rect;

  default_rect = client->GetCaretBounds();
  default_rect.set_width(0);

  // If possible, modify actual_range to correspond to caret position.
  gfx::Range selection_range;
  if (client->GetSelectionRange(&selection_range)) {
    // Caret bounds correspond to end index of selection_range.
    *actual_range = gfx::Range(selection_range.end());
  }

  gfx::Range composition_range;
  if (!client->HasCompositionText() ||
      !client->GetCompositionTextRange(&composition_range) ||
      !composition_range.Contains(requested_range))
    return default_rect;

  DCHECK(!composition_range.is_reversed());

  const size_t from = requested_range.start() - composition_range.start();
  const size_t to = requested_range.end() - composition_range.start();

  // Pick the first character's bounds as the initial rectangle, then grow it to
  // the full |requested_range| if possible.
  const bool request_is_composition_end = from == composition_range.length();
  const size_t first_index = request_is_composition_end ? from - 1 : from;
  gfx::Rect union_rect;
  if (!client->GetCompositionCharacterBounds(first_index, &union_rect))
    return default_rect;

  // If requested_range is empty, return a zero width rectangle corresponding to
  // it.
  if (from == to) {
    if (request_is_composition_end && !IsTextRTL(client)) {
      // In case of an empty requested range at end of composition, return the
      // rectangle to the right of the last compositioned character.
      union_rect.set_origin(union_rect.top_right());
    }
    union_rect.set_width(0);
    *actual_range = requested_range;
    return union_rect;
  }

  // Toolkit-views textfields are always single-line, so no need to check for
  // line breaks.
  for (size_t i = from + 1; i < to; i++) {
    gfx::Rect current_rect;
    if (client->GetCompositionCharacterBounds(i, &current_rect)) {
      union_rect.Union(current_rect);
    } else {
      *actual_range =
          gfx::Range(requested_range.start(), i + composition_range.start());
      return union_rect;
    }
  }
  *actual_range = requested_range;
  return union_rect;
}

// Returns the string corresponding to |requested_range| for the given |client|.
// If a gfx::Range::InvalidRange() is passed, the full string stored by |client|
// is returned. Sets |actual_range| corresponding to the returned string.
base::string16 AttributedSubstringForRangeHelper(
    const ui::TextInputClient* client,
    const gfx::Range& requested_range,
    gfx::Range* actual_range) {
  // NSRange doesn't support reversed ranges.
  DCHECK(!requested_range.is_reversed());
  DCHECK(actual_range);

  base::string16 substring;
  gfx::Range text_range;
  *actual_range = gfx::Range::InvalidRange();
  if (!client || !client->GetTextRange(&text_range))
    return substring;

  // gfx::Range::Intersect() behaves a bit weirdly. If B is an empty range
  // contained inside a non-empty range A, B intersection A returns
  // gfx::Range::InvalidRange(), instead of returning B.
  *actual_range = text_range.Contains(requested_range)
                      ? requested_range
                      : text_range.Intersect(requested_range);

  // This is a special case for which the complete string should should be
  // returned. NSTextView also follows this, though the same is not mentioned in
  // NSTextInputClient documentation.
  if (!requested_range.IsValid())
    *actual_range = text_range;

  client->GetTextFromRange(*actual_range, &substring);
  return substring;
}

ui::TextEditCommand GetTextEditCommandForMenuAction(SEL action) {
  if (action == @selector(undo:))
    return ui::TextEditCommand::UNDO;
  if (action == @selector(redo:))
    return ui::TextEditCommand::REDO;
  if (action == @selector(cut:))
    return ui::TextEditCommand::CUT;
  if (action == @selector(copy:))
    return ui::TextEditCommand::COPY;
  if (action == @selector(paste:))
    return ui::TextEditCommand::PASTE;
  if (action == @selector(pasteAndMatchStyle:))
    return ui::TextEditCommand::PASTE;
  if (action == @selector(selectAll:))
    return ui::TextEditCommand::SELECT_ALL;
  return ui::TextEditCommand::INVALID_COMMAND;
}

}  // namespace

@interface BridgedContentView ()

// Dispatch |event| to |bridge_|'s host.
- (void)dispatchKeyEvent:(ui::KeyEvent*)event;

// Returns true if active menu controller corresponds to this widget. Note that
// this will synchronously call into the browser process.
- (BOOL)hasActiveMenuController;

// Dispatch |event| to |menu_controller| and return true if |event| is
// swallowed.
- (BOOL)dispatchKeyEventToMenuController:(ui::KeyEvent*)event;

// Passes |event| to the InputMethod for dispatch.
- (void)handleKeyEvent:(ui::KeyEvent*)event;

// Allows accelerators to be handled at different points in AppKit key event
// dispatch. Checks for an unhandled event passed in to -keyDown: and passes it
// to the Widget for processing. Returns YES if the Widget handles it.
- (BOOL)handleUnhandledKeyDownAsKeyEvent;

// Handles an NSResponder Action Message by mapping it to a corresponding text
// editing command from ui_strings.grd and, when not being sent to a
// TextInputClient, the keyCode that toolkit-views expects internally.
// For example, moveToLeftEndOfLine: would pass ui::VKEY_HOME in non-RTL locales
// even though the Home key on Mac defaults to moveToBeginningOfDocument:.
// This approach also allows action messages a user
// may have remapped in ~/Library/KeyBindings/DefaultKeyBinding.dict to be
// catered for.
// Note: default key bindings in Mac can be read from StandardKeyBinding.dict
// which lives in /System/Library/Frameworks/AppKit.framework/Resources. Do
// `plutil -convert xml1 -o StandardKeyBinding.xml StandardKeyBinding.dict` to
// get something readable.
- (void)handleAction:(ui::TextEditCommand)command
             keyCode:(ui::KeyboardCode)keyCode
             domCode:(ui::DomCode)domCode
          eventFlags:(int)eventFlags;

// ui::EventLocationFromNative() assumes the event hit the contentView.
// Adjust |event| if that's not the case (e.g. for reparented views).
- (void)adjustUiEventLocation:(ui::LocatedEvent*)event
              fromNativeEvent:(NSEvent*)nativeEvent;

// Notification handler invoked when the Full Keyboard Access mode is changed.
- (void)onFullKeyboardAccessModeChanged:(NSNotification*)notification;

// Helper method which forwards |text| to the active menu or |textInputClient_|.
- (void)insertTextInternal:(id)text;

// Returns the native Widget's drag drop client. Possibly null.
- (views_bridge_mac::DragDropClient*)dragDropClient NS_RETURNS_INNER_POINTER;

// Menu action handlers.
- (void)undo:(id)sender;
- (void)redo:(id)sender;
- (void)cut:(id)sender;
- (void)copy:(id)sender;
- (void)paste:(id)sender;
- (void)pasteAndMatchStyle:(id)sender;
- (void)selectAll:(id)sender;

@end

@implementation BridgedContentView

@synthesize bridge = bridge_;
@synthesize textInputClient = textInputClient_;
@synthesize drawMenuBackgroundForBlur = drawMenuBackgroundForBlur_;

- (instancetype)initWithBridge:(views::BridgedNativeWidgetImpl*)bridge
                        bounds:(gfx::Rect)bounds {
  // To keep things simple, assume the origin is (0, 0) until there exists a use
  // case for something other than that.
  DCHECK(bounds.origin().IsOrigin());
  NSRect initialFrame = NSMakeRect(0, 0, bounds.width(), bounds.height());
  if ((self = [super initWithFrame:initialFrame])) {
    bridge_ = bridge;

    // Apple's documentation says that NSTrackingActiveAlways is incompatible
    // with NSTrackingCursorUpdate, so use NSTrackingActiveInActiveApp.
    cursorTrackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:NSZeroRect
             options:NSTrackingMouseMoved | NSTrackingCursorUpdate |
                     NSTrackingActiveInActiveApp | NSTrackingInVisibleRect |
                     NSTrackingMouseEnteredAndExited
               owner:self
            userInfo:nil]);
    [self addTrackingArea:cursorTrackingArea_.get()];

    // Get notified whenever Full Keyboard Access mode is changed.
    [[NSDistributedNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onFullKeyboardAccessModeChanged:)
               name:kFullKeyboardAccessChangedNotification
             object:nil];

    // Initialize the focus manager with the correct keyboard accessibility
    // setting.
    [self updateFullKeyboardAccess];
    [self registerForDraggedTypes:ui::OSExchangeDataProviderMac::
                                      SupportedPasteboardTypes()];
  }
  return self;
}

- (void)dealloc {
  // By the time |self| is dealloc'd, it should never be in an NSWindow, and it
  // should never be the current input context.
  DCHECK_EQ(nil, [self window]);
  // Sanity check: NSView always provides an -inputContext.
  DCHECK_NE(nil, [super inputContext]);
  DCHECK_NE([NSTextInputContext currentInputContext], [super inputContext]);
  [super dealloc];
}

- (void)clearView {
  [self setTextInputClient:nullptr];
  bridge_ = nullptr;
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [cursorTrackingArea_.get() clearOwner];
  [self removeTrackingArea:cursorTrackingArea_.get()];
}

- (void)setTextInputClient:(ui::TextInputClient*)newTextInputClient {
  if (pendingTextInputClient_ == newTextInputClient)
    return;

  // This method may cause the IME window to dismiss, which may cause it to
  // insert text (e.g. to replace marked text with "real" text). That should
  // happen in the old -inputContext (which AppKit stores a reference to).
  // Unfortunately, the only way to invalidate the the old -inputContext is to
  // invoke -[NSApp updateWindows], which also wants a reference to the _new_
  // -inputContext. So put the new inputContext in |pendingTextInputClient_| and
  // only use it for -inputContext.
  ui::TextInputClient* oldInputClient = textInputClient_;

  // Since dismissing an IME may insert text, a misbehaving IME or a
  // ui::TextInputClient that acts on InsertChar() to change focus a second time
  // may invoke -setTextInputClient: recursively; with [NSApp updateWindows]
  // still on the stack. Calling [NSApp updateWindows] recursively may upset
  // an IME. Since the rest of this method is only to decide whether to call
  // updateWindows, and we're already calling it, just bail out.
  if (textInputClient_ != pendingTextInputClient_) {
    pendingTextInputClient_ = newTextInputClient;
    return;
  }

  // Start by assuming no need to invoke -updateWindows.
  textInputClient_ = newTextInputClient;
  pendingTextInputClient_ = newTextInputClient;

  // If |self| was being used for the input context, and would now report a
  // different input context, manually invoke [NSApp updateWindows]. This is
  // necessary because AppKit holds on to a raw pointer to a NSTextInputContext
  // (which may have been the one returned by [self inputContext]) that is only
  // updated by -updateWindows. And although AppKit invokes that on each
  // iteration through most runloop modes, it does not call it when running
  // NSEventTrackingRunLoopMode, and not _within_ a run loop iteration, where
  // the inputContext may change before further event processing.
  NSTextInputContext* current = [NSTextInputContext currentInputContext];
  if (!current)
    return;

  NSTextInputContext* newContext = [self inputContext];
  // If the newContext is non-nil, then it can only be [super inputContext]. So
  // the input context is either not changing, or it was not from |self|. In
  // both cases, there's no need to call -updateWindows.
  if (newContext) {
    DCHECK_EQ(newContext, [super inputContext]);
    return;
  }

  if (current == [super inputContext]) {
    DCHECK_NE(oldInputClient, textInputClient_);
    textInputClient_ = oldInputClient;
    [NSApp updateWindows];
    // Note: |pendingTextInputClient_| (and therefore +[NSTextInputContext
    // currentInputContext] may have changed if called recursively.
    textInputClient_ = pendingTextInputClient_;
  }
}

// If |point| is classified as a draggable background (HTCAPTION), return nil so
// that it can lead to a window drag or double-click in the title bar. Dragging
// could be optimized by telling the window server which regions should be
// instantly draggable without asking (tracked at https://crbug.com/830962).
- (NSView*)hitTest:(NSPoint)point {
  gfx::Point flippedPoint(point.x, NSHeight(self.superview.bounds) - point.y);
  bool isDraggableBackground = false;
  bridge_->host()->GetIsDraggableBackgroundAt(flippedPoint,
                                              &isDraggableBackground);
  if (isDraggableBackground)
    return nil;
  return [super hitTest:point];
}

- (void)processCapturedMouseEvent:(NSEvent*)theEvent {
  if (!bridge_)
    return;

  NSWindow* source = [theEvent window];
  NSWindow* target = [self window];
  DCHECK(target);

  BOOL isScrollEvent = [theEvent type] == NSScrollWheel;

  // If it's the view's window, process normally.
  if ([target isEqual:source]) {
    if (isScrollEvent)
      [self scrollWheel:theEvent];
    else
      [self mouseEvent:theEvent];

    return;
  }

  gfx::Point event_location =
      MovePointToWindow([theEvent locationInWindow], source, target);
  [self updateTooltipIfRequiredAt:event_location];

  if (isScrollEvent) {
    auto event = std::make_unique<ui::ScrollEvent>(theEvent);
    event->set_location(event_location);
    bridge_->host()->OnScrollEvent(std::move(event));
  } else {
    auto event = std::make_unique<ui::MouseEvent>(theEvent);
    event->set_location(event_location);
    bridge_->host()->OnMouseEvent(std::move(event));
  }
}

- (void)updateTooltipIfRequiredAt:(const gfx::Point&)locationInContent {
  DCHECK(bridge_);
  base::string16 newTooltipText;

  bridge_->host()->GetTooltipTextAt(locationInContent, &newTooltipText);
  if (newTooltipText != lastTooltipText_) {
    std::swap(newTooltipText, lastTooltipText_);
    [self setToolTipAtMousePoint:base::SysUTF16ToNSString(lastTooltipText_)];
  }
}

- (void)updateFullKeyboardAccess {
  if (!bridge_)
    return;
  bridge_->host()->SetKeyboardAccessible([NSApp isFullKeyboardAccessEnabled]);
}

// BridgedContentView private implementation.

- (void)dispatchKeyEvent:(ui::KeyEvent*)event {
  if (bridge_)
    bridge_->host_helper()->DispatchKeyEvent(event);
}

- (BOOL)hasActiveMenuController {
  bool hasMenuController = false;
  if (bridge_)
    bridge_->host()->GetHasMenuController(&hasMenuController);
  return hasMenuController;
}

- (BOOL)dispatchKeyEventToMenuController:(ui::KeyEvent*)event {
  if (bridge_)
    return bridge_->host_helper()->DispatchKeyEventToMenuController(event);
  return false;
}

- (void)handleKeyEvent:(ui::KeyEvent*)event {
  DCHECK(event);
  if ([self dispatchKeyEventToMenuController:event])
    return;

  [self dispatchKeyEvent:event];
}

- (BOOL)handleUnhandledKeyDownAsKeyEvent {
  if (!hasUnhandledKeyDownEvent_)
    return NO;

  ui::KeyEvent event(keyDownEvent_);
  [self handleKeyEvent:&event];
  hasUnhandledKeyDownEvent_ = NO;
  return event.handled();
}

- (void)handleAction:(ui::TextEditCommand)command
             keyCode:(ui::KeyboardCode)keyCode
             domCode:(ui::DomCode)domCode
          eventFlags:(int)eventFlags {
  if (!bridge_)
    return;

  // Always propagate the shift modifier if present. Shift doesn't always alter
  // the command selector, but should always be passed along. Control and Alt
  // have different meanings on Mac, so they do not propagate automatically.
  if ([keyDownEvent_ modifierFlags] & NSShiftKeyMask)
    eventFlags |= ui::EF_SHIFT_DOWN;

  // Generate a synthetic event with the keycode toolkit-views expects.
  ui::KeyEvent event(ui::ET_KEY_PRESSED, keyCode, domCode, eventFlags);

  if ([self dispatchKeyEventToMenuController:&event])
    return;

  // If there's an active TextInputClient, schedule the editing command to be
  // performed.
  if (textInputClient_ && textInputClient_->IsTextEditCommandEnabled(command))
    textInputClient_->SetTextEditCommandForNextKeyEvent(command);

  [self dispatchKeyEvent:&event];
}

- (void)adjustUiEventLocation:(ui::LocatedEvent*)event
              fromNativeEvent:(NSEvent*)nativeEvent {
  if ([nativeEvent window] && [[self window] contentView] != self) {
    NSPoint p = [self convertPoint:[nativeEvent locationInWindow] fromView:nil];
    event->set_location(gfx::Point(p.x, NSHeight([self frame]) - p.y));
  }
}

- (void)onFullKeyboardAccessModeChanged:(NSNotification*)notification {
  DCHECK([[notification name]
      isEqualToString:kFullKeyboardAccessChangedNotification]);
  [self updateFullKeyboardAccess];
}

- (void)insertTextInternal:(id)text {
  if (!bridge_)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];

  bool isCharacterEvent = keyDownEvent_ && [text length] == 1;
  // Pass "character" events to the View hierarchy. Cases this handles (non-
  // exhaustive)-
  //    - Space key press on controls. Unlike Tab and newline which have
  //      corresponding action messages, an insertText: message is generated for
  //      the Space key (insertText:replacementRange: when there's an active
  //      input context).
  //    - Menu mnemonic selection.
  // Note we create a custom character ui::KeyEvent (and not use the
  // ui::KeyEvent(NSEvent*) constructor) since we can't just rely on the event
  // key code to get the actual characters from the ui::KeyEvent. This for
  // example is necessary for menu mnemonic selection of non-latin text.

  // Don't generate a key event when there is marked composition text. These key
  // down events should be consumed by the IME and not reach the Views layer.
  // For example, on pressing Return to commit composition text, if we passed a
  // synthetic key event to the View hierarchy, it will have the effect of
  // performing the default action on the current dialog. We do not want this
  // when there is marked text (Return should only confirm the IME).

  // However, IME for phonetic languages such as Korean do not always _mark_
  // text when a composition is active. For these, correct behaviour is to
  // handle the final -keyDown: that caused the composition to be committed, but
  // only _after_ the sequence of insertText: messages coming from IME have been
  // sent to the TextInputClient. Detect this by comparing to -[NSEvent
  // characters]. Note we do not use -charactersIgnoringModifiers: so that,
  // e.g., ß (Alt+s) will match mnemonics with ß rather than s.
  bool isFinalInsertForKeyEvent =
      isCharacterEvent && [text isEqualToString:[keyDownEvent_ characters]];

  // Also note that a single, non-IME key down event can also cause multiple
  // insertText:replacementRange: action messages being generated from within
  // -keyDown:'s call to -interpretKeyEvents:. One example, on pressing Alt+e,
  // the accent (´) character is composed via setMarkedText:. Now on pressing
  // the character 'r', two insertText:replacementRange: action messages are
  // generated with the text value of accent (´) and 'r' respectively. The key
  // down event will have characters field of length 2. The first of these
  // insertText messages won't generate a KeyEvent since there'll be active
  // marked text. However, a KeyEvent will be generated corresponding to 'r'.

  // Currently there seems to be no use case to pass non-character events routed
  // from insertText: handlers to the View hierarchy.
  if (isFinalInsertForKeyEvent && ![self hasMarkedText]) {
    ui::KeyEvent charEvent([text characterAtIndex:0],
                           ui::KeyboardCodeFromNSEvent(keyDownEvent_),
                           ui::DomCodeFromNSEvent(keyDownEvent_), ui::EF_NONE);
    [self handleKeyEvent:&charEvent];
    hasUnhandledKeyDownEvent_ = NO;
    if (charEvent.handled())
      return;
  }

  // Forward the |text| to |textInputClient_| if no menu is active.
  if (textInputClient_ && ![self hasActiveMenuController]) {
    // If a single character is inserted by keyDown's call to
    // interpretKeyEvents: then use InsertChar() to allow editing events to be
    // merged. We use ui::VKEY_UNKNOWN as the key code since it's not feasible
    // to determine the correct key code for each unicode character. Also a
    // correct keycode is not needed in the current context. Send ui::EF_NONE as
    // the key modifier since |text| already accounts for the pressed key
    // modifiers.

    // Also, note we don't check isFinalInsertForKeyEvent, nor use
    // |keyDownEvent_| to generate the synthetic ui::KeyEvent since:  For
    //  composed text, [keyDownEvent_ characters] might not be the same as
    // |text|. This is because |keyDownEvent_| will correspond to the event that
    // caused the composition text to be confirmed, say, Return key press.
    if (isCharacterEvent) {
      textInputClient_->InsertChar(
          ui::KeyEvent([text characterAtIndex:0], ui::VKEY_UNKNOWN,
                       ui::DomCode::NONE, ui::EF_NONE));
    } else {
      textInputClient_->InsertText(base::SysNSStringToUTF16(text));
    }
    // Suppress accelerators that may be bound to this key, since it inserted
    // text instead. But note that IME may follow with -insertNewLine:, which
    // will resurrect the keyEvent for accelerator handling.
    hasUnhandledKeyDownEvent_ = NO;
  }
}

- (views_bridge_mac::DragDropClient*)dragDropClient {
  return bridge_ ? bridge_->drag_drop_client() : nullptr;
}

- (void)undo:(id)sender {
  [self handleAction:ui::TextEditCommand::UNDO
             keyCode:ui::VKEY_Z
             domCode:ui::DomCode::US_Z
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)redo:(id)sender {
  [self handleAction:ui::TextEditCommand::REDO
             keyCode:ui::VKEY_Z
             domCode:ui::DomCode::US_Z
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)cut:(id)sender {
  [self handleAction:ui::TextEditCommand::CUT
             keyCode:ui::VKEY_X
             domCode:ui::DomCode::US_X
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)copy:(id)sender {
  [self handleAction:ui::TextEditCommand::COPY
             keyCode:ui::VKEY_C
             domCode:ui::DomCode::US_C
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)paste:(id)sender {
  [self handleAction:ui::TextEditCommand::PASTE
             keyCode:ui::VKEY_V
             domCode:ui::DomCode::US_V
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)pasteAndMatchStyle:(id)sender {
  [self handleAction:ui::TextEditCommand::PASTE
             keyCode:ui::VKEY_V
             domCode:ui::DomCode::US_V
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)selectAll:(id)sender {
  [self handleAction:ui::TextEditCommand::SELECT_ALL
             keyCode:ui::VKEY_A
             domCode:ui::DomCode::US_A
          eventFlags:ui::EF_CONTROL_DOWN];
}

// BaseView implementation.

// Don't use tracking areas from BaseView. BridgedContentView's tracks
// NSTrackingCursorUpdate and Apple's documentation suggests it's incompatible.
- (void)enableTracking {
}

// Translates the location of |theEvent| to toolkit-views coordinates and passes
// the event to NativeWidgetMac for handling.
- (void)mouseEvent:(NSEvent*)theEvent {
  if (!bridge_)
    return;

  DCHECK([theEvent type] != NSScrollWheel);
  auto event = std::make_unique<ui::MouseEvent>(theEvent);
  [self adjustUiEventLocation:event.get() fromNativeEvent:theEvent];

  // Aura updates tooltips with the help of aura::Window::AddPreTargetHandler().
  // Mac hooks in here.
  [self updateTooltipIfRequiredAt:event->location()];
  bridge_->host()->OnMouseEvent(std::move(event));
}

- (void)forceTouchEvent:(NSEvent*)theEvent {
  if (ui::ForceClickInvokesQuickLook())
    [self quickLookWithEvent:theEvent];
}

// NSView implementation.

// This view must consistently return YES or else dragging a tab may drag the
// entire window. See r549802 for details.
- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  if ([[self window] firstResponder] != self)
    return NO;
  BOOL result = [super becomeFirstResponder];
  if (result && bridge_)
    bridge_->host()->OnIsFirstResponderChanged(true);
  return result;
}

- (BOOL)resignFirstResponder {
  BOOL result = [super resignFirstResponder];
  if (result && bridge_)
    bridge_->host()->OnIsFirstResponderChanged(false);
  return result;
}

- (void)viewDidMoveToWindow {
  // When this view is added to a window, AppKit calls setFrameSize before it is
  // added to the window, so the behavior in setFrameSize is not triggered.
  NSWindow* window = [self window];
  if (window)
    [self setFrameSize:NSZeroSize];
}

- (void)setFrameSize:(NSSize)newSize {
  // The size passed in here does not always use
  // -[NSWindow contentRectForFrameRect]. The following ensures that the
  // contentView for a frameless window can extend over the titlebar of the new
  // window containing it, since AppKit requires a titlebar to give frameless
  // windows correct shadows and rounded corners.
  NSWindow* window = [self window];
  if (window && [window contentView] == self) {
    newSize = [window contentRectForFrameRect:[window frame]].size;
    // Ensure that the window geometry be updated on the host side before the
    // view size is updated.
    // TODO(ccameron): Consider updating the view size and window size and
    // position together in UpdateWindowGeometry.
    // https://crbug.com/875776, https://crbug.com/875731
    if (bridge_)
      bridge_->UpdateWindowGeometry();
  }

  [super setFrameSize:newSize];

  if (bridge_)
    bridge_->host()->OnViewSizeChanged(
        gfx::Size(newSize.width, newSize.height));
}

- (BOOL)isOpaque {
  return bridge_ ? !bridge_->is_translucent_window() : NO;
}

// To maximize consistency with the Cocoa browser (mac_views_browser=0), accept
// mouse clicks immediately so that clicking on Chrome from an inactive window
// will allow the event to be processed, rather than merely activate the window.
- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

// NSDraggingDestination protocol overrides.

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [self draggingUpdated:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  views_bridge_mac::DragDropClient* client = [self dragDropClient];
  return client ? client->DragUpdate(sender) : ui::DragDropTypes::DRAG_NONE;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  views_bridge_mac::DragDropClient* client = [self dragDropClient];
  if (client)
    client->DragExit();
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  views_bridge_mac::DragDropClient* client = [self dragDropClient];
  return client && client->Drop(sender) != NSDragOperationNone;
}

- (NSTextInputContext*)inputContext {
  // If the textInputClient_ does not exist, return nil since this view does not
  // conform to NSTextInputClient protocol.
  if (!pendingTextInputClient_)
    return nil;

  // If a menu is active, and -[NSView interpretKeyEvents:] asks for the
  // input context, return nil. This ensures the action message is sent to
  // the view, rather than any NSTextInputClient a subview has installed.
  if ([self hasActiveMenuController])
    return nil;

  // When not in an editable mode, or while entering passwords
  // (http://crbug.com/23219), we don't want to show IME candidate windows.
  // Returning nil prevents this view from getting messages defined as part of
  // the NSTextInputClient protocol.
  switch (pendingTextInputClient_->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return nil;
    default:
      return [super inputContext];
  }
}

// NSResponder implementation.

- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  // This is a SPI that AppKit apparently calls after |performKeyEquivalent:|
  // returned NO. If this function returns |YES|, Cocoa sends the event to
  // |keyDown:| instead of doing other things with it. Ctrl-tab will be sent
  // to us instead of doing key view loop control, ctrl-left/right get handled
  // correctly, etc.
  // (However, there are still some keys that Cocoa swallows, e.g. the key
  // equivalent that Cocoa uses for toggling the input language. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (void)keyDown:(NSEvent*)theEvent {
  BOOL hadMarkedTextAtKeyDown = [self hasMarkedText];

  // Convert the event into an action message, according to OSX key mappings.
  keyDownEvent_ = theEvent;
  hasUnhandledKeyDownEvent_ = YES;
  wantsKeyHandledForInsert_ = NO;
  [self interpretKeyEvents:@[ theEvent ]];

  // When there is marked text, -[NSView interpretKeyEvents:] may handle the
  // event by dismissing the IME window in a way that neither unmarks text, nor
  // updates any composition. That is, no signal is given either to the
  // NSTextInputClient or the NSTextInputContext that the IME changed state.
  // However, we must ensure this key down is not processed as an accelerator.
  // TODO(tapted): Investigate removing the IsImeTriggerEvent() check - it's
  // probably not required, but helps tests that expect some events to always
  // get processed (i.e. TextfieldTest.TextInputClientTest).
  if (hadMarkedTextAtKeyDown && IsImeTriggerEvent(theEvent))
    hasUnhandledKeyDownEvent_ = NO;

  // Even with marked text, some IMEs may follow with -insertNewLine:;
  // simultaneously confirming the composition. In this case, always generate
  // the corresponding ui::KeyEvent. Note this is done even if there was no
  // marked text, so it is orthogonal to the case above.
  if (wantsKeyHandledForInsert_)
    hasUnhandledKeyDownEvent_ = YES;

  // If |hasUnhandledKeyDownEvent_| wasn't set to NO during
  // -interpretKeyEvents:, it wasn't handled. Give Widget accelerators a chance
  // to handle it.
  [self handleUnhandledKeyDownAsKeyEvent];
  DCHECK(!hasUnhandledKeyDownEvent_);
  keyDownEvent_ = nil;
}

- (void)keyUp:(NSEvent*)theEvent {
  ui::KeyEvent event(theEvent);
  [self handleKeyEvent:&event];
}

- (void)flagsChanged:(NSEvent*)theEvent {
  if (theEvent.keyCode == 0) {
    // An event like this gets sent when sending some key commands via
    // AppleScript. Since 0 is VKEY_A, we end up interpreting this as Cmd+A
    // which is incorrect. The correct event for command up/down (keyCode = 55)
    // is also sent, so we should drop this one. See https://crbug.com/889618
    return;
  }
  ui::KeyEvent event(theEvent);
  [self handleKeyEvent:&event];
}

- (void)scrollWheel:(NSEvent*)theEvent {
  if (!bridge_)
    return;

  auto event = std::make_unique<ui::ScrollEvent>(theEvent);
  [self adjustUiEventLocation:event.get() fromNativeEvent:theEvent];

  // Aura updates tooltips with the help of aura::Window::AddPreTargetHandler().
  // Mac hooks in here.
  [self updateTooltipIfRequiredAt:event->location()];
  bridge_->host()->OnScrollEvent(std::move(event));
}

// Called when we get a three-finger swipe, and they're enabled in System
// Preferences.
- (void)swipeWithEvent:(NSEvent*)event {
  if (!bridge_)
    return;

  // themblsha: In my testing all three-finger swipes send only a single event
  // with a value of +/-1 on either axis. Diagonal swipes are not handled by
  // AppKit.

  // We need to invert deltas in order to match GestureEventDetails's
  // directions.
  ui::GestureEventDetails swipeDetails(ui::ET_GESTURE_SWIPE, -[event deltaX],
                                       -[event deltaY]);
  swipeDetails.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  swipeDetails.set_touch_points(3);

  gfx::PointF location = ui::EventLocationFromNative(event);
  // Note this uses the default unique_touch_event_id of 0 (Swipe events do not
  // support -[NSEvent eventNumber]). This doesn't seem like a real omission
  // because the three-finger swipes are instant and can't be tracked anyway.
  auto gestureEvent = std::make_unique<ui::GestureEvent>(
      location.x(), location.y(), ui::EventFlagsFromNative(event),
      ui::EventTimeFromNative(event), swipeDetails);
  bridge_->host()->OnGestureEvent(std::move(gestureEvent));
}

- (void)quickLookWithEvent:(NSEvent*)theEvent {
  if (!bridge_)
    return;

  const gfx::Point locationInContent =
      gfx::ToFlooredPoint(ui::EventLocationFromNative(theEvent));

  bool foundWord = false;
  gfx::DecoratedText decoratedWord;
  gfx::Point baselinePoint;
  bridge_->host_helper()->GetWordAt(locationInContent, &foundWord,
                                    &decoratedWord, &baselinePoint);
  if (!foundWord)
    return;

  NSPoint baselinePointAppKit = NSMakePoint(
      baselinePoint.x(), NSHeight([self frame]) - baselinePoint.y());
  [self showDefinitionForAttributedString:
            gfx::GetAttributedStringFromDecoratedText(decoratedWord)
                                  atPoint:baselinePointAppKit];
}

////////////////////////////////////////////////////////////////////////////////
// NSResponder Action Messages. Keep sorted according NSResponder.h (from the
// 10.9 SDK). The list should eventually be complete. Anything not defined will
// beep when interpretKeyEvents: would otherwise call it.
// TODO(tapted): Make this list complete, except for insert* methods which are
// dispatched as regular key events in doCommandBySelector:.

// views::Textfields are single-line only, map Paragraph and Document commands
// to Line. Also, Up/Down commands correspond to beginning/end of line.

// The insertText action message forwards to the TextInputClient unless a menu
// is active. Note that NSResponder's interpretKeyEvents: implementation doesn't
// direct insertText: through doCommandBySelector:, so this is still needed to
// handle the case when inputContext: is nil. When inputContext: returns non-nil
// text goes directly to insertText:replacementRange:.
- (void)insertText:(id)text {
  DCHECK_EQ(nil, [self inputContext]);
  [self insertTextInternal:text];
}

// Selection movement and scrolling.

- (void)moveForward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_FORWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveRight:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_RIGHT
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:0];
}

- (void)moveBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_BACKWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveLeft:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_LEFT
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:0];
}

- (void)moveUp:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_UP
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:0];
}

- (void)moveDown:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_DOWN
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:0];
}

- (void)moveWordForward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_FORWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveWordBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_BACKWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToBeginningOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:0];
}

- (void)moveToEndOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:0];
}

- (void)moveToBeginningOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveToBeginningOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)pageDown:(id)sender {
  // The pageDown: action message is bound to the key combination
  // [Option+PageDown].
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_DOWN
             keyCode:ui::VKEY_NEXT
             domCode:ui::DomCode::PAGE_DOWN
          eventFlags:ui::EF_ALT_DOWN];
}

- (void)pageUp:(id)sender {
  // The pageUp: action message is bound to the key combination [Option+PageUp].
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_UP
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:ui::EF_ALT_DOWN];
}

- (void)moveBackwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveForwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveWordForwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_FORWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveWordBackwardAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_WORD_BACKWARD_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_UNKNOWN
           domCode:ui::DomCode::NONE
        eventFlags:0];
}

- (void)moveUpAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveDownAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfLineAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToEndOfLineAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_END
           domCode:ui::DomCode::END
        eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)pageDownAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_NEXT
             domCode:ui::DomCode::PAGE_DOWN
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)pageUpAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveParagraphForwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveParagraphBackwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveWordRight:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_RIGHT
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveWordLeft:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_LEFT
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveRightAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveLeftAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveWordRightAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveWordLeftAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveToLeftEndOfLine:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveToEndOfLine:sender]
                              : [self moveToBeginningOfLine:sender];
}

- (void)moveToRightEndOfLine:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveToBeginningOfLine:sender]
                              : [self moveToEndOfLine:sender];
}

- (void)moveToLeftEndOfLineAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_)
      ? [self moveToEndOfLineAndModifySelection:sender]
      : [self moveToBeginningOfLineAndModifySelection:sender];
}

- (void)moveToRightEndOfLineAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_)
      ? [self moveToBeginningOfLineAndModifySelection:sender]
      : [self moveToEndOfLineAndModifySelection:sender];
}

// Graphical Element transposition

- (void)transpose:(id)sender {
  [self handleAction:ui::TextEditCommand::TRANSPOSE
             keyCode:ui::VKEY_T
             domCode:ui::DomCode::US_T
          eventFlags:ui::EF_CONTROL_DOWN];
}

// Deletions.

- (void)deleteForward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_FORWARD
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:0];
}

- (void)deleteBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_BACKWARD
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:0];
}

- (void)deleteWordForward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_WORD_FORWARD
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)deleteWordBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_WORD_BACKWARD
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)deleteToBeginningOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)deleteToEndOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_END_OF_LINE
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)deleteToBeginningOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)deleteToEndOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)yank:(id)sender {
  [self handleAction:ui::TextEditCommand::YANK
             keyCode:ui::VKEY_Y
             domCode:ui::DomCode::US_Y
          eventFlags:ui::EF_CONTROL_DOWN];
}

// Cancellation.

- (void)cancelOperation:(id)sender {
  [self handleAction:ui::TextEditCommand::INVALID_COMMAND
             keyCode:ui::VKEY_ESCAPE
             domCode:ui::DomCode::ESCAPE
          eventFlags:0];
}

// Support for Services in context menus.
// Currently we only support reading and writing plain strings.
- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  NSString* const utf8Type = base::mac::CFToNSCast(kUTTypeUTF8PlainText);
  BOOL canWrite =
      [sendType isEqualToString:utf8Type] && [self selectedRange].length > 0;
  BOOL canRead = [returnType isEqualToString:utf8Type];
  // Valid if (sendType, returnType) is either (string, nil), (nil, string),
  // or (string, string).
  BOOL valid = textInputClient_ && ((canWrite && (canRead || !returnType)) ||
                                    (canRead && (canWrite || !sendType)));
  return valid
             ? self
             : [super validRequestorForSendType:sendType returnType:returnType];
}

// NSServicesMenuRequestor protocol

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  // NB: The NSServicesMenuRequestor protocol has not (as of 10.14) been
  // upgraded to request UTIs rather than obsolete PboardType constants. Handle
  // either for when it is upgraded.
  DCHECK([types containsObject:NSStringPboardType] ||
         [types containsObject:base::mac::CFToNSCast(kUTTypeUTF8PlainText)]);
  if (!textInputClient_)
    return NO;

  gfx::Range selectionRange;
  if (!textInputClient_->GetSelectionRange(&selectionRange))
    return NO;

  base::string16 text;
  textInputClient_->GetTextFromRange(selectionRange, &text);
  return [pboard writeObjects:@[ base::SysUTF16ToNSString(text) ]];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSArray* objects =
      [pboard readObjectsForClasses:@[ [NSString class] ] options:0];
  DCHECK([objects count] == 1);
  [self insertText:[objects lastObject]];
  return YES;
}

// NSTextInputClient protocol implementation.

// IMPORTANT: Always null-check |textInputClient_|. It can change (or be
// cleared) in -setTextInputClient:, which requires informing AppKit that the
// -inputContext has changed and to update its raw pointer. However, the AppKit
// method which does that may also spin a nested run loop communicating with an
// IME window and cause it to *use* the exact same NSTextInputClient (i.e.,
// |self|) that we're trying to invalidate in -setTextInputClient:.
// See https://crbug.com/817097#c12 for further details on this atrocity.

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                   (NSRangePointer)actualRange {
  // On TouchBar Macs, the IME subsystem sometimes sends an invalid range with a
  // non-zero length. This will cause a DCHECK in gfx::Range, so repair it here.
  // See https://crbug.com/888782.
  if (range.location == NSNotFound)
    range.length = 0;

  gfx::Range actual_range;
  base::string16 substring = AttributedSubstringForRangeHelper(
      textInputClient_, gfx::Range(range), &actual_range);
  if (actualRange) {
    // To maintain consistency with NSTextView, return range {0,0} for an out of
    // bounds requested range.
    *actualRange =
        actual_range.IsValid() ? actual_range.ToNSRange() : NSMakeRange(0, 0);
  }
  return substring.empty()
             ? nil
             : [[[NSAttributedString alloc]
                   initWithString:base::SysUTF16ToNSString(substring)]
                   autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint {
  NOTIMPLEMENTED();
  return 0;
}

- (void)doCommandBySelector:(SEL)selector {
  // Like the renderer, handle insert action messages as a regular key dispatch.
  // This ensures, e.g., insertTab correctly changes focus between fields. This
  // handles:
  //  -insertTab:(id)sender
  //  -insertBacktab:
  //  -insertNewline:
  //  -insertParagraphSeparator:
  //  -insertNewlineIgnoringFieldEditor:
  //  -insertTabIgnoringFieldEditor:
  //  -insertLineBreak:
  //  -insertContainerBreak:
  //  -insertSingleQuoteIgnoringSubstitution:
  //  -insertDoubleQuoteIgnoringSubstitution:
  // It does not handle |-insertText:(id)insertString|, which is not a command.
  // I.e. AppKit invokes _either_ -insertText: or -doCommandBySelector:. Also
  // note -insertText: is only invoked if -inputContext: has returned nil.
  DCHECK_NE(@selector(insertText:), selector);
  if (keyDownEvent_ && [NSStringFromSelector(selector) hasPrefix:@"insert"]) {
    // When return is pressed during IME composition, engines typically first
    // confirm the composition with a series of -insertText:replacementRange:
    // calls. Then, some also invoke -insertNewLine: (some do not). If an engine
    // DOES invokes -insertNewLine:, we always want a corresponding VKEY_RETURN
    // ui::KeyEvent generated. If it does NOT follow with -insertNewLine:, the
    // VKEY_RETURN must be suppressed in keyDown:, since it typically will have
    // merely dismissed the IME window: the composition is still ongoing.
    // Setting this ensures keyDown: always generates a ui::KeyEvent.
    wantsKeyHandledForInsert_ = YES;
    return;  // Handle in -keyDown:.
  }

  if ([self respondsToSelector:selector]) {
    [self performSelector:selector withObject:nil];
    hasUnhandledKeyDownEvent_ = NO;
    return;
  }

  // For events that AppKit sends via doCommandBySelector:, first attempt to
  // handle as a Widget accelerator. Forward along the responder chain only if
  // the Widget doesn't handle it.
  if (![self handleUnhandledKeyDownAsKeyEvent])
    [[self nextResponder] doCommandBySelector:selector];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualNSRange {
  gfx::Range actualRange;
  gfx::Rect rect = GetFirstRectForRangeHelper(textInputClient_,
                                              gfx::Range(range), &actualRange);
  if (actualNSRange)
    *actualNSRange = actualRange.ToNSRange();
  return gfx::ScreenRectToNSRect(rect);
}

- (BOOL)hasMarkedText {
  return textInputClient_ && textInputClient_->HasCompositionText();
}

- (void)insertText:(id)text replacementRange:(NSRange)replacementRange {
  if (!bridge_ || !textInputClient_)
    return;

  textInputClient_->DeleteRange(gfx::Range(replacementRange));
  [self insertTextInternal:text];
}

- (NSRange)markedRange {
  if (!textInputClient_)
    return NSMakeRange(NSNotFound, 0);

  gfx::Range range;
  textInputClient_->GetCompositionTextRange(&range);
  return range.ToNSRange();
}

- (NSRange)selectedRange {
  if (!textInputClient_)
    return NSMakeRange(NSNotFound, 0);

  gfx::Range range;
  textInputClient_->GetSelectionRange(&range);
  return range.ToNSRange();
}

- (void)setMarkedText:(id)text
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  if (!textInputClient_)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];

  textInputClient_->DeleteRange(gfx::Range(replacementRange));
  ui::CompositionText composition;
  composition.text = base::SysNSStringToUTF16(text);
  composition.selection = gfx::Range(selectedRange);

  // Add an underline with text color and a transparent background to the
  // composition text. TODO(karandeepb): On Cocoa textfields, the target clause
  // of the composition has a thick underlines. The composition text also has
  // discontinous underlines for different clauses. This is also supported in
  // the Chrome renderer. Add code to extract underlines from |text| once our
  // render text implementation supports thick underlines and discontinous
  // underlines for consecutive characters. See http://crbug.com/612675.
  composition.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, [text length],
                      ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  textInputClient_->SetCompositionText(composition);
  hasUnhandledKeyDownEvent_ = NO;
}

- (void)unmarkText {
  if (!textInputClient_)
    return;

  textInputClient_->ConfirmCompositionText();
  hasUnhandledKeyDownEvent_ = NO;
}

- (NSArray*)validAttributesForMarkedText {
  return @[];
}

// NSUserInterfaceValidations protocol implementation.

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  ui::TextEditCommand command = GetTextEditCommandForMenuAction([item action]);

  if (command == ui::TextEditCommand::INVALID_COMMAND)
    return NO;

  if (textInputClient_)
    return textInputClient_->IsTextEditCommandEnabled(command);

  // views::Label does not implement the TextInputClient interface but still
  // needs to intercept the Copy and Select All menu actions.
  if (command != ui::TextEditCommand::COPY &&
      command != ui::TextEditCommand::SELECT_ALL)
    return NO;

  bool is_textual = false;
  bridge_->host()->GetIsFocusedViewTextual(&is_textual);
  return is_textual;
}

// NSDraggingSource protocol implementation.

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return NSDragOperationEvery;
}

- (void)draggingSession:(NSDraggingSession*)session
           endedAtPoint:(NSPoint)screenPoint
              operation:(NSDragOperation)operation {
  views_bridge_mac::DragDropClient* client = [self dragDropClient];
  if (client)
    client->EndDrag();
}

// NSAccessibility informal protocol implementation.

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    if (id accessible = bridge_->host_helper()->GetNativeViewAccessible())
      return @[ accessible ];
  }

  return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityHitTest:(NSPoint)point {
  return [bridge_->host_helper()->GetNativeViewAccessible()
      accessibilityHitTest:point];
}

- (id)accessibilityFocusedUIElement {
  if (!bridge_)
    return nil;
  return [bridge_->host_helper()->GetNativeViewAccessible()
              accessibilityFocusedUIElement];
}

@end
