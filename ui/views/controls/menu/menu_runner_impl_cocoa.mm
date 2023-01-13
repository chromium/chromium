// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#include <dispatch/dispatch.h>

#include "base/i18n/rtl.h"
#include "base/mac/mac_util.h"
#import "base/message_loop/message_pump_mac.h"
#include "base/numerics/safe_conversions.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/controls/menu/new_badge.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr CGFloat kNativeCheckmarkWidth = 18;
constexpr CGFloat kNativeMenuItemHeight = 18;
constexpr CGFloat kIPHDotSize = 6;

NSImage* NewTagImage(const ui::ColorProvider* color_provider) {
  // 1. Make the attributed string.

  NSString* badge_text = l10n_util::GetNSString(IDS_NEW_BADGE);

  // The preferred font is slightly smaller and slightly more bold than the
  // menu font. The size change is required to make it look correct in the
  // badge; we add a small degree of bold to prevent color smearing/blurring
  // due to font smoothing. This ensures readability on all platforms and in
  // both light and dark modes.
  gfx::Font badge_font = gfx::Font(
      new gfx::PlatformFontMac(gfx::PlatformFontMac::SystemFontType::kMenu));
  badge_font = badge_font.Derive(views::NewBadge::kNewBadgeFontSizeAdjustment,
                                 gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM);

  DCHECK(color_provider);
  NSColor* badge_text_color = skia::SkColorToSRGBNSColor(
      color_provider->GetColor(ui::kColorButtonBackgroundProminent));

  NSDictionary* badge_attrs = @{
    NSFontAttributeName : badge_font.GetNativeFont(),
    NSForegroundColorAttributeName : badge_text_color,
  };

  NSMutableAttributedString* badge_attr_string =
      [[NSMutableAttributedString alloc] initWithString:badge_text
                                             attributes:badge_attrs];

  // 2. Calculate the size required.

  NSSize badge_size = [badge_attr_string size];
  badge_size.width = trunc(badge_size.width);
  badge_size.height = trunc(badge_size.height);

  badge_size.width += 2 * views::NewBadge::kNewBadgeInternalPadding +
                      2 * views::NewBadge::kNewBadgeHorizontalMargin;
  badge_size.height += views::NewBadge::kNewBadgeInternalPaddingTopMac;

  // 3. Craft the image.

  return [NSImage
       imageWithSize:badge_size
             flipped:NO
      drawingHandler:^(NSRect dest_rect) {
        NSRect badge_frame = NSInsetRect(
            dest_rect, views::NewBadge::kNewBadgeHorizontalMargin, 0);
        NSBezierPath* rounded_badge_rect = [NSBezierPath
            bezierPathWithRoundedRect:badge_frame
                              xRadius:views::NewBadge::kNewBadgeCornerRadius
                              yRadius:views::NewBadge::kNewBadgeCornerRadius];
        DCHECK(color_provider);
        NSColor* badge_color = skia::SkColorToSRGBNSColor(
            color_provider->GetColor(ui::kColorButtonBackgroundProminent));
        [badge_color set];
        [rounded_badge_rect fill];

        NSPoint badge_text_location = NSMakePoint(
            NSMinX(badge_frame) + views::NewBadge::kNewBadgeInternalPadding,
            NSMinY(badge_frame) +
                views::NewBadge::kNewBadgeInternalPaddingTopMac);
        [badge_attr_string drawAtPoint:badge_text_location];

        return YES;
      }];
}

NSImage* IPHDotImage(const ui::ColorProvider* color_provider) {
  // Embed horizontal centering space as NSMenuItem will otherwise left-align
  // it.
  return [NSImage
       imageWithSize:NSMakeSize(2 * kIPHDotSize, kIPHDotSize)
             flipped:NO
      drawingHandler:^(NSRect dest_rect) {
        NSBezierPath* dot_path = [NSBezierPath
            bezierPathWithOvalInRect:NSMakeRect(kIPHDotSize / 2, 0, kIPHDotSize,
                                                kIPHDotSize)];
        NSColor* dot_color = skia::SkColorToSRGBNSColor(
            color_provider->GetColor(ui::kColorButtonBackgroundProminent));
        [dot_color set];
        [dot_path fill];

        return YES;
      }];
}

}  // namespace

