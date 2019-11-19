// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_feedback.h"

#include <stddef.h>
#include <time.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"

namespace ui {

namespace {

// Binary paths.
const char kGzipCommand[] = "/bin/gzip";

const size_t kTouchLogTimestampMaxSize = 80;

// Return the values in an array in one string. Used for touch logging.
template <typename T>
std::string DumpArrayProperty(const std::vector<T>& value, const char* format) {
  std::string ret;
  for (size_t i = 0; i < value.size(); ++i) {
    if (i > 0)
      ret.append(", ");
    ret.append(base::StringPrintf(format, value[i]));
  }
  return ret;
}

// Return the values in a gesture property in one string. Used for touch
// logging.
std::string DumpGesturePropertyValue(GesturesProp* property) {
  switch (property->type()) {
    case GesturePropertyProvider::PT_INT:
      return DumpArrayProperty(property->GetIntValue(), "%d");
      break;
    case GesturePropertyProvider::PT_SHORT:
      return DumpArrayProperty(property->GetShortValue(), "%d");
      break;
    case GesturePropertyProvider::PT_BOOL:
      return DumpArrayProperty(property->GetBoolValue(), "%d");
      break;
    case GesturePropertyProvider::PT_STRING:
      return "\"" + property->GetStringValue() + "\"";
      break;
    case GesturePropertyProvider::PT_REAL:
      return DumpArrayProperty(property->GetDoubleValue(), "%lf");
      break;
    default:
      NOTREACHED();
      break;
  }
  return std::string();
}

// Compress dumped event logs in place.
void CompressDumpedLog(std::unique_ptr<std::vector<std::string>> log_paths) {
  for (size_t i = 0; i < log_paths->size(); ++i) {
    // Zip the file.
    base::CommandLine command = base::CommandLine(base::FilePath(kGzipCommand));
    command.AppendArg("-f");
    command.AppendArg((*log_paths)[i]);
    std::string output;
    base::GetAppOutput(command, &output);

    // Replace the original file with the zipped one.
    base::Move(base::FilePath((*log_paths)[i] + ".gz"),
               base::FilePath((*log_paths)[i]));
  }
}

// Get the current time in a string.
std::string GetCurrentTimeForLogging() {
  time_t rawtime;
  struct tm timeinfo;
  char buffer[kTouchLogTimestampMaxSize];

  time(&rawtime);
  if (!localtime_r(&rawtime, &timeinfo)) {
    PLOG(ERROR) << "localtime_r failed";
    return "";
  }
  if (!strftime(buffer, kTouchLogTimestampMaxSize, "%Y%m%d-%H%M%S", &timeinfo))
    return "";
  return std::string(buffer);
}

// Canonize the device name for logging.
std::string GetCanonicalDeviceName(const std::string& name) {
  std::string ret(name);
  for (size_t i = 0; i < ret.size(); ++i)
    if (!base::IsAsciiAlpha(ret[i]))
      ret[i] = '_';
  return ret;
}

// Name event logs in a way that is compatible with existing toolchain.
std::string GenerateEventLogName(const base::FilePath& out_dir,
                                 const std::string& prefix,
                                 const std::string& now,
                                 int id) {
  return out_dir.value() + "/" + prefix + now + "." + base::NumberToString(id);
}
// Name event logs in a way that is compatible with existing toolchain.
std::string GenerateEventLogName(GesturePropertyProvider* provider,
                                 const base::FilePath& out_dir,
                                 const std::string& prefix,
                                 const std::string& now,
                                 int id) {
  return out_dir.value() + "/" + prefix + now + "." + base::NumberToString(id) +
         "." + GetCanonicalDeviceName(provider->GetDeviceNameById(id));
}

// Set the logging properties to dump event logs.
void StartToDumpEventLog(GesturePropertyProvider* provider,
                         const int device_id) {
  // Dump gesture log.
  GesturesProp* property = provider->GetProperty(device_id, "Log Path");
  property->SetStringValue(kTouchpadGestureLogPath);
  property = provider->GetProperty(device_id, "Logging Notify");
  property->SetIntValue(std::vector<int>(1, 1));

  // Dump evdev log.
  property = provider->GetProperty(device_id, "Dump Debug Log");
  property->SetBoolValue(std::vector<bool>(1, true));
}

}  // namespace

// Dump touch device property values to a string.
void DumpTouchDeviceStatus(GesturePropertyProvider* provider,
                           std::string* status) {
  // We use DT_ALL since we want gesture property values for all devices that
  // run with the gesture library, not just mice or touchpads.
  std::vector<int> ids;
  provider->GetDeviceIdsByType(DT_ALL, &ids);

  // Dump the property names and values for each device.
  for (size_t i = 0; i < ids.size(); ++i) {
    std::vector<std::string> names = provider->GetPropertyNamesById(ids[i]);
    status->append("\n");
    status->append(base::StringPrintf("ID %d:\n", ids[i]));
    status->append(base::StringPrintf(
        "Device \'%s\':\n", provider->GetDeviceNameById(ids[i]).c_str()));

    // Note that, unlike X11, we don't maintain the "atom" concept here.
    // Therefore, the property name indices we output here shouldn't be treated
    // as unique identifiers of the properties.
    std::sort(names.begin(), names.end());
    for (size_t j = 0; j < names.size(); ++j) {
      status->append(base::StringPrintf("\t%s (%zu):", names[j].c_str(), j));
      GesturesProp* property = provider->GetProperty(ids[i], names[j]);
      status->append("\t" + DumpGesturePropertyValue(property) + '\n');
    }
  }
}

// Dump touch event logs.
void DumpTouchEventLog(
    const std::map<base::FilePath, std::unique_ptr<EventConverterEvdev>>&
        converters,
    GesturePropertyProvider* provider,
    const base::FilePath& out_dir,
    InputController::GetTouchEventLogReply reply) {
  std::vector<base::FilePath> log_paths;
  // Get device ids.
  std::vector<int> ids;
  provider->GetDeviceIdsByType(DT_ALL, &ids);

  // Get current time stamp.
  std::string now = GetCurrentTimeForLogging();

  // Dump event logs for gesture devices.
  std::unique_ptr<std::vector<std::string>> log_paths_to_be_compressed(
      new std::vector<std::string>);
  for (size_t i = 0; i < ids.size(); ++i) {
    // First, see if the device actually uses the gesture library by checking
    // if it has any gesture property.
    std::vector<std::string> names = provider->GetPropertyNamesById(ids[i]);
    if (names.size() == 0)
      continue;

    // Set the logging properties to dump event logs. This needs to be done
    // synchronously for now or we might have race conditions on the debug
    // buffer. If the performance becomes a concern then, we can fork and
    // synchronize it.
    //
    // TODO(sheckylin): Make sure this has no performance impact for user
    // feedbacks.
    StartToDumpEventLog(provider, ids[i]);

    // Rename/move the file to another place since each device's log is
    // always dumped using the same name.
    std::string gesture_log_filename = GenerateEventLogName(
        provider, out_dir, "touchpad_activity_", now, ids[i]);
    base::Move(base::FilePath(kTouchpadGestureLogPath),
               base::FilePath(gesture_log_filename));
    std::string evdev_log_filename = GenerateEventLogName(
        provider, out_dir, "cmt_input_events_", now, ids[i]);
    base::Move(base::FilePath(kTouchpadEvdevLogPath),
               base::FilePath(evdev_log_filename));

    // Historically, we compress touchpad/mouse logs with gzip before tarring
    // them up. We DONT compress touchscreen logs though.
    log_paths_to_be_compressed->push_back(gesture_log_filename);
    log_paths.push_back(base::FilePath(gesture_log_filename));
    log_paths_to_be_compressed->push_back(evdev_log_filename);
    log_paths.push_back(base::FilePath(evdev_log_filename));
  }

  for (const auto& converter_pair : converters) {
    EventConverterEvdev* converter = converter_pair.second.get();
    if (converter->HasTouchscreen()) {
      std::string touch_evdev_log_filename = GenerateEventLogName(
          out_dir, "evdev_input_events_", now, converter->id());
#if defined(OS_CHROMEOS)
      converter->DumpTouchEventLog(touch_evdev_log_filename.c_str());
#else
      converter->DumpTouchEventLog(kInputEventsLogFile);
      base::Move(base::FilePath(kInputEventsLogFile),
                 base::FilePath(touch_evdev_log_filename));
#endif  // defined(OS_CHROMEOS)
      log_paths.push_back(base::FilePath(touch_evdev_log_filename));
    }
  }

  // Compress touchpad/mouse logs asynchronously
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CompressDumpedLog,
                     base::Passed(&log_paths_to_be_compressed)),
      base::BindOnce(std::move(reply), log_paths));
}

}  // namespace ui
