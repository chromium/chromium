// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_controller_cocoa_delegate_impl.h"

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/logging.h"
#import "base/message_loop/message_pump_apple.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/badge_painter.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/layout_provider.h"

namespace {

constexpr CGFloat kIPHDotSize = 6;

NSImage* NewTagImage(const ui::ColorProvider* color_provider) {
  // 1. Make the attributed string.

  NSString* badge_text = l10n_util::GetNSString(features::IsChromeRefresh2023()
                                                    ? IDS_NEW_BADGE_UPPERCASE
                                                    : IDS_NEW_BADGE);

  // The preferred font is slightly smaller and slightly more bold than the
  // menu font. The size change is required to make it look correct in the
  // badge; we add a small degree of bold to prevent color smearing/blurring
  // due to font smoothing. This ensures readability on all platforms and in
  // both light and dark modes.
  gfx::Font badge_font =
      views::BadgePainter::GetBadgeFont(views::MenuConfig::instance().font_list)
          .GetPrimaryFont();

  DCHECK(color_provider);
  NSColor* badge_text_color = skia::SkColorToSRGBNSColor(
      color_provider->GetColor(ui::kColorBadgeInCocoaMenuForeground));

  NSDictionary* badge_attrs = @{
    NSFontAttributeName : base::apple::CFToNSPtrCast(badge_font.GetCTFont()),
    NSForegroundColorAttributeName : badge_text_color,
  };

  NSMutableAttributedString* badge_attr_string =
      [[NSMutableAttributedString alloc] initWithString:badge_text
                                             attributes:badge_attrs];

  // 2. Calculate the size required.

  NSSize badge_size = [badge_attr_string size];
  badge_size.width = trunc(badge_size.width);
  badge_size.height = trunc(badge_size.height);

  badge_size.width += 2 * views::BadgePainter::kBadgeInternalPadding +
                      2 * views::BadgePainter::kBadgeHorizontalMargin;
  badge_size.height += views::BadgePainter::kBadgeInternalPaddingTopMac;

  // 3. Craft the image.

  return [NSImage
       imageWithSize:badge_size
             flipped:NO
      drawingHandler:^(NSRect dest_rect) {
        NSRect badge_frame = NSInsetRect(
            dest_rect, views::BadgePainter::kBadgeHorizontalMargin, 0);
        const int badge_radius =
            views::LayoutProvider::Get()->GetCornerRadiusMetric(
                views::ShapeContextTokens::kBadgeRadius);
        NSBezierPath* rounded_badge_rect =
            [NSBezierPath bezierPathWithRoundedRect:badge_frame
                                            xRadius:badge_radius
                                            yRadius:badge_radius];
        DCHECK(color_provider);
        NSColor* badge_color = skia::SkColorToSRGBNSColor(
            color_provider->GetColor(ui::kColorBadgeInCocoaMenuBackground));

        [badge_color set];
        [rounded_badge_rect fill];

        NSPoint badge_text_location = NSMakePoint(
            NSMinX(badge_frame) + views::BadgePainter::kBadgeInternalPadding,
            NSMinY(badge_frame) +
                views::BadgePainter::kBadgeInternalPaddingTopMac);
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

// In macOS 13 and earlier, the internals of menus are handled by HI Toolbox,
// and the bridge to that code is NSCarbonMenuImpl. Starting with macOS 14, the
// internals of menus are in NSCocoaMenuImpl. Abstract away into a protocol the
// (one) common method that this code uses that is present on both Impl classes.
@protocol CrNSMenuImpl <NSObject>
@optional
- (void)highlightItemAtIndex:(NSInteger)index;
@end

@interface NSMenu (Impl)
- (id<CrNSMenuImpl>)_menuImpl;
- (CGRect)_boundsIfOpen;
@end

// --- Private API end ---

@implementation MenuControllerCocoaDelegateImpl {
  NSMutableArray* __strong _menuObservers;
  gfx::Rect _anchorRect;
}

- (instancetype)init {
  if (self = [super init]) {
    _menuObservers = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)dealloc {
  for (NSObject* obj in _menuObservers) {
    [NSNotificationCenter.defaultCenter removeObserver:obj];
  }
}

- (void)setAnchorRect:(gfx::Rect)rect {
  _anchorRect = rect;
}

- (void)controllerWillAddItem:(NSMenuItem*)menuItem
                    fromModel:(ui::MenuModel*)model
                      atIndex:(size_t)index
            withColorProvider:(const ui::ColorProvider*)colorProvider {
  if (model->IsNewFeatureAt(index)) {
    NSTextAttachment* attachment = [[NSTextAttachment alloc] initWithData:nil
                                                                   ofType:nil];
    attachment.image = NewTagImage(colorProvider);
    NSSize newTagSize = attachment.image.size;

    // The baseline offset of the badge image to the menu text baseline.
    const int kBadgeBaselineOffset = features::IsChromeRefresh2023() ? -2 : -4;
    attachment.bounds = NSMakeRect(0, kBadgeBaselineOffset, newTagSize.width,
                                   newTagSize.height);

    NSMutableAttributedString* attrTitle =
        [[NSMutableAttributedString alloc] initWithString:menuItem.title];
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
          id<CrNSMenuImpl> menuImpl = [menu_obj _menuImpl];
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
            gfx::Rect screen_rect = self->_anchorRect;
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
        addObject:[NSNotificationCenter.defaultCenter
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
        addObject:[NSNotificationCenter.defaultCenter
                      addObserverForName:NSMenuDidEndTrackingNotification
                                  object:menu
                                   queue:nil
                              usingBlock:hidden_callback]];
  }
}

@end
