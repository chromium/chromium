// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_NORMALIZED_GEOMETRY_H_
#define UI_VIEWS_LAYOUT_NORMALIZED_GEOMETRY_H_

#include <string>
#include <utility>

#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Insets;
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace views {

class SizeBounds;

// Represents a point in layout space - that is, a point on the main and cross
// axes of the layout (regardless of whether it is vertically or horizontally
// oriented.
class VIEWS_EXPORT NormalizedPoint {
 public:
  constexpr NormalizedPoint() = default;
  constexpr NormalizedPoint(int main, int cross) : main_(main), cross_(cross) {}

  constexpr int main() const { return main_; }
  void set_main(int main) { main_ = main; }

  constexpr int cross() const { return cross_; }
  void set_cross(int cross) { cross_ = cross; }

  void SetPoint(int main, int cross);
  void Offset(int delta_main, int delta_cross);

  bool operator==(const NormalizedPoint& other) const;
  bool operator!=(const NormalizedPoint& other) const;
  bool operator<(const NormalizedPoint& other) const;

  std::string ToString() const;

 private:
  int main_ = 0;
  int cross_ = 0;
};

// Represents a size in layout space - that is, a size on the main and cross
// axes of the layout (regardless of whether it is vertically or horizontally
// oriented.
class VIEWS_EXPORT NormalizedSize {
 public:
  constexpr NormalizedSize() = default;
  constexpr NormalizedSize(int main, int cross) : main_(main), cross_(cross) {}

  constexpr int main() const { return main_; }
  void set_main(int main) { main_ = main; }

  constexpr int cross() const { return cross_; }
  void set_cross(int cross) { cross_ = cross; }

  void SetSize(int main, int cross);
  void Enlarge(int delta_main, int delta_cross);
  void SetToMax(int main, int cross);
  void SetToMax(const NormalizedSize& other);
  void SetToMin(int main, int cross);
  void SetToMin(const NormalizedSize& other);

  constexpr bool is_empty() const { return main_ == 0 || cross_ == 0; }

  bool operator==(const NormalizedSize& other) const;
  bool operator!=(const NormalizedSize& other) const;
  bool operator<(const NormalizedSize& other) const;

  std::string ToString() const;

 private:
  int main_ = 0;
  int cross_ = 0;
};

// Represents insets in layout space - that is, insets on the main and cross
// axes of the layout (regardless of whether it is vertically or horizontally
// oriented.
class VIEWS_EXPORT NormalizedInsets {
 public:
  constexpr NormalizedInsets() = default;
  constexpr explicit NormalizedInsets(int all) : main_(all), cross_(all) {}
  constexpr NormalizedInsets(int main, int cross)
      : main_(main), cross_(cross) {}
  constexpr NormalizedInsets(const Inset1D& main, const Inset1D& cross)
      : main_(main), cross_(cross) {}
  constexpr NormalizedInsets(int main_leading,
                             int cross_leading,
                             int main_trailing,
                             int cross_trailing)
      : main_(main_leading, main_trailing),
        cross_(cross_leading, cross_trailing) {}

  constexpr int main_leading() const { return main_.leading(); }
  void set_main_leading(int main_leading) { main_.set_leading(main_leading); }

  constexpr int main_trailing() const { return main_.trailing(); }
  void set_main_trailing(int main_trailing) {
    main_.set_trailing(main_trailing);
  }

  constexpr int main_size() const { return main_.size(); }

  constexpr int cross_leading() const { return cross_.leading(); }
  void set_cross_leading(int cross_leading) {
    cross_.set_leading(cross_leading);
  }

  constexpr int cross_trailing() const { return cross_.trailing(); }
  void set_cross_trailing(int cross_trailing) {
    cross_.set_trailing(cross_trailing);
  }

  constexpr int cross_size() const { return cross_.size(); }

  const Inset1D& main() const { return main_; }
  void set_main(const Inset1D& main) { main_ = main; }

  const Inset1D& cross() const { return cross_; }
  void set_cross(const Inset1D& cross) { cross_ = cross; }

  bool operator==(const NormalizedInsets& other) const;
  bool operator!=(const NormalizedInsets& other) const;
  bool operator<(const NormalizedInsets& other) const;

  std::string ToString() const;

 private:
  Inset1D main_;
  Inset1D cross_;
};

// Represents size bounds in layout space - that is, a set of size bounds using
// the main and cross axes of the layout (regardless of whether it is vertically
// or horizontally oriented).
class VIEWS_EXPORT NormalizedSizeBounds {
 public:
  NormalizedSizeBounds();
  NormalizedSizeBounds(SizeBound main, SizeBound cross);
  explicit NormalizedSizeBounds(const NormalizedSize& size);
  NormalizedSizeBounds(const NormalizedSizeBounds& size_bounds);

  const SizeBound& main() const { return main_; }
  void set_main(SizeBound main) { main_ = std::move(main); }

  const SizeBound& cross() const { return cross_; }
  void set_cross(SizeBound cross) { cross_ = std::move(cross); }

  void Expand(int main, int cross);
  void Inset(const NormalizedInsets& insets);

  bool operator==(const NormalizedSizeBounds& other) const;
  bool operator!=(const NormalizedSizeBounds& other) const;
  bool operator<(const NormalizedSizeBounds& other) const;

  std::string ToString() const;

 private:
  SizeBound main_;
  SizeBound cross_;
};

// Represents a rectangle in layout space - that is, a rectangle whose
// dimensions align with the main and cross axis of the layout (regardless of
// whether the layout is vertically or horizontally oriented).
class VIEWS_EXPORT NormalizedRect {
 public:
  constexpr NormalizedRect() = default;
  constexpr NormalizedRect(const NormalizedPoint& origin,
                           const NormalizedSize& size)
      : origin_(origin), size_(size) {}
  constexpr NormalizedRect(const Span& main, const Span& cross)
      : origin_(main.start(), cross.start()),
        size_(main.length(), cross.length()) {}
  constexpr NormalizedRect(int origin_main,
                           int origin_cross,
                           int size_main,
                           int size_cross)
      : origin_(origin_main, origin_cross), size_(size_main, size_cross) {}

  constexpr int origin_main() const { return origin_.main(); }
  void set_origin_main(int main) { origin_.set_main(main); }

  constexpr int origin_cross() const { return origin_.cross(); }
  void set_origin_cross(int cross) { origin_.set_cross(cross); }

  constexpr const NormalizedPoint& origin() const { return origin_; }
  void set_origin(const NormalizedPoint& origin) { origin_ = origin; }

  constexpr int size_main() const { return size_.main(); }
  void set_size_main(int main) { size_.set_main(main); }

  constexpr int size_cross() const { return size_.cross(); }
  void set_size_cross(int cross) { size_.set_cross(cross); }

  constexpr const NormalizedSize& size() const { return size_; }
  void set_size(const NormalizedSize& size) { size_ = size; }

  constexpr int max_main() const { return origin_.main() + size_.main(); }
  constexpr int max_cross() const { return origin_.cross() + size_.cross(); }

  Span GetMainSpan() const;
  void SetMainSpan(const Span& span);
  void AlignMain(const Span& container,
                 LayoutAlignment alignment,
                 const Inset1D& margins = Inset1D());

  Span GetCrossSpan() const;
  void SetCrossSpan(const Span& span);
  void AlignCross(const Span& container,
                  LayoutAlignment alignment,
                  const Inset1D& margins = Inset1D());

  void SetRect(int origin_main,
               int origin_cross,
               int size_main,
               int size_cross);
  void SetByBounds(int origin_main,
                   int origin_cross,
                   int max_main,
                   int max_cross);
  void Inset(const NormalizedInsets& insets);
  void Inset(int main, int cross);
  void Inset(int main_leading,
             int cross_leading,
             int main_trailing,
             int cross_trailing);
  void Offset(int main, int cross);

  constexpr bool is_empty() const { return size_.is_empty(); }
  bool operator==(const NormalizedRect& other) const;
  bool operator!=(const NormalizedRect& other) const;
  bool operator<(const NormalizedRect& other) const;

  std::string ToString() const;

 private:
  NormalizedPoint origin_;
  NormalizedSize size_;
};

// Normalization and Denormalization -------------------------------------------

NormalizedPoint VIEWS_EXPORT Normalize(LayoutOrientation orientation,
                                       const gfx::Point& point);
gfx::Point VIEWS_EXPORT Denormalize(LayoutOrientation orientation,
                                    const NormalizedPoint& point);

NormalizedSize VIEWS_EXPORT Normalize(LayoutOrientation orientation,
                                      const gfx::Size& size);
gfx::Size VIEWS_EXPORT Denormalize(LayoutOrientation orientation,
                                   const NormalizedSize& size);

NormalizedSizeBounds VIEWS_EXPORT Normalize(LayoutOrientation orientation,
                                            const SizeBounds& bounds);
SizeBounds VIEWS_EXPORT Denormalize(LayoutOrientation orientation,
                                    const NormalizedSizeBounds& bounds);

NormalizedInsets VIEWS_EXPORT Normalize(LayoutOrientation orientation,
                                        const gfx::Insets& insets);
gfx::Insets VIEWS_EXPORT Denormalize(LayoutOrientation orientation,
                                     const NormalizedInsets& insets);

NormalizedRect VIEWS_EXPORT Normalize(LayoutOrientation orientation,
                                      const gfx::Rect& rect);
gfx::Rect VIEWS_EXPORT Denormalize(LayoutOrientation orientation,
                                   const NormalizedRect& rect);

// Convenience methods to get and set main and cross-axis elements of
// denormalized geometry elements.
int VIEWS_EXPORT GetMainAxis(LayoutOrientation orientation,
                             const gfx::Size& size);
int VIEWS_EXPORT GetCrossAxis(LayoutOrientation orientation,
                              const gfx::Size& size);
SizeBound VIEWS_EXPORT GetMainAxis(LayoutOrientation orientation,
                                   const SizeBounds& size);
SizeBound VIEWS_EXPORT GetCrossAxis(LayoutOrientation orientation,
                                    const SizeBounds& size);
void VIEWS_EXPORT SetMainAxis(gfx::Size* size,
                              LayoutOrientation orientation,
                              int main);
void VIEWS_EXPORT SetCrossAxis(gfx::Size* size,
                               LayoutOrientation orientation,
                               int cross);
void VIEWS_EXPORT SetMainAxis(SizeBounds* size,
                              LayoutOrientation orientation,
                              SizeBound main);
void VIEWS_EXPORT SetCrossAxis(SizeBounds* size,
                               LayoutOrientation orientation,
                               SizeBound cross);

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_NORMALIZED_GEOMETRY_H_
