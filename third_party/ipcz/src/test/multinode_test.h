// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_TEST_MULTINODE_TEST_H_
#define IPCZ_SRC_TEST_MULTINODE_TEST_H_

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ipcz/ipcz.h"
#include "test/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/ipcz/src/test_buildflags.h"
#include "util/ref_counted.h"

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
#include "test/test_child_launcher.h"
#endif

namespace ipcz::test {

class TestNode;

template <typename TestNodeType>
class MultinodeTest;

// Selects which driver will be used by test nodes. Interconnecting nodes must
// always use the same driver.
//
// Multinode tests are parameterized over these modes to provide coverage of
// various interesting constraints encountered in production. Some platforms
// require driver objects to be relayed through a broker. Some environments
// prevent nodes from allocating their own shared memory regions.
//
// Incongruity between synchronous and asynchronous test failures generally
// indicates race conditions within ipcz, but many bugs will cause failures in
// all driver modes. The synchronous version is generally easier to debug in
// such cases.
enum class DriverMode {
  // Use the synchronous, single-process reference driver. This driver does not
  // create any background threads and all ipcz operations (e.g. message
  // delivery, portal transfer, proxy elimination, etc) complete synchronously
  // from end-to-end. Each test node runs its test body on a dedicated thread
  // within the test process.
  kSync,

  // Use the asynchronous single-process reference driver. Transport messages
  // are received asynchronously, similar to how most production drivers are
  // likely to operate in practice. Such asynchrony gives rise to
  // non-determinism throughout ipcz proper and provides good coverage of
  // potential race conditions.
  //
  // As with the kSync driver, each test node runs its test body on a dedicated
  // thread within the test process.
  kAsync,

  // Use the same driver as kAsync, but non-broker nodes are forced to delegate
  // shared memory allocation to their broker. This simulates the production
  // constraints of some sandbox environments and exercises additional
  // asynchrony in ipcz proper.
  kAsyncDelegatedAlloc,

  // Use the same driver as kAsync, but driver objects cannot be transmitted
  // directly between non-brokers and must instead be relayed by a broker. This
  // simulates the production constraints of some sandbox environments and
  // exercises additional asynchrony in ipcz proper.
  kAsyncObjectBrokering,

  // Use the same driver as kAsync, imposing the additional constraints of both
  // kAsyncDelegatedAlloc and kAsyncObjectBrokering as described above.
  kAsyncObjectBrokeringAndDelegatedAlloc,

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
  // Use a multiprocess-capable driver (Linux only for now), with each test node
  // running in its own isolated child process.
  kMultiprocess,
#endif
};

namespace internal {

using TestNodeFactory = std::unique_ptr<TestNode> (*)();

template <typename TestNodeType>
std::unique_ptr<TestNode> MakeTestNode() {
  return std::make_unique<TestNodeType>();
}

// Type used to package metadata about a MULTINODE_TEST_NODE() invocation.
struct TestNodeDetails {
  const std::string_view name;
  const TestNodeFactory factory;
};

template <typename T>
static constexpr bool IsValidTestNodeType = std::is_base_of_v<TestNode, T>;

}  // namespace internal

// Base class to support tests which exercise behavior across multiple ipcz
// nodes. These may be single-process on a synchronous driver, single-process on
// an asynchronous (e.g. multiprocess) driver, or fully multiprocess.
//
// This class provides convenience methods for creating and connecting nodes
// in various useful configurations. Note that it does NOT inherit from GTest's
// Test class, as multiple instances may run in parallel for a single test, and
// GTest's Test class is not compatible with that behavior.
//
// Instead, while MULTINODE_TEST_NODE() invocations should be based directly on
// TestNode or a derivative thereof. TEST_P() invocations for multinode tests
// should be based on derivatives of MultinodeTest<T> (see below this class),
// where T itself is a TestNode or some derivative thereof.
//
// This arrangement allows the main test body and its related
// MULTINODE_TEST_NODE() invocations to be based on the same essential type,
// making multinode tests easier to read and write.
class TestNode : public internal::TestBase {
 public:
  // Exposes interaction with one node spawned by another.
  class TestNodeController : public RefCounted {
   public:
    // Blocks until the spawned node has terminated. Returns true if the node
    // executed and terminated cleanly, or false if it encountered at least one
    // test expectation failure while running.
    virtual bool WaitForShutdown() = 0;
  };

