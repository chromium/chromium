// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string_view>

#include "ipcz/ipcz.h"
#include "test/multinode_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using BoxTestNode = test::TestNode;
using BoxTest = test::MultinodeTest<BoxTestNode>;

MULTINODE_TEST(BoxTest, BoxAndUnbox) {
  constexpr const char kMessage[] = "Hello, world?";
  EXPECT_EQ(kMessage, UnboxBlob(BoxBlob(kMessage)));
}

MULTINODE_TEST(BoxTest, CloseBox) {
  // Verifies that box closure releases its underlying driver object. This test
  // does not explicitly observe side effects of that release, but LSan will
  // fail if something's off.
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Close(BoxBlob("meh"), IPCZ_NO_FLAGS, nullptr));
}

MULTINODE_TEST(BoxTest, Peek) {
  constexpr std::string_view kMessage = "Hello, world?";
  IpczHandle box = BoxBlob(kMessage);

  IpczBoxContents box_contents = {.size = sizeof(box_contents)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &box_contents));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &box_contents));
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_UNBOX_PEEK, nullptr, &box_contents));
  EXPECT_EQ(IPCZ_BOX_TYPE_DRIVER_OBJECT, box_contents.type);
  EXPECT_NE(IPCZ_INVALID_DRIVER_HANDLE, box_contents.object.driver_object);

  const IpczDriverHandle memory = box_contents.object.driver_object;
  IpczDriverHandle mapping;
  volatile void* base;
  EXPECT_EQ(IPCZ_RESULT_OK,
            GetDriver().MapSharedMemory(memory, IPCZ_NO_FLAGS, nullptr, &base,
                                        &mapping));
  std::string contents(static_cast<const char*>(const_cast<const void*>(base)),
                       kMessage.size());
  EXPECT_EQ(kMessage, contents);
  EXPECT_EQ(IPCZ_RESULT_OK, GetDriver().Close(mapping, IPCZ_NO_FLAGS, nullptr));

  EXPECT_EQ(kMessage, UnboxBlob(box));
}

constexpr const char kMessage1[] = "Hello, world?";
constexpr const char kMessage2[] = "Hello, world!";
constexpr const char kMessage3[] = "Hello. World.";

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxClient) {
  IpczHandle b = ConnectToBroker();

  std::string message;
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&box, 1}));
  EXPECT_EQ(kMessage2, message);
  EXPECT_EQ(kMessage1, UnboxBlob(box));
  Close(b);
}

MULTINODE_TEST(BoxTest, TransferBox) {
  IpczHandle c = SpawnTestNode<TransferBoxClient>();
  IpczHandle box = BoxBlob(kMessage1);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kMessage2, {&box, 1}));
  Close(c);
}

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxAndPortalClient) {
  IpczHandle b = ConnectToBroker();

  IpczHandle handles[2];
  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, handles));
  EXPECT_EQ(kMessage2, message);
  EXPECT_EQ(IPCZ_RESULT_OK, Put(handles[1], kMessage3));
  EXPECT_EQ(kMessage1, UnboxBlob(handles[0]));
  CloseAll({b, handles[1]});
}

MULTINODE_TEST(BoxTest, TransferBoxAndPortal) {
  IpczHandle c = SpawnTestNode<TransferBoxAndPortalClient>();

  auto [q, p] = OpenPortals();
  IpczHandle box = BoxBlob(kMessage1);
  IpczHandle handles[] = {box, p};
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c, kMessage2, absl::MakeSpan(handles)));

  std::string message;
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message));
  EXPECT_EQ(kMessage3, message);
  CloseAll({c, q});
}

constexpr size_t TransferBoxBetweenNonBrokersNumIterations = 50;

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient1) {
  IpczHandle q;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&q, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczHandle box = BoxBlob(kMessage1);
    EXPECT_EQ(IPCZ_RESULT_OK, Put(q, kMessage2, {&box, 1}));
    box = IPCZ_INVALID_DRIVER_HANDLE;

    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(q, &message, {&box, 1}));
    EXPECT_EQ(kMessage1, message);
    EXPECT_EQ(kMessage2, UnboxBlob(box));
  }

  WaitForDirectRemoteLink(q);
  CloseAll({q, b});
}

