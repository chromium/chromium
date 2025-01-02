// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef SHARED_PROVIDER
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "core/common/logging/logging.h"
#include "core/common/status.h"
#include "core/framework/data_transfer.h"
#include "core/framework/external_data_loader.h"
#include "core/framework/tensor.h"

namespace onnxruntime {
class GraphViewer;
struct ComputeCapability;
class KernelRegistry;
struct KernelCreateInfo;
class Node;
}  // namespace onnxruntime
#else
#include <memory>
#endif

#include "core/common/basic_types.h"
#include "core/common/profiler_common.h"
#include "core/framework/allocator_utils.h"
#include "core/framework/func_api.h"
#include "core/framework/provider_options.h"
#include "core/framework/framework_provider_common.h"
#include "core/framework/stream_handles.h"
#include "core/framework/tuning_context.h"

struct OrtRunOptions;

namespace onnxruntime {

/**
   Logical device representation.
*/

// if we are export the fused function to dll, the function will still in the same binary as onnxruntime
// use std function to give execution provider some chance to capture some state.
using CreateFunctionStateFunc = std::function<int(ComputeContext*, FunctionState*)>;
using ComputeFunc = std::function<Status(FunctionState, const OrtApi*, OrtKernelContext*)>;
using DestroyFunctionStateFunc = std::function<void(FunctionState)>;

struct NodeComputeInfo {
  CreateFunctionStateFunc create_state_func;
  ComputeFunc compute_func;
  DestroyFunctionStateFunc release_state_func;
};

using RunOptions = ::OrtRunOptions;

enum class DataLayout {
  NCHW,
  NHWC,
  NCHWC,
};

class IExecutionProvider {
 protected:
  IExecutionProvider(const std::string& type)
      : IExecutionProvider(type, OrtDevice()) {}

  IExecutionProvider(const std::string& type, OrtDevice device)
      : default_device_(device), type_{type} {
  }

  /*
     default device for this ExecutionProvider
  */
  const OrtDevice default_device_;

 public:
  virtual ~IExecutionProvider() = default;

  /**
   * Returns a data transfer object that implements methods to copy to and
   * from this device.
   * If no copy is required for the successful operation of this provider,
   * return a nullptr.
   */
  virtual std::unique_ptr<onnxruntime::IDataTransfer> GetDataTransfer() const {
    return nullptr;
  }

  /**
   * Returns an external data loader object that implements methods to load data from external sources.
   *
   * By default, framework will handle external data loading by loading the data into CPU memory and then copying
   * it to the target device if required. So in most cases, it's not necessary to override this method. Specifically,
   * in WebAssembly build, because the memory is limited and Web platform supports loading data from external sources
   * directly into GPU memory, this method is overridden to provide a custom external data loader to avoid the extra
   * CPU memory usage.
   */
  virtual std::unique_ptr<onnxruntime::IExternalDataLoader> GetExternalDataLoader() const {
    return nullptr;
  }

  /**
   * Interface for performing kernel lookup within kernel registries.
   * Abstracts away lower-level details about kernel registries and kernel matching.
   */
  class IKernelLookup {
   public:
    /**
     * Given `node`, try to find a matching kernel for this EP.
     * The return value is non-null if and only if a matching kernel was found.
     */
    virtual const KernelCreateInfo* LookUpKernel(const Node& node) const = 0;

   protected:
    ~IKernelLookup() = default;
  };

  /**
     Get execution provider's capability for the specified <graph>.
     Return a bunch of IndexedSubGraphs <*this> execution provider can run if
     the sub-graph contains only one node or can fuse to run if the sub-graph
     contains more than one node. The node indexes contained in sub-graphs may
     have overlap, and it's ONNXRuntime's responsibility to do the partition
     and decide whether a node will be assigned to <*this> execution provider.
     For kernels registered in a kernel registry, `kernel_lookup` must be used
     to find a matching kernel for this EP.
  */
  virtual std::vector<std::unique_ptr<ComputeCapability>>
  GetCapability(const onnxruntime::GraphViewer& graph_viewer,
                const IKernelLookup& kernel_lookup) const;

