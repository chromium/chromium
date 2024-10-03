// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_vector_icon.h"

#include <algorithm>
#include <map>
#include <tuple>

#include "base/containers/span.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"

namespace gfx {

namespace {

// The default size of a single side of the square canvas to which path
// coordinates are relative, in device independent pixels.
constexpr int kReferenceSizeDip = 48;

constexpr int kEmptyIconSize = -1;

// Retrieves the specified CANVAS_DIMENSIONS size from a PathElement.
int GetCanvasDimensions(const base::span<const PathElement>& path) {
  if (path.empty()) {
    return kEmptyIconSize;
  }
  return path[0].command == CANVAS_DIMENSIONS ? path[1].arg : kReferenceSizeDip;
}

// Retrieves the appropriate icon representation to draw when the pixel size
// requested is |icon_size_px|.
const VectorIconRep* GetRepForPxSize(const VectorIcon& icon, int icon_size_px) {
  if (icon.is_empty())
    return nullptr;

  const VectorIconRep* best_rep = nullptr;

  // Prefer an exact match. If none exists, prefer the largest rep that is an
  // exact divisor of the icon size (e.g. 20 for a 40px icon). If none exists,
  // return the smallest rep that is larger than the target. If none exists,
  // use the largest rep. The rep array is sorted by size in descending order,
  // so start at the back and work towards the front.
  for (const VectorIconRep& rep : base::Reversed(icon.reps)) {
    int rep_size = GetCanvasDimensions(rep.path);
    if (rep_size == icon_size_px)
      return &rep;

    if (icon_size_px % rep_size == 0)
      best_rep = &rep;

    if (rep_size > icon_size_px)
      return best_rep ? best_rep : &rep;
  }
  return best_rep ? best_rep : &icon.reps[0];
}

struct CompareIconDescription {
  bool operator()(const IconDescription& a, const IconDescription& b) const {
    const VectorIcon* a_icon = &*a.icon;
    const VectorIcon* b_icon = &*b.icon;
    const VectorIcon* a_badge = &*a.badge_icon;
    const VectorIcon* b_badge = &*b.badge_icon;
    return std::tie(a_icon, a.dip_size, a.color, a_badge) <
           std::tie(b_icon, b.dip_size, b.color, b_badge);
  }
};

// Helper that simplifies iterating over a sequence of PathElements.
class PathParser {
 public:
  PathParser(base::span<const PathElement> elements) : elements_(elements) {}

  PathParser(const PathParser&) = delete;
  PathParser& operator=(const PathParser&) = delete;

  ~PathParser() {}

  void Advance() { command_index_ += GetArgumentCount() + 1; }

  bool HasCommandsRemaining() const {
    return command_index_ < elements_.size();
  }

  CommandType CurrentCommand() const {
    return elements_[command_index_].command;
  }

  SkScalar GetArgument(int index) const {
    CHECK_LT(index, GetArgumentCount());
    return elements_[command_index_ + 1 + index].arg;
  }

 private:
  int GetArgumentCount() const {
    return GetCommandArgumentCount(CurrentCommand());
  }

