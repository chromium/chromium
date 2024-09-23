// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/multinode_test.h"

#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <thread>

#include "ipcz/ipcz.h"
#include "reference_drivers/async_reference_driver.h"
#include "reference_drivers/sync_reference_driver.h"
#include "test_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/str_split.h"
#include "third_party/abseil-cpp/absl/types/span.h"

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
#include "reference_drivers/file_descriptor.h"
#include "reference_drivers/multiprocess_reference_driver.h"
#include "reference_drivers/socket_transport.h"
#include "test/test_child_launcher.h"
#endif

namespace ipcz::test {

namespace {

using TestDriverMap = std::map<std::string, TestDriver*>;
TestDriverMap& GetTestDrivers() {
  static TestDriverMap* drivers = new TestDriverMap();
  return *drivers;
}

using TestNodeMap = std::map<std::string, TestNodeFactory>;
TestNodeMap& GetTestNodes() {
  static TestNodeMap* nodes = new TestNodeMap();
  return *nodes;
}

using TestFeatureSetMap = std::map<std::string, std::vector<IpczFeature>>;
const TestFeatureSetMap& GetTestFeatureSets() {
  using FeatureList = std::vector<IpczFeature>;
  static const std::pair<std::string_view, FeatureList> feature_lists[] = {
      {"Default", {}},
      {"MemV2", {IPCZ_FEATURE_MEM_V2}},
  };
  static const TestFeatureSetMap map{std::begin(feature_lists),
                                     std::end(feature_lists)};
  return map;
}

struct RegisteredMultinodeTest {
  RegisteredMultinodeTest() = default;
  ~RegisteredMultinodeTest() = default;

  const char* test_suite_name;
  const char* test_name;
  const char* filename;
  int line;
  MultinodeTestFactory factory;
};

std::vector<RegisteredMultinodeTest>& GetRegisteredMultinodeTests() {
  static auto* tests = new std::vector<RegisteredMultinodeTest>();
  return *tests;
}

// Launches a new node on a dedicated thread within the same process. All
// connections use the synchronous single-process driver.
class InProcessTestNodeController : public TestNode::TestNodeController {
 public:
  InProcessTestNodeController(TestNode& source,
                              TestDriver* test_driver,
                              const std::string& feature_set,
                              std::unique_ptr<TestNode> test_node)
      : source_(source),
        client_thread_(std::in_place,
                       &RunTestNode,
                       test_driver,
                       feature_set,
                       std::move(test_node)) {}

  ~InProcessTestNodeController() override { ABSL_ASSERT(!client_thread_); }

  // TestNode::TestNodeController:
  bool WaitForShutdown() override {
    if (client_thread_) {
      client_thread_->join();
      client_thread_.reset();
    }

    // In spirit, the point of WaitForShutdown()'s return value is to signal to
    // the running test whether something went wrong in a spawned node. This is
    // necessary to propagate test expectation failures from within child
    // processes when running in a multiprocess test mode.
    //
    // When spawned nodes are running in the main test process however, their
    // test expectation failures already affect the pass/fail state of the
    // running test. In this case there's no need to propagate a redundant
    // failure signal here, hence we always return true.
    return true;
  }

  TransportPair CreateNewTransports() override {
    return source_.CreateTransports();
  }

 private:
  static void RunTestNode(TestDriver* test_driver,
                          const std::string& feature_set,
                          std::unique_ptr<TestNode> test_node) {
    test_node->Initialize(test_driver, feature_set);
    test_node->NodeBody();
  }

  TestNode& source_;
  std::optional<std::thread> client_thread_;
};

class InProcessTestDriverBase : public TestDriver {
 public:
  Ref<TestNode::TestNodeController> SpawnTestNode(
      TestNode& source,
      const TestNodeDetails& details,
      const std::string& feature_set,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) override {
    std::unique_ptr<TestNode> test_node = details.factory();
    test_node->SetTransport(their_transport);
    return MakeRefCounted<InProcessTestNodeController>(
        source, this, feature_set, std::move(test_node));
  }

