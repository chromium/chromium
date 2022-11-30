// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init_webrtc.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"
#include "third_party/webrtc/rtc_base/event_tracer.h"
#include "third_party/webrtc/system_wrappers/include/cpu_info.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

const unsigned char* GetCategoryGroupEnabled(const char* category_group) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

void AddTraceEvent(char phase,
                   const unsigned char* category_group_enabled,
                   const char* name,
                   unsigned long long id,
                   int num_args,
                   const char** arg_names,
                   const unsigned char* arg_types,
                   const unsigned long long* arg_values,
                   unsigned char flags) {
  base::trace_event::TraceArguments args(num_args, arg_names, arg_types,
                                         arg_values);
  TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_group_enabled, name,
                                  trace_event_internal::kGlobalScope, id, &args,
                                  flags);
}

bool InitializeWebRtcModule() {
  // Workaround for crbug.com/176522
  // On Linux, we can't fetch the number of cores after the sandbox has been
  // initialized, so we call DetectNumberOfCores() here, to cache the value.
  webrtc::CpuInfo::DetectNumberOfCores();
  webrtc::SetupEventTracer(&GetCategoryGroupEnabled, &AddTraceEvent);
  return true;
}
