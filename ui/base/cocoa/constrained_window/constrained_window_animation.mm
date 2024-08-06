// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"

#include <stdint.h>
#include <stdlib.h>

#import "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/native_library.h"
#include "base/notreached.h"
#include "ui/gfx/animation/tween.h"

// The window animations in this file use private APIs as described here:
// https://github.com/MarkVillacampa/undocumented-goodness/blob/master/CoreGraphics/CGSPrivate.h
// There are two important things to keep in mind when modifying this file:
// - For most operations the origin of the coordinate system is top left.
// - Perspective and shear transformations get clipped if they are bigger
//   than the window size. This does not seem to apply to scale transformations.

// Length of the animation in seconds.
const NSTimeInterval kAnimationDuration = 0.18;

// The number of pixels above the final destination to animate from.
const CGFloat kShowHideVerticalOffset = 20;

// Scale the window by this factor when animating.
const CGFloat kShowHideScaleFactor = 0.99;

// Size of the perspective effect as a factor of the window width.
const CGFloat kShowHidePerspectiveFactor = 0.04;

// Forward declare private CoreGraphics APIs used to transform windows.
extern "C" {

typedef float float32;

typedef int32_t CGSWindow;
typedef int32_t CGSConnection;

typedef struct {
  float32 x;
  float32 y;
} MeshPoint;

typedef struct {
  MeshPoint local;
  MeshPoint global;
} CGPointWarp;

CGSConnection _CGSDefaultConnection();
CGError CGSSetWindowTransform(const CGSConnection cid,
                              const CGSWindow wid,
                              CGAffineTransform transform);
CGError CGSSetWindowWarp(const CGSConnection cid,
                         const CGSWindow wid,
                         int32_t w,
                         int32_t h,
                         CGPointWarp* mesh);
CGError CGSSetWindowAlpha(const CGSConnection cid,
                          const CGSWindow wid,
                          float32 alpha);

}  // extern "C"

