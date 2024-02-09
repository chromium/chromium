// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_H_
#define UI_GFX_CANVAS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_text_property_value.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"

namespace cc {
class SkottieWrapper;
}  // namespace cc

namespace gfx {

class Rect;
class RectF;
class FontList;
class Point;
class PointF;
class Size;
class Transform;
class Vector2d;

// Canvas is a PaintCanvas wrapper that provides a number of methods for
// common operations used throughout an application built using ui/gfx.
//
// All methods that take integer arguments (as is used throughout views)
// end with Int. If you need to use methods provided by PaintCanvas, you'll
// need to do a conversion. In particular you'll need to use |SkIntToScalar()|,
// or if converting from a scalar to an integer |SkScalarRound()|.
//
// A handful of methods in this class are overloaded providing an additional
// argument of type SkBlendMode. SkBlendMode specifies how the
// source and destination colors are combined. Unless otherwise specified,
// the variant that does not take a SkBlendMode uses a transfer mode
// of kSrcOver_Mode.
class GFX_EXPORT Canvas {
 public:
  enum {
    // Specifies the alignment for text rendered with the DrawStringRect method.
    TEXT_ALIGN_LEFT = 1 << 0,
    TEXT_ALIGN_CENTER = 1 << 1,
    TEXT_ALIGN_RIGHT = 1 << 2,
    TEXT_ALIGN_TO_HEAD = 1 << 3,

    // Specifies the text consists of multiple lines.
    MULTI_LINE = 1 << 4,

    // By default DrawStringRect does not process the prefix ('&') character
    // specially. That is, the string "&foo" is rendered as "&foo". When
    // rendering text from a resource that uses the prefix character for
    // mnemonics, the prefix should be processed and can be rendered as an
    // underline (SHOW_PREFIX), or not rendered at all (HIDE_PREFIX).
    SHOW_PREFIX = 1 << 5,
    HIDE_PREFIX = 1 << 6,

    // Prevent ellipsizing
    NO_ELLIPSIS = 1 << 7,

    // Specifies if words can be split by new lines.
    // This only works with MULTI_LINE.
    CHARACTER_BREAKABLE = 1 << 8,

    // Instructs DrawStringRect() to not use subpixel rendering.  This is useful
    // when rendering text onto a fully- or partially-transparent background
    // that will later be blended with another image.
    NO_SUBPIXEL_RENDERING = 1 << 9,
  };

  // Creates an empty canvas with image_scale of 1x.
  Canvas();

  // Creates canvas with provided DIP |size| and |image_scale|.
  // If this canvas is not opaque, it's explicitly cleared to transparent before
  // being returned.
  Canvas(const Size& size, float image_scale, bool is_opaque);

  // Creates a Canvas backed by an |sk_canvas| with |image_scale_|.
  // |sk_canvas| is assumed to be already scaled based on |image_scale|
  // so no additional scaling is applied.
  // Note: the caller must ensure that sk_canvas outlives this object, or until
  // RecreateBackingCanvas is called.
  Canvas(cc::PaintCanvas* sk_canvas, float image_scale);

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  virtual ~Canvas();

  // Recreates the backing platform canvas with DIP |size| and |image_scale_|.
  // If the canvas is not opaque, it is explicitly cleared.
  // TODO(pkotwicz): Push the image_scale into skia::PlatformCanvas such that
  // this method can be private.
  void RecreateBackingCanvas(const Size& size,
                             float image_scale,
                             bool is_opaque);

  // Compute the size required to draw some text with the provided fonts.
  // Attempts to fit the text with the provided width and height. Increases
  // height and then width as needed to make the text fit. This method
  // supports multiple lines. On Skia only a line_height can be specified and
  // specifying a 0 value for it will cause the default height to be used.
  static void SizeStringInt(const std::u16string& text,
                            const FontList& font_list,
                            int* width,
                            int* height,
                            int line_height,
                            int flags);

  // This is same as SizeStringInt except that fractional size is returned.
  // See comment in GetStringWidthF for its usage.
  static void SizeStringFloat(const std::u16string& text,
                              const FontList& font_list,
                              float* width,
                              float* height,
                              int line_height,
                              int flags);

  // Returns the number of horizontal pixels needed to display the specified
  // |text| with |font_list|.
  static int GetStringWidth(const std::u16string& text,
                            const FontList& font_list);