MULTINODE_TEST_NODE(BoxTestNode, TransferBoxBetweenNonBrokersClient2) {
  IpczHandle p;
  IpczHandle b = ConnectToBroker();
  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&p, 1}));

  for (size_t i = 0; i < TransferBoxBetweenNonBrokersNumIterations; ++i) {
    IpczHandle box;
    std::string message;
    EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(p, &message, {&box, 1}));
    EXPECT_EQ(kMessage2, message);
    EXPECT_EQ(kMessage1, UnboxBlob(box));

    box = BoxBlob(kMessage2);
    EXPECT_EQ(IPCZ_RESULT_OK, Put(p, kMessage1, {&box, 1}));
  }

  WaitForDirectRemoteLink(p);
  CloseAll({p, b});
}

MULTINODE_TEST(BoxTest, TransferBoxBetweenNonBrokers) {
  IpczHandle c1 = SpawnTestNode<TransferBoxBetweenNonBrokersClient1>();
  IpczHandle c2 = SpawnTestNode<TransferBoxBetweenNonBrokersClient2>();

  // Create a new portal pair and send each end to one of the two non-brokers so
  // they'll establish a direct link.
  auto [q, p] = OpenPortals();
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c1, "", {&q, 1}));
  EXPECT_EQ(IPCZ_RESULT_OK, Put(c2, "", {&p, 1}));

  // Wait for the clients to finish their business and go away.
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c1, IPCZ_TRAP_PEER_CLOSED));
  EXPECT_EQ(IPCZ_RESULT_OK, WaitForConditionFlags(c2, IPCZ_TRAP_PEER_CLOSED));
  CloseAll({c1, c2});
}

// A simple object which we can boxed and transmitted to other nodes, by
// providing ipcz with a custom serialization function.
class NamedPortal {
 public:
  struct Header {
    uint32_t name_length;
  };

  NamedPortal(const IpczAPI& ipcz, std::string_view name, IpczHandle portal)
      : ipcz_(ipcz), name_(name.begin(), name.end()), portal_(portal) {}
  ~NamedPortal() { reset(); }

  static uintptr_t Release(std::unique_ptr<NamedPortal> named_portal) {
    return reinterpret_cast<uintptr_t>(named_portal.release());
  }

  const std::string& name() const { return name_; }
  IpczHandle portal() const { return portal_; }

  void reset() {
    if (portal_ != IPCZ_INVALID_HANDLE) {
      ipcz_.Close(std::exchange(portal_, IPCZ_INVALID_HANDLE), IPCZ_NO_FLAGS,
                  nullptr);
    }
  }

  // Functions to interface with the Box() API.
  static IpczResult Serialize(uintptr_t object,
                              uint32_t,
                              const void*,
                              volatile void* data,
                              size_t* num_bytes,
                              IpczHandle* handles,
                              size_t* num_handles) {
    auto& portal = *reinterpret_cast<NamedPortal*>(object);
    const size_t required_byte_capacity = sizeof(Header) + portal.name_.size();
    const size_t required_handle_capacity = 1;
    const size_t byte_capacity = num_bytes ? *num_bytes : 0;
    const size_t handle_capacity = num_handles ? *num_handles : 0;
    if (num_bytes) {
      *num_bytes = required_byte_capacity;
    }
    if (num_handles) {
      *num_handles = required_handle_capacity;
    }
    if (byte_capacity < required_byte_capacity ||
        handle_capacity < required_handle_capacity) {
      return IPCZ_RESULT_RESOURCE_EXHAUSTED;
    }

    auto* header = static_cast<volatile Header*>(data);
    header->name_length = static_cast<size_t>(portal.name_.size());
    memcpy(const_cast<Header*>(header + 1), portal.name_.data(),
           portal.name_.size());
    handles[0] = std::exchange(portal.portal_, IPCZ_INVALID_HANDLE);
    return IPCZ_RESULT_OK;
  }

  static void Destroy(uintptr_t object, uint32_t, const void*) {
    delete reinterpret_cast<NamedPortal*>(object);
  }

