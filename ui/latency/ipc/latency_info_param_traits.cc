// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/latency/ipc/latency_info_param_traits_macros.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_LATENCY_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/latency/ipc/latency_info_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_LATENCY_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/latency/ipc/latency_info_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_LATENCY_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/latency/ipc/latency_info_param_traits_macros.h"
}  // namespace IPC

// Implemetation for ParamTraits<ui::LatencyInfo>.
#include "ui/latency/ipc/latency_info_param_traits.h"

namespace IPC {

void ParamTraits<ui::LatencyInfo>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.latency_components_);
  WriteParam(m, p.trace_id_);
  WriteParam(m, p.coalesced_);
  WriteParam(m, p.began_);
  WriteParam(m, p.terminated_);
}

bool ParamTraits<ui::LatencyInfo>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  if (!ReadParam(m, iter, &p->latency_components_))
    return false;

  if (!ReadParam(m, iter, &p->trace_id_))
    return false;
  if (!ReadParam(m, iter, &p->coalesced_))
    return false;
  if (!ReadParam(m, iter, &p->began_))
    return false;
  if (!ReadParam(m, iter, &p->terminated_))
    return false;

  return true;
}

void ParamTraits<ui::LatencyInfo>::Log(const param_type& p, std::string* l) {
  LogParam(p.latency_components_, l);
  l->append(" ");
  LogParam(p.trace_id_, l);
  l->append(" ");
  LogParam(p.coalesced_, l);
  l->append(" ");
  LogParam(p.began_, l);
  l->append(" ");
  LogParam(p.terminated_, l);
}

}  // namespace IPC