  // This is same as GetStringWidth except that fractional width is returned.
  // Use this method for the scenario that multiple string widths need to be
  // summed up. This is because GetStringWidth returns the ceiled width and
  // adding multiple ceiled widths could cause more precision loss for certain
  // platform like Mac where the fractional width is used.
  static float GetStringWidthF(const std::u16string& text,
                               const FontList& font_list);

  // Returns the default text alignment to be used when drawing text on a
  // Canvas based on the directionality of the system locale language.
  // This function is used by Canvas::DrawStringRect when the text alignment
  // is not specified.
  //
  // This function returns either Canvas::TEXT_ALIGN_LEFT or
  // Canvas::TEXT_ALIGN_RIGHT.
  static int DefaultCanvasTextAlignment();

  // Unscales by the image scale factor (aka device scale factor), and returns
  // that factor.  This is useful when callers want to draw directly in the
  // native scale.
  float UndoDeviceScaleFactor();

  // Saves a copy of the drawing state onto a stack, operating on this copy
  // until a balanced call to Restore() is made.
  void Save();

  // As with Save(), except draws to a layer that is blended with the canvas
  // at the specified alpha once Restore() is called.
  // |layer_bounds| are the bounds of the layer relative to the current
  // transform.
  void SaveLayerAlpha(uint8_t alpha);
  void SaveLayerAlpha(uint8_t alpha, const Rect& layer_bounds);

  // Like SaveLayerAlpha but draws the layer with an arbitrary set of
  // PaintFlags once Restore() is called.
  void SaveLayerWithFlags(const cc::PaintFlags& flags);

  // Restores the drawing state after a call to Save*(). It is an error to
  // call Restore() more times than Save*().
  void Restore();

  // Applies |rect| to the current clip using the specified region |op|.
  void ClipRect(const Rect& rect, SkClipOp op = SkClipOp::kIntersect);
  void ClipRect(const RectF& rect, SkClipOp op = SkClipOp::kIntersect);

  // Adds |path| to the current clip. |do_anti_alias| is true if the clip
  // should be antialiased.
  void ClipPath(const SkPath& path, bool do_anti_alias);

  // Returns the bounds of the current clip (in local coordinates) in the
  // |bounds| parameter, and returns true if it is non empty.
  bool GetClipBounds(Rect* bounds);

  void Translate(const Vector2d& offset);

  void Scale(float x_scale, float y_scale);

  // Fills the entire canvas' bitmap (restricted to current clip) with
  // specified |color| using a transfer mode of SkBlendMode::kSrcOver.
  void DrawColor(SkColor color);

  // Fills the entire canvas' bitmap (restricted to current clip) with
  // specified |color| and |mode|.
  void DrawColor(SkColor color, SkBlendMode mode);

  // Fills |rect| with |color| using a transfer mode of
  // SkBlendMode::kSrcOver.
  void FillRect(const Rect& rect, SkColor color);

  // Fills |rect| with the specified |color| and |mode|.
  void FillRect(const Rect& rect, SkColor color, SkBlendMode mode);

  // Draws a single pixel rect in the specified region with the specified
  // color, using a transfer mode of SkBlendMode::kSrcOver.
  //
  // NOTE: if you need a single pixel line, use DrawLine.
  void DrawRect(const RectF& rect, SkColor color);

  // Draws a single pixel rect in the specified region with the specified
  // color and transfer mode.
  //
  // NOTE: if you need a single pixel line, use DrawLine.
  void DrawRect(const RectF& rect, SkColor color, SkBlendMode mode);

  // Draws the given rectangle with the given |flags| parameters.
  // DEPRECATED in favor of the RectF version below.
  // TODO(funkysidd): Remove this (http://crbug.com/553726)
  void DrawRect(const Rect& rect, const cc::PaintFlags& flags);

  // Draws the given rectangle with the given |flags| parameters.
  void DrawRect(const RectF& rect, const cc::PaintFlags& flags);

  // Draws a single pixel line with the specified color.
  // DEPRECATED in favor of the RectF version below.
  // TODO(funkysidd): Remove this (http://crbug.com/553726)
  void DrawLine(const Point& p1, const Point& p2, SkColor color);

  // Draws a single dip line with the specified color.
  void DrawLine(const PointF& p1, const PointF& p2, SkColor color);

  // Draws a line with the given |flags| parameters.
  // DEPRECATED in favor of the RectF version below.
  // TODO(funkysidd): Remove this (http://crbug.com/553726)
  void DrawLine(const Point& p1, const Point& p2, const cc::PaintFlags& flags);

  // Draws a line with the given |flags| parameters.
  void DrawLine(const PointF& p1,
                const PointF& p2,
                const cc::PaintFlags& flags);