// --- Private API begin ---

@interface NSCarbonMenuImpl : NSObject
- (void)highlightItemAtIndex:(NSInteger)index;
@end

@interface NSMenu ()
- (NSCarbonMenuImpl*)_menuImpl;
- (CGRect)_boundsIfOpen;
@end

// --- Private API end ---

// An NSTextAttachmentCell to show the [New] tag on a menu item.
//
// /!\ WARNING /!\
//
// Do NOT update to the "new in macOS 10.11" API of NSTextAttachment.image until
// macOS 10.15 is the minimum required macOS for Chromium. Because menus are
// Carbon-based, the new NSTextAttachment.image API did not function correctly
// until then. Specifically, in macOS 10.11-10.12, images that use the new API
// do not appear. In macOS 10.13-10.14, the flipped flag of -[NSImage
// imageWithSize:flipped:drawingHandler:] is not respected. Only when 10.15 is
// the minimum required OS can https://crrev.com/c/2572937 be relanded.
@interface NewTagAttachmentCell : NSTextAttachmentCell
@end

@implementation NewTagAttachmentCell

- (instancetype)initWithColorProvider:(const ui::ColorProvider*)colorProvider {
  if (self = [super init]) {
    self.image = NewTagImage(colorProvider);
  }
  return self;
}

- (NSPoint)cellBaselineOffset {
  return NSMakePoint(0, views::NewBadge::kNewBadgeBaselineOffsetMac);
}

- (NSSize)cellSize {
  return [self.image size];
}

@end

@interface IdentifierContainer : NSObject
- (std::vector<ui::ElementIdentifier>&)ids;
@end

@implementation IdentifierContainer {
  std::vector<ui::ElementIdentifier> _ids;
}
- (std::vector<ui::ElementIdentifier>&)ids {
  return _ids;
}
@end

@interface MenuControllerDelegate : NSObject <MenuControllerCocoaDelegate> {
  NSMutableArray* _menuObservers;
  gfx::Rect _anchorRect;
}
@end

@implementation MenuControllerDelegate