  IpczDriverHandle GetClientTestNodeTransport() override {
    ABSL_HARDENING_ASSERT(false);
    return IPCZ_INVALID_DRIVER_HANDLE;
  }
};

class SyncTestDriver : public InProcessTestDriverBase {
 public:
  const IpczDriver& GetIpczDriver() const override {
    return reference_drivers::kSyncReferenceDriver;
  }

  const char* GetName() const override { return internal::kSyncTestDriverName; }

  TransportPair CreateTransports(TestNode& source,
                                 bool for_broker_target) const override {
    TransportPair transports;
    const IpczResult result = GetIpczDriver().CreateTransports(
        IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
        nullptr, &transports.ours, &transports.theirs);
    ABSL_ASSERT(result == IPCZ_RESULT_OK);
    return transports;
  }

  IpczConnectNodeFlags GetExtraClientConnectNodeFlags() const override {
    return IPCZ_NO_FLAGS;
  }
};

class AsyncTestDriver : public InProcessTestDriverBase {
 public:
  enum Mode {
    kDefault,
    kForceBrokering,
    kDelegateAllocation,
    kForceBrokeringAndDelegateAllocation,
  };
  AsyncTestDriver(const char* name, Mode mode) : name_(name), mode_(mode) {}

  const IpczDriver& GetIpczDriver() const override {
    if (mode_ == kForceBrokering ||
        mode_ == kForceBrokeringAndDelegateAllocation) {
      return reference_drivers::kAsyncReferenceDriverWithForcedBrokering;
    }
    return reference_drivers::kAsyncReferenceDriver;
  }

  const char* GetName() const override { return name_; }

  TransportPair CreateTransports(TestNode& source,
                                 bool for_broker_target) const override {
    if (for_broker_target) {
      auto [ours, theirs] =
          reference_drivers::CreateAsyncTransportPairForBrokers();
      return {.ours = ours, .theirs = theirs};
    }

    reference_drivers::AsyncTransportPair transports =
        reference_drivers::CreateAsyncTransportPair();
    return {
        .ours = transports.broker,
        .theirs = transports.non_broker,
    };
  }

  IpczConnectNodeFlags GetExtraClientConnectNodeFlags() const override {
    if (mode_ == kDelegateAllocation ||
        mode_ == kForceBrokeringAndDelegateAllocation) {
      return IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
    }
    return IPCZ_NO_FLAGS;
  }

 private:
  const char* const name_;
  const Mode mode_;
};

TestDriverRegistration<SyncTestDriver> kRegisterSyncDriver;
TestDriverRegistration<AsyncTestDriver> kRegisterAsyncDriver{
    internal::kAsyncTestDriverName, AsyncTestDriver::kDefault};
TestDriverRegistration<AsyncTestDriver> kRegisterAsyncDriverWithDelegatedAlloc{
    internal::kAsyncDelegatedAllocTestDriverName,
    AsyncTestDriver::kDelegateAllocation};
TestDriverRegistration<AsyncTestDriver> kRegisterAsyncDriverWithForcedBrokering{
    internal::kAsyncForcedBrokeringTestDriverName,
    AsyncTestDriver::kForceBrokering};
TestDriverRegistration<AsyncTestDriver>
    kRegisterAsyncDriverWithDelegatedAllocAndForcedBrokering{
        internal::kAsyncDelegatedAllocAndForcedBrokeringTestDriverName,
        AsyncTestDriver::kForceBrokeringAndDelegateAllocation};

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
// Controls a node running within an isolated child process.
class ChildProcessTestNodeController : public TestNode::TestNodeController {
 public:
  ChildProcessTestNodeController(TestNode& source, pid_t pid)
      : source_(source), pid_(pid) {}
  ~ChildProcessTestNodeController() override {
    ABSL_ASSERT(result_.has_value());
  }

