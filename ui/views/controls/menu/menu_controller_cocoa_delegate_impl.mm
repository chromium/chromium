// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_controller_cocoa_delegate_impl.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "base/message_loop/message_pump_mac.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/new_badge.h"

namespace {

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

@interface MenuControllerCocoaDelegateImpl () {
  NSMutableArray* _menuObservers;
  gfx::Rect _anchorRect;
}
@end

@implementation MenuControllerCocoaDelegateImpl

- (instancetype)init {
  if (self = [super init]) {
    _menuObservers = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)dealloc {
  for (NSObject* obj in _menuObservers) {
    [[NSNotificationCenter defaultCenter] removeObserver:obj];
  }

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

  // This list will be copied into callback blocks later if it's non-empty, but
  // since it's fairly small that's not a big deal.
  std::vector<ui::ElementIdentifier> element_ids;

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->IsAlertedAt(i)) {
      DCHECK(!alerted_index.has_value());
      alerted_index = i;
    }
    const ui::ElementIdentifier identifier = model->GetElementIdentifierAt(i);
    if (identifier) {
      element_ids.push_back(identifier);
    }
  }

  if (alerted_index.has_value() || !element_ids.empty()) {
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

            for (ui::ElementIdentifier element_id : element_ids) {
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

  if (!element_ids.empty()) {
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
            for (ui::ElementIdentifier element_id : element_ids) {
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
