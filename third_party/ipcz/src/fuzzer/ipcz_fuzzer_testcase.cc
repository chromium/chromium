// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuzzer/ipcz_fuzzer_testcase.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

#include "api.h"
#include "fuzzer/driver.h"
#include "fuzzer/fuzzer.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::fuzzer {

namespace {

const IpczAPI& GetIpcz() {
  static IpczAPI api = [] {
    IpczAPI ipcz = {sizeof(ipcz)};
    IpczGetAPI(&ipcz);
    return ipcz;
  }();
  return api;
}

class ScopedIpczHandle {
 public:
  ScopedIpczHandle() = default;
  explicit ScopedIpczHandle(IpczHandle handle) : handle_(handle) {}
  ScopedIpczHandle(ScopedIpczHandle&& other) : handle_(other.release()) {}
  ScopedIpczHandle& operator=(ScopedIpczHandle&& other) {
    reset();
    handle_ = other.release();
    return *this;
  }
  ~ScopedIpczHandle() { reset(); }

  bool is_valid() const { return handle_ != IPCZ_INVALID_HANDLE; }

  void reset() {
    if (is_valid()) {
      GetIpcz().Close(release(), IPCZ_NO_FLAGS, nullptr);
    }
  }

  IpczHandle get() const { return handle_; }
  IpczHandle release() { return std::exchange(handle_, IPCZ_INVALID_HANDLE); }

  IpczHandle* receive() {
    reset();
    return &handle_;
  }

  bool SendPortal(ScopedIpczHandle handle) const {
    if (!is_valid()) {
      return false;
    }

    IpczHandle value = handle.get();
    if (GetIpcz().Put(handle_, nullptr, 0, &value, 1, IPCZ_NO_FLAGS, nullptr) !=
        IPCZ_RESULT_OK) {
      return false;
    }

    std::ignore = handle.release();
    return true;
  }

  ScopedIpczHandle ReceivePortal() const {
    if (!is_valid()) {
      return {};
    }

    IpczHandle handle;
    size_t num_handles = 1;
    if (GetIpcz().Get(handle_, IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                      &handle, &num_handles, nullptr) != IPCZ_RESULT_OK ||
        num_handles == 0) {
      return {};
    }

    return ScopedIpczHandle(handle);
  }

  bool SendData(absl::Span<const uint8_t> data) const {
    if (!is_valid()) {
      return false;
    }

    return GetIpcz().Put(handle_, &data[0], data.size(), nullptr, 0,
                         IPCZ_NO_FLAGS, nullptr) == IPCZ_RESULT_OK;
  }

  void DiscardData() const {
    if (!is_valid()) {
      return;
    }

    size_t num_bytes = 0;
    if (GetIpcz().Get(handle_, IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                      nullptr, nullptr,
                      nullptr) != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
      return;
    }

    std::vector<uint8_t> data(num_bytes);
    GetIpcz().Get(handle_, IPCZ_NO_FLAGS, nullptr, data.data(), &num_bytes,
                  nullptr, nullptr, nullptr);
  }

