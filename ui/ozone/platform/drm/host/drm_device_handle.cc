// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_device_handle.h"

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace ui {

namespace {

// Sleep this many milliseconds before retrying after authentication fails.
const int kAuthFailSleepMs = 100;

// Log a warning after failing to authenticate for this many milliseconds.
const int kLogAuthFailDelayMs = 1000;

bool Authenticate(int fd) {
  drm_magic_t magic;
  memset(&magic, 0, sizeof(magic));
  // We need to make sure the DRM device has enough privilege. Use the DRM
  // authentication logic to figure out if the device has enough permissions.
  return !drmGetMagic(fd, &magic) && !drmAuthMagic(fd, magic);
}

}  // namespace

DrmDeviceHandle::DrmDeviceHandle() {
}

DrmDeviceHandle::~DrmDeviceHandle() {
  if (file_.is_valid()) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    file_.reset();
  }
}

bool DrmDeviceHandle::Initialize(const base::FilePath& dev_path,
                                 const base::FilePath& sys_path) {
  // Security folks have requested that we assert the graphics device has the
  // expected path, so use a CHECK instead of a DCHECK. The sys_path is only
  // used a label and is otherwise unvalidated.
  CHECK(dev_path.DirName() == base::FilePath("/dev/dri"));
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int num_auth_attempts = 0;
  bool logged_warning = false;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (true) {
    file_.reset();
    int fd = HANDLE_EINTR(open(dev_path.value().c_str(), O_RDWR | O_CLOEXEC));
    if (fd < 0) {
      PLOG(ERROR) << "Failed to open " << dev_path.value();
      return false;
    }

    file_.reset(fd);
    sys_path_ = sys_path;

    num_auth_attempts++;
    if (Authenticate(file_.get())) {
      struct drm_set_client_cap cap = {DRM_CLIENT_CAP_ATOMIC, 1};
      has_atomic_capabilities_ =
          !drmIoctl(file_.get(), DRM_IOCTL_SET_CLIENT_CAP, &cap);

      cap = {DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1};
      drmIoctl(file_.get(), DRM_IOCTL_SET_CLIENT_CAP, &cap);
      break;
    }

    // To avoid spamming the logs, hold off before logging a warning (some
    // failures are expected at first) and only log a single message.
    if (!logged_warning &&
        (base::TimeTicks::Now() - start_time).InMilliseconds() >=
            kLogAuthFailDelayMs) {
      LOG(WARNING) << "Failed to authenticate " << dev_path.value()
                   << " within " << kLogAuthFailDelayMs << " ms";
      logged_warning = true;
    }
    usleep(kAuthFailSleepMs * 1000);
  }

  VLOG(1) << "Succeeded authenticating " << dev_path.value() << " in "
          << (base::TimeTicks::Now() - start_time).InMilliseconds() << " ms "
          << "with " << num_auth_attempts << " attempt(s)";
  return true;
}

bool DrmDeviceHandle::IsValid() const {
  return file_.is_valid();
}

base::ScopedFD DrmDeviceHandle::PassFD() {
  return std::move(file_);
}

}  // namespace ui