  /**
     Get kernel registry per execution provider type.
     The KernelRegistry share pointer returned is shared across sessions.

     NOTE: this approach was taken to achieve the following goals,
     1. The execution provider type based kernel registry should be shared
     across sessions.
     Only one copy of this kind of kernel registry exists in ONNXRuntime
     with multiple sessions/models.
     2. Adding an execution provider into ONNXRuntime does not need to touch ONNXRuntime
     framework/session code.
     3. onnxruntime (framework/session) does not depend on any specific
     execution provider lib.
  */
  virtual std::shared_ptr<KernelRegistry> GetKernelRegistry() const { return nullptr; }

  /**
     Get the device id of current execution provider
  */
  virtual int GetDeviceId() const { return 0; };

  /**
     Get execution provider's configuration options.
   */
  virtual ProviderOptions GetProviderOptions() const { return {}; }

  /**
     Get provider specific custom op domain list.
     Provider has the responsibility to release OrtCustomOpDomain instances it creates.

     NOTE: In the case of ONNX model having EP specific custom nodes and don't want to ask user to register those nodes,
     EP might need to a way to register those custom nodes. This API is added for the purpose where EP can use it to
     leverage ORT custom op to register those custom nodes with one or more custom op domains.

     For example, TensorRT EP uses this API to support TRT plugins where each custom op is mapped to TRT plugin and no
     kernel implementation is needed for custom op since the real implementation is inside TRT. This custom op acts as
     a role to help pass ONNX model validation.
   */
  virtual void GetCustomOpDomainList(std::vector<OrtCustomOpDomain*>& /*provider custom op domain list*/) const {};

  /**
     Returns an opaque handle whose exact type varies based on the provider
     and is interpreted accordingly by the corresponding kernel implementation.
     For Direct3D operator kernels, this may return an IUnknown supporting
     QueryInterface to ID3D12GraphicsCommandList1.
  */
  virtual const void* GetExecutionHandle() const noexcept {
    return nullptr;
  }

  /**
     @return type of the execution provider; should match that set in the node
     through the SetExecutionProvider API. Example valid return values are:
     kCpuExecutionProvider, kCudaExecutionProvider
  */
  const std::string& Type() const { return type_; }

  /**
     Blocks until the device has completed all preceding requested tasks.
     Currently this is primarily used by the IOBinding object to ensure that all
     inputs have been copied to the device before execution begins.
  */
  virtual common::Status Sync() const { return Status::OK(); }

  /**
     Called when InferenceSession::Run started
     NOTE that due to async execution in provider, the actual work of previous
     Run may not be finished on device This function should be regarded as the
     point after which a new Run would start to submit commands from CPU
  */
  virtual common::Status OnRunStart(const onnxruntime::RunOptions& /*run_options*/) { return Status::OK(); }

  /**
     Called when InferenceSession::Run ended
     NOTE that due to async execution in provider, the actual work of this Run
     may not be finished on device This function should be regarded as the point
     that all commands of current Run has been submmited by CPU
  */
  virtual common::Status OnRunEnd(bool /*sync_stream*/, const onnxruntime::RunOptions& /*run_options*/) {
    return Status::OK();
  }

  /**
     Called when InferenceSession::SetEpDynamicOptions is called
  */
  virtual common::Status SetEpDynamicOptions(gsl::span<const char* const> /*keys*/,
                                             gsl::span<const char* const> /*values*/) {
    return Status::OK();
  }

  /**
     Indicate whether the graph capturing mode (e.g., cuda graph) is enabled for
     the provider.
   */
  virtual bool IsGraphCaptureEnabled() const { return false; }

  /**
     Indicate whether the graph has been captured and instantiated.
   */
  virtual bool IsGraphCaptured(int /*graph_annotation_id*/) const { return false; }

  /**
     Run the instantiated graph.
   */
  virtual common::Status ReplayGraph(int /*graph_annotation_id*/) {
    return Status::OK();
  }

  /**
     Called when session creation is complete
     This provides an opportunity for execution providers to optionally synchronize and
     clean up its temporary resources to reduce memory and ensure the first run is fast.
  */
  virtual common::Status OnSessionInitializationEnd() { return Status::OK(); }

