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

  NSSize text_size = [badge_attr_string size];
  NSSize canvas_size = NSMakeSize(
      trunc(text_size.width) + 2 * views::BadgePainter::kBadgeInternalPadding +
          2 * views::BadgePainter::kBadgeHorizontalMargin,
      fmax(trunc(text_size.height), views::BadgePainter::kBadgeMinHeightCocoa));

  // 3. Craft the image.

  return [NSImage
       imageWithSize:canvas_size
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

        // Place the text rect at the center of the badge frame.
        NSPoint badge_text_location =
            NSMakePoint(NSMinX(badge_frame) +
                            (badge_frame.size.width - text_size.width) / 2.0,
                        NSMinY(badge_frame) +
                            (badge_frame.size.height - text_size.height) / 2.0);
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
// and the bridge to that code is NSCarbonMenuImpl. While in reality this is a
// class, abstract its method that is used by this code into a protocol.
@protocol CrNSCarbonMenuImpl <NSObject>

// Highlights the menu item at the provided index.
- (void)highlightItemAtIndex:(NSInteger)index;

@end

@interface NSMenu (Impl)

// Returns the impl. (If called on macOS 14 this would return a subclass of
// NSCocoaMenuImpl, but private API use is not needed on macOS 14.)
- (id<CrNSCarbonMenuImpl>)_menuImpl;

// Returns the bounds of the entire menu in screen coordinate space. Available
// on both Carbon and Cocoa impls, but always (incorrectly) returns a zero
// origin with the Cocoa impl. Therefore, do not use with macOS 14 or later.
- (CGRect)_boundsIfOpen;

@end

// --- Private API end ---

@implementation MenuControllerCocoaDelegateImpl {
  NSMutableArray* __strong _menuObservers;
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
    const int kBadgeBaselineOffset = -4;
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
  absl::optional<size_t> alertedIndex;

  // A map containing elements that need to be tracked, mapping from their
  // identifiers to their indexes in the menu.
  std::map<ui::ElementIdentifier, NSInteger> elementIds;

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->IsAlertedAt(i)) {
      CHECK(!alertedIndex.has_value())
          << "Mac menu code can only alert for one item in a menu";
      alertedIndex = i;
    }
    const ui::ElementIdentifier identifier = model->GetElementIdentifierAt(i);
    if (identifier) {
      elementIds.emplace(identifier, base::checked_cast<NSInteger>(i));
    }
  }

  // A weak reference to the menu for the two blocks. This shouldn't be
  // necessary, as there aren't any references back that make a retain cycle,
  // but it's hard to be fully convinced that such a cycle isn't possible now or
  // in the future with updates.
  __weak NSMenu* weakMenu = menu;

  if (alertedIndex.has_value() || !elementIds.empty()) {
    __block bool menuShown = false;
    auto shownCallback = ^(NSNotification* note) {
      NSMenu* strongMenu = weakMenu;
      if (!strongMenu) {
        return;
      }

      if (@available(macOS 14.0, *)) {
        // This early return handles two cases.
        //
        // 1. If this window isn't the window implementing the menu, this call
        //    to get the frame will return an empty rect. Return, as this would
        //    be the wrong window.
        //
        // 2. When the notification first fires, the layout isn't complete and
        //    the menu item bounds will be reported to have a width of 10. If
        //    the width is too small, early return, as the callback will happen
        //    again, that time with the correct bounds.
        if (strongMenu.numberOfItems &&
            NSWidth([strongMenu itemAtIndex:0].accessibilityFrame) < 20) {
          return;
        }
      }

      // The notification may fire more than once; only process the first
      // time.
      if (menuShown) {
        return;
      }
      menuShown = true;

      if (alertedIndex.has_value()) {
        const auto index = base::checked_cast<NSInteger>(alertedIndex.value());
        if (@available(macOS 14.0, *)) {
          [strongMenu itemAtIndex:index].accessibilitySelected = true;
        } else {
          [strongMenu._menuImpl highlightItemAtIndex:index];
        }
      }

      if (@available(macOS 14.0, *)) {
        for (auto [elementId, index] : elementIds) {
          NSRect frame = [strongMenu itemAtIndex:index].accessibilityFrame;
          ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
              strongMenu, elementId, gfx::ScreenRectFromNSRect(frame));
        }
      } else {
        // macOS 13 and earlier use the old Carbon Menu Manager, and getting the
        // bounds of menus is pretty wackadoodle.
        //
        // Even though with macOS 11 through macOS 13 watching for
        // NSWindowDidOrderOnScreenAndFinishAnimatingNotification would work to
        // guarantee that the menu is on the screen and can be queried for size,
        // with macOS 10.15, there's no involvement from Cocoa, so there's no
        // good notification that the menu window was shown. Therefore, rely on
        // NSMenuDidBeginTrackingNotification, but then spin the event loop
        // once. This practically guarantees that the menu is on screen and can
        // be queried for size.
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC),
            dispatch_get_main_queue(), ^{
              gfx::Rect bounds =
                  gfx::ScreenRectFromNSRect(strongMenu._boundsIfOpen);
              for (auto [elementId, index] : elementIds) {
                ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
                    strongMenu, elementId, bounds);
              }
            });
      };
    };

    if (@available(macOS 14.0, *)) {
      NSString* notificationName =
          @"NSWindowDidOrderOnScreenAndFinishAnimatingNotification";
      [_menuObservers addObject:[NSNotificationCenter.defaultCenter
                                    addObserverForName:notificationName
                                                object:nil
                                                 queue:nil
                                            usingBlock:shownCallback]];
    } else {
      [_menuObservers
          addObject:[NSNotificationCenter.defaultCenter
                        addObserverForName:NSMenuDidBeginTrackingNotification
                                    object:menu
                                     queue:nil
                                usingBlock:shownCallback]];
    }
  }

  if (!elementIds.empty()) {
    auto hiddenCallback = ^(NSNotification* note) {
      NSMenu* strongMenu = weakMenu;
      if (!strongMenu) {
        return;
      }

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
            for (auto [elementId, index] : elementIds) {
              ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
                  strongMenu, elementId);
            }
          });
    };

    [_menuObservers
        addObject:[NSNotificationCenter.defaultCenter
                      addObserverForName:NSMenuDidEndTrackingNotification
                                  object:menu
                                   queue:nil
                              usingBlock:hiddenCallback]];
  }
}

@end