  // TestNode::TestNodeController:
  bool WaitForShutdown() override {
    if (result_.has_value()) {
      return *result_;
    }

    result_ = TestChildLauncher::WaitForSuccessfulProcessTermination(pid_);
    return *result_;
  }

  TransportPair CreateNewTransports() override {
    return source_.CreateTransports();
  }

  TestNode& source_;
  const pid_t pid_;
  std::optional<bool> result_;
};

class MultiprocessTestDriver : public TestDriver {
 public:
  const IpczDriver& GetIpczDriver() const override {
    return reference_drivers::kMultiprocessReferenceDriver;
  }

  const char* GetName() const override {
    return internal::kMultiprocessTestDriverName;
  }

  TransportPair CreateTransports(TestNode& source,
                                 bool for_broker_target) const override {
    TransportPair transports;
    const IpczResult result = GetIpczDriver().CreateTransports(
        IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
        nullptr, &transports.ours, &transports.theirs);
    ABSL_ASSERT(result == IPCZ_RESULT_OK);
    return transports;
  }

  Ref<TestNode::TestNodeController> SpawnTestNode(
      TestNode& source,
      const TestNodeDetails& details,
      const std::string& feature_set,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) override {
    reference_drivers::FileDescriptor socket =
        reference_drivers::TakeMultiprocessTransportDescriptor(their_transport);
    return MakeRefCounted<ChildProcessTestNodeController>(
        source,
        child_launcher_.Launch(details.name, feature_set, std::move(socket)));
  }

  IpczConnectNodeFlags GetExtraClientConnectNodeFlags() const override {
    return IPCZ_NO_FLAGS;
  }

  IpczDriverHandle GetClientTestNodeTransport() override {
    auto transport = MakeRefCounted<reference_drivers::SocketTransport>(
        TestChildLauncher::TakeChildSocketDescriptor());
    return reference_drivers::CreateMultiprocessTransport(std::move(transport));
  }

 private:
  TestChildLauncher child_launcher_;
};

TestDriverRegistration<MultiprocessTestDriver> kRegisterMultiprocessDriver;

#endif

}  // namespace

namespace internal {

const char kSyncTestDriverName[] = "Sync";
const char kAsyncTestDriverName[] = "Async";
const char kAsyncDelegatedAllocTestDriverName[] = "AsyncDelegatedAlloc";
const char kAsyncForcedBrokeringTestDriverName[] = "AsyncForcedBrokering";
const char kAsyncDelegatedAllocAndForcedBrokeringTestDriverName[] =
    "AsyncDelegatedAllocAndForcedBrokering";
const char kMultiprocessTestDriverName[] = "Multiprocess";

}  // namespace internal

TestDriverRegistrationImpl::TestDriverRegistrationImpl(TestDriver& driver) {
  GetTestDrivers()[driver.GetName()] = &driver;
}

TestNode::~TestNode() {
  for (auto& spawned_node : spawned_nodes_) {
    EXPECT_TRUE(spawned_node->WaitForShutdown());
  }

  // If we never connected to the broker, make sure we don't leak our transport.
  if (transport_ != IPCZ_INVALID_DRIVER_HANDLE) {
    test_driver_->GetIpczDriver().Close(transport_, IPCZ_NO_FLAGS, nullptr);
  }

  CloseThisNode();
}

const IpczDriver& TestNode::GetDriver() const {
  return test_driver_->GetIpczDriver();
}

void TestNode::Initialize(TestDriver* test_driver,
                          const std::string& feature_set) {
  test_driver_ = test_driver;
  feature_set_ = feature_set;

  absl::Span<const IpczFeature> enabled_features;
  const auto feature_sets = GetTestFeatureSets();
  auto feature_set_it = feature_sets.find(feature_set);
  if (feature_set_it != feature_sets.end()) {
    enabled_features = absl::MakeSpan(feature_set_it->second);
  }
  const IpczCreateNodeFlags flags =
      GetDetails().is_broker ? IPCZ_CREATE_NODE_AS_BROKER : IPCZ_NO_FLAGS;
  const IpczCreateNodeOptions options{
      .size = sizeof(options),
      .enabled_features = enabled_features.data(),
      .num_enabled_features = enabled_features.size(),
  };
  ABSL_ASSERT(node_ == IPCZ_INVALID_HANDLE);
  const IpczResult result = ipcz().CreateNode(&test_driver_->GetIpczDriver(),
                                              flags, &options, &node_);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
}

void TestNode::ConnectToParent(absl::Span<IpczHandle> portals,
                               IpczConnectNodeFlags flags) {
  flags |= test_driver_->GetExtraClientConnectNodeFlags();
  IpczDriverHandle transport =
      std::exchange(transport_, IPCZ_INVALID_DRIVER_HANDLE);
  ABSL_ASSERT(transport != IPCZ_INVALID_DRIVER_HANDLE);
  const IpczResult result = ipcz().ConnectNode(
      node(), transport, portals.size(), flags, nullptr, portals.data());
  ASSERT_EQ(IPCZ_RESULT_OK, result);
}

void TestNode::ConnectToBroker(absl::Span<IpczHandle> portals) {
  ConnectToParent(portals, IPCZ_CONNECT_NODE_TO_BROKER);
}

IpczHandle TestNode::ConnectToParent(IpczConnectNodeFlags flags) {
  IpczHandle portal;
  ConnectToParent({&portal, 1}, flags);
  return portal;
}

IpczHandle TestNode::ConnectToBroker() {
  return ConnectToParent(IPCZ_CONNECT_NODE_TO_BROKER);
}

std::pair<IpczHandle, IpczHandle> TestNode::OpenPortals() {
  return TestBase::OpenPortals(node_);
}

IpczHandle TestNode::BoxBlob(std::string_view contents) {
  IpczDriverHandle memory;
  IpczResult result = GetDriver().AllocateSharedMemory(
      contents.size(), IPCZ_NO_FLAGS, nullptr, &memory);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);