- (instancetype)init {
  if (self = [super init]) {
    _menuObservers = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)dealloc {
  for (NSObject* obj in _menuObservers)
    [[NSNotificationCenter defaultCenter] removeObserver:obj];

  [_menuObservers release];

  [super dealloc];
}

- (void)setAnchorRect:(gfx::Rect)rect {
  _anchorRect = rect;
}

- (void)controllerWillAddItem:(NSMenuItem*)menuItem
                    fromModel:(ui::MenuModel*)model
                      atIndex:(size_t)index
            withColorProvider:(const ui::ColorProvider*)colorProvider {
  if (model->IsNewFeatureAt(index)) {
    NSMutableAttributedString* attrTitle = [[[NSMutableAttributedString alloc]
        initWithString:menuItem.title] autorelease];

    // /!\ WARNING /!\ Do not update this to use NSTextAttachment.image until
    // macOS 10.15 is the minimum required OS. See the details on the class
    // comment above.
    NSTextAttachment* attachment =
        [[[NSTextAttachment alloc] init] autorelease];
    attachment.attachmentCell = [[[NewTagAttachmentCell alloc]
        initWithColorProvider:colorProvider] autorelease];

    [attrTitle
        appendAttributedString:[NSAttributedString
                                   attributedStringWithAttachment:attachment]];

    menuItem.attributedTitle = attrTitle;
  }

  if (model->IsAlertedAt(index)) {
    NSImage* iphDotImage = IPHDotImage(colorProvider);
    menuItem.onStateImage = iphDotImage;
    menuItem.offStateImage = iphDotImage;
    menuItem.mixedStateImage = iphDotImage;
  }
}

- (void)controllerWillAddMenu:(NSMenu*)menu fromModel:(ui::MenuModel*)model {
  absl::optional<size_t> alerted_index;
  IdentifierContainer* const element_ids =
      [[[IdentifierContainer alloc] init] autorelease];
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->IsAlertedAt(i)) {
      DCHECK(!alerted_index.has_value());
      alerted_index = i;
    }
    const ui::ElementIdentifier identifier = model->GetElementIdentifierAt(i);
    if (identifier)
      [element_ids ids].push_back(identifier);
  }

  if (alerted_index.has_value() || ![element_ids ids].empty()) {
    auto shown_callback = ^(NSNotification* note) {
      NSMenu* const menu_obj = note.object;
      if (alerted_index.has_value()) {
        if ([menu respondsToSelector:@selector(_menuImpl)]) {
          NSCarbonMenuImpl* menuImpl = [menu_obj _menuImpl];
          if ([menuImpl respondsToSelector:@selector(highlightItemAtIndex:)]) {
            const auto index =
                base::checked_cast<NSInteger>(alerted_index.value());
            [menuImpl highlightItemAtIndex:index];
          }
        }
      }

      // This situation is broken.
      //
      // First, NSMenuDidBeginTrackingNotification is the best way to get called
      // right before the menu is shown, but at the moment of the call, the menu
      // isn't open yet. Second, to make things worse, the implementation of
      // -_boundsIfOpen *tries* to return an NSZeroRect if the menu isn't open
      // yet but fails to detect it correctly, and instead falls over and
      // returns a bogus bounds. Fortunately, those bounds are broken in a
      // predictable way, so that situation can be detected. Don't even bother
      // trying to make the -_boundsIfOpen call on the notification; there's no
      // point.
      //
      // However, it takes just one trip through the main loop for the menu to
      // appear and the -_boundsIfOpen call to work.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC),
          dispatch_get_main_queue(), ^{
            // Even though all supported macOS releases have `-_boundsIfOpen`,
            // because it's not official API, retain the fallback code written
            // for earlier versions of macOSes.
            //
            // The fallback bounds are intentionally twice as wide as they
            // should be because even though we could check the RTL bit and
            // guess whether the menu should appear to the left or right of the
            // anchor, if the anchor is near one side of the screen the menu
            // could end up on the other side.
            gfx::Rect screen_rect = _anchorRect;
            CGSize menu_size = [menu_obj size];
            screen_rect.Inset(gfx::Insets::TLBR(
                0, -menu_size.width, -menu_size.height, -menu_size.width));
            if ([menu_obj respondsToSelector:@selector(_boundsIfOpen)]) {
              CGRect bounds = [menu_obj _boundsIfOpen];
              // A broken bounds for a menu that isn't
              // actually yet open looks like: {{zeroish,
              // main display height}, {zeroish, zeroish}}.
              auto is_zeroish = [](CGFloat f) { return f >= 0 && f < 0.00001; };
              if (is_zeroish(bounds.origin.x) && bounds.origin.y > 300 &&
                  is_zeroish(bounds.size.width) &&
                  is_zeroish(bounds.size.height)) {
                // FYI, this never actually happens.
                LOG(ERROR) << "Get menu bounds failed.";
              } else {
                screen_rect = gfx::ScreenRectFromNSRect(bounds);
              }
            }

            for (ui::ElementIdentifier element_id : [element_ids ids]) {
              ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
                  menu_obj, element_id, screen_rect);
            }
          });
    };

    [_menuObservers
        addObject:[[NSNotificationCenter defaultCenter]
                      addObserverForName:NSMenuDidBeginTrackingNotification
                                  object:menu
                                   queue:nil
                              usingBlock:shown_callback]];
  }

  if (![element_ids ids].empty()) {
    auto hidden_callback = ^(NSNotification* note) {
      NSMenu* const menu_obj = note.object;
      // We expect to see the following order of events:
      // - element shown
      // - element activated (optional)
      // - element hidden
      // However, the code that detects menu item activation is called *after*
      // the current callback. To make sure the events happen in the right order
      // we'll defer processing of element hidden events until the end of the
      // current system event queue.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC),
          dispatch_get_main_queue(), ^{
            for (ui::ElementIdentifier element_id : [element_ids ids]) {
              ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
                  menu_obj, element_id);
            }
          });
    };

    [_menuObservers
        addObject:[[NSNotificationCenter defaultCenter]
                      addObserverForName:NSMenuDidEndTrackingNotification
                                  object:menu
                                   queue:nil
                              usingBlock:hidden_callback]];
  }
}

