// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_GEOMETRY_GFX_PARAM_TRAITS_H_
#define UI_GFX_IPC_GEOMETRY_GFX_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ui/gfx/ipc/geometry/gfx_ipc_geometry_export.h"

namespace gfx {
class Point;
class PointF;
class Point3F;
class Rect;
class RectF;
class Size;
class SizeF;
class Vector2d;
class Vector2dF;
}  // namespace gfx

namespace IPC {

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::Point> {
  typedef gfx::Point param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::PointF> {
  typedef gfx::PointF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::Point3F> {
  typedef gfx::Point3F param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::Size> {
  typedef gfx::Size param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::SizeF> {
  typedef gfx::SizeF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::Vector2d> {
  typedef gfx::Vector2d param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::Vector2dF> {
  typedef gfx::Vector2dF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::Rect> {
  typedef gfx::Rect param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_GEOMETRY_EXPORT ParamTraits<gfx::RectF> {
  typedef gfx::RectF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // UI_GFX_IPC_GEOMETRY_GFX_PARAM_TRAITS_H_
