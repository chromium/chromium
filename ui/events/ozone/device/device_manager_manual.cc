// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/device/device_manager_manual.h"

#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"

namespace ui {

namespace {

const base::FilePath::CharType kDevInput[] = FILE_PATH_LITERAL("/dev/input");

void ScanDevicesOnWorkerThread(std::vector<base::FilePath>* result) {
  base::FileEnumerator file_enum(base::FilePath(kDevInput), false,
                                 base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("event*[0-9]"));
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    result->push_back(path);
  }
}
}  // namespace

DeviceManagerManual::DeviceManagerManual()
    : blocking_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      watcher_(new base::FilePathWatcher,
               base::OnTaskRunnerDeleter(blocking_task_runner_)) {}

DeviceManagerManual::~DeviceManagerManual() {}

void DeviceManagerManual::ScanDevices(DeviceEventObserver* observer) {
  if (!is_watching_) {
    is_watching_ = true;
    StartWatching();
  }

  InitiateScanDevices();
}

void DeviceManagerManual::AddObserver(DeviceEventObserver* observer) {
  observers_.AddObserver(observer);
  // Notify the new observer about existing devices.
  for (const auto& path : devices_) {
    DeviceEvent event(DeviceEvent::INPUT, DeviceEvent::ADD, path);
    observer->OnDeviceEvent(event);
  }
}

void DeviceManagerManual::RemoveObserver(DeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceManagerManual::StartWatching() {
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &base::FilePathWatcher::Watch, base::Unretained(watcher_.get()),
          base::FilePath(kDevInput), base::FilePathWatcher::Type::kNonRecursive,
          base::BindRepeating(&DeviceManagerManual::OnWatcherEventOnUiSequence,
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              weak_ptr_factory_.GetWeakPtr())),
      base::BindOnce([](bool watch_started) {
        if (!watch_started)
          LOG(ERROR) << "Failed to start FilePathWatcher";
      }));
}

void DeviceManagerManual::InitiateScanDevices() {
  std::vector<base::FilePath>* result = new std::vector<base::FilePath>();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ScanDevicesOnWorkerThread, result),
      base::BindOnce(&DeviceManagerManual::OnDevicesScanned,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(result)));
}

void DeviceManagerManual::OnDevicesScanned(
    std::vector<base::FilePath>* result) {
  std::set<base::FilePath> new_devices;

  // Reported newly added devices.
  for (const auto& path : *result) {
    new_devices.insert(path);
    // Don't report devices already added.
    if (devices_.find(path) != devices_.end())
      continue;

    DeviceEvent event(DeviceEvent::INPUT, DeviceEvent::ADD, path);
    observers_.Notify(&DeviceEventObserver::OnDeviceEvent, event);
  }

  // Report removed devices.
  for (const auto& path : devices_) {
    if (new_devices.find(path) != new_devices.end())
      continue;

    DeviceEvent event(DeviceEvent::INPUT, DeviceEvent::REMOVE, path);
    observers_.Notify(&DeviceEventObserver::OnDeviceEvent, event);
  }

  devices_.swap(new_devices);
}

void DeviceManagerManual::OnWatcherEvent(const base::FilePath& path,
                                         bool error) {
  // We need to restart watching if there's an error.
  if (error) {
    StartWatching();
  }
  InitiateScanDevices();
}

// static
void DeviceManagerManual::OnWatcherEventOnUiSequence(
    scoped_refptr<base::TaskRunner> ui_thread_runner,
    base::WeakPtr<DeviceManagerManual> device_manager,
    const base::FilePath& path,
    bool error) {
  ui_thread_runner->PostTask(
      FROM_HERE, BindOnce(&DeviceManagerManual::OnWatcherEvent, device_manager,
                          path, error));
}

}  // namespace ui
