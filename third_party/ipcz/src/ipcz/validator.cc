// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/validator.h"

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "util/ref_counted.h"

namespace ipcz {

Validator::Validator(Ref<NodeLink> remote_source)
    : remote_source_(std::move(remote_source)) {}

Validator::~Validator() = default;

IpczResult Validator::Close() {
  return IPCZ_RESULT_OK;
}

IpczResult Validator::Reject(uintptr_t context) {
  if (!remote_source_) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  const IpczDriver& driver = remote_source_->node()->driver();
  const Ref<DriverTransport>& transport = remote_source_->transport();
  driver.ReportBadTransportActivity(transport->driver_object().handle(),
                                    context, IPCZ_NO_FLAGS, nullptr);
  return IPCZ_RESULT_OK;
}

}  // namespace ipcz
