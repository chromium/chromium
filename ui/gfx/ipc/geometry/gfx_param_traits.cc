// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

#include <stdint.h>

#include "base/check_op.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace IPC {

void ParamTraits<gfx::Point>::Write(base::Pickle* m, const gfx::Point& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
}

bool ParamTraits<gfx::Point>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::Point* r) {
  int x, y;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::PointF>::Write(base::Pickle* m, const gfx::PointF& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
}

bool ParamTraits<gfx::PointF>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    gfx::PointF* r) {
  float x, y;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Point3F>::Write(base::Pickle* m, const gfx::Point3F& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
  WriteParam(m, p.z());
}

bool ParamTraits<gfx::Point3F>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     gfx::Point3F* r) {
  float x, y, z;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y) ||
      !ReadParam(m, iter, &z))
    return false;
  r->set_x(x);
  r->set_y(y);
  r->set_z(z);
  return true;
}

void ParamTraits<gfx::Size>::Write(base::Pickle* m, const gfx::Size& p) {
  DCHECK_GE(p.width(), 0);
  DCHECK_GE(p.height(), 0);
  WriteParam(m, p.width());
  WriteParam(m, p.height());
}

bool ParamTraits<gfx::Size>::Read(const base::Pickle* m,
                                  base::PickleIterator* iter,
                                  gfx::Size* r) {
  int width, height;
  if (!ReadParam(m, iter, &width) || !ReadParam(m, iter, &height) ||
      width < 0 || height < 0) {
    return false;
  }
  r->set_width(width);
  r->set_height(height);
  return true;
}

void ParamTraits<gfx::SizeF>::Write(base::Pickle* m, const gfx::SizeF& p) {
  WriteParam(m, p.width());
  WriteParam(m, p.height());
}

bool ParamTraits<gfx::SizeF>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::SizeF* r) {
  float width, height;
  if (!ReadParam(m, iter, &width) || !ReadParam(m, iter, &height)) {
    return false;
  }
  r->set_width(width);
  r->set_height(height);
  return true;
}

void ParamTraits<gfx::Vector2d>::Write(base::Pickle* m,
                                       const gfx::Vector2d& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
}

bool ParamTraits<gfx::Vector2d>::Read(const base::Pickle* m,
                                      base::PickleIterator* iter,
                                      gfx::Vector2d* r) {
  int x, y;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y)) {
    return false;
  }
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Vector2dF>::Write(base::Pickle* m,
                                        const gfx::Vector2dF& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
}

bool ParamTraits<gfx::Vector2dF>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       gfx::Vector2dF* r) {
  float x, y;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y)) {
    return false;
  }
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Rect>::Write(base::Pickle* m, const gfx::Rect& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
  WriteParam(m, p.width());
  WriteParam(m, p.height());
}

bool ParamTraits<gfx::Rect>::Read(const base::Pickle* m,
                                  base::PickleIterator* iter,
                                  gfx::Rect* r) {
  int x, y, width, height;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y) ||
      !ReadParam(m, iter, &width) || !ReadParam(m, iter, &height) ||
      width < 0 || height < 0) {
    return false;
  }
  r->SetRect(x, y, width, height);
  return true;
}

void ParamTraits<gfx::RectF>::Write(base::Pickle* m, const gfx::RectF& p) {
  WriteParam(m, p.x());
  WriteParam(m, p.y());
  WriteParam(m, p.width());
  WriteParam(m, p.height());
}

bool ParamTraits<gfx::RectF>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::RectF* r) {
  float x, y, width, height;
  if (!ReadParam(m, iter, &x) || !ReadParam(m, iter, &y) ||
      !ReadParam(m, iter, &width) || !ReadParam(m, iter, &height)) {
    return false;
  }
  r->SetRect(x, y, width, height);
  return true;
}

}  // namespace IPC