 private:
  IpczHandle handle_ = IPCZ_INVALID_HANDLE;
};

// An uninteresting driver object which we use to exercise driver boxing below.
class InertObject : public DriverObjectImpl<> {};

struct SendMessagesParams {
  size_t message_size;
  int num_messages;
};
void SendMessages(Fuzzer& fuzzer,
                  const ScopedIpczHandle& a,
                  const ScopedIpczHandle& b,
                  SendMessagesParams params) {
  std::vector<uint8_t> data(params.message_size);
  for (int i = 0; i < params.num_messages; ++i) {
    a.SendData(absl::MakeSpan(data));
  }
  fuzzer.FlushTransports();
  for (int i = 0; i < params.num_messages; ++i) {
    b.DiscardData();
  }
}

ScopedIpczHandle MakeDriverObjectBox(Fuzzer& fuzzer,
                                     const ScopedIpczHandle& node) {
  ScopedIpczHandle box;
  const IpczBoxContents contents = {
      .size = sizeof(contents),
      .type = IPCZ_BOX_TYPE_DRIVER_OBJECT,
      .object =
          {
              .driver_object =
                  fuzzer.RegisterDriverObject(MakeRefCounted<InertObject>()),
          },
  };
  GetIpcz().Box(node.get(), &contents, IPCZ_NO_FLAGS, nullptr, box.receive());
  return box;
}

ScopedIpczHandle MakeApplicationObjectBox(Fuzzer& fuzzer,
                                          const ScopedIpczHandle& node) {
  ScopedIpczHandle box;
  const IpczApplicationObjectSerializer serialize =
      [](uintptr_t, uint32_t, const void*, volatile void* data,
         size_t* num_bytes, IpczHandle*, size_t*) {
        const size_t data_capacity = num_bytes ? *num_bytes : 0;
        if (num_bytes) {
          *num_bytes = 1;
        }
        if (!data_capacity) {
          return IPCZ_RESULT_RESOURCE_EXHAUSTED;
        }
        static_cast<volatile uint8_t*>(data)[0] = 42;
        return IPCZ_RESULT_OK;
      };
  const IpczApplicationObjectDestructor destroy = [](uintptr_t, uint32_t,
                                                     const void*) {};
  const IpczBoxContents contents = {
      .size = sizeof(contents),
      .type = IPCZ_BOX_TYPE_APPLICATION_OBJECT,
      .object = {.application_object = 1},
      .serializer = serialize,
      .destructor = destroy,
  };
  GetIpcz().Box(node.get(), &contents, IPCZ_NO_FLAGS, nullptr, box.receive());
  return box;
}

void MoveBoxes(Fuzzer& fuzzer,
               const ScopedIpczHandle& from,
               const ScopedIpczHandle& to,
               absl::Span<ScopedIpczHandle> boxes) {
  if (!from.is_valid() || !to.is_valid()) {
    return;
  }

  std::vector<IpczHandle> handles;
  for (auto& handle : boxes) {
    if (handle.is_valid()) {
      handles.push_back(handle.release());
    }
  }
  if (GetIpcz().Put(from.get(), nullptr, 0, handles.data(), handles.size(),
                    IPCZ_NO_FLAGS, nullptr) != IPCZ_RESULT_OK) {
    for (size_t i = 0; i < handles.size(); ++i) {
      boxes[i] = ScopedIpczHandle(handles[i]);
    }
    return;
  }

  fuzzer.FlushTransports();

  size_t num_handles = handles.size();
  std::fill(handles.begin(), handles.end(), IPCZ_INVALID_HANDLE);
  const IpczResult result =
      GetIpcz().Get(to.get(), IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                    handles.data(), &num_handles, nullptr);
  if (result != IPCZ_RESULT_OK) {
    return;
  }

  for (size_t i = 0; i < num_handles; ++i) {
    // Unbox just to get some more code coverage.
    IpczBoxContents contents = {sizeof(contents)};
    GetIpcz().Unbox(handles[i], IPCZ_UNBOX_PEEK, nullptr, &contents);
    GetIpcz().Close(handles[i], IPCZ_NO_FLAGS, nullptr);
  }
}

}  // namespace

IpczFuzzerTestcase::IpczFuzzerTestcase(Fuzzer& fuzzer) : fuzzer_(fuzzer) {}

