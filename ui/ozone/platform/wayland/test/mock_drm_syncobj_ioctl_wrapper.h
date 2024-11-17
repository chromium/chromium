// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_DRM_SYNCOBJ_IOCTL_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_DRM_SYNCOBJ_IOCTL_WRAPPER_H_

#include "base/files/scoped_file.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/host/drm_syncobj_ioctl_wrapper.h"
#include "ui/ozone/platform/wayland/test/test_fd_factory.h"

namespace ui {

class MockDrmSyncobjIoctlWrapper : public DrmSyncobjIoctlWrapper {
 public:
  MockDrmSyncobjIoctlWrapper();
  ~MockDrmSyncobjIoctlWrapper() override;

  MockDrmSyncobjIoctlWrapper(const MockDrmSyncobjIoctlWrapper&) = delete;
  MockDrmSyncobjIoctlWrapper& operator=(const MockDrmSyncobjIoctlWrapper&) =
      delete;

  MOCK_METHOD(int,
              SyncobjTransfer,
              (uint32_t dst_handle,
               uint64_t dst_point,
               uint32_t src_handle,
               uint64_t src_point,
               uint32_t flags),
              (override));
  MOCK_METHOD(int,
              SyncobjImportSyncFile,
              (uint32_t handle, int sync_file_fd),
              (override));
  MOCK_METHOD(int,
              SyncobjExportSyncFile,
              (uint32_t handle, int* sync_file_fd),
              (override));
  MOCK_METHOD(int,
              SyncobjEventfd,
              (uint32_t handle, uint64_t point, int ev_fd, uint32_t flags),
              (override));

  const std::map<uint32_t, base::ScopedFD>& syncobjs() const {
    return syncobjs_;
  }

  // These are used to force failure conditions in tests using base::AutoReset.
  static bool fail_on_syncobj_create;
  static bool fail_on_syncobj_handle_to_fd;

 private:
  int SyncobjCreate(uint32_t flags, uint32_t* handle) override;
  int SyncobjDestroy(uint32_t handle) override;
  int SyncobjHandleToFD(uint32_t handle, int* obj_fd) override;

  wl::TestFdFactory fd_factory_;
  int handle_counter_ = 0;
  std::map<uint32_t, base::ScopedFD> syncobjs_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_DRM_SYNCOBJ_IOCTL_WRAPPER_H_
