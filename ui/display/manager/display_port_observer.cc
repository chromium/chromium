// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_port_observer.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/display/types/display_snapshot.h"

namespace display {

namespace {

const LazyRE2 kTypecConnUeventPattern = {R"(TYPEC_PORT=port(\d+))"};

std::vector<uint32_t> ParseDrmSysfsAndFindPort(
    const std::vector<std::pair<uint64_t, base::FilePath>>&
        base_connector_id_and_syspath) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::vector<uint32_t> port_nums;
  // Each pair is from each DisplaySnapshot.
  for (const auto& pair : base_connector_id_and_syspath) {
    auto base_connector_id = pair.first;
    const auto& sys_path = pair.second;
    // `sys_path` of a DRM device, i.e. /sys/class/drm/cardX.
    base::FileEnumerator enumerator(sys_path, false,
                                    base::FileEnumerator::DIRECTORIES,
                                    FILE_PATH_LITERAL("card*-DP-*"));
    // Each directory in `sys_path` represents a connector, so we fetch each
    // connector's ID by reading the `/sys/class/drm/*/connector_id` file
    // (e.g. /sys/class/drm/card0/card0-DP-1/connector_id stores 256). We use
    // the fetched connector id to compare and match the corresponding base
    // connector (root of MST hub tree) per each DisplaySnapshot.
    for (auto path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      std::string connector_id_str;
      uint64_t connector_id_int;
      if (!base::ReadFileToString(path.Append("connector_id"),
                                  &connector_id_str)) {
        continue;
      }
      base::TrimWhitespaceASCII(connector_id_str, base::TRIM_ALL,
                                &connector_id_str);
      if (!base::StringToUint64(connector_id_str, &connector_id_int)) {
        LOG(WARNING) << "Invalid connector id " << connector_id_str << " at "
                     << path.value();
        continue;
      }
      if (connector_id_int != base_connector_id) {
        continue;
      }
      // A connector with a matching id is found at this point, so break the
      // loop beyond here as there is no need to further look for a connector.

      // We find the USB-C port which the DP connector is routed to by
      // referring to the symlink to Type C connector at `typec_connector`
      // (i.e. /sys/class/drm/cardX/cardX-DP-X/typec_connector). This symlink
      // may not be present for older kernel and firmware.
      std::string typec_conn_uevent;
      uint32_t port_num;
      if (!base::ReadFileToString(path.Append("typec_connector/uevent"),
                                  &typec_conn_uevent)) {
        break;
      }
      if (!RE2::PartialMatch(typec_conn_uevent, *kTypecConnUeventPattern,
                             &port_num)) {
        break;
      }
      port_nums.push_back(port_num);
      break;
    }
  }
  return port_nums;
}

}  // namespace

DisplayPortObserver::DisplayPortObserver(
    DisplayConfigurator* configurator,
    base::RepeatingCallback<void(const std::vector<uint32_t>&)>
        on_port_change_callback)
    : configurator_(configurator),
      on_port_change_callback_(on_port_change_callback) {
  if (configurator_) {
    configurator_->AddObserver(this);
  }
}

DisplayPortObserver::~DisplayPortObserver() {
  if (configurator_) {
    configurator_->RemoveObserver(this);
  }
}

void DisplayPortObserver::OnDisplayConfigurationChanged(
    const DisplayConfigurator::DisplayStateList& display_states) {
  // If no changes in base connector ids, then assume no changes on ports, so
  // return early to prevent overkill.
  std::set<uint64_t> base_connector_ids_;
  for (display::DisplaySnapshot* state : display_states) {
    base_connector_ids_.insert(state->base_connector_id());
  }
  if (base_connector_ids_ == prev_base_connector_ids_) {
    return;
  }
  prev_base_connector_ids_ = base_connector_ids_;

  // For each DisplaySnapshot, extract base_connector_id and sys_path.
  std::vector<std::pair<uint64_t, base::FilePath>>
      base_connector_id_and_syspath;
  for (display::DisplaySnapshot* state : display_states) {
    base_connector_id_and_syspath.push_back(
        std::make_pair(state->base_connector_id(), state->sys_path()));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ParseDrmSysfsAndFindPort, base_connector_id_and_syspath),
      base::BindOnce(&DisplayPortObserver::SetTypeCPortsUsingDisplays,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisplayPortObserver::OnDisplayConfigurationChangeFailed(
    const DisplayConfigurator::DisplayStateList& displays,
    MultipleDisplayState failed_new_state) {}

void DisplayPortObserver::SetTypeCPortsUsingDisplays(
    std::vector<uint32_t> port_nums) {
  on_port_change_callback_.Run(port_nums);
}

}  // namespace display