  virtual ~TestNode();

  // Handle to this node.
  IpczHandle node() const { return node_; }

  // Handle to this node's broker-facing transport, if and only if
  // ConnectToBroker() hasn't been called yet.
  IpczDriverHandle transport() const { return transport_; }

  // Releases transport() to the caller. After calling this, it is no longer
  // valid to call either transport() or ConnectToBroker(), and this fixture
  // will not automatically close the transport on destruction.
  IpczDriverHandle ReleaseTransport() {
    return std::exchange(transport_, IPCZ_INVALID_DRIVER_HANDLE);
  }

  // The driver currently in use. Selected by test parameter.
  const IpczDriver& GetDriver() const;

  // One-time initialization. Called internally during test setup. Should never
  // be called by individual test code.
  void Initialize(DriverMode driver_mode,
                  IpczCreateNodeFlags create_node_flags);

  // May be called at most once by the TestNode body, to connect initial
  // `portals` to the broker.
  void ConnectToBroker(absl::Span<IpczHandle> portals);

  // Shorthand for the above, for the common case with only one initial portal.
  IpczHandle ConnectToBroker();

  // Opens a new portal pair on this node.
  std::pair<IpczHandle, IpczHandle> OpenPortals();

  // Spawns a new test node of TestNodeType and populates `portals` with a set
  // of initial portals connected to the node, via a new transport.
  template <typename TestNodeType>
  Ref<TestNodeController> SpawnTestNode(absl::Span<IpczHandle> portals) {
    return SpawnTestNodeImpl(node_, TestNodeType::kDetails, portals);
  }

  // Shorthand for the above, for the common case with only one initial portal
  // and no need for the test body to retain a controller for the node.
  template <typename TestNodeType>
  IpczHandle SpawnTestNode() {
    IpczHandle portal;
    SpawnTestNode<TestNodeType>({&portal, 1});
    return portal;
  }

  // Spawns a new test node of TestNodeType, giving it `transport` to use for
  // its broker connection. The caller is resposible for the other end of that
  // connection.
  template <typename TestNodeType>
  Ref<TestNodeController> SpawnTestNode(IpczDriverHandle transport) {
    return SpawnTestNodeImpl(node_, TestNodeType::kDetails, transport);
  }

  // Forcibly closes this Node, severing all links to other nodes and implicitly
  // disconnecting any portals which relied on those links.
  void CloseThisNode();

  // The TestNode body provided by a MULTINODE_TEST_NODE() invocation. For main
  // test definitions via TEST_P() with a MultinodeTest<T> fixture, this is
  // unused in favor of TestBody().
  virtual void NodeBody() {}

  // Creates a pair of transports appropriate for connecting this (broker or
  // non-broker) node to another non-broker node. Most tests should not use this
  // directly, but should instead connect to other nodes using the more
  // convenient helpers ConnectToBroker() or SpawnTestNode().
  struct TransportPair {
    IpczDriverHandle ours;
    IpczDriverHandle theirs;
  };
  TransportPair CreateTransports();

  // Helper used to support multiprocess TestNode invocation.
  int RunAsChild();

 private:
  // Sets the transport to use when connecting to a broker via ConnectBroker.
  // Must only be called once.
  void SetTransport(IpczDriverHandle transport);