  base::raw_span<const PathElement> elements_;
  size_t command_index_ = 0;
};

// Translates a string such as "MOVE_TO" into a command such as MOVE_TO.
CommandType CommandFromString(const std::string& source) {
#define RETURN_IF_IS(command) \
  if (source == #command)     \
    return command;

  RETURN_IF_IS(NEW_PATH);
  RETURN_IF_IS(FILL_RULE_NONZERO);
  RETURN_IF_IS(PATH_COLOR_ALPHA);
  RETURN_IF_IS(PATH_COLOR_ARGB);
  RETURN_IF_IS(PATH_MODE_CLEAR);
  RETURN_IF_IS(STROKE);
  RETURN_IF_IS(CAP_SQUARE);
  RETURN_IF_IS(MOVE_TO);
  RETURN_IF_IS(R_MOVE_TO);
  RETURN_IF_IS(ARC_TO);
  RETURN_IF_IS(R_ARC_TO);
  RETURN_IF_IS(LINE_TO);
  RETURN_IF_IS(R_LINE_TO);
  RETURN_IF_IS(H_LINE_TO);
  RETURN_IF_IS(R_H_LINE_TO);
  RETURN_IF_IS(V_LINE_TO);
  RETURN_IF_IS(R_V_LINE_TO);
  RETURN_IF_IS(CUBIC_TO);
  RETURN_IF_IS(R_CUBIC_TO);
  RETURN_IF_IS(CUBIC_TO_SHORTHAND);
  RETURN_IF_IS(QUADRATIC_TO);
  RETURN_IF_IS(R_QUADRATIC_TO);
  RETURN_IF_IS(QUADRATIC_TO_SHORTHAND);
  RETURN_IF_IS(R_QUADRATIC_TO_SHORTHAND);
  RETURN_IF_IS(CIRCLE);
  RETURN_IF_IS(OVAL);
  RETURN_IF_IS(ROUND_RECT);
  RETURN_IF_IS(CLOSE);
  RETURN_IF_IS(CANVAS_DIMENSIONS);
  RETURN_IF_IS(CLIP);
  RETURN_IF_IS(DISABLE_AA);
  RETURN_IF_IS(FLIPS_IN_RTL);
#undef RETURN_IF_IS

  NOTREACHED_IN_MIGRATION() << "Unrecognized command: " << source;
  return CLOSE;
}

bool StringToDouble(std::string_view piece, double* out) {
  if (piece[piece.size() - 1] == 'f') {
    piece.remove_suffix(1);
  }

  return base::StringToDouble(piece, out);
}

std::vector<PathElement> PathFromSource(const std::string& source) {
  std::vector<PathElement> path;
  std::vector<std::string> pieces = base::SplitString(
      source, "\n ,", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& piece : pieces) {
    double value = 0;
    int hex_value = 0;
    if (StringToDouble(piece, &value)) {
      path.push_back(PathElement(SkDoubleToScalar(value)));
    } else if (base::HexStringToInt(piece, &hex_value))
      path.push_back(PathElement(SkIntToScalar(hex_value)));
    else
      path.push_back(PathElement(CommandFromString(piece)));
  }
  return path;
}

bool IsCommandTypeCurve(CommandType command) {
  return command == CUBIC_TO || command == R_CUBIC_TO ||
         command == CUBIC_TO_SHORTHAND || command == QUADRATIC_TO ||
         command == R_QUADRATIC_TO || command == QUADRATIC_TO_SHORTHAND ||
         command == R_QUADRATIC_TO_SHORTHAND;
}

void PaintPath(Canvas* canvas,
               const base::span<const PathElement>& path_elements,
               int dip_size,
               SkColor color) {
  int canvas_size = kReferenceSizeDip;
  std::vector<SkPath> paths;
  std::vector<cc::PaintFlags> flags_array;
  SkRect clip_rect = SkRect::MakeEmpty();
  bool flips_in_rtl = false;
  CommandType previous_command_type = NEW_PATH;

  for (PathParser parser(path_elements); parser.HasCommandsRemaining();
       parser.Advance()) {
    auto arg = [&parser](int i) { return parser.GetArgument(i); };
    const CommandType command_type = parser.CurrentCommand();
    auto start_new_path = [&paths]() {
      paths.push_back(SkPath());
      paths.back().setFillType(SkPathFillType::kEvenOdd);
    };
    auto start_new_flags = [&flags_array, &color]() {
      flags_array.push_back(cc::PaintFlags());
      flags_array.back().setColor(color);
      flags_array.back().setAntiAlias(true);
      flags_array.back().setStrokeCap(cc::PaintFlags::kRound_Cap);
    };

    if (paths.empty() || command_type == NEW_PATH) {
      start_new_path();
      start_new_flags();
    }

    SkPath& path = paths.back();
    cc::PaintFlags& flags = flags_array.back();
    switch (command_type) {
      // Handled above.
      case NEW_PATH:
        break;

      case FILL_RULE_NONZERO:
        path.setFillType(SkPathFillType::kWinding);
        break;

      case PATH_COLOR_ALPHA:
        flags.setAlphaf(SkScalarFloorToInt(arg(0)) / 255.0f);
        break;

      case PATH_COLOR_ARGB:
        flags.setColor(SkColorSetARGB(
            SkScalarFloorToInt(arg(0)), SkScalarFloorToInt(arg(1)),
            SkScalarFloorToInt(arg(2)), SkScalarFloorToInt(arg(3))));
        break;

      case PATH_MODE_CLEAR:
        flags.setBlendMode(SkBlendMode::kClear);
        break;

      case STROKE:
        flags.setStyle(cc::PaintFlags::kStroke_Style);
        flags.setStrokeWidth(arg(0));
        break;

      case CAP_SQUARE:
        flags.setStrokeCap(cc::PaintFlags::kSquare_Cap);
        break;

      case MOVE_TO:
        path.moveTo(arg(0), arg(1));
        break;

      case R_MOVE_TO:
        if (previous_command_type == CLOSE) {
          // This triggers injectMoveToIfNeeded() so that the next subpath
          // will start at the correct place. See [
          // https://www.w3.org/TR/SVG/paths.html#PathDataClosePathCommand ].
          path.rLineTo(0, 0);
        }

        path.rMoveTo(arg(0), arg(1));
        break;

      case ARC_TO:
      case R_ARC_TO: {
        SkScalar rx = arg(0);
        SkScalar ry = arg(1);
        SkScalar angle = arg(2);
        SkScalar large_arc_flag = arg(3);
        SkScalar arc_sweep_flag = arg(4);
        SkScalar x = arg(5);
        SkScalar y = arg(6);
        SkPath::ArcSize arc_size =
            large_arc_flag ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize;
        SkPathDirection direction =
            arc_sweep_flag ? SkPathDirection::kCW : SkPathDirection::kCCW;

        if (command_type == ARC_TO)
          path.arcTo(rx, ry, angle, arc_size, direction, x, y);
        else
          path.rArcTo(rx, ry, angle, arc_size, direction, x, y);
        break;
      }

      case LINE_TO:
        path.lineTo(arg(0), arg(1));
        break;

      case R_LINE_TO:
        path.rLineTo(arg(0), arg(1));
        break;

      case H_LINE_TO: {
        SkPoint last_point;
        path.getLastPt(&last_point);
        path.lineTo(arg(0), last_point.fY);
        break;
      }

      case R_H_LINE_TO:
        path.rLineTo(arg(0), 0);
        break;

      case V_LINE_TO: {
        SkPoint last_point;
        path.getLastPt(&last_point);
        path.lineTo(last_point.fX, arg(0));
        break;
      }

      case R_V_LINE_TO:
        path.rLineTo(0, arg(0));
        break;

      case CUBIC_TO:
        path.cubicTo(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        break;

      case R_CUBIC_TO:
        path.rCubicTo(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        break;

      case CUBIC_TO_SHORTHAND:
      case QUADRATIC_TO_SHORTHAND:
      case R_QUADRATIC_TO_SHORTHAND: {
        // Compute the first control point (|x1| and |y1|) as the reflection of
        // the last control point on the previous command relative to the
        // current point. If there is no previous command or if the previous
        // command is not a Bezier curve, the first control point is coincident
        // with the current point. Refer to the SVG path specs for further
        // details.
        // Note that |x1| and |y1| will correspond to the sole control point if
        // calculating a quadratic curve.
        SkPoint last_point;
        path.getLastPt(&last_point);
        SkScalar delta_x = 0;
        SkScalar delta_y = 0;
        if (IsCommandTypeCurve(previous_command_type)) {
          SkPoint last_control_point = path.getPoint(path.countPoints() - 2);
          // We find what the delta was between the last curve's starting point
          // and the control point. This difference is what we will reflect on
          // the current point, creating our new control point.
          delta_x = last_point.fX - last_control_point.fX;
          delta_y = last_point.fY - last_control_point.fY;
        }

        SkScalar x1 = last_point.fX + delta_x;
        SkScalar y1 = last_point.fY + delta_y;
        if (command_type == CUBIC_TO_SHORTHAND)
          path.cubicTo(x1, y1, arg(0), arg(1), arg(2), arg(3));
        else if (command_type == QUADRATIC_TO_SHORTHAND)
          path.quadTo(x1, y1, arg(0), arg(1));
        else if (command_type == R_QUADRATIC_TO_SHORTHAND)
          path.rQuadTo(x1, y1, arg(0), arg(1));
        break;
      }

      case QUADRATIC_TO:
        path.quadTo(arg(0), arg(1), arg(2), arg(3));
        break;

      case R_QUADRATIC_TO:
        path.rQuadTo(arg(0), arg(1), arg(2), arg(3));
        break;

      case OVAL: {
        SkScalar x = arg(0);
        SkScalar y = arg(1);
        SkScalar rx = arg(2);
        SkScalar ry = arg(3);
        path.addOval(SkRect::MakeLTRB(x - rx, y - ry, x + rx, y + ry));
        break;
      }

      case CIRCLE:
        path.addCircle(arg(0), arg(1), arg(2));
        break;

      case ROUND_RECT:
        path.addRoundRect(SkRect::MakeXYWH(arg(0), arg(1), arg(2), arg(3)),
                          arg(4), arg(4));
        break;

      case CLOSE:
        path.close();
        break;

      case CANVAS_DIMENSIONS:
        canvas_size = SkScalarTruncToInt(arg(0));
        break;

      case CLIP:
        clip_rect = SkRect::MakeXYWH(arg(0), arg(1), arg(2), arg(3));
        break;

      case DISABLE_AA:
        flags.setAntiAlias(false);
        break;

      case FLIPS_IN_RTL:
        flips_in_rtl = true;
        break;
    }

    previous_command_type = command_type;
  }

  ScopedCanvas scoped_canvas(canvas);

  if (dip_size != canvas_size) {
    SkScalar scale = SkIntToScalar(dip_size) / SkIntToScalar(canvas_size);
    canvas->sk_canvas()->scale(scale, scale);
  }

  if (flips_in_rtl)
    scoped_canvas.FlipIfRTL(canvas_size);

  if (!clip_rect.isEmpty())
    canvas->sk_canvas()->clipRect(clip_rect);

  DCHECK_EQ(flags_array.size(), paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    canvas->DrawPath(paths[i], flags_array[i]);
}

class VectorIconSource : public CanvasImageSource {
 public:
  explicit VectorIconSource(const IconDescription& data)
      : CanvasImageSource(Size(data.dip_size, data.dip_size)), data_(data) {}

  VectorIconSource(const std::string& definition, int dip_size, SkColor color)
      : CanvasImageSource(Size(dip_size, dip_size)),
        data_(kNoneIcon, dip_size, color, &kNoneIcon),
        path_(PathFromSource(definition)) {}

  VectorIconSource(const VectorIconSource&) = delete;
  VectorIconSource& operator=(const VectorIconSource&) = delete;

  ~VectorIconSource() override {}

  // CanvasImageSource:
  bool HasRepresentationAtAllScales() const override {
    return !data_.icon->is_empty();
  }

  void Draw(Canvas* canvas) override {
    if (path_.empty()) {
      PaintVectorIcon(canvas, *data_.icon, size_.width(), data_.color);
      if (!data_.badge_icon->is_empty())
        PaintVectorIcon(canvas, *data_.badge_icon, size_.width(), data_.color);
    } else {
      PaintPath(canvas, path_, size_.width(), data_.color);
    }
  }

 private:
  const IconDescription data_;
  const std::vector<PathElement> path_;
};

// This class caches vector icons (as ImageSkia) so they don't have to be drawn
// more than once. This also guarantees the backing data for the images returned
// by CreateVectorIcon will persist in memory until program termination.
class VectorIconCache {
 public:
  VectorIconCache() {}

  VectorIconCache(const VectorIconCache&) = delete;
  VectorIconCache& operator=(const VectorIconCache&) = delete;

  ~VectorIconCache() {}

  ImageSkia GetOrCreateIcon(const IconDescription& description) {
    auto iter = images_.find(description);
    if (iter != images_.end())
      return iter->second;

    ImageSkia icon_image(std::make_unique<VectorIconSource>(description),
                         Size(description.dip_size, description.dip_size));
    images_.emplace(description, icon_image);
    return icon_image;
  }

 private:
  std::map<IconDescription, ImageSkia, CompareIconDescription> images_;
};

static base::LazyInstance<VectorIconCache>::DestructorAtExit g_icon_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

IconDescription::IconDescription(const IconDescription& other) = default;

IconDescription::IconDescription(const VectorIcon& icon,
                                 int dip_size,
                                 SkColor color,
                                 const VectorIcon* badge_icon)
    : icon(icon),
      dip_size(dip_size),
      color(color),
      badge_icon(badge_icon ? *badge_icon : kNoneIcon) {
  if (dip_size == 0)
    this->dip_size = GetDefaultSizeOfVectorIcon(icon);
}

IconDescription::~IconDescription() {}

const VectorIcon kNoneIcon = {};

void PaintVectorIcon(Canvas* canvas, const VectorIcon& icon, SkColor color) {
  PaintVectorIcon(canvas, icon, GetDefaultSizeOfVectorIcon(icon), color);
}

void PaintVectorIcon(Canvas* canvas,
                     const VectorIcon& icon,
                     int dip_size,
                     SkColor color) {
  CHECK(!icon.is_empty());
  for (const auto& rep : icon.reps) {
    CHECK(!rep.path.empty());
  }
  const int px_size = base::ClampCeil(canvas->image_scale() * dip_size);
  const VectorIconRep* rep = GetRepForPxSize(icon, px_size);
  PaintPath(canvas, rep->path, dip_size, color);
}

ImageSkia CreateVectorIcon(const IconDescription& params) {
  if (params.icon->is_empty())
    return ImageSkia();

  return g_icon_cache.Get().GetOrCreateIcon(params);
}

ImageSkia CreateVectorIcon(const VectorIcon& icon, SkColor color) {
  return CreateVectorIcon(icon, GetDefaultSizeOfVectorIcon(icon), color);
}

ImageSkia CreateVectorIcon(const VectorIcon& icon,
                           int dip_size,
                           SkColor color) {
  return CreateVectorIcon(IconDescription(icon, dip_size, color, &kNoneIcon));
}

ImageSkia CreateVectorIconWithBadge(const VectorIcon& icon,
                                    int dip_size,
                                    SkColor color,
                                    const VectorIcon& badge_icon) {
  return CreateVectorIcon(IconDescription(icon, dip_size, color, &badge_icon));
}

ImageSkia CreateVectorIconFromSource(const std::string& source,
                                     int dip_size,
                                     SkColor color) {
  return CanvasImageSource::MakeImageSkia<VectorIconSource>(source, dip_size,
                                                            color);
}

}  // namespace gfx