  IpczDriverHandle mapping;
  volatile void* base;
  result = GetDriver().MapSharedMemory(memory, IPCZ_NO_FLAGS, nullptr, &base,
                                       &mapping);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  memcpy(const_cast<void*>(base), contents.data(), contents.size());
  GetDriver().Close(mapping, IPCZ_NO_FLAGS, nullptr);

  IpczHandle box;
  const IpczBoxContents box_contents = {
      .size = sizeof(box_contents),
      .type = IPCZ_BOX_TYPE_DRIVER_OBJECT,
      .object = {.driver_object = memory},
  };
  result = ipcz().Box(node_, &box_contents, IPCZ_NO_FLAGS, nullptr, &box);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  return box;
}

std::string TestNode::UnboxBlob(IpczHandle box) {
  IpczBoxContents box_contents = {.size = sizeof(box_contents)};
  IpczResult result = ipcz().Unbox(box, IPCZ_NO_FLAGS, nullptr, &box_contents);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);
  ABSL_ASSERT(box_contents.type == IPCZ_BOX_TYPE_DRIVER_OBJECT);

  const IpczDriverHandle memory = box_contents.object.driver_object;
  IpczSharedMemoryInfo info = {.size = sizeof(info)};
  result =
      GetDriver().GetSharedMemoryInfo(memory, IPCZ_NO_FLAGS, nullptr, &info);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);

  IpczDriverHandle mapping;
  volatile void* base;
  result = GetDriver().MapSharedMemory(memory, IPCZ_NO_FLAGS, nullptr, &base,
                                       &mapping);
  ABSL_ASSERT(result == IPCZ_RESULT_OK);

  std::string contents(static_cast<const char*>(const_cast<const void*>(base)),
                       info.region_num_bytes);
  GetDriver().Close(mapping, IPCZ_NO_FLAGS, nullptr);
  GetDriver().Close(memory, IPCZ_NO_FLAGS, nullptr);
  return contents;
}