  // Spawns a new node using an appropriate configuration for the current
  // driver. Returns a controller which can be used to interact with the node
  // outside of ipcz (e.g. to wait on its termination). `factory` is a function
  // which can produce an in-process instance of the TestNode; `test_node_name`
  // is a string which can be used to run the same TestNode subclass in a child
  // process.
  //
  // If `portals_or_transport` is a span of IpczHandles, this creates a new
  // pair of transports. One is given to the new node for connection back to us,
  // and the other is connected immediately by the broker, filling in the
  // handles with initial portals for the connection.
  //
  // Otherwise it's assumed to be a transport that will be given to the new
  // node for connecting back to us. In this case the caller is responsible for
  // the transport's peer.
  using PortalsOrTransport =
      absl::variant<absl::Span<IpczHandle>, IpczDriverHandle>;
  Ref<TestNodeController> SpawnTestNodeImpl(
      IpczHandle from_node,
      const internal::TestNodeDetails& details,
      PortalsOrTransport portals_or_transport);

  DriverMode driver_mode_ = DriverMode::kSync;
  IpczHandle node_ = IPCZ_INVALID_HANDLE;
  IpczDriverHandle transport_ = IPCZ_INVALID_DRIVER_HANDLE;
  std::vector<Ref<TestNodeController>> spawned_nodes_;

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
  TestChildLauncher child_launcher_;
#endif
};

// Actual parameterized GTest Test fixture for multinode tests. This or a
// subclass of it is required for TEST_P() invocations to function as proper
// multinode tests.
template <typename TestNodeType = TestNode>
class MultinodeTest : public TestNodeType,
                      public ::testing::Test,
                      public ::testing::WithParamInterface<DriverMode> {
 public:
  static_assert(internal::IsValidTestNodeType<TestNodeType>,
                "MultinodeTest<T> requires T to be a subclass of TestNode.");
  MultinodeTest() {
    TestNode::Initialize(GetParam(), IPCZ_CREATE_NODE_AS_BROKER);
  }
};

}  // namespace ipcz::test

#define MULTINODE_TEST_CHILD_MAIN_HELPER(func, node_name) \
  MULTIPROCESS_TEST_MAIN(func) {                          \
    node_name node;                                       \
    return node.RunAsChild();                             \
  }

#define MULTINODE_TEST_CHILD_MAIN(fixture, node_name) \
  MULTINODE_TEST_CHILD_MAIN_HELPER(fixture##_##node_name##_Node, node_name)

// Defines the main body of a non-broker test node for a multinode test. The
// named node can be spawned by another node using SpawnTestNode<T> where T is
// the unique name given by `node_name` here. `fixture` must be
/// ipcz::test::TestNode or a subclass thereof.
#define MULTINODE_TEST_NODE(fixture, node_name)                            \
  class node_name : public fixture {                                       \
    static_assert(::ipcz::test::internal::IsValidTestNodeType<fixture>,    \
                  "MULTINODE_TEST_NODE() requires a fixture derived from " \
                  "ipcz::test::TestNode.");                                \
                                                                           \
   public:                                                                 \
    static constexpr ::ipcz::test::internal::TestNodeDetails kDetails = {  \
        .name = #fixture "_" #node_name "_Node",                           \
        .factory = &::ipcz::test::internal::MakeTestNode<node_name>,       \
    };                                                                     \
    void NodeBody() override;                                              \
  };                                                                       \
  MULTINODE_TEST_CHILD_MAIN(fixture, node_name);                           \
  void node_name::NodeBody()

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
#define IPCZ_EXTRA_DRIVER_MODES , ipcz::test::DriverMode::kMultiprocess
#else
#define IPCZ_EXTRA_DRIVER_MODES
#endif

// TODO: Add other DriverMode enumerators here as support is landed.
#define INSTANTIATE_MULTINODE_TEST_SUITE_P(suite)                    \
  INSTANTIATE_TEST_SUITE_P(                                          \
      , suite,                                                       \
      ::testing::Values(ipcz::test::DriverMode::kSync,               \
                        ipcz::test::DriverMode::kAsync,              \
                        ipcz::test::DriverMode::kAsyncDelegatedAlloc \
                            IPCZ_EXTRA_DRIVER_MODES))

#endif  // IPCZ_SRC_TEST_MULTINODE_TEST_H_
