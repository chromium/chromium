// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_

#include <wayland-server-protocol.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

struct wl_resource;

namespace base {
class SequencedTaskRunner;
}

namespace wl {

extern const struct wl_data_source_interface kTestDataSourceImpl;

class TestDataSource : public TestSelectionSource {
 public:
  explicit TestDataSource(wl_resource* resource);
  ~TestDataSource() override;

  void Offer(const std::string& mime_type);
  void SetActions(uint32_t dnd_actions);

  using ReadDataCallback = base::OnceCallback<void(std::vector<uint8_t>&&)>;
  void ReadData(const std::string& mime_type, ReadDataCallback callback);

  void OnCancelled();

  std::vector<std::string> mime_types() const { return mime_types_; }

  uint32_t actions() const { return actions_; }

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::vector<std::string> mime_types_;
  uint32_t actions_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  DISALLOW_COPY_AND_ASSIGN(TestDataSource);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_