@end

namespace views::internal {
namespace {

// Returns the first item in |menu_controller|'s menu that will be checked.
NSMenuItem* FirstCheckedItem(MenuControllerCocoa* menu_controller) {
  for (NSMenuItem* item in [[menu_controller menu] itemArray]) {
    if ([menu_controller model]->IsItemCheckedAt(
            base::checked_cast<size_t>([item tag]))) {
      return item;
    }
  }
  return nil;
}

// Places a temporary, hidden NSView at |screen_bounds| within |window|. Used
// with -[NSMenu popUpMenuPositioningItem:atLocation:inView:] to position the
// menu for a combobox. The caller must remove the returned NSView from its
// superview when the menu is closed.
base::scoped_nsobject<NSView> CreateMenuAnchorView(
    NSWindow* window,
    const gfx::Rect& screen_bounds,
    NSMenuItem* checked_item,
    CGFloat actual_menu_width,
    MenuAnchorPosition position) {
  NSRect rect = gfx::ScreenRectToNSRect(screen_bounds);
  rect = [window convertRectFromScreen:rect];
  rect = [[window contentView] convertRect:rect fromView:nil];

  // If there's no checked item (e.g. Combobox::STYLE_ACTION), NSMenu will
  // anchor at the top left of the frame. Action buttons should anchor below.
  if (!checked_item) {
    rect.size.height = 0;
    if (base::i18n::IsRTL())
      rect.origin.x += rect.size.width;
  } else {
    // To ensure a consistent anchoring that's vertically centered in the
    // bounds, fix the height to be the same as a menu item.
    rect.origin.y = NSMidY(rect) - kNativeMenuItemHeight / 2;
    rect.size.height = kNativeMenuItemHeight;
    if (base::i18n::IsRTL()) {
      // The Views menu controller flips the MenuAnchorPosition value from left
      // to right in RTL. NSMenu does this automatically: the menu opens to the
      // left of the anchor, but AppKit doesn't account for the anchor width.
      // So the width needs to be added to anchor at the right of the view.
      // Note the checkmark width is not also added - it doesn't quite line up
      // the text. A Yosemite NSComboBox doesn't line up in RTL either: just
      // adding the width is a good match for the native behavior.
      rect.origin.x += rect.size.width;
    } else {
      rect.origin.x -= kNativeCheckmarkWidth;
    }
  }

  // When the actual menu width is larger than the anchor, right alignment
  // should be respected.
  if (actual_menu_width > rect.size.width &&
      position == views::MenuAnchorPosition::kTopRight &&
      !base::i18n::IsRTL()) {
    int width_diff = actual_menu_width - rect.size.width;
    rect.origin.x -= width_diff;
  }
  // A plain NSView will anchor below rather than "over", so use an NSButton.
  base::scoped_nsobject<NSView> anchor_view(
      [[NSButton alloc] initWithFrame:rect]);
  [anchor_view setHidden:YES];
  [[window contentView] addSubview:anchor_view];
  return anchor_view;
}

// Returns an appropriate event (with a location) suitable for showing a context
// menu. Uses [NSApp currentEvent] if it's a non-nil mouse click event,
// otherwise creates an autoreleased dummy event located at |anchor|.
NSEvent* EventForPositioningContextMenu(const gfx::Rect& anchor,
                                        NSWindow* window) {
  NSEvent* event = [NSApp currentEvent];
  switch ([event type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
      return event;
    default:
      break;
  }
  NSPoint location_in_window = ui::ConvertPointFromScreenToWindow(
      window, gfx::ScreenPointToNSPoint(anchor.CenterPoint()));
  return [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                            location:location_in_window
                       modifierFlags:0
                           timestamp:0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:0];
}

}  // namespace

// static
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32_t run_types,
    base::RepeatingClosure on_menu_closed_callback) {
  if ((run_types & MenuRunner::CONTEXT_MENU) &&
      !(run_types & (MenuRunner::IS_NESTED))) {
    return new MenuRunnerImplCocoa(menu_model,
                                   std::move(on_menu_closed_callback));
  }
  return new MenuRunnerImplAdapter(menu_model,
                                   std::move(on_menu_closed_callback));
}

MenuRunnerImplCocoa::MenuRunnerImplCocoa(
    ui::MenuModel* menu,
    base::RepeatingClosure on_menu_closed_callback)
    : on_menu_closed_callback_(std::move(on_menu_closed_callback)) {
  menu_delegate_.reset([[MenuControllerDelegate alloc] init]);
  menu_controller_.reset([[MenuControllerCocoa alloc]
               initWithModel:menu
                    delegate:menu_delegate_.get()
      useWithPopUpButtonCell:NO]);
}

bool MenuRunnerImplCocoa::IsRunning() const {
  return running_;
}

void MenuRunnerImplCocoa::Release() {
  if (IsRunning()) {
    if (delete_after_run_)
      return;  // We already canceled.

    delete_after_run_ = true;

    // Reset |menu_controller_| to ensure it clears itself as a delegate to
    // prevent NSMenu attempting to access the weak pointer to the ui::MenuModel
    // it holds (which is not owned by |this|). Toolkit-views menus use
    // MenuRunnerImpl::empty_delegate_ to handle this case.
    [menu_controller_ cancel];
    menu_controller_.reset();
  } else {
    delete this;
  }
}

void MenuRunnerImplCocoa::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    int32_t run_types,
    gfx::NativeView native_view_for_gestures,
    absl::optional<gfx::RoundedCornersF> corners) {
  DCHECK(!IsRunning());
  DCHECK(parent);
  closing_event_time_ = base::TimeTicks();
  running_ = true;
  [menu_delegate_ setAnchorRect:bounds];

  // Ensure the UI can update while the menu is fading out.
  base::ScopedPumpMessagesInPrivateModes pump_private;

  NSWindow* window = parent->GetNativeWindow().GetNativeNSWindow();
  NSView* view = parent->GetNativeView().GetNativeNSView();
  [menu_controller_ maybeBuildWithColorProvider:parent->GetColorProvider()];
  NSMenu* const menu = [menu_controller_ menu];
  if (run_types & MenuRunner::CONTEXT_MENU) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuWillShow(
        menu, views::ElementTrackerViews::GetContextForWidget(parent));

    [NSMenu popUpContextMenu:menu
                   withEvent:EventForPositioningContextMenu(bounds, window)
                     forView:view];

    ui::ElementTrackerMac::GetInstance()->NotifyMenuDoneShowing(menu);

  } else if (run_types & MenuRunner::COMBOBOX) {
    NSMenuItem* checked_item = FirstCheckedItem(menu_controller_);
    base::scoped_nsobject<NSView> anchor_view(CreateMenuAnchorView(
        window, bounds, checked_item, menu.size.width, anchor));
    [menu setMinimumWidth:bounds.width() + kNativeCheckmarkWidth];
    [menu popUpMenuPositioningItem:checked_item
                        atLocation:NSZeroPoint
                            inView:anchor_view];

    [anchor_view removeFromSuperview];
  } else {
    NOTREACHED();
  }

  closing_event_time_ = ui::EventTimeForNow();
  running_ = false;

  if (delete_after_run_) {
    delete this;
    return;
  }

  // Don't invoke the callback if Release() was called, since that usually means
  // the owning instance is being destroyed.
  if (!on_menu_closed_callback_.is_null())
    on_menu_closed_callback_.Run();
}

void MenuRunnerImplCocoa::Cancel() {
  [menu_controller_ cancel];
}

base::TimeTicks MenuRunnerImplCocoa::GetClosingEventTime() const {
  return closing_event_time_;
}

MenuRunnerImplCocoa::~MenuRunnerImplCocoa() = default;

}  // namespace views::internal