void IpczFuzzerTestcase::Run() {
  // We create a total of 6 nodes in order to exercise all interesting node
  // connection and introduction paths:
  //
  //   0. Broker A
  //   1. Broker B
  //   2. Client of broker A
  //   3. Client of broker A which delegates shm allocation to the broker
  //   4. Client of broker A which is introduced transitively by client #2
  //   5. Client of broker B
  //
  struct FuzzNode {
    // The node handle.
    ScopedIpczHandle handle;

    // A portal for each possible peer.
    std::array<ScopedIpczHandle, 6> peers;
  };
  std::vector<FuzzNode> nodes(6);

  const IpczAPI& ipcz = GetIpcz();
  const auto connect_nodes = [&ipcz, &nodes, this](
                                 size_t index0, uint32_t flags0, size_t index1,
                                 uint32_t flags1) {
    IpczDriverHandle t0, t1;
    fuzzer_.CreateTransports(&t0, &t1);
    ipcz.ConnectNode(nodes[index0].handle.get(), t0, 1, flags0, nullptr,
                     nodes[index0].peers[index1].receive());
    ipcz.ConnectNode(nodes[index1].handle.get(), t1, 1, flags1, nullptr,
                     nodes[index1].peers[index0].receive());
    fuzzer_.FlushTransports();
  };

  const auto introduce_nodes = [&ipcz, &nodes, this](size_t from_node,
                                                     size_t first,
                                                     size_t second) {
    ScopedIpczHandle q, p;
    ipcz.OpenPortals(nodes[from_node].handle.get(), IPCZ_NO_FLAGS, nullptr,
                     q.receive(), p.receive());
    if (!nodes[from_node].peers[first].SendPortal(std::move(q)) ||
        !nodes[from_node].peers[second].SendPortal(std::move(p))) {
      return;
    }
    fuzzer_.FlushTransports();
    nodes[first].peers[second] = nodes[first].peers[from_node].ReceivePortal();
    nodes[second].peers[first] = nodes[second].peers[from_node].ReceivePortal();
  };

  ipcz.CreateNode(&kDriver, IPCZ_CREATE_NODE_AS_BROKER, nullptr,
                  nodes[0].handle.receive());
  ipcz.CreateNode(&kDriver, IPCZ_CREATE_NODE_AS_BROKER, nullptr,
                  nodes[1].handle.receive());
  for (size_t i = 2; i < nodes.size(); ++i) {
    ipcz.CreateNode(&kDriver, IPCZ_NO_FLAGS, nullptr,
                    nodes[i].handle.receive());
  }
  connect_nodes(2, IPCZ_CONNECT_NODE_SHARE_BROKER, 4,
                IPCZ_CONNECT_NODE_INHERIT_BROKER);
  connect_nodes(0, IPCZ_NO_FLAGS, 2, IPCZ_CONNECT_NODE_TO_BROKER);
  connect_nodes(
      0, IPCZ_NO_FLAGS, 3,
      IPCZ_CONNECT_NODE_TO_BROKER | IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE);
  connect_nodes(0, IPCZ_CONNECT_NODE_TO_BROKER, 1, IPCZ_CONNECT_NODE_TO_BROKER);
  connect_nodes(1, IPCZ_NO_FLAGS, 5, IPCZ_CONNECT_NODE_TO_BROKER);

  introduce_nodes(0, 2, 3);
  introduce_nodes(2, 0, 4);
  introduce_nodes(2, 3, 4);
  introduce_nodes(1, 0, 5);
  introduce_nodes(0, 2, 5);
  introduce_nodes(2, 3, 5);
  introduce_nodes(3, 4, 5);

  // First cover some basic parallel allocation and large message cases.
  SendMessages(fuzzer_, nodes[0].peers[2], nodes[2].peers[0],
               {.message_size = 64, .num_messages = 4});
  SendMessages(fuzzer_, nodes[0].peers[3], nodes[3].peers[0],
               {.message_size = 8 * 1024, .num_messages = 1});

  // Now exercise each pair of nodes with different arbitrary messages.
  for (size_t i = 0; i < nodes.size() - 1; ++i) {
    for (size_t j = i + 1; j < nodes.size(); ++j) {
      const auto& q = nodes[i].peers[j];
      const auto& p = nodes[j].peers[i];
      switch ((i + j) % 4) {
        case 0:
          SendMessages(fuzzer_, q, p, {.message_size = 64, .num_messages = 1});
          break;

        case 1:
          SendMessages(fuzzer_, p, q, {.message_size = 64, .num_messages = 1});
          break;

        case 2: {
          ScopedIpczHandle driver_boxes[] = {
              MakeDriverObjectBox(fuzzer_, nodes[i].handle),
              MakeDriverObjectBox(fuzzer_, nodes[i].handle),
          };
          MoveBoxes(fuzzer_, q, p, absl::MakeSpan(driver_boxes));
          break;
        }

        case 3: {
          ScopedIpczHandle application_boxes[] = {
              MakeApplicationObjectBox(fuzzer_, nodes[j].handle),
              MakeApplicationObjectBox(fuzzer_, nodes[j].handle),
          };
          MoveBoxes(fuzzer_, p, q, absl::MakeSpan(application_boxes));
          break;
        }
      }
    }
  }
}

}  // namespace ipcz::fuzzer
