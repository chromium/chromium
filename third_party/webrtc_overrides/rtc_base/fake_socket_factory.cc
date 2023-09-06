// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/rtc_base/fake_socket_factory.h"

#include "base/check.h"
#include "base/check_op.h"

#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace blink {

namespace {
const uint16_t kFirstEphemeralPort = 49152;
const uint16_t kLastEphemeralPort = 65535;
const uint16_t kEphemeralPortCount =
    kLastEphemeralPort - kFirstEphemeralPort + 1;
}  // unnamed namespace

FakeSocket::FakeSocket(FakeSocketFactory* factory, int family, int type)
    : factory_(factory), error_(0), state_(rtc::Socket::CS_CLOSED) {}

rtc::SocketAddress FakeSocket::GetLocalAddress() const {
  return local_addr_;
}

rtc::SocketAddress FakeSocket::GetRemoteAddress() const {
  return remote_addr_;
}

int FakeSocket::Bind(const rtc::SocketAddress& addr) {
  if (!local_addr_.IsNil()) {
    error_ = EINVAL;
    return -1;
  }

  local_addr_ = factory_->AssignBindAddress(addr);
  int result = factory_->Bind(this, local_addr_);
  if (result != 0) {
    local_addr_.Clear();
    error_ = EADDRINUSE;
  } else {
    bound_ = true;
  }

  return result;
}

int FakeSocket::Close() {
  if (!local_addr_.IsNil() && bound_) {
    factory_->Unbind(local_addr_, this);
    bound_ = false;
  }
  return 0;
}

int FakeSocket::GetError() const {
  return error_;
}

void FakeSocket::SetError(int error) {
  error_ = error;
}

rtc::Socket::ConnState FakeSocket::GetState() const {
  return state_;
}

FakeSocketFactory::FakeSocketFactory() : next_port_(kFirstEphemeralPort) {}

rtc::Socket* FakeSocketFactory::CreateSocket(int family, int type) {
  return new FakeSocket(this, family, type);
}

uint16_t FakeSocketFactory::GetNextPort() {
  uint16_t port = next_port_;
  if (next_port_ < kLastEphemeralPort) {
    ++next_port_;
  } else {
    next_port_ = kFirstEphemeralPort;
  }
  return port;
}

rtc::SocketAddress FakeSocketFactory::AssignBindAddress(
    const rtc::SocketAddress& addr) {
  DCHECK(!rtc::IPIsUnspec(addr.ipaddr()));

  rtc::SocketAddress assigned_addr;
  assigned_addr.SetIP(addr.ipaddr().Normalized());

  if (addr.port() != 0) {
    assigned_addr.SetPort(addr.port());
  } else {
    for (int i = 0; i < kEphemeralPortCount; ++i) {
      assigned_addr.SetPort(GetNextPort());
      if (bindings_.find(assigned_addr) == bindings_.end()) {
        break;
      }
    }
  }

  return assigned_addr;
}

int FakeSocketFactory::Bind(FakeSocket* socket,
                            const rtc::SocketAddress& addr) {
  DCHECK_NE(socket, nullptr);
  DCHECK(!rtc::IPIsUnspec(addr.ipaddr()));
  DCHECK_NE(addr.port(), 0);

  rtc::SocketAddress normalized(addr.ipaddr().Normalized(), addr.port());
  AddressMap::value_type entry(normalized, socket);
  return bindings_.insert(entry).second ? 0 : -1;
}

int FakeSocketFactory::Unbind(const rtc::SocketAddress& addr,
                              FakeSocket* socket) {
  rtc::SocketAddress normalized(addr.ipaddr().Normalized(), addr.port());
  DCHECK((bindings_)[normalized] == socket);
  bindings_.erase(bindings_.find(normalized));
  return 0;
}

}  // namespace blink