  // Draws a line that's a single DIP. At fractional scale factors, this is
  // floored to the nearest integral number of pixels.
  void DrawSharpLine(PointF p1, PointF p2, SkColor color);

  // As above, but draws a single pixel at all scale factors.
  void Draw1pxLine(PointF p1, PointF p2, SkColor color);

  // Draws a circle with the given |flags| parameters.
  // DEPRECATED in favor of the RectF version below.
  // TODO(funkysidd): Remove this (http://crbug.com/553726)
  void DrawCircle(const Point& center_point,
                  int radius,
                  const cc::PaintFlags& flags);

  // Draws a circle with the given |flags| parameters.
  void DrawCircle(const PointF& center_point,
                  float radius,
                  const cc::PaintFlags& flags);

  // Draws the given rectangle with rounded corners of |radius| using the
  // given |flags| parameters. DEPRECATED in favor of the RectF version below.
  // TODO(mgiuca): Remove this (http://crbug.com/553726).
  void DrawRoundRect(const Rect& rect, int radius, const cc::PaintFlags& flags);

  // Draws the given rectangle with rounded corners of |radius| using the
  // given |flags| parameters.
  void DrawRoundRect(const RectF& rect,
                     float radius,
                     const cc::PaintFlags& flags);

  // Draws the given path using the given |flags| parameters.
  void DrawPath(const SkPath& path, const cc::PaintFlags& flags);

  // Draws an image with the origin at the specified location. The upper left
  // corner of the bitmap is rendered at the specified location.
  // Parameters are specified relative to current canvas scale not in pixels.
  // Thus, x is 2 pixels if canvas scale = 2 & |x| = 1.
  void DrawImageInt(const ImageSkia&, int x, int y);

  // Helper for DrawImageInt(..., flags) that constructs a temporary flags and
  // calls flags.setAlpha(alpha).
  void DrawImageInt(const ImageSkia&, int x, int y, uint8_t alpha);

  // Draws an image with the origin at the specified location, using the
  // specified flags. The upper left corner of the bitmap is rendered at the
  // specified location.
  // Parameters are specified relative to current canvas scale not in pixels.
  // Thus, |x| is 2 pixels if canvas scale = 2 & |x| = 1.
  void DrawImageInt(const ImageSkia& image,
                    int x,
                    int y,
                    const cc::PaintFlags& flags);

  // Draws a portion of an image in the specified location. The src parameters
  // correspond to the region of the bitmap to draw in the region defined
  // by the dest coordinates.
  //
  // If the width or height of the source differs from that of the destination,
  // the image will be scaled. When scaling down, a mipmap will be generated.
  // Set |filter| to use filtering for images, otherwise the nearest-neighbor
  // algorithm is used for resampling.
  //
  // An optional custom cc::PaintFlags can be provided.
  // Parameters are specified relative to current canvas scale not in pixels.
  // Thus, |x| is 2 pixels if canvas scale = 2 & |x| = 1.
  void DrawImageInt(const ImageSkia& image,
                    int src_x,
                    int src_y,
                    int src_w,
                    int src_h,
                    int dest_x,
                    int dest_y,
                    int dest_w,
                    int dest_h,
                    bool filter);
  void DrawImageInt(const ImageSkia& image,
                    int src_x,
                    int src_y,
                    int src_w,
                    int src_h,
                    int dest_x,
                    int dest_y,
                    int dest_w,
                    int dest_h,
                    bool filter,
                    const cc::PaintFlags& flags);

  // Same as the DrawImageInt functions above. Difference being this does not
  // do any scaling, i.e. it does not scale the output by the device scale
  // factor (the internal image_scale_). It takes an ImageSkiaRep instead of
  // an ImageSkia as the caller chooses the exact scale/pixel representation to
  // use, which will not be scaled while drawing it into the canvas.
  void DrawImageIntInPixel(const ImageSkiaRep& image_rep,
                           int dest_x,
                           int dest_y,
                           int dest_w,
                           int dest_h,
                           bool filter,
                           const cc::PaintFlags& flags);

  // Draws an |image| with the top left corner at |x| and |y|, clipped to
  // |path|.
  // Parameters are specified relative to current canvas scale not in pixels.
  // Thus, x is 2 pixels if canvas scale = 2 & |x| = 1.
  void DrawImageInPath(const ImageSkia& image,
                       int x,
                       int y,
                       const SkPath& path,
                       const cc::PaintFlags& flags);