namespace {

struct KeyFrame {
  float value;
  float scale;
};

// Get the window location relative to the top left of the main screen.
// Most Cocoa APIs use a coordinate system where the screen origin is the
// bottom left. The various CGSSetWindow* APIs use a coordinate system where
// the screen origin is the top left.
NSPoint GetCGSWindowScreenOrigin(NSWindow* window) {
  NSArray* screens = NSScreen.screens;
  if (screens.count == 0) {
    return NSZeroPoint;
  }
  // Origin is relative to the screen with the menu bar (the screen at index 0).
  // Note, this is not the same as mainScreen which is the screen with the key
  // window.
  NSScreen* main_screen = screens[0];

  NSRect window_frame = window.frame;
  NSRect screen_frame = main_screen.frame;
  return NSMakePoint(NSMinX(window_frame),
                     NSHeight(screen_frame) - NSMaxY(window_frame));
}

// Set the transparency of the window.
void SetWindowAlpha(NSWindow* window, float alpha) {
  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowAlpha(cid, static_cast<CGSWindow>(window.windowNumber), alpha);
}

// Scales the window and translates it so that it stays centered relative
// to its original position.
void SetWindowScale(NSWindow* window, float scale) {
  CGFloat scale_delta = 1.0 - scale;
  CGFloat cur_scale = 1.0 + scale_delta;
  CGAffineTransform transform =
      CGAffineTransformMakeScale(cur_scale, cur_scale);

  // Translate the window to keep it centered at the original location.
  NSSize window_size = window.frame.size;
  CGFloat scale_offset_x = window_size.width * (1 - cur_scale) / 2.0;
  CGFloat scale_offset_y = window_size.height * (1 - cur_scale) / 2.0;

  NSPoint origin = GetCGSWindowScreenOrigin(window);
  CGFloat new_x = -origin.x + scale_offset_x;
  CGFloat new_y = -origin.y + scale_offset_y;
  transform = CGAffineTransformTranslate(transform, new_x, new_y);

  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowTransform(cid, static_cast<CGSWindow>(window.windowNumber),
                        transform);
}

// Unsets any window warp that may have been previously applied.
// Window warp prevents other effects such as CGSSetWindowTransform from
// being applied.
void ClearWindowWarp(NSWindow* window) {
  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowWarp(cid, static_cast<CGSWindow>(window.windowNumber), 0, 0,
                   nullptr);
}

// Applies various transformations using a warp effect. The window is
// translated vertically by |y_offset|. The window is scaled by |scale| and
// translated so that the it remains centered relative to its original position.
// Finally, perspective is effect is applied by shrinking the top of the window.
void SetWindowWarp(NSWindow* window,
                   float y_offset,
                   float scale,
                   float perspective_offset) {
  NSRect win_rect = window.frame;
  win_rect.origin = NSZeroPoint;
  NSRect screen_rect = win_rect;
  screen_rect.origin = GetCGSWindowScreenOrigin(window);

  // Apply a vertical translate.
  screen_rect.origin.y -= y_offset;

  // Apply a scale and translate to keep the window centered.
  screen_rect.origin.x += (NSWidth(win_rect) - NSWidth(screen_rect)) / 2.0;
  screen_rect.origin.y += (NSHeight(win_rect) - NSHeight(screen_rect)) / 2.0;

  // A 2 x 2 mesh that maps each corner of the window to a location in screen
  // coordinates. Note that the origin of the coordinate system is top, left.
  CGPointWarp mesh[2][2] = {
      {{
           // Top left.
           {static_cast<float>(NSMinX(win_rect)),
            static_cast<float>(NSMinY(win_rect))},
           {static_cast<float>(NSMinX(screen_rect) + perspective_offset),
            static_cast<float>(NSMinY(screen_rect))},
       },
       {
           // Top right.
           {static_cast<float>(NSMaxX(win_rect)),
            static_cast<float>(NSMinY(win_rect))},
           {static_cast<float>(NSMaxX(screen_rect) - perspective_offset),
            static_cast<float>(NSMinY(screen_rect))},
       }},
      {{
           // Bottom left.
           {static_cast<float>(NSMinX(win_rect)),
            static_cast<float>(NSMaxY(win_rect))},
           {static_cast<float>(NSMinX(screen_rect)),
            static_cast<float>(NSMaxY(screen_rect))},
       },
       {
           // Bottom right.
           {static_cast<float>(NSMaxX(win_rect)),
            static_cast<float>(NSMaxY(win_rect))},
           {static_cast<float>(NSMaxX(screen_rect)),
            static_cast<float>(NSMaxY(screen_rect))},
       }},
  };

  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowWarp(cid, static_cast<CGSWindow>(window.windowNumber), 2, 2,
                   &(mesh[0][0]));
}

// Sets the various effects that are a part of the Show/Hide animation.
// Value is a number between 0 and 1 where 0 means the window is completely
// hidden and 1 means the window is fully visible.
void UpdateWindowShowHideAnimationState(NSWindow* window, CGFloat value) {
  CGFloat inverse_value = 1.0 - value;

  SetWindowAlpha(window, value);
  CGFloat y_offset = kShowHideVerticalOffset * inverse_value;
  CGFloat scale = 1.0 - (1.0 - kShowHideScaleFactor) * inverse_value;
  CGFloat perspective_offset =
      (window.frame.size.width * kShowHidePerspectiveFactor) * inverse_value;

  SetWindowWarp(window, y_offset, scale, perspective_offset);
}

bool AreWindowServerEffectsDisabled() {
  // If the CHROME_HEADLESS env variable is set, this code is running in a
  // test environment. The custom constrained window animations may be
  // causing the WindowServer to crash (https://crbug.com/828031), so use the
  // simple animations.
  static bool is_headless = getenv("CHROME_HEADLESS") != nullptr;
  return is_headless;
}

}  // namespace

@interface ConstrainedWindowAnimationBase ()
// Subclasses should override these to update the window state for the current
// animation value.
- (void)setWindowStateForStart;
- (void)setWindowStateForValue:(float)value;
- (void)setWindowStateForEnd;

@property(strong) NSWindow* window;

@end

@implementation ConstrainedWindowAnimationBase

