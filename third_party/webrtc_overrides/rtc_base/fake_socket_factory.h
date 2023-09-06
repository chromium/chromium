// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_FAKE_SOCKET_SERVER_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_FAKE_SOCKET_SERVER_H_

#include <map>

#include "base/notreached.h"

#include "third_party/webrtc/rtc_base/socket.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "third_party/webrtc/rtc_base/socket_factory.h"

namespace blink {

class FakeSocketFactory;

// Barebone implementation of the socket interface for use in tests with minimal
// dependencies on webrtc test constructs, threads or clocks.
class FakeSocket : public rtc::Socket {
 public:
  FakeSocket(FakeSocketFactory* factory, int family, int type);

  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;

  int Bind(const rtc::SocketAddress& addr) override;
  int Close() override;

  int GetError() const override;
  void SetError(int error) override;
  rtc::Socket::ConnState GetState() const override;

  // Unimplemented Socket overrides.

  int Connect(const rtc::SocketAddress& addr) override {
    NOTIMPLEMENTED();
    return -1;
  }
  int Send(const void* pv, size_t cb) override {
    NOTIMPLEMENTED();
    return -1;
  }
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr) override {
    NOTIMPLEMENTED();
    return -1;
  }
  int Recv(void* pv, size_t cb, int64_t* timestamp) override {
    NOTIMPLEMENTED();
    return -1;
  }
  int RecvFrom(void* pv,
               size_t cb,
               rtc::SocketAddress* paddr,
               int64_t* timestamp) override {
    NOTIMPLEMENTED();
    return -1;
  }
  int Listen(int backlog) override {
    NOTIMPLEMENTED();
    return -1;
  }
  FakeSocket* Accept(rtc::SocketAddress* paddr) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  int GetOption(rtc::Socket::Option opt, int* value) override {
    NOTIMPLEMENTED();
    return 0;
  }
  int SetOption(rtc::Socket::Option opt, int value) override {
    NOTIMPLEMENTED();
    return 0;
  }

 private:
  FakeSocketFactory* factory_;
  bool bound_;
  int error_;
  rtc::Socket::ConnState state_;
  rtc::SocketAddress local_addr_;
  rtc::SocketAddress remote_addr_;
};

// Generates fake socket objects.
class FakeSocketFactory : public rtc::SocketFactory {
 public:
  FakeSocketFactory();
  ~FakeSocketFactory() override = default;

  rtc::Socket* CreateSocket(int family, int type) override;

  // Assigns a binding address for the requested address.
  rtc::SocketAddress AssignBindAddress(const rtc::SocketAddress& addr);
  int Bind(FakeSocket* socket, const rtc::SocketAddress& addr);
  int Unbind(const rtc::SocketAddress& addr, FakeSocket* socket);

 private:
  typedef std::map<rtc::SocketAddress, FakeSocket*> AddressMap;

  uint16_t GetNextPort();

  uint16_t next_port_;
  AddressMap bindings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_RTC_BASE_FAKE_SOCKET_SERVER_H_