  // Draws the frame of the |skottie| animation specified by the normalized time
  // instant t [0->first frame .. 1->last frame] onto the region corresponded by
  // |dst| in the canvas. |images| is a map from asset id to the corresponding
  // image to use when rendering this frame; it may be empty if this animation
  // frame does not contain any images in it.
  void DrawSkottie(scoped_refptr<cc::SkottieWrapper> skottie,
                   const Rect& dst,
                   float t,
                   cc::SkottieFrameDataMap images,
                   const cc::SkottieColorMap& color_map,
                   cc::SkottieTextPropertyValueMap text_map);

  // Draws text with the specified color, fonts and location. The text is
  // aligned to the left, vertically centered, clipped to the region. If the
  // text is too big, it is truncated and '...' is added to the end.
  void DrawStringRect(const std::u16string& text,
                      const FontList& font_list,
                      SkColor color,
                      const Rect& display_rect);

  // Draws text with the specified color, fonts and location. The last argument
  // specifies flags for how the text should be rendered. It can be one of
  // TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT or TEXT_ALIGN_LEFT.
  void DrawStringRectWithFlags(const std::u16string& text,
                               const FontList& font_list,
                               SkColor color,
                               const Rect& display_rect,
                               int flags);

  // Draws a |rect| in the specified region with the specified |color|. The
  // width of the stroke is |thickness| dip, but the actual pixel width will be
  // floored to ensure an integral value.
  void DrawSolidFocusRect(RectF rect, SkColor color, int thickness);

  // Tiles the image in the specified region.
  // Parameters are specified relative to current canvas scale not in pixels.
  // Thus, |x| is 2 pixels if canvas scale = 2 & |x| = 1.
  void TileImageInt(const ImageSkia& image,
                    int x,
                    int y,
                    int w,
                    int h);
  void TileImageInt(const ImageSkia& image,
                    int src_x,
                    int src_y,
                    int dest_x,
                    int dest_y,
                    int w,
                    int h,
                    float tile_scale = 1.0f,
                    SkTileMode tile_mode_x = SkTileMode::kRepeat,
                    SkTileMode tile_mode_y = SkTileMode::kRepeat,
                    cc::PaintFlags* flags = nullptr);

  // Helper for TileImageInt().  Initializes |flags| for tiling |image| with the
  // given parameters.  Returns false if the provided image does not have a
  // representation for the current scale.
  bool InitPaintFlagsForTiling(const ImageSkia& image,
                               int src_x,
                               int src_y,
                               float tile_scale_x,
                               float tile_scale_y,
                               int dest_x,
                               int dest_y,
                               SkTileMode tile_mode_x,
                               SkTileMode tile_mode_y,
                               cc::PaintFlags* flags);

  // Apply transformation on the canvas.
  void Transform(const Transform& transform);

  // Note that writing to this bitmap will modify pixels stored in this canvas.
  SkBitmap GetBitmap() const;

  // TODO(enne): rename sk_canvas members and interface.
  cc::PaintCanvas* sk_canvas() { return canvas_; }
  float image_scale() const { return image_scale_; }

 private:
  // Tests whether the provided rectangle intersects the current clip rect.
  bool IntersectsClipRect(const SkRect& rect);

  // Helper for the DrawImageInt functions declared above. The
  // |remove_image_scale| parameter indicates if the scale of the |image_rep|
  // should be removed when drawing the image, to avoid double-scaling it.
  void DrawImageIntHelper(const ImageSkiaRep& image_rep,
                          int src_x,
                          int src_y,
                          int src_w,
                          int src_h,
                          int dest_x,
                          int dest_y,
                          int dest_w,
                          int dest_h,
                          bool filter,
                          const cc::PaintFlags& flags,
                          bool remove_image_scale);
  cc::PaintCanvas* CreateOwnedCanvas(const Size& size, bool is_opaque);

  // The device scale factor at which drawing on this canvas occurs.
  // An additional scale can be applied via Canvas::Scale(). However,
  // Canvas::Scale() does not affect |image_scale_|.
  float image_scale_;

  // canvas_ is our active canvas object. Sometimes we are also the owner,
  // in which case bitmap_ and owned_canvas_ will be set. Other times we are
  // just borrowing someone else's canvas, in which case canvas_ will point
  // there but bitmap_ and owned_canvas_ will not exist.
  std::optional<SkBitmap> bitmap_;
  std::optional<cc::SkiaPaintCanvas> owned_canvas_;
  raw_ptr<cc::PaintCanvas> canvas_;
};

}  // namespace gfx

#endif  // UI_GFX_CANVAS_H_
