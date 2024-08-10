// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_evdev_debug_buffer.h"

#include <stdio.h>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/event_device_info.h"

using base::File;

namespace ui {

TouchEventLogEvdev::TouchEventLogEvdev()
    : logged_events_(new TouchEvent[kDebugBufferSize]) {
}

TouchEventLogEvdev::~TouchEventLogEvdev() {
}

void TouchEventLogEvdev::Initialize(const EventDeviceInfo& devinfo) {
  device_name_ = devinfo.name();
  for (int code = ABS_X; code <= ABS_MAX; code++) {
    if (devinfo.HasAbsEvent(code)) {
      axes_.push_back(AbsAxisData(code, devinfo.GetAbsInfoByCode(code)));
    }
  }
}

void TouchEventLogEvdev::ProcessEvent(size_t cur_slot, const input_event* ev) {
  if (ev->type == EV_ABS || ev->type == EV_SYN ||
      (ev->type == EV_KEY && ev->code == BTN_TOUCH)) {
    logged_events_[debug_buffer_tail_].ev = *ev;
    logged_events_[debug_buffer_tail_].slot = cur_slot;
    debug_buffer_tail_++;
    debug_buffer_tail_ %= kDebugBufferSize;
  }
}

void TouchEventLogEvdev::DumpLog(const char* filename) {
  base::FilePath fp = base::FilePath(filename);
  File file(fp, File::FLAG_CREATE | File::FLAG_WRITE);
  std::string report_content("");
  std::string device_name_str =
      base::StringPrintf("# device: %s\n", device_name_.c_str());
  report_content += device_name_str;
  for (size_t i = 0; i < axes_.size(); ++i) {
    std::string absinfo = base::StringPrintf(
        "# absinfo: %d %d %d %d %d %d\n", axes_[i].code, axes_[i].info.maximum,
        axes_[i].info.maximum, axes_[i].info.fuzz, axes_[i].info.flat,
        axes_[i].info.resolution);
    report_content += absinfo;
  }
  for (int i = 0; i < kDebugBufferSize; ++i) {
    struct TouchEvent* te =
        &logged_events_[(debug_buffer_tail_ + i) % kDebugBufferSize];
    if (te->ev.input_event_sec == 0 && te->ev.input_event_usec == 0)
      continue;
    std::string event_string = base::StringPrintf(
        "E: %ld.%06ld %04x %04x %d %d\n", te->ev.input_event_sec,
        te->ev.input_event_usec, te->ev.type, te->ev.code, te->ev.value,
        te->slot);
    report_content += event_string;
  }
  file.Write(0, base::as_byte_span(report_content));
}

TouchEventLogEvdev::AbsAxisData::AbsAxisData(int code,
                                             const input_absinfo& info)
    : code(code), info(info) {
}

TouchEventLogEvdev::AbsAxisData::AbsAxisData(const AbsAxisData& other) =
    default;

TouchEventLogEvdev::AbsAxisData::~AbsAxisData() {
}

}  // namespace ui
