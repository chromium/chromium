// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Do not include this file directly. Please include "onnxruntime_c_api.h" instead.

#ifdef __cplusplus
extern "C" {
#endif

ORT_RUNTIME_CLASS(Ep);
ORT_RUNTIME_CLASS(EpFactory);
ORT_RUNTIME_CLASS(EpGraphSupportInfo);
ORT_RUNTIME_CLASS(MemoryDevice);  // opaque class to wrap onnxruntime::OrtDevice
ORT_RUNTIME_CLASS(NodeComputeContext);

ORT_RUNTIME_CLASS(DataTransferImpl);
ORT_RUNTIME_CLASS(SyncNotificationImpl);
ORT_RUNTIME_CLASS(SyncStreamImpl);

// struct that an EP implements for IDataTransfer to copy between devices it uses and CPU
struct OrtDataTransferImpl {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION

  /** \brief Release the OrtDataTransferImpl instance.
   *
   * This is called by ORT when the OrtDataTransferImpl instance is no longer needed.
   * The implementation should release any resources held by the instance.
   *
   * \param[in] this_ptr Pointer to the OrtDataTransferImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API_T(void, Release, _In_ OrtDataTransferImpl* this_ptr);

  /** \brief Check if the implementation can copy between the source and destination memory devices.
   *
   * \param[in] this_ptr Pointer to the OrtDataTransferImpl instance.
   * \param[in] src_memory_device Source OrtMemoryDevice to copy from.
   * \param[in] dst_memory_device Destination OrtMemoryDevice to copy to.
   * \return True if the implementation can copy between the devices.
   *
   * \since Version 1.23.
   */
  ORT_API_T(bool, CanCopy, _In_ const OrtDataTransferImpl* this_ptr,
            _In_ const OrtMemoryDevice* src_memory_device, _In_ const OrtMemoryDevice* dst_memory_device);

  /** \brief Copy tensors from src_tensors to dst_tensors using the provided streams.
   *
   * The implementation can use the provided streams to perform asynchronous copies if supported.
   * If a stream is not available, the copy is performed synchronously.
   *
   * \param[in] this_ptr Pointer to the OrtDataTransferImpl instance.
   * \param[in] src_tensors Array of source OrtValue pointers to copy from.
   * \param[in] dst_tensors Array of destination OrtValue pointers to copy to.
   * \param[in] streams Array of OrtSyncStream pointers for the copy operations, if the execution provider is stream
   *                    aware. nullptr if it is not.
   * \param[in] num_tensors Number of tensors to copy.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CopyTensors, _In_ OrtDataTransferImpl* this_ptr,
                  _In_reads_(num_tensors) const OrtValue** src_tensors,
                  _In_reads_(num_tensors) OrtValue** dst_tensors,
                  _In_reads_(num_tensors) OrtSyncStream** streams,
                  _In_ size_t num_tensors);
};

/** \brief Struct that an EP implements for Stream Notifications.
 *
 * \since Version 1.23.
 */
struct OrtSyncNotificationImpl {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION

  /** \brief Release the OrtSyncNotificationImpl instance.
   *
   * This is called by ORT when the OrtSyncNotificationImpl instance is no longer needed.
   * The implementation should release any resources held by the instance.
   *
   * \param[in] this_ptr Pointer to the OrtSyncNotificationImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API_T(void, Release, _In_ OrtSyncNotificationImpl* this_ptr);

  /** \brief Called by ORT to activate the notification.
   *
   * \param[in] this_ptr Pointer to the OrtSyncNotificationImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(Activate, _In_ OrtSyncNotificationImpl* this_ptr);

  /** \brief Wait for a device to device operation to complete.
   *
   * \param[in] this_ptr Pointer to the OrtSyncNotificationImpl instance.
   * \param[in] stream The OrtSyncStream instance that will wait on this notification to be activated.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(WaitOnDevice, _In_ OrtSyncNotificationImpl* this_ptr, _In_ OrtSyncStream* consumer_stream);

  /** \brief Wait for a device to host operation to complete.
   *
   * \param[in] this_ptr Pointer to the OrtSyncNotificationImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(WaitOnHost, _In_ OrtSyncNotificationImpl* this_ptr);
};

/** \brief Struct that an EP implements if it wishes to implement Stream support.
 *
 * This struct provides the overrides for onnxruntime::Stream's virtual methods.
 *
 * \since Version 1.23.
 */
struct OrtSyncStreamImpl {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION

  /** \brief Release the OrtSyncStreamImpl instance.
   *
   * This is called by ORT when the OrtSyncStreamImpl instance is no longer needed.
   * The implementation should release any resources held by the instance.
   *
   * \param[in] this_ptr Pointer to the OrtSyncStreamImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API_T(void, Release, _In_ OrtSyncStreamImpl* this_ptr);

  /** \brief Get the handle of the stream.
   *
   * This returns the native handle for the stream. e.g. cudaStream_t for CUDA streams.
   *
   * \param[in] this_ptr Pointer to the OrtSyncStreamImpl instance.
   * \return The handle of the stream.
   *
   * \since Version 1.23.
   */
  ORT_API_T(void*, GetHandle, _In_ OrtSyncStreamImpl* this_ptr);

  /** \brief Create an OrtSyncNotificationImpl for the OrtSyncStreamImpl instance.
   *
   * \param[in] this_ptr Pointer to the OrtSyncStreamImpl instance
   * \param[out] notification The new OrtSyncNotificationImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CreateNotification, _In_ OrtSyncStreamImpl* this_ptr,
                  _Outptr_ OrtSyncNotificationImpl** notification);

  /** \brief Flush the stream.
   *
   * This is called by ORT to flush the stream, ensuring that all operations submitted to the stream are completed.
   *
   * \param[in] this_ptr Pointer to the OrtSyncStreamImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(Flush, _In_ OrtSyncStreamImpl* this_ptr);

  /** \brief Notify the stream that a session run has ended.
   *
   * This is called by ORT to notify the stream that a session run has ended, allowing the stream to perform any
   * necessary cleanup or finalization.
   *
   * \param[in] this_ptr Pointer to the OrtSyncStreamImpl instance.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(OnSessionRunEnd, _In_ OrtSyncStreamImpl* this_ptr);
};

struct OrtNodeFusionOptions;
typedef struct OrtNodeFusionOptions OrtNodeFusionOptions;

struct OrtNodeComputeInfo;
typedef struct OrtNodeComputeInfo OrtNodeComputeInfo;

/**
 * \brief The OrtNodeFusionOptions struct specifies options for fusing nodes supported by an execution provider.
 *
 * Refer to OrtEpApi::EpGraphSupportInfo_AddNodesToFuse.
 *
 * \since Version 1.23.
 */
struct OrtNodeFusionOptions {
  /** \brief The ONNX Runtime version the OrtNodeFusionOptions was compiled with.
   *
   * Implementation should set to ORT_API_VERSION.
   * ORT will use this to ensure it does not use members that were not available when the EP library was compiled.
   *
   * \since Version 1.23.
   */
  uint32_t ort_version_supported;

  /** \brief If set to true, specify that the execution provider does not require ONNX Runtime to provide constant
   * initializers as inputs to the fused node during model inference. This is used when the execution
   * provider saves a copy of constant initializers, and allows ONNX Runtime to release constant initializers that
   * are not used by any execution provider.
   *
   * If not specified, defaults to false. That is, ONNX Runtime provides constant initializers as inputs to
   * the fused node by default.
   *
   * \since Version 1.23.
   */
  bool drop_constant_initializers;

  // const OrtNode* fused_node_schema;
};

/**
 * \brief The OrtNodeComputeInfo struct provides functions that an OrtEp implements to specify the compute
 * function for a compiled OrtGraph instance.
 * \since Version 1.23.
 */
struct OrtNodeComputeInfo {
  /** \brief The ONNX Runtime version the OrtNodeComputeInfo was compiled with.
   *
   * Implementation should set to ORT_API_VERSION.
   * ORT will use this to ensure it does not call functions that were not available when the EP library was compiled.
   *
   * \since Version 1.23.
   */
  uint32_t ort_version_supported;

  /** \brief Creates an opaque compute state object that is then passed to the Compute() function during inference.
   * \param[in] this_ptr The OrtNodeComputeInfo instance.
   * \param[in] compute_context OrtNodeComputeContext instance that contains compiled/fused node's name and host
   *                            memory allocation functions. Can optionally be used to build the compute state.
   * \param[out] compute_state Output parameter that is assigned the opaque computation state. ONNX Runtime calls
   *                           ReleaseState() (after calling Compute()) to allow the implementer to release the
   *                           compute state.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  OrtStatus*(ORT_API_CALL* CreateState)(_In_ OrtNodeComputeInfo* this_ptr,
                                        _In_ OrtNodeComputeContext* compute_context,
                                        _Outptr_ void** compute_state);

  /** \brief Computation function called to execute the fused node compiled by an OrtEp instance.
   * \param[in] this_ptr The OrtNodeComputeInfo instance.
   * \param[in] compute_state The opaque computation state returned by CreateState().
   * \param[in] kernel_context The OrtKernelContext instance used to access inputs/outputs.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  OrtStatus*(ORT_API_CALL* Compute)(_In_ OrtNodeComputeInfo* this_ptr, _In_ void* compute_state,
                                    _In_ OrtKernelContext* kernel_context);

  /** \brief Releases the compute state returned by CreateState().
   * \param[in] this_ptr The OrtNodeComputeInfo instance.
   * \param[inout] compute_state The opaque compute state returned by CreateState().
   *
   * \since Version 1.23.
   */
  void(ORT_API_CALL* ReleaseState)(_In_ OrtNodeComputeInfo* this_ptr, _Frees_ptr_opt_ void* compute_state);
};

struct OrtEpApi {
  /** \brief Create an OrtEpDevice for the EP and an OrtHardwareDevice.
   * \param[in] ep_factory Execution provider factory that is creating the instance.
   * \param[in] hardware_device Hardware device that the EP can utilize.
   * \param[in] ep_metadata Optional OrtKeyValuePairs instance for execution provider metadata that may be used
   *                        during execution provider selection and passed to CreateEp.
   *                        ep_device will copy this instance and the user should call ReleaseKeyValuePairs.
   * \param[in] ep_options  Optional OrtKeyValuePairs instance for execution provider options that will be added
   *                        to the Session configuration options if the execution provider is selected.
   *                        ep_device will copy this instance and the user should call ReleaseKeyValuePairs.
   * \param ep_device OrtExecutionDevice that is created.
   *
   * \since Version 1.22.
   */
  ORT_API2_STATUS(CreateEpDevice, _In_ OrtEpFactory* ep_factory,
                  _In_ const OrtHardwareDevice* hardware_device,
                  _In_opt_ const OrtKeyValuePairs* ep_metadata,
                  _In_opt_ const OrtKeyValuePairs* ep_options,
                  _Out_ OrtEpDevice** ep_device);

  ORT_CLASS_RELEASE(EpDevice);

  /** \brief Specify nodes that are supported by an OrtEp and should be fused into one node.
   *
   * Because the nodes will be fused into one "fused node", there must not exist an unsupported node in
   * a path between two of the provided nodes. Otherwise, the graph will become invalid.
   *
   * This function can be called multiple times. A subsequent call to this function will force the next set of
   * nodes to be fused into a different node.
   *
   * \param[in] graph_support_info OrtEpGraphSupportInfo instance to which to add the supported nodes.
   * \param[in] nodes Array of nodes supported by the EP that should be fused/compiled.
   * \param[in] num_nodes The number of supported nodes.
   * \param[in] node_fusion_options Optional node fusion options. Ignored if set to NULL.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(EpGraphSupportInfo_AddNodesToFuse, _In_ OrtEpGraphSupportInfo* graph_support_info,
                  _In_reads_(num_nodes) const OrtNode* const* nodes, _In_ size_t num_nodes,
                  _In_opt_ const OrtNodeFusionOptions* node_fusion_options);

  /** \brief Specify a node that is supported by an OrtEp and should be run with a registered EP kernel.
   *
   * \param[in] graph_support_info OrtEpGraphSupportInfo instance to which to add the supported node.
   * \param[in] node The supported OrtNode instance.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(EpGraphSupportInfo_AddSingleNode, _In_ OrtEpGraphSupportInfo* graph_support_info,
                  _In_ const OrtNode* node);

  /** \brief Query a OrtNodeComputeContext for the name of the node that encapsulates the compiled/fused node.
   *
   * Used in OrtNodeComputeInfo::CreateComputeState().
   *
   * \param[in] context The OrtNodeComputeContext instance to query.
   * \return The node's name.
   *
   * \note Returned string is owned by ORT and valid only while OrtNodeComputeInfo::CreateComputeState() is called.
   *
   * \since Version 1.23.
   */
  ORT_API_T(const char*, NodeComputeContext_NodeName, _In_ const OrtNodeComputeContext* context);

  /** \brief Register an allocator with the OrtEpDevice.
   *
   * This allows an EP to provide OrtMemoryInfo for DEFAULT and HOST_ACCESSIBLE memory type as needed.
   * The registered values will be used in calls to OrtEpFactory::CreateAllocator to ensure the required allocator/s
   * are available for EP usage.
   *
   * Multiple calls for the same entry type will replace a previous entry.
   *
   * Available entries:
   *   - OrtDeviceAllocator with type of OrtDeviceMemoryType_DEFAULT
   *   - OrtDeviceAllocator with type of OrtDeviceMemoryType_HOST_ACCESSIBLE
   *   - OrtReadOnlyAllocator with type of OrtDeviceMemoryType_DEFAULT
   *     - if provided this allocator will only be used to copy initializers to the device the EP uses.
   *       ORT will use the OrtDeviceAllocator if not provided.
   *
   * \param[in] ep_device The OrtEpDevice instance to register the OrtMemoryInfo with.
   * \param[in] allocator_memory_info The OrtMemoryInfo information for the allocator.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(EpDevice_AddAllocatorInfo, _In_ OrtEpDevice* ep_device,
                  _In_ const OrtMemoryInfo* allocator_memory_info);

  /** \brief Get the OrtMemoryDevice from an OrtMemoryInfo instance.
   *
   * This is required for OrtDataTransferImpl (which implements onnxruntime::IDataTransfer) where the OrtMemoryDevice
   * is used in the CanCopy and CopyTensors functions.
   *
   * \param[in] memory_info The OrtMemoryInfo instance to get the memory device from.
   * \return The OrtMemoryDevice associated with the OrtMemoryInfo instance.
   *
   * \since Version 1.23.
   */
  ORT_API_T(const OrtMemoryDevice*, MemoryInfo_GetMemoryDevice, _In_ const OrtMemoryInfo* memory_info);

  /** \brief Get the OrtMemoryDevice from an OrtValue instance if it contains a Tensor.
   *
   * \param[in] value The OrtValue instance to get the memory device from.
   * \return Memory device if OrtValue contains a Tensor, nullptr otherwise.
   *
   * \since Version 1.23.
   */
  ORT_API_T(const OrtMemoryDevice*, Value_GetMemoryDevice, _In_ const OrtValue* value);

  /** \brief Compare two OrtMemoryDevice instances for equality.
   *
   * This is used to check if two memory devices are the same.
   * Used to implement DataTransferImpl::CanCopy.
   *
   * \param[in] a The first OrtMemoryDevice instance to compare.
   * \param[in] b The second OrtMemoryDevice instance to compare.
   * \return True if the two OrtMemoryDevice instances are equal, false otherwise.
   *
   * \since Version 1.23.
   */
  ORT_API_T(bool, MemoryDevice_AreEqual, _In_ const OrtMemoryDevice* a, _In_ const OrtMemoryDevice* b);

  /** \brief Get the OrtMemoryInfoDeviceType value from an OrtMemoryDevice instance.
   *
   * \param[in] memory_device OrtMemoryDevice instance.
   * \return The OrtMemoryInfoDeviceType value.
   *
   * \since Version 1.23.
   */
  ORT_API_T(OrtMemoryInfoDeviceType, MemoryDevice_GetDeviceType, _In_ const OrtMemoryDevice* memory_device);

  /** \brief Get the OrtDeviceMemoryType value from an OrtMemoryDevice instance.
   *
   * \param[in] memory_device OrtMemoryDevice instance.
   * \return The OrtDeviceMemoryType value.
   *
   * \since Version 1.23.
   */
  ORT_API_T(OrtDeviceMemoryType, MemoryDevice_GetMemoryType, _In_ const OrtMemoryDevice* memory_device);

  /** \brief Get the vendor ID from an OrtMemoryDevice instance.
   *
   * The vendor ID is used to identify the vendor of the device, and is typically set to the PCI vendor ID.
   *
   * If the device is not vendor specific (e.g. CPU memory) the vendor ID is set to 0.
   *
   * \param[in] memory_device OrtMemoryDevice instance.
   * \return The vendor ID value.
   *
   * \since Version 1.23.
   */
  ORT_API_T(uint32_t, MemoryDevice_GetVendorId, _In_ const OrtMemoryDevice* memory_device);

  /** \brief Get the device ID from an OrtMemoryDevice instance.
   *
   * \param[in] memory_device OrtMemoryDevice instance.
   * \return The device ID.
   *
   * \since Version 1.23.
   */
  ORT_API_T(uint32_t, MemoryDevice_GetDeviceId, _In_ const OrtMemoryDevice* memory_device);

  /** \brief Get the OrtSyncStreamImpl associated with an OrtSyncStream instance.
   *
   * This allows an the plugin library to connect its OrtSyncStreamImpl instance with an OrtSyncStream if needed.
   *
   * \param[in] stream The OrtSyncStream instance to find an OrtSyncStreamImpl for.
   * \return The associated OrtSyncStreamImpl if found. nullptr otherwise.
   *
   * \since Version 1.23.
   *
   * \remarks There should always be an OrtSyncStreamImpl associated with an OrtSyncStream instance that the EP gets.
   */
  ORT_API_T(const OrtSyncStreamImpl*, SyncStream_GetImpl, _In_ const OrtSyncStream* stream);

  /** \brief Get the current sync ID for a stream.
   *
   * \param[in] stream The OrtSyncStream to get the sync ID for.
   * \return Current sync ID.
   *
   * \since Version 1.23.
   */
  ORT_API_T(uint64_t, SyncStream_GetSyncId, _In_ const OrtSyncStream* stream);

  /** \brief Get the sync ID for the last time the consumer_stream waited on the producer_stream.
   *
   * When two streams are synchronized, the sync id represents the event used in that synchronization.
   *
   * \param[in] producer_stream The OrtSyncStream that produced the data.
   * \param[in] consumer_stream The OrtSyncStream that waited on the producer_stream.
   * \return ID for last sync. 0 if no sync has occurred between the two streams.
   *
   * \since Version 1.23.
   */
  ORT_API_T(uint64_t, GetSyncIdForLastWaitOnSyncStream,
            _In_ const OrtSyncStream* producer_stream, _In_ const OrtSyncStream* consumer_stream);
};

/**
 * \brief The data layout type.
 *
 * EPs may specify a preferred data layout type. ORT's default layout type is OrtEpDataLayout_NCHW, or
 * OrtEpDataLayout_Default.
 *
 * \since Version 1.23.
 */
typedef enum OrtEpDataLayout {
  OrtEpDataLayout_NCHW = 0,
  OrtEpDataLayout_NHWC,

  OrtEpDataLayout_Default = OrtEpDataLayout_NCHW,
} OrtEpDataLayout;

/**
 * \brief The OrtEp struct provides functions to implement for an execution provider.
 * \since Version 1.22.
 */
struct OrtEp {
  /** \brief The ONNX Runtime version the execution provider was compiled with.
   *
   * Implementation should set to ORT_API_VERSION.
   * ORT will use this to ensure it does not call functions that were not available when the library was compiled.
   *
   * \since Version 1.22.
   */
  uint32_t ort_version_supported;

  /** \brief Get the execution provider name.
   *
   * The returned string should be a null-terminated, UTF-8 encoded string. ORT will copy it.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \return The execution provider name.
   *
   * \since Version 1.22.
   */
  ORT_API_T(const char*, GetName, _In_ const OrtEp* this_ptr);

  /** \brief Get information about the nodes supported by the OrtEp instance.
   *
   * IMPORTANT: This is not the final version of this API function. This is currently experimental but will
   * be stabilized by the ONNX Runtime 1.23 release.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] graph The OrtGraph instance for which to populate node support. The OrtGraph could be a nested subgraph
   *                  contained by a node (e.g., an If or Loop node). ONNX Runtime calls this function separately
   *                  for each nested subgraph.
   * \param[inout] graph_support_info OrtEpGraphSupportInfo instance that the implementer must fill out in order to
   *                                  specify the supported nodes.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(GetCapability, _In_ OrtEp* this_ptr, _In_ const OrtGraph* graph,
                  _Inout_ OrtEpGraphSupportInfo* graph_support_info);

  /** \brief Compile OrtGraph instances assigned to the OrtEp. Implementer must set a OrtNodeComputeInfo instance
   * for each OrtGraph in order to define its computation function.
   *
   * If the session is configured to generate a pre-compiled model, the execution provider must return EPContext nodes,
   * as OrtNode instances, that ONNX Runtime uses to create a pre-compiled model, known as an "EPContext model".
   * An EPContext model contains EPContext nodes. Each EPContext node encapsulates the pre-compiled binary data for a
   * OrtGraph compiled for a specific execution provider. For more details about the EPContext design, refer to:
   *  \htmlonly
   *  <a href="https://onnxruntime.ai/docs/execution-providers/EP-Context-Design.html">EPContext design document.</a>
   *  \endhtmlonly
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] graphs Array of `count` OrtGraph instances to compile. Each graph contains only the nodes for
   *                   which the execution provider indicated support. Nested subgraphs contained by a
   *                   node, such as an If or Loop, have separate OrtGraph instances.
   * \param[in] fused_nodes Array of `count` fused nodes that will replace the compiled graphs.
   *                        Each fused node is an OrtNode initialized with the intended fused node name and
   *                        input/output information.
   * \param[in] count The number of OrtGraph instances to compile.
   * \param[out] node_compute_infos Array of `count` OrtNodeComputeInfo instances that define each OrtGraph instance's
   *                                computation function. The implementer allocates the OrtNodeComputeInfo instances.
   *                                ORT calls ReleaseNodeComputeInfos() to release multiple instances in a batch.
   * \param[out] ep_context_nodes Output array of `count` OrtNode instances, each representing an EPContext
   *                              node for a compiled OrtGraph. The execution provider must use
   *                              OrtModelEditorApi::CreateNode to create the OrtNode instances. ONNX Runtime takes
   *                              ownership of the OrtNode instances, so the execution provider must NOT call
   *                              OrtApi::ReleaseNode. Should be ignored if the session is not configured to generate an
   *                              EPContext model.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Do NOT cache the provided OrtGraph instances in any of the OrtNodeComputeInfo functions because the
   *       graphs are only valid for the duration of the call to Compile. Any graph/node/input/output
   *       names that are needed by the OrtNodeComputeInfo functions must be copied and stored by the OrtEp.
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(Compile, _In_ OrtEp* this_ptr, _In_ const OrtGraph** graphs,
                  _In_ const OrtNode** fused_nodes, _In_ size_t count,
                  _Out_writes_all_(count) OrtNodeComputeInfo** node_compute_infos,
                  _Out_writes_(count) OrtNode** ep_context_nodes);

  /** \brief Release OrtNodeComputeInfo instances.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[inout] node_compute_infos The OrtNodeComputeInfo instances to release.
   * \param[in] num_node_compute_infos The number of OrtNodeComputeInfo instances.
   *
   * \since Version 1.23.
   */
  ORT_API_T(void, ReleaseNodeComputeInfos, _In_ OrtEp* this_ptr,
            OrtNodeComputeInfo** node_compute_infos,
            _In_ size_t num_node_compute_infos);

  /** \brief Get the EP's preferred data layout.
   *
   * \note Implementation of this function is optional.
   *       If not implemented, ORT will assume that this EP prefers the data layout `OrtEpDataLayout::NCHW`.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[out] preferred_data_layout The EP's preferred data layout.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(GetPreferredDataLayout, _In_ OrtEp* this_ptr, _Out_ OrtEpDataLayout* preferred_data_layout);

  /** \brief Given an op with domain `domain` and type `op_type`, determine whether an associated node's data layout
   *         should be converted to `target_data_layout`.
   *         If the EP prefers a non-default data layout (see `GetPreferredDataLayout()`), this function will be called
   *         during layout transformation with `target_data_layout` set to the EP's preferred data layout.
   *
   * \note Implementation of this function is optional.
   *       If an EP prefers a non-default data layout, it may implement this to customize the specific op data layout
   *       preferences at a finer granularity.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] domain The op domain. An empty string means the ONNX domain.
   * \param[in] op_type The op type.
   * \param[in] target_data_layout The target data layout.
   * \param[out] should_convert Whether the associated node's data layout should be converted to `target_data_layout`.
   *                            If greater than 0, convert.
   *                            If 0, don't convert.
   *                            Otherwise, if less than 0, leave the decision to ORT.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(ShouldConvertDataLayoutForOp, _In_ OrtEp* this_ptr,
                  _In_z_ const char* domain, _In_z_ const char* op_type,
                  _In_ OrtEpDataLayout target_data_layout,
                  _Outptr_ int* should_convert);

  /** \brief Set dynamic options on this EP.
   *
   * Dynamic options can be set by the user at any time after session creation with `OrtApi::SetEpDynamicOptions()`.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] option_keys The dynamic option keys.
   * \param[in] option_values The dynamic option values.
   * \param[in] num_options The number of dynamic options.
   *
   * \note Implementation of this function is optional.
   *       An EP should only implement this if it needs to handle any dynamic options.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(SetDynamicOptions, _In_ OrtEp* this_ptr,
                  _In_reads_(num_options) const char* const* option_keys,
                  _In_reads_(num_options) const char* const* option_values,
                  _In_ size_t num_options);

  /** \brief Called by ORT to notify the EP of the start of a run.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] run_options The run options for this run.
   *
   * \note Implementation of this function is optional.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(OnRunStart, _In_ OrtEp* this_ptr, _In_ const OrtRunOptions* run_options);

  /** \brief Called by ORT to notify the EP of the end of a run.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] run_options The run options for this run.
   * \param[in] sync_stream Whether any associated stream should be synchronized during this call.
   *                        Only applicable if there is such a stream.
   *
   * \note Implementation of this function is optional.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(OnRunEnd, _In_ OrtEp* this_ptr, _In_ const OrtRunOptions* run_options, _In_ bool sync_stream);

  /** \brief Create an OrtAllocator for the given OrtMemoryInfo for an OrtSession.
   *
   * The OrtMemoryInfo instance will match one of the values set in the OrtEpDevice using EpDevice_AddAllocatorInfo.
   * Any allocator specific options should be read from the session options.
   *
   * If nullptr OrtEpFactory::CreateAllocator will be used.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] memory_info The OrtMemoryInfo to create the allocator for. May be nullptr.
   * \param[out] allocator The created OrtAllocator instance. Set to nullptr if the default CPU allocator is used.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CreateAllocator, _In_ OrtEp* this_ptr,
                  _In_ const OrtMemoryInfo* memory_info,
                  _Outptr_result_maybenull_ OrtAllocator** allocator);

  /** \brief Create a synchronization stream for the given memory device for an OrtSession.
   *
   * This is used to create a synchronization stream for the execution provider and is used to synchronize
   * operations on the device during model execution.
   * Any stream specific options should be read from the session options.
   *
   * If nullptr OrtEpFactory::CreateSyncStreamForDevice will be used.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] memory_device The OrtMemoryDevice to create the synchronization stream for.
   * \param[out] stream The created OrtSyncStreamImpl instance. nullptr if the execution provider is not stream aware.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CreateSyncStreamForDevice, _In_ OrtEp* this_ptr,
                  _In_ const OrtMemoryDevice* memory_device,
                  _Outptr_ OrtSyncStreamImpl** stream);

  /** \brief Get a string with details about the EP stack used to produce a compiled model.
   *
   * This function gets a compatibility information string that contains details about the execution provider
   * used to compile a given model. This string can later be used with ValidateCompiledModelCompatibilityInfo
   * to determine if a compiled model is compatible with the EP.
   *
   * The returned string should be a null-terminated, UTF-8 encoded string. ORT will copy it.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] graph The OrtGraph instance for which to generate compatibility information.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API_T(const char*, GetCompiledModelCompatibilityInfo, _In_ OrtEp* this_ptr,
            _In_ const OrtGraph* graph);
};

/** \brief The function signature that ORT will call to create OrtEpFactory instances.
 *
 * This must be available in a function called 'CreateEpFactories' in the execution provider library.
 *
 * \param[in] registered_name The name the execution library is registered with by RegisterExecutionProviderLibrary
 * \param[in] ort_api_base The OrtApiBase instance that is used by the factory to get the OrtApi instance for the
 *                         version of ORT that the library was compiled against.
 * \param[in] default_logger The default ORT logger that can be used for logging outside of an inference session.
 * \param[in,out] factories The implementation should create and add OrtEpFactory instances to this
 *                          pre-allocated array.
 *                          i.e. usage is `factories[0] = new MyEpFactory();`
 * \param[in] max_factories The maximum number of OrtEpFactory instances that can be added to `factories`.
 *                          Current default is to allow 4 factories. This can be increased in the future if needed.
 * \param[out] num_factories The number of OrtEpFactory instances created by the factory and added to `factories`.
 *
 * \snippet{doc} snippets.dox OrtStatus Return Value
 *
 * \since Version 1.22.
 */
typedef OrtStatus* (*CreateEpApiFactoriesFn)(_In_ const char* registered_name, _In_ const OrtApiBase* ort_api_base,
                                             _In_ const OrtLogger* default_logger,
                                             _Inout_ OrtEpFactory** factories, _In_ size_t max_factories,
                                             _Out_ size_t* num_factories);

/** \brief The function signature that ORT will call to release an OrtEpFactory instance.
 *
 * This must be available in a function called 'ReleaseEpFactory' in the execution provider library.
 *
 * \param[in] factory The OrtEpFactory instance to release.
 *
 * \snippet{doc} snippets.dox OrtStatus Return Value
 *
 * \since Version 1.22.
 */
typedef OrtStatus* (*ReleaseEpApiFactoryFn)(_In_ OrtEpFactory* factory);

/**
 * \brief The OrtEpFactory provides functions to create and manage execution providers.
 * \since Version 1.22.
 */
struct OrtEpFactory {
  /** \brief The ONNX Runtime version the execution provider was compiled with.
   *
   * Implementation should set to ORT_API_VERSION.
   * ORT will use this to ensure it does not call functions that were not available when the library was compiled.
   *
   * \since Version 1.22.
   */
  uint32_t ort_version_supported;

  /** \brief Get the name of the execution provider that the factory creates.
   *
   * The returned string should be a null-terminated, UTF-8 encoded string. ORT will copy it.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \return The name of the execution provider the factory creates.
   *
   * \since Version 1.22.
   */
  ORT_API_T(const char*, GetName, const OrtEpFactory* this_ptr);

  /** \brief Get the name of vendor who owns the execution provider that the factory creates.
   *
   * The returned string should be a null-terminated, UTF-8 encoded string. ORT will copy it.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \return vendor The vendor name of the execution provider the factory creates.
   *
   * \since Version 1.22.
   */
  ORT_API_T(const char*, GetVendor, const OrtEpFactory* this_ptr);  // return EP vendor

  /** \brief Get information from the execution provider about OrtHardwareDevice support.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   *                     Non-const as the factory is passed through to the CreateEp call via the OrtEpDevice.
   * \param[in] devices The OrtHardwareDevice instances that are available.
   * \param[in] num_devices The number of OrtHardwareDevice instances.
   * \param[out] ep_devices OrtEpDevice instances for each OrtHardwareDevice that the EP can use.
   *                        The implementation should call OrtEpApi::CreateEpDevice to create, and add the OrtEpDevice
   *                        instances to this pre-allocated array. ORT will take ownership of the values returned.
   *                        i.e. usage is `ep_devices[0] = <ptr to OrtEpDevice created with OrtEpApi::CreateEpDevice>;`
   * \param[in] max_ep_devices The maximum number of OrtEpDevices that can be added to ep_devices.
   *                           Current default is 8. This can be increased if needed.
   * \param[out] num_ep_devices The number of EP devices added to ep_devices.
   * \return true if the factory can create an execution provider that uses `device`.
   *
   * \since Version 1.22.
   */
  ORT_API2_STATUS(GetSupportedDevices, _In_ OrtEpFactory* this_ptr,
                  _In_reads_(num_devices) const OrtHardwareDevice* const* devices,
                  _In_ size_t num_devices,
                  _Inout_ OrtEpDevice** ep_devices,
                  _In_ size_t max_ep_devices,
                  _Out_ size_t* num_ep_devices);

  /** \brief Function to create an OrtEp instance for use in a Session.
   *
   *  ORT will call ReleaseEp to release the instance when it is no longer needed.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] devices The OrtHardwareDevice instances that the execution provider was selected to use.
   *                    May be a subset of the OrtHardwareDevice instances that the execution provider's factory
   *                    set as supported in the call to OrtEpFactory::GetSupportedDevices.
   * \param[in] ep_metadata_pairs Execution provider metadata that was provided to OrtEpApi::CreateEpDevice, for each
   *                              device.
   * \param[in] num_devices The number of devices the execution provider was selected for.
   * \param[in] session_options The OrtSessionOptions instance that contains the configuration options for the
   *                            session. This will include ep_options from GetSupportedDevices as well as any
   *                            user provided overrides.
   *                            Execution provider options will have been added with a prefix of 'ep.[ep name].'.
   *                            The OrtSessionOptions instance will NOT be valid after this call and should not be
   *                            stored for later use.
   * \param[in] logger The OrtLogger instance for the session that the execution provider should use for logging.
   * \param[out] ep The OrtEp instance created by the factory.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.22.
   */
  ORT_API2_STATUS(CreateEp, _In_ OrtEpFactory* this_ptr,
                  _In_reads_(num_devices) const OrtHardwareDevice* const* devices,
                  _In_reads_(num_devices) const OrtKeyValuePairs* const* ep_metadata_pairs,
                  _In_ size_t num_devices,
                  _In_ const OrtSessionOptions* session_options,
                  _In_ const OrtLogger* logger, _Outptr_ OrtEp** ep);

  /** \brief Release the OrtEp instance.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] ep The OrtEp instance to release.
   *
   * \since Version 1.22.
   */
  ORT_API_T(void, ReleaseEp, OrtEpFactory* this_ptr, struct OrtEp* ep);

  /** \brief Get the vendor id who owns the execution provider that the factory creates.
   *
   * This is typically the PCI vendor ID. See https://pcisig.com/membership/member-companies
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \return vendor_id The vendor ID of the execution provider the factory creates.
   *
   * \since Version 1.23.
   */
  ORT_API_T(uint32_t, GetVendorId, const OrtEpFactory* this_ptr);

  /** \brief Get the version of the execution provider that the factory creates.
   *
   * The version string should adhere to the Semantic Versioning 2.0 specification
   * (https://github.com/semver/semver/blob/v2.0.0/semver.md).
   *
   * The returned string should be a null-terminated, UTF-8 encoded string. ORT will copy it.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \return The execution provider version string.
   *
   * \since Version 1.23.
   */
  ORT_API_T(const char*, GetVersion, _In_ const OrtEpFactory* this_ptr);

  /** \brief Validate the compatibility of a compiled model with the execution provider factory for one or more devices.
   *
   * Given a compatibility info string produced during model compilation, the EP factory should determine whether the
   * compiled model is compatible with the EP factory when targeting the provided hardware devices. All devices provided
   * must belong to the same execution provider instance that this factory creates.
   *
   * The EP factory implementation should consider the set of devices (e.g., multi-adapter or multi-GPU scenarios) when
   * evaluating compatibility and set `model_compatibility` accordingly.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] devices Array of OrtHardwareDevice pointers that the EP would run on. All must map to this EP.
   * \param[in] num_devices Number of entries in `devices`.
   * \param[in] compatibility_info The compatibility information string produced when the model was compiled.
   * \param[out] model_compatibility OrtCompiledModelCompatibility value describing the compatibility of the model with the EP.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(ValidateCompiledModelCompatibilityInfo, _In_ OrtEpFactory* this_ptr,
                  _In_reads_(num_devices) const OrtHardwareDevice* const* devices,
                  _In_ size_t num_devices,
                  _In_ const char* compatibility_info,
                  _Out_ OrtCompiledModelCompatibility* model_compatibility);

  /** \brief Create an OrtAllocator that can be shared across sessions for the given OrtMemoryInfo.
   *
   * The factory that creates the EP is responsible for providing the allocators required by the EP.
   * The OrtMemoryInfo instance will match one of the values set in the OrtEpDevice using EpDevice_AddAllocatorInfo.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] memory_info The OrtMemoryInfo to create the allocator for. May be nullptr.
   * \param[in] allocator_options Optional key-value pairs for allocator options, can be nullptr.
   * \param[out] allocator The created OrtAllocator instance. Set to nullptr if the default CPU allocator is used.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CreateAllocator, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtMemoryInfo* memory_info,
                  _In_opt_ const OrtKeyValuePairs* allocator_options,
                  _Outptr_result_maybenull_ OrtAllocator** allocator);

  /** \brief Release an OrtAllocator created by the factory.
   *
   * \since Version 1.23.
   */
  ORT_API_T(void, ReleaseAllocator, _In_ OrtEpFactory* this_ptr, _In_ OrtAllocator* allocator);

  /** \brief Create an OrtDataTransferImpl instance for the factory.
   *
   * This is used to create an IDataTransfer implementation that can be used to copy data between devices
   * that the execution provider supports.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[out] data_transfer The created OrtDataTransferImpl instance. Set to nullptr if not required.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CreateDataTransfer, _In_ OrtEpFactory* this_ptr,
                  _Outptr_result_maybenull_ OrtDataTransferImpl** data_transfer);

  /** \brief Check if execution providers created by the factory are stream aware.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \return True if the factory creates execution providers that are stream aware and it implements CreateSyncStreamForDevice.
   *
   * \since Version 1.23.
   */
  ORT_API_T(bool, IsStreamAware, _In_ const OrtEpFactory* this_ptr);

  /** \brief Create a synchronization stream for the given memory device.
   *
   * This is used to create a synchronization stream for the memory device that can be used for operations outside of
   * a session.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] memory_device The OrtMemoryDevice to create the synchronization stream for.
   * \param[in] stream_options Options for stream creation. May be nullptr.
   * \param[out] stream The created OrtSyncStreamImpl instance. nullptr if the execution provider is not stream aware.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.23.
   */
  ORT_API2_STATUS(CreateSyncStreamForDevice, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtMemoryDevice* memory_device,
                  _In_opt_ const OrtKeyValuePairs* stream_options,
                  _Outptr_ OrtSyncStreamImpl** stream);
};

#ifdef __cplusplus
}
#endif
