// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace base {
class SequencedTaskRunner;
}

namespace wl {

class TestSelectionSource;
class TestSelectionDevice;

// Base classes for data device implementations. Protocol specific derived
// classes must bind request handlers and factory methods for data device and
// source instances. E.g: Standard data device (wl_data_*), as well as zwp and
// gtk primary selection protocols.

class TestSelectionDeviceManager : public GlobalObject {
 public:
  struct Delegate {
    virtual ~Delegate() = default;

    virtual TestSelectionDevice* CreateDevice(wl_client* client,
                                              uint32_t id) = 0;
    virtual TestSelectionSource* CreateSource(wl_client* client,
                                              uint32_t id) = 0;
  };

  struct InterfaceInfo {
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #global-scope
    RAW_PTR_EXCLUSION const struct wl_interface* interface;
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #global-scope
    RAW_PTR_EXCLUSION const void* implementation;
    uint32_t version;
  };

  TestSelectionDeviceManager(const InterfaceInfo& info,
                             std::unique_ptr<Delegate> delegate);
  ~TestSelectionDeviceManager() override;

  TestSelectionDeviceManager(const TestSelectionDeviceManager&) = delete;
  TestSelectionDeviceManager& operator=(const TestSelectionDeviceManager&) =
      delete;

  TestSelectionDevice* device() { return device_; }
  TestSelectionSource* source() { return source_; }

  void set_source(TestSelectionSource* source) { source_ = source; }

  // Protocol object requests:
  static void CreateSource(wl_client* client,
                           wl_resource* manager_resource,
                           uint32_t id);
  static void GetDevice(wl_client* client,
                        wl_resource* manager_resource,
                        uint32_t id,
                        wl_resource* seat_resource);

 private:
  const std::unique_ptr<Delegate> delegate_;
  raw_ptr<TestSelectionDevice, DanglingUntriaged> device_ = nullptr;
  raw_ptr<TestSelectionSource, DanglingUntriaged> source_ = nullptr;
};

class TestSelectionOffer : public ServerObject {
 public:
  struct Delegate {
    virtual ~Delegate() = default;

    virtual void SendOffer(const std::string& mime_type) = 0;
  };

  TestSelectionOffer(wl_resource* resource, std::unique_ptr<Delegate> delegate);
  ~TestSelectionOffer() override;

  TestSelectionOffer(const TestSelectionOffer&) = delete;
  TestSelectionOffer& operator=(const TestSelectionOffer&) = delete;

  void OnOffer(const std::string& mime_type, ui::PlatformClipboard::Data data);

  // Protocol object requests:
  static void Receive(wl_client* client,
                      wl_resource* resource,
                      const char* mime_type,
                      int fd);

 private:
  const std::unique_ptr<Delegate> delegate_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ui::PlatformClipboard::DataMap data_to_offer_;
};

class TestSelectionSource : public ServerObject {
 public:
  struct Delegate {
    virtual ~Delegate() = default;

    virtual void SendSend(const std::string& mime_type,
                          base::ScopedFD write_fd) = 0;
    virtual void SendFinished() = 0;
    virtual void SendCancelled() = 0;
    virtual void SendDndAction(uint32_t action) = 0;
    virtual void SendDndDropPerformed() = 0;
  };

  TestSelectionSource(wl_resource* resource,
                      std::unique_ptr<Delegate> delegate);
  ~TestSelectionSource() override;

  using ReadDataCallback = base::OnceCallback<void(std::vector<uint8_t>&&)>;
  void ReadData(const std::string& mime_type, ReadDataCallback callback);

  void OnFinished();
  void OnCancelled();
  void OnDndAction(uint32_t action);
  void OnDndDropPerformed();

  const std::vector<std::string>& mime_types() const { return mime_types_; }

  // Protocol object requests:
  static void Offer(struct wl_client* client,
                    struct wl_resource* resource,
                    const char* mime_type);

 private:
  const std::unique_ptr<Delegate> delegate_;

  std::vector<std::string> mime_types_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class TestSelectionDevice : public ServerObject {
 public:
  struct Delegate {
    virtual ~Delegate() = default;

    virtual TestSelectionOffer* CreateAndSendOffer() = 0;
    virtual void SendSelection(TestSelectionOffer* offer) = 0;
    virtual void HandleSetSelection(TestSelectionSource* source,
                                    uint32_t serial) = 0;
  };

  TestSelectionDevice(wl_resource* resource,
                      std::unique_ptr<Delegate> delegate);
  ~TestSelectionDevice() override;

  TestSelectionDevice(const TestSelectionDevice&) = delete;
  TestSelectionDevice& operator=(const TestSelectionDevice&) = delete;

  TestSelectionOffer* OnDataOffer();
  void OnSelection(TestSelectionOffer* offer);

  void set_manager(TestSelectionDeviceManager* manager) { manager_ = manager; }

  // Protocol object requests:
  static void SetSelection(struct wl_client* client,
                           struct wl_resource* resource,
                           struct wl_resource* source,
                           uint32_t serial);

  uint32_t selection_serial() const { return selection_serial_; }

 private:
  const std::unique_ptr<Delegate> delegate_;
  uint32_t selection_serial_ = 0;
  raw_ptr<TestSelectionDeviceManager> manager_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SELECTION_DEVICE_MANAGER_H_