@synthesize window = _window;

- (instancetype)initWithWindow:(NSWindow*)window {
  if ((self = [self initWithDuration:kAnimationDuration
                      animationCurve:NSAnimationEaseInOut])) {
    self.window = window;
    self.animationBlockingMode = NSAnimationBlocking;
    [self setWindowStateForStart];
  }
  return self;
}

- (void)stopAnimation {
  [super stopAnimation];
  [self setWindowStateForEnd];
  if ([self.delegate respondsToSelector:@selector(animationDidEnd:)]) {
    [self.delegate animationDidEnd:self];
  }
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];

  if (progress >= 1.0) {
    [self setWindowStateForEnd];

    // Starting in 10.10, the WindowServer forgets to draw the shadow on windows
    // that animate in this way on retina screens. -[NSWindow invalidateShadow]
    // doesn't fix it. Neither does toggling -setHasShadow:. But forcing an
    // update to the window size, and then undoing it, seems to fix the problem.
    // See http://crbug.com/436884.
    // TODO(tapted): Find a better fix (this is horrible).
    if (!AreWindowServerEffectsDisabled()) {
      NSRect frame = self.window.frame;
      [self.window setFrame:NSInsetRect(frame, 1, 1) display:NO animate:NO];
      [self.window setFrame:frame display:NO animate:NO];
    }
    return;
  }
  [self setWindowStateForValue:[self currentValue]];
}

- (void)setWindowStateForStart {
  // Subclasses can optionally override this method.
}

- (void)setWindowStateForValue:(float)value {
  // Subclasses must override this method.
  NOTREACHED();
}

- (void)setWindowStateForEnd {
  // Subclasses can optionally override this method.
}

@end

@implementation ConstrainedWindowAnimationShow

- (void)setWindowStateForStart {
  if (AreWindowServerEffectsDisabled()) {
    self.window.alphaValue = 0.0;
    return;
  }
  SetWindowAlpha(self.window, 0.0);
}

- (void)setWindowStateForValue:(float)value {
  if (AreWindowServerEffectsDisabled()) {
    self.window.alphaValue = value;
    return;
  }
  UpdateWindowShowHideAnimationState(self.window, value);
}

- (void)setWindowStateForEnd {
  if (AreWindowServerEffectsDisabled()) {
    self.window.alphaValue = 1.0;
    return;
  }
  SetWindowAlpha(self.window, 1.0);
  ClearWindowWarp(self.window);
}

@end

@implementation ConstrainedWindowAnimationHide

- (void)setWindowStateForValue:(float)value {
  if (AreWindowServerEffectsDisabled()) {
    self.window.alphaValue = 1.0 - value;
    return;
  }
  UpdateWindowShowHideAnimationState(self.window, 1.0 - value);
}

- (void)setWindowStateForEnd {
  if (AreWindowServerEffectsDisabled()) {
    self.window.alphaValue = 0.0;
    return;
  }
  SetWindowAlpha(self.window, 0.0);
  ClearWindowWarp(self.window);
}

@end

@implementation ConstrainedWindowAnimationPulse

// Sets the window scale based on the animation progress.
- (void)setWindowStateForValue:(float)value {
  if (AreWindowServerEffectsDisabled())
    return;

  KeyFrame frames[] = {
      {0.00, 1.0}, {0.40, 1.02}, {0.60, 1.02}, {1.00, 1.0},
  };

  CGFloat scale = 1;
  for (int i = std::size(frames) - 1; i >= 0; --i) {
    if (value >= frames[i].value) {
      CGFloat delta = frames[i + 1].value - frames[i].value;
      CGFloat frame_progress = (value - frames[i].value) / delta;
      scale = gfx::Tween::FloatValueBetween(frame_progress, frames[i].scale,
                                            frames[i + 1].scale);
      break;
    }
  }

  SetWindowScale(self.window, scale);
}

- (void)setWindowStateForEnd {
  if (AreWindowServerEffectsDisabled()) {
    NSBeep();
    return;
  }

  SetWindowScale(self.window, 1.0);
}

@end
