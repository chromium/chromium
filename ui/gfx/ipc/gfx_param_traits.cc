// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/gfx_param_traits.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/range/range.h"

#if BUILDFLAG(IS_APPLE)
#include "ipc/mach_port_mac.h"
#endif

namespace IPC {

void ParamTraits<gfx::Range>::Write(base::Pickle* m, const gfx::Range& r) {
  m->WriteUInt32(static_cast<uint32_t>(r.start()));
  m->WriteUInt32(static_cast<uint32_t>(r.end()));
}

bool ParamTraits<gfx::Range>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   gfx::Range* r) {
  uint32_t start, end;
  if (!iter->ReadUInt32(&start) || !iter->ReadUInt32(&end))
    return false;
  r->set_start(start);
  r->set_end(end);
  return true;
}

void ParamTraits<gfx::Range>::Log(const gfx::Range& r, std::string* l) {
  l->append(base::StringPrintf("(%" PRIuS ", %" PRIuS ")", r.start(), r.end()));
}

#if BUILDFLAG(IS_APPLE)
void ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort>::Write(
    base::Pickle* m,
    const param_type p) {
  MachPortMac mach_port_mac(p.get());
  ParamTraits<MachPortMac>::Write(m, mach_port_mac);
}

bool ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  MachPortMac mach_port_mac;
  if (!ParamTraits<MachPortMac>::Read(m, iter, &mach_port_mac))
    return false;
  r->reset(mach_port_mac.get_mach_port());
  return true;
}

void ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort>::Log(
    const param_type& p,
    std::string* l) {
  l->append("IOSurface Mach send right: ");
  LogParam(p.get(), l);
}

void ParamTraits<gfx::ScopedIOSurface>::Write(base::Pickle* m,
                                              const param_type p) {
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port(
      IOSurfaceCreateMachPort(p.get()));
  MachPortMac mach_port_mac(io_surface_mach_port.get());
  ParamTraits<MachPortMac>::Write(m, mach_port_mac);
}

bool ParamTraits<gfx::ScopedIOSurface>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  MachPortMac mach_port_mac;
  if (!ParamTraits<MachPortMac>::Read(m, iter, &mach_port_mac))
    return false;
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port(
      mach_port_mac.get_mach_port());
  if (io_surface_mach_port)
    r->reset(IOSurfaceLookupFromMachPort(io_surface_mach_port.get()));
  else
    r->reset();
  return true;
}

void ParamTraits<gfx::ScopedIOSurface>::Log(const param_type& p,
                                            std::string* l) {
  l->append("IOSurface(");
  if (p) {
    uint32_t io_surface_id = IOSurfaceGetID(p.get());
    LogParam(io_surface_id, l);
  }
  l->append(")");
}
#endif  // BUILDFLAG(IS_APPLE)

void ParamTraits<gfx::SelectionBound>::Write(base::Pickle* m,
                                             const param_type& p) {
  WriteParam(m, static_cast<uint32_t>(p.type()));
  WriteParam(m, p.edge_start());
  WriteParam(m, p.edge_end());
  WriteParam(m, p.visible_edge_start());
  WriteParam(m, p.visible_edge_end());
  WriteParam(m, p.visible());
}

bool ParamTraits<gfx::SelectionBound>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* r) {
  gfx::SelectionBound::Type type;
  gfx::PointF edge_start;
  gfx::PointF edge_end;
  gfx::PointF visible_edge_start;
  gfx::PointF visible_edge_end;
  bool visible = false;

  if (!ReadParam(m, iter, &type) || !ReadParam(m, iter, &edge_start) ||
      !ReadParam(m, iter, &edge_end) ||
      !ReadParam(m, iter, &visible_edge_start) ||
      !ReadParam(m, iter, &visible_edge_end) || !ReadParam(m, iter, &visible)) {
    return false;
  }

  r->set_type(type);
  r->SetEdgeStart(edge_start);
  r->SetEdgeEnd(edge_end);
  r->SetVisibleEdgeStart(visible_edge_start);
  r->SetVisibleEdgeEnd(visible_edge_end);
  r->set_visible(visible);
  return true;
}

void ParamTraits<gfx::SelectionBound>::Log(const param_type& p,
                                           std::string* l) {
  l->append("gfx::SelectionBound(");
  LogParam(static_cast<uint32_t>(p.type()), l);
  l->append(", ");
  LogParam(p.edge_start(), l);
  l->append(", ");
  LogParam(p.edge_end(), l);
  l->append(", ");
  LogParam(p.visible_edge_start(), l);
  l->append(", ");
  LogParam(p.visible_edge_end(), l);
  l->append(", ");
  LogParam(p.visible(), l);
  l->append(")");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/gfx_param_traits_macros.h"
}  // namespace IPC
