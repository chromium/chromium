// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_

#include <memory>
#include <string>

#include <wayland-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/public/platform_clipboard.h"

struct wl_resource;

namespace base {
class SequencedTaskRunner;
}

namespace wl {

extern const struct wl_data_offer_interface kTestDataOfferImpl;

class TestDataOffer : public ServerObject {
 public:
  explicit TestDataOffer(wl_resource* resource);
  ~TestDataOffer() override;

  void Receive(const std::string& mime_type, base::ScopedFD fd);
  void OnOffer(const std::string& mime_type, ui::PlatformClipboard::Data data);
  void SetActions(uint32_t dnd_actions, uint32_t preferred_action);

  void OnSourceActions(uint32_t source_actions);
  void OnAction(uint32_t dnd_action);

  uint32_t supported_actions() const { return client_supported_actions_; }
  uint32_t preferred_action() const { return client_preferred_action_; }

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ui::PlatformClipboard::DataMap data_to_offer_;

  uint32_t client_supported_actions_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  uint32_t client_preferred_action_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  base::WeakPtrFactory<TestDataOffer> write_data_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestDataOffer);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_