  static std::unique_ptr<NamedPortal> Deserialize(
      const IpczAPI& ipcz,
      absl::Span<const uint8_t> bytes,
      absl::Span<const IpczHandle> handles) {
    ABSL_HARDENING_ASSERT(bytes.size() >= sizeof(Header));
    const auto& header = *reinterpret_cast<const Header*>(bytes.data());
    ABSL_HARDENING_ASSERT(bytes.size() == sizeof(Header) + header.name_length);
    auto name_bytes = bytes.subspan(sizeof(Header), header.name_length);
    const std::string name(name_bytes.begin(), name_bytes.end());
    ABSL_HARDENING_ASSERT(handles.size() == 1);
    return std::make_unique<NamedPortal>(ipcz, name, handles[0]);
  }

 private:
  const IpczAPI& ipcz_;
  const std::string name_;
  IpczHandle portal_;
};

constexpr std::string_view kPortalName = "yahoo?";

MULTINODE_TEST_NODE(BoxTestNode, SerializedApplicationObjectClient) {
  IpczHandle b = ConnectToBroker();
  IpczHandle box;
  std::string message;
  ASSERT_EQ(IPCZ_RESULT_OK, WaitToGet(b, &message, {&box, 1}));
  EXPECT_EQ("hey", message);

  IpczBoxContents contents = {.size = sizeof(contents)};
  ASSERT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &contents));
  EXPECT_EQ(IPCZ_BOX_TYPE_SUBPARCEL, contents.type);
  EXPECT_NE(IPCZ_INVALID_HANDLE, contents.object.subparcel);

  const IpczHandle subparcel = contents.object.subparcel;
  constexpr size_t kDataSize = sizeof(NamedPortal::Header) + kPortalName.size();
  uint8_t data[kDataSize];
  size_t num_bytes = kDataSize;
  IpczHandle p;
  size_t num_handles = 1;
  EXPECT_EQ(IPCZ_RESULT_OK, ipcz().Get(subparcel, IPCZ_NO_FLAGS, nullptr, data,
                                       &num_bytes, &p, &num_handles, nullptr));
  auto portal = NamedPortal::Deserialize(ipcz(), absl::MakeSpan(data), {&p, 1});
  EXPECT_EQ(kPortalName, portal->name());
  VerifyEndToEnd(portal->portal());
  CloseAll({b, subparcel});
}

MULTINODE_TEST(BoxTest, SerializedApplicationObject) {
  IpczHandle c = SpawnTestNode<SerializedApplicationObjectClient>();
  auto [q, p] = OpenPortals();
  auto portal = std::make_unique<NamedPortal>(ipcz(), kPortalName, p);

  const IpczBoxContents contents = {
      .size = sizeof(contents),
      .type = IPCZ_BOX_TYPE_APPLICATION_OBJECT,
      .object = {.application_object = NamedPortal::Release(std::move(portal))},
      .serializer = &NamedPortal::Serialize,
      .destructor = &NamedPortal::Destroy,
  };
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), &contents, IPCZ_NO_FLAGS, nullptr, &box));
  Put(c, "hey", {&box, 1});
  VerifyEndToEnd(q);
  CloseAll({c, q});
}

MULTINODE_TEST(BoxTest, UnserializedApplicationObject) {
  auto [a, b] = OpenPortals();
  auto [q, p] = OpenPortals();
  auto portal = std::make_unique<NamedPortal>(ipcz(), kPortalName, p);

  // We can box an object without a serializer and transfer it between local
  // portals.
  const IpczBoxContents in_contents = {
      .size = sizeof(in_contents),
      .type = IPCZ_BOX_TYPE_APPLICATION_OBJECT,
      .object = {.application_object = NamedPortal::Release(std::move(portal))},
      .destructor = &NamedPortal::Destroy,
  };
  IpczHandle box;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Box(node(), &in_contents, IPCZ_NO_FLAGS, nullptr, &box));
  Put(a, "", {&box, 1});
  box = IPCZ_INVALID_HANDLE;

  EXPECT_EQ(IPCZ_RESULT_OK, WaitToGet(b, nullptr, {&box, 1}));

  IpczBoxContents out_contents = {.size = sizeof(out_contents)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &out_contents));
  EXPECT_EQ(IPCZ_BOX_TYPE_APPLICATION_OBJECT, out_contents.type);

  portal = std::unique_ptr<NamedPortal>(
      reinterpret_cast<NamedPortal*>(out_contents.object.application_object));
  VerifyEndToEndLocal(q, portal->portal());
  CloseAll({a, b, q});
}

}  // namespace
}  // namespace ipcz