void TestNode::CloseThisNode() {
  if (node_ != IPCZ_INVALID_HANDLE) {
    IpczHandle node = std::exchange(node_, IPCZ_INVALID_HANDLE);
    ipcz().Close(node, IPCZ_NO_FLAGS, nullptr);
  }
}

Ref<TestNode::TestNodeController> TestNode::SpawnTestNodeImpl(
    const TestNodeDetails& details,
    IpczDriverHandle& our_transport) {
  TransportPair transports;
  if (details.is_broker) {
    transports = CreateBrokerToBrokerTransports();
  } else {
    transports = CreateTransports();
  }
  Ref<TestNodeController> controller = test_driver_->SpawnTestNode(
      *this, details, feature_set_, transports.ours, transports.theirs);
  spawned_nodes_.push_back(controller);
  our_transport = transports.ours;
  return controller;
}

TransportPair TestNode::CreateTransports() {
  return test_driver_->CreateTransports(*this, /*for_broker_target=*/false);
}

TransportPair TestNode::CreateBrokerToBrokerTransports() {
  return test_driver_->CreateTransports(*this, /*for_broker_target=*/true);
}

void TestNode::SetTransport(IpczDriverHandle transport) {
  ABSL_ASSERT(transport_ == IPCZ_INVALID_DRIVER_HANDLE);
  transport_ = transport;
}

int TestNode::RunAsChild(TestDriver* test_driver,
                         const std::string& feature_set) {
  SetTransport(test_driver->GetClientTestNodeTransport());
  Initialize(test_driver, feature_set);
  NodeBody();

  const int exit_code = ::testing::Test::HasFailure() ? 1 : 0;
  return exit_code;
}

void RegisterMultinodeTestNode(std::string_view node_name,
                               TestNodeFactory factory) {
  GetTestNodes()[std::string(node_name)] = factory;
}

void RegisterMultinodeTest(const char* test_suite_name,
                           const char* test_name,
                           const char* filename,
                           int line,
                           MultinodeTestFactory factory) {
  RegisteredMultinodeTest test;
  test.test_suite_name = test_suite_name;
  test.test_name = test_name;
  test.filename = filename;
  test.line = line;
  test.factory = factory;
  GetRegisteredMultinodeTests().push_back(std::move(test));
}

int RunChildProcessTest(const std::string& test_name) {
  std::pair<std::string, std::string> split = absl::StrSplit(test_name, '/');
  const auto [node_name, params] = split;
  if (params.empty()) {
    return multi_process_function_list::InvokeChildProcessTestMain(test_name);
  }
  split = absl::StrSplit(params, '_');
  const auto [driver_name, feature_set] = split;

  auto& drivers = GetTestDrivers();
  auto driver_it = drivers.find(driver_name);
  if (driver_it == drivers.end()) {
    return -1;
  }

  auto& nodes = GetTestNodes();
  auto node_it = nodes.find(node_name);
  if (node_it == nodes.end()) {
    return -1;
  }

  std::unique_ptr<TestNode> node = node_it->second();
  return node->RunAsChild(driver_it->second, feature_set);
}

void RegisterMultinodeTests() {
  multi_process_function_list::SetChildProcessTestRunner(&RunChildProcessTest);
  for (auto& test : GetRegisteredMultinodeTests()) {
    for (const auto& [feature_set_name, features] : GetTestFeatureSets()) {
      for (const auto& [test_driver_name, test_driver] : GetTestDrivers()) {
        const std::string test_name = absl::StrCat(
            test.test_name, "/", test_driver_name, "_", feature_set_name);
        ::testing::RegisterTest(test.test_suite_name, test_name.c_str(),
                                nullptr, nullptr, test.filename, test.line,
                                [factory = test.factory, driver = test_driver,
                                 feature_set = feature_set_name] {
                                  return factory(driver, feature_set);
                                });
      }
    }
  }
}

}  // namespace ipcz::test
