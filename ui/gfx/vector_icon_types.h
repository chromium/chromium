// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_VECTOR_ICON_TYPES_H_
#define UI_GFX_VECTOR_ICON_TYPES_H_

#include "base/memory/raw_span.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/animation/tween.h"

namespace gfx {

// This macro allows defining the list of commands in this file, then pulling
// them each in to the template files via using-declarations.  Files which want
// to do this should do the following:
//   #define DECLARE_VECTOR_COMMAND(x) using gfx::x;
//   DECLARE_VECTOR_COMMANDS
// The alternative would be to have the template files pull in the whole gfx
// namespace via using-directives, which is banned by the style guide.
#define DECLARE_VECTOR_COMMANDS                                                \
  /* A new <path> element. For the first path, this is assumed. By default */  \
  /* the path has the 'kEvenOdd' fill type. */                                 \
  DECLARE_VECTOR_COMMAND(NEW_PATH)                                             \
  /* Set the fill type to Non-zero aka 'kWinding'. */                          \
  DECLARE_VECTOR_COMMAND(FILL_RULE_NONZERO)                                    \
  /* Sets the alpha for the current path. */                                   \
  DECLARE_VECTOR_COMMAND(PATH_COLOR_ALPHA)                                     \
  /* Sets the color for the current path. */                                   \
  DECLARE_VECTOR_COMMAND(PATH_COLOR_ARGB)                                      \
  /* Sets the path to clear mode (Skia's kClear_Mode). */                      \
  DECLARE_VECTOR_COMMAND(PATH_MODE_CLEAR)                                      \
  /* By default, the path will be filled. This changes the paint action to */  \
  /* stroke at the given width. */                                             \
  DECLARE_VECTOR_COMMAND(STROKE)                                               \
  /* By default, a stroke has a round cap. This sets it to square. */          \
  DECLARE_VECTOR_COMMAND(CAP_SQUARE)                                           \
  /* These correspond to pathing commands. */                                  \
  DECLARE_VECTOR_COMMAND(MOVE_TO)                                              \
  DECLARE_VECTOR_COMMAND(R_MOVE_TO)                                            \
  DECLARE_VECTOR_COMMAND(ARC_TO)                                               \
  DECLARE_VECTOR_COMMAND(R_ARC_TO)                                             \
  DECLARE_VECTOR_COMMAND(LINE_TO)                                              \
  DECLARE_VECTOR_COMMAND(R_LINE_TO)                                            \
  DECLARE_VECTOR_COMMAND(H_LINE_TO)                                            \
  DECLARE_VECTOR_COMMAND(R_H_LINE_TO)                                          \
  DECLARE_VECTOR_COMMAND(V_LINE_TO)                                            \
  DECLARE_VECTOR_COMMAND(R_V_LINE_TO)                                          \
  DECLARE_VECTOR_COMMAND(CUBIC_TO)                                             \
  DECLARE_VECTOR_COMMAND(R_CUBIC_TO)                                           \
  DECLARE_VECTOR_COMMAND(CUBIC_TO_SHORTHAND)                                   \
  DECLARE_VECTOR_COMMAND(QUADRATIC_TO)                                         \
  DECLARE_VECTOR_COMMAND(R_QUADRATIC_TO)                                       \
  DECLARE_VECTOR_COMMAND(QUADRATIC_TO_SHORTHAND)                               \
  DECLARE_VECTOR_COMMAND(R_QUADRATIC_TO_SHORTHAND)                             \
  DECLARE_VECTOR_COMMAND(CIRCLE)                                               \
  DECLARE_VECTOR_COMMAND(OVAL)                                                 \
  DECLARE_VECTOR_COMMAND(ROUND_RECT)                                           \
  DECLARE_VECTOR_COMMAND(CLOSE)                                                \
  /* Sets the dimensions of the canvas in dip. */                              \
  DECLARE_VECTOR_COMMAND(CANVAS_DIMENSIONS)                                    \
  /* Sets a bounding rect for the path. This allows fine adjustment because */ \
  /* it can tweak edge anti-aliasing. Args are x, y, w, h. */                  \
  DECLARE_VECTOR_COMMAND(CLIP)                                                 \
  /* Disables anti-aliasing for this path. */                                  \
  DECLARE_VECTOR_COMMAND(DISABLE_AA)                                           \
  /* Flips the x-axis in RTL locales. Default is false, this command sets */   \
  /* it to true. */                                                            \
  DECLARE_VECTOR_COMMAND(FLIPS_IN_RTL)

#define DECLARE_VECTOR_COMMAND(x) x,

// A command to Skia.
enum CommandType { DECLARE_VECTOR_COMMANDS };

#undef DECLARE_VECTOR_COMMAND

// A POD that describes either a path command or an argument for it.
struct PathElement {
  constexpr PathElement(CommandType value) : command(value) {}
  constexpr PathElement(SkScalar value) : arg(value) {}

  union {
    CommandType command;
    SkScalar arg;
  };
};

// Describes the drawing commands for a single vector icon at a particular pixel
// size or range of sizes.
struct VectorIconRep {
  VectorIconRep() = default;
  constexpr VectorIconRep(const PathElement* path_data, size_t path_size)
      : path(path_data, path_size) {}

  VectorIconRep(const VectorIconRep&) = delete;
  VectorIconRep& operator=(const VectorIconRep&) = delete;

  base::raw_span<const PathElement> path;
};

// A vector icon that stores one or more representations to be used for various
// scale factors and pixel dimensions.
struct VectorIcon {
  VectorIcon() = default;
  constexpr VectorIcon(const VectorIconRep* reps_data,
                       size_t reps_size,
                       const char* name)
      : reps(reps_data, reps_size), name(name) {}

  VectorIcon(const VectorIcon&) = delete;
  VectorIcon& operator=(const VectorIcon&) = delete;

  bool is_empty() const { return reps.empty(); }

  base::raw_span<const VectorIconRep> reps;

  // A human-readable name, useful for debugging, derived from the name of the
  // icon file. This can also be used as an identifier, but vector icon targets
  // should be careful to ensure this is unique.
  const char* name = nullptr;

  bool operator<(const VectorIcon& other) const;
};

}  // namespace gfx

#endif  // UI_GFX_VECTOR_ICON_TYPES_H_
