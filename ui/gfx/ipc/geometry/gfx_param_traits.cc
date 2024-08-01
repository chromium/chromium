// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/stringprintf.h"
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

void ParamTraits<gfx::Point>::Log(const gfx::Point& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.x(), p.y()));
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

void ParamTraits<gfx::PointF>::Log(const gfx::PointF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", p.x(), p.y()));
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

void ParamTraits<gfx::Point3F>::Log(const gfx::Point3F& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f, %f)", p.x(), p.y(), p.z()));
}

void ParamTraits<gfx::Size>::Write(base::Pickle* m, const gfx::Size& p) {
  DCHECK_GE(p.width(), 0);
  DCHECK_GE(p.height(), 0);
  int values[2] = {p.width(), p.height()};
  m->WriteBytes(&values, sizeof(int) * 2);
}

bool ParamTraits<gfx::Size>::Read(const base::Pickle* m,
                                  base::PickleIterator* iter,
                                  gfx::Size* r) {
  const char* char_values;
  if (!iter->ReadBytes(&char_values, sizeof(int) * 2))
    return false;
  const int* values = reinterpret_cast<const int*>(char_values);
  if (values[0] < 0 || values[1] < 0)
    return false;
  r->set_width(values[0]);
  r->set_height(values[1]);
  return true;
}

void ParamTraits<gfx::Size>::Log(const gfx::Size& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", p.width(), p.height()));
}

void ParamTraits<gfx::SizeF>::Write(base::Pickle* m, const gfx::SizeF& p) {
  float values[2] = {p.width(), p.height()};
  m->WriteBytes(&values, sizeof(float) * 2);
}

bool ParamTraits<gfx::SizeF>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::SizeF* r) {
  const char* char_values;
  if (!iter->ReadBytes(&char_values, sizeof(float) * 2))
    return false;
  const float* values = reinterpret_cast<const float*>(char_values);
  r->set_width(values[0]);
  r->set_height(values[1]);
  return true;
}

void ParamTraits<gfx::SizeF>::Log(const gfx::SizeF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", p.width(), p.height()));
}

void ParamTraits<gfx::Vector2d>::Write(base::Pickle* m,
                                       const gfx::Vector2d& p) {
  int values[2] = {p.x(), p.y()};
  m->WriteBytes(&values, sizeof(int) * 2);
}

bool ParamTraits<gfx::Vector2d>::Read(const base::Pickle* m,
                                      base::PickleIterator* iter,
                                      gfx::Vector2d* r) {
  const char* char_values;
  if (!iter->ReadBytes(&char_values, sizeof(int) * 2))
    return false;
  const int* values = reinterpret_cast<const int*>(char_values);
  r->set_x(values[0]);
  r->set_y(values[1]);
  return true;
}

void ParamTraits<gfx::Vector2d>::Log(const gfx::Vector2d& v, std::string* l) {
  l->append(base::StringPrintf("(%d, %d)", v.x(), v.y()));
}

void ParamTraits<gfx::Vector2dF>::Write(base::Pickle* m,
                                        const gfx::Vector2dF& p) {
  float values[2] = {p.x(), p.y()};
  m->WriteBytes(&values, sizeof(float) * 2);
}

bool ParamTraits<gfx::Vector2dF>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       gfx::Vector2dF* r) {
  const char* char_values;
  if (!iter->ReadBytes(&char_values, sizeof(float) * 2))
    return false;
  const float* values = reinterpret_cast<const float*>(char_values);
  r->set_x(values[0]);
  r->set_y(values[1]);
  return true;
}

void ParamTraits<gfx::Vector2dF>::Log(const gfx::Vector2dF& v, std::string* l) {
  l->append(base::StringPrintf("(%f, %f)", v.x(), v.y()));
}

void ParamTraits<gfx::Rect>::Write(base::Pickle* m, const gfx::Rect& p) {
  int values[4] = {p.x(), p.y(), p.width(), p.height()};
  m->WriteBytes(&values, sizeof(int) * 4);
}

bool ParamTraits<gfx::Rect>::Read(const base::Pickle* m,
                                  base::PickleIterator* iter,
                                  gfx::Rect* r) {
  const char* char_values;
  if (!iter->ReadBytes(&char_values, sizeof(int) * 4))
    return false;
  const int* values = reinterpret_cast<const int*>(char_values);
  if (values[2] < 0 || values[3] < 0)
    return false;
  r->SetRect(values[0], values[1], values[2], values[3]);
  return true;
}

void ParamTraits<gfx::Rect>::Log(const gfx::Rect& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d, %d, %d)", p.x(), p.y(), p.width(),
                               p.height()));
}

void ParamTraits<gfx::RectF>::Write(base::Pickle* m, const gfx::RectF& p) {
  float values[4] = {p.x(), p.y(), p.width(), p.height()};
  m->WriteBytes(&values, sizeof(float) * 4);
}

bool ParamTraits<gfx::RectF>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::RectF* r) {
  const char* char_values;
  if (!iter->ReadBytes(&char_values, sizeof(float) * 4))
    return false;
  const float* values = reinterpret_cast<const float*>(char_values);
  r->SetRect(values[0], values[1], values[2], values[3]);
  return true;
}

void ParamTraits<gfx::RectF>::Log(const gfx::RectF& p, std::string* l) {
  l->append(base::StringPrintf("(%f, %f, %f, %f)", p.x(), p.y(), p.width(),
                               p.height()));
}

}  // namespace IPC
