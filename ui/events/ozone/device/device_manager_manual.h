// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_DEVICE_MANAGER_MANUAL_H_
#define UI_EVENTS_OZONE_DEVICE_DEVICE_MANAGER_MANUAL_H_

#include <set>
#include <vector>

#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/observer_list.h"

#include "ui/events/ozone/device/device_manager.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
struct OnTaskRunnerDeleter;
}  // namespace base

namespace ui {

class DeviceManagerManual : public DeviceManager {
 public:
  DeviceManagerManual();
  ~DeviceManagerManual() override;

 private:
  // DeviceManager overrides:
  void ScanDevices(DeviceEventObserver* observer) override;
  void AddObserver(DeviceEventObserver* observer) override;
  void RemoveObserver(DeviceEventObserver* observer) override;

  void StartWatching();
  void InitiateScanDevices();
  void OnDevicesScanned(std::vector<base::FilePath>* result);
  void OnWatcherEvent(const base::FilePath& path, bool error);
  static void OnWatcherEventOnUiSequence(
      scoped_refptr<base::TaskRunner> ui_thread_runner,
      base::WeakPtr<DeviceManagerManual> device_manager,
      const base::FilePath& path,
      bool error);

  std::set<base::FilePath> devices_;
  base::ObserverList<DeviceEventObserver>::Unchecked observers_;
  bool is_watching_ = false;

  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<base::FilePathWatcher, base::OnTaskRunnerDeleter> watcher_;

  base::WeakPtrFactory<DeviceManagerManual> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerManual);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_DEVICE_MANAGER_MANUAL_H_