  struct FusedNodeAndGraph {
    const std::reference_wrapper<onnxruntime::Node> fused_node;
    // GraphViewer that filters the full graph to the nodes that are covered by 'node'
    const std::reference_wrapper<GraphViewer> filtered_graph;
  };

  // Fusion approach that is supported
  // !!! The "Function" FusionStyle is deprecated.
  // !!! If your EP is using this fusion style, please migrate it to "FilteredGraphViewer" style.
  enum class FusionStyle {
    // The node fusion will create an onnxruntime::Function based Node that contains a completely new Graph instance
    // in the Node body. The original nodes and initializers are copied to the new Graph instance in Function::Body().
    // A GraphProto can be produced from the Node body.
    Function,

    // The node fusion will create a new Node that defines the inputs and outputs using the IndexedSubGraph
    // that GetCapability returned. The Node will not be onnxruntime::Function based so will have no Body().
    // Instead a GraphViewer that filters the full Graph to the fused Nodes will be created.
    // This is significantly cheaper as it doesn't incur the cost of creating a new Graph instance,
    // and can be supported in a minimal build.
    FilteredGraphViewer
  };

  virtual FusionStyle GetFusionStyle() const {
    // All the ORT build in EP has migrate to FilteredGraphViewer style.
    // For newer EPs, please avoid use Function style as it is deprecated.
    return FusionStyle::FilteredGraphViewer;
  }

#if !defined(ORT_MINIMAL_BUILD) || defined(ORT_EXTENDED_MINIMAL_BUILD)
  /**
  Given a collection of fused Nodes and the respective GraphViewer instance for the nodes that were fused,
  return create_state/compute/release_state func for each node.
  @remarks This is now the default interface when execution provider wants to compile nodes
           for both minimal build and complete ort build.

           Do NOT cache the GraphViewer in FusedNodeAndGraph.filtered_graph in any of the NodeComputeInfo functions
           as it is only valid for the duration of the call to Compile.
  */
  virtual common::Status Compile(const std::vector<FusedNodeAndGraph>& fused_nodes_and_graphs,
                                 std::vector<NodeComputeInfo>& node_compute_funcs);

#endif

  void SetLogger(const logging::Logger* logger) {
    logger_ = logger;
  }

  const logging::Logger* GetLogger() const {
    return logger_;
  }

  virtual std::unique_ptr<profiling::EpProfiler> GetProfiler() {
    return {};
  }

  virtual DataLayout GetPreferredLayout() const {
    // NCHW is the default ONNX standard data layout. So default to it.
    // EPs which prefer a different layout should override to return their preferred layout.
    return DataLayout::NCHW;
  }

  virtual void RegisterStreamHandlers(IStreamCommandHandleRegistry& /*stream_handle_registry*/, AllocatorMap&) const {}

  /** Does the EP support concurrent calls to InferenceSession::Run to execute the model.
   */
  virtual bool ConcurrentRunSupported() const { return true; }

  /**
   * Return the tuning context which holds all TunableOp state.
   */
  virtual ITuningContext* GetTuningContext() const {
    return nullptr;
  }

  /**
   * Return the appropriate OrtDevice object given OrtMemType.
   */
  virtual OrtDevice GetOrtDeviceByMemType(OrtMemType mem_type) const {
    if (mem_type == OrtMemTypeCPUInput || mem_type == OrtMemTypeCPUOutput) {
      return OrtDevice();  // default return CPU device.
    }
    return default_device_;
  };

  /**
   * Create Preferred allocators for the current Execution Provider
   * This function is a stateless function which creates new instances of Allocator, without storing them in EP.
   */
  virtual std::vector<AllocatorPtr> CreatePreferredAllocators() { return std::vector<AllocatorPtr>(); };

  /**
   * Get the array of pointers for EPContext nodes
   * EP needs to implement this if has the requirement to generate the context cache model. Otherwise leave it.
   * Default return an empty vector if not provided by the Execution Provider
   */
  virtual const InlinedVector<const Node*> GetEpContextNodes() const {
    return InlinedVector<const Node*>();
  }

 private:
  const std::string type_;

  // It will be set when this object is registered to a session
  const logging::Logger* logger_ = nullptr;
};
}  // namespace onnxruntime
