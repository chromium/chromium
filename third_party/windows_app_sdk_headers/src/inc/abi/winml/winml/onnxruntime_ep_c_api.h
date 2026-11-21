// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Do not include this file directly. Please include "onnxruntime_c_api.h" instead.

#if defined(__DOXYGEN__)
// When running a Doxygen build, include onnxruntime_c_api.h. Doxygen expects header files to be self-contained.
#include "onnxruntime_c_api.h"
#else
// In normal usage, do not include onnxruntime_c_api.h. This file is explicitly included in onnxruntime_c_api.h.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup Global
 * @{
 */
ORT_RUNTIME_CLASS(Ep);
ORT_RUNTIME_CLASS(EpFactory);
ORT_RUNTIME_CLASS(EpGraphSupportInfo);
ORT_RUNTIME_CLASS(MemoryDevice);  // opaque class to wrap onnxruntime::OrtDevice
ORT_RUNTIME_CLASS(NodeComputeContext);

ORT_RUNTIME_CLASS(DataTransferImpl);
ORT_RUNTIME_CLASS(SyncNotificationImpl);
ORT_RUNTIME_CLASS(SyncStreamImpl);

ORT_RUNTIME_CLASS(ExternalResourceImporterImpl);

ORT_RUNTIME_CLASS(OpSchema);
ORT_RUNTIME_CLASS(OpSchemaTypeConstraint);
ORT_RUNTIME_CLASS(ProfilingEventsContainer);
ORT_RUNTIME_CLASS(ProfilingEvent);  // Based on the Trace Event Format's "complete event"
/// @}

/** \brief Base struct for imported external memory handles.
 *
 * EPs derive from this struct to add EP-specific fields (e.g., CUdeviceptr for CUDA).
 * EP is responsible for creating and releasing instances of the derived type.
 *
 * Example derived type for CUDA EP:
 * \code
 * struct MyCudaExternalMemoryHandle : OrtExternalMemoryHandle {
 *   CUexternalMemory ext_memory;
 *   CUdeviceptr mapped_ptr;
 *   bool is_dedicated;
 * };
 * \endcode
 *
 * \since Version 1.24.
 */
struct OrtExternalMemoryHandle {
  uint32_t version;                        ///< Must be ORT_API_VERSION
  const OrtEpDevice* ep_device;            ///< EP device that created this handle
  OrtExternalMemoryDescriptor descriptor;  ///< External memory descriptor

  /** \brief Release callback for this handle. EP sets this to its release function.
   *
   * ORT calls this when ReleaseExternalMemoryHandle is invoked. The EP's callback
   * should cast the handle to its derived type and delete it.
   */
  void(ORT_API_CALL* Release)(_In_ OrtExternalMemoryHandle* handle);
};

/** \brief Base struct for imported external semaphore handles.
 *
 * EPs derive from this struct to add EP-specific fields (e.g., CUexternalSemaphore for CUDA).
 * EP is responsible for creating and releasing instances of the derived type.
 *
 * Example derived type for CUDA EP:
 * \code
 * struct MyCudaExternalSemaphoreHandle : OrtExternalSemaphoreHandle {
 *   CUexternalSemaphore ext_semaphore;
 * };
 * \endcode
 *
 * \since Version 1.24.
 */
struct OrtExternalSemaphoreHandle {
  uint32_t version;                           ///< Must be ORT_API_VERSION
  const OrtEpDevice* ep_device;               ///< EP device that created this handle
  OrtExternalSemaphoreDescriptor descriptor;  ///< External semaphore descriptor

  /** \brief Release callback for this handle. EP sets this to its release function.
   *
   * ORT calls this when ReleaseExternalSemaphoreHandle is invoked. The EP's callback
   * should cast the handle to its derived type and delete it.
   */
  void(ORT_API_CALL* Release)(_In_ OrtExternalSemaphoreHandle* handle);
};

// Opaque types for kernel-based EPs
ORT_RUNTIME_CLASS(KernelRegistry);
ORT_RUNTIME_CLASS(KernelDefBuilder);
ORT_RUNTIME_CLASS(KernelDef);
ORT_RUNTIME_CLASS(DataType);  // combination of ONNXType (e.g., Tensor, Map, Sequence) and ONNXTensorElementDataType
ORT_RUNTIME_CLASS(SharedPrePackedWeightCache);

/** \brief Struct that an EP implements for IDataTransfer to copy between devices it uses and CPU.
 *
 * \since Version 1.23.
 */
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
   * \param[in] consumer_stream The OrtSyncStream instance that will wait on this notification to be activated.
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

/** \brief Struct that an EP implements for external resource import (memory + semaphore import).
 *
 * This capability object provides methods for importing external GPU memory and semaphores
 * for zero-copy import. EPs that support D3D12, CUDA, HIP, or Vulkan external resource APIs
 * can implement this interface.
 *
 * \since Version 1.24.
 */
struct OrtExternalResourceImporterImpl {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION

  // Memory operations (stream-independent)

  /** \brief Check if the implementation can import external memory of the given handle type.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] handle_type The type of external memory handle to check.
   * \return True if the handle type is supported.
   *
   * \since Version 1.24.
   */
  ORT_API_T(bool, CanImportMemory,
            _In_ const OrtExternalResourceImporterImpl* this_ptr,
            _In_ OrtExternalMemoryHandleType handle_type);

  /** \brief Import external memory.
   *
   * The EP creates a derived type of OrtExternalMemoryHandle and returns a pointer to the base.
   * EP is responsible for the lifetime of the handle (release via ReleaseMemory).
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] desc Descriptor containing the external memory handle and properties.
   * \param[out] out_handle Output parameter set to the created OrtExternalMemoryHandle (EP's derived type).
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(ImportMemory,
                  _In_ OrtExternalResourceImporterImpl* this_ptr,
                  _In_ const OrtExternalMemoryDescriptor* desc,
                  _Outptr_ OrtExternalMemoryHandle** out_handle);

  /** \brief Release an imported external memory handle.
   *
   * The EP deletes its derived type instance.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] handle The OrtExternalMemoryHandle to release (EP casts to its derived type).
   *
   * \since Version 1.24.
   */
  ORT_API_T(void, ReleaseMemory,
            _In_ OrtExternalResourceImporterImpl* this_ptr,
            _In_ OrtExternalMemoryHandle* handle);

  /** \brief Create a tensor backed by imported external memory.
   *
   * The created tensor is a view over the imported memory and does not copy data.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] mem_handle The imported external memory handle (EP casts to its derived type).
   * \param[in] tensor_desc Descriptor specifying tensor element type, shape, and optional offset.
   * \param[out] out_tensor Output parameter set to the created OrtValue containing the tensor.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(CreateTensorFromMemory,
                  _In_ OrtExternalResourceImporterImpl* this_ptr,
                  _In_ const OrtExternalMemoryHandle* mem_handle,
                  _In_ const OrtExternalTensorDescriptor* tensor_desc,
                  _Outptr_ OrtValue** out_tensor);

  // Semaphore operations (require stream)

  /** \brief Check if the implementation can import external semaphores of the given type.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] type The type of external semaphore to check.
   * \return True if the semaphore type is supported.
   *
   * \since Version 1.24.
   */
  ORT_API_T(bool, CanImportSemaphore,
            _In_ const OrtExternalResourceImporterImpl* this_ptr,
            _In_ OrtExternalSemaphoreType type);

  /** \brief Import an external semaphore.
   *
   * The EP creates a derived type of OrtExternalSemaphoreHandle and returns a pointer to the base.
   * EP is responsible for the lifetime of the handle (release via ReleaseSemaphore).
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] desc Descriptor containing the external semaphore handle and type.
   * \param[out] out_handle Output parameter set to the created OrtExternalSemaphoreHandle (EP's derived type).
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(ImportSemaphore,
                  _In_ OrtExternalResourceImporterImpl* this_ptr,
                  _In_ const OrtExternalSemaphoreDescriptor* desc,
                  _Outptr_ OrtExternalSemaphoreHandle** out_handle);

  /** \brief Release an imported external semaphore handle.
   *
   * The EP deletes its derived type instance.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] handle The OrtExternalSemaphoreHandle to release (EP casts to its derived type).
   *
   * \since Version 1.24.
   */
  ORT_API_T(void, ReleaseSemaphore,
            _In_ OrtExternalResourceImporterImpl* this_ptr,
            _In_ OrtExternalSemaphoreHandle* handle);

  /** \brief Wait on an external semaphore on the EP's stream.
   *
   * Inserts a wait operation into the EP's stream that blocks until the semaphore
   * reaches the specified value.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] handle The imported external semaphore (EP casts to its derived type).
   * \param[in] stream The OrtSyncStream to wait on.
   * \param[in] value The fence/semaphore value to wait for.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(WaitSemaphore,
                  _In_ OrtExternalResourceImporterImpl* this_ptr,
                  _In_ OrtExternalSemaphoreHandle* handle,
                  _In_ OrtSyncStream* stream,
                  _In_ uint64_t value);

  /** \brief Signal an external semaphore from the EP's stream.
   *
   * Inserts a signal operation into the EP's stream that sets the semaphore
   * to the specified value when reached.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   * \param[in] handle The imported external semaphore (EP casts to its derived type).
   * \param[in] stream The OrtSyncStream to signal from.
   * \param[in] value The fence/semaphore value to signal.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(SignalSemaphore,
                  _In_ OrtExternalResourceImporterImpl* this_ptr,
                  _In_ OrtExternalSemaphoreHandle* handle,
                  _In_ OrtSyncStream* stream,
                  _In_ uint64_t value);

  // Release the capability object itself

  /** \brief Release the OrtExternalResourceImporterImpl instance.
   *
   * This is called by ORT when the OrtExternalResourceImporterImpl instance is no longer needed.
   * The implementation should release any resources held by the instance.
   *
   * \param[in] this_ptr Pointer to the OrtExternalResourceImporterImpl instance.
   *
   * \since Version 1.24.
   */
  ORT_API_T(void, Release, _In_ OrtExternalResourceImporterImpl* this_ptr);
};

/** \brief The event category for profiling events reported by an execution provider.
 *
 * \since Version 1.25.
 */
typedef enum OrtProfilingEventCategory {
  OrtProfilingEventCategory_SESSION = 0,  ///< Session-level event
  OrtProfilingEventCategory_NODE = 1,     ///< Node-level event
  OrtProfilingEventCategory_KERNEL = 2,   ///< Kernel-level event
  OrtProfilingEventCategory_API = 3,      ///< API-level event
} OrtProfilingEventCategory;

struct OrtEpProfilerImpl;
typedef struct OrtEpProfilerImpl OrtEpProfilerImpl;

/** \brief Struct that an EP implements for profiling support.
 *
 * An execution provider optionally implements this struct to participate in ONNX Runtime's profiling system.
 * The EP creates and returns an instance of this struct via OrtEp::CreateProfiler.
 *
 * ORT calls the function pointers at appropriate times during a profiling session:
 *  - StartProfiling once when profiling begins.
 *  - [Optional] StartEvent / StopEvent around each ORT event (operator executions, session events, etc.).
 *  - EndProfiling once when profiling ends to collect EP events.
 *  - Release when ORT no longer needs the profiler.
 *
 * Profiling scenarios:
 *  - ORT session profiling: Captures session initialization and one or more runs with a single ORT session.
 *    - Enabled via OrtApi::EnableProfiling(session_options).
 *    - There is one ORT session, one OrtEp, and one OrtEpProfilerImpl
 *    - Concurrency notes: An application may use a single ORT session (and the single OrtEpProfilerImpl) to run
 *      multiple inferences concurrently. The OrtEpProfilerImpl::StartEvent and OrtEpProfilerImpl::StopEvent
 *      functions will be called with ORT events from multiple concurrent runs. The OrtEpProfilerImpl::EndProfiling
 *      function is expected to return all EP events from all runs with correct correlations with the original ORT
 *      events.
 *  - ORT run profiling: Captures events for a given run. An application can either enable session profiling or run
 *    profiling, but not both at the same time.
 *    - Enabled via OrtApi::RunOptionsEnableProfiling(run_options).
 *    - There is one ORT session, one OrtEp, and multiple OrtEpProfilerImpl instances (one per profiled run).
 *    - Concurrency notes: each OrtEpProfilerImpl only receives calls for its specific run.
 *      OrtEpProfilerImpl::EndProfiling must only return EP events for its specific run.
 *
 * \since Version 1.25.
 */
struct OrtEpProfilerImpl {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION.

  /** \brief Release the OrtEpProfilerImpl instance.
   *
   * Called by ORT when the profiler is no longer needed.
   * The implementation should release any resources held by the instance.
   *
   * \param[in] this_ptr Pointer to the OrtEpProfilerImpl instance.
   *
   * \note Implementation of this function is required.
   *
   * \since Version 1.25.
   */
  ORT_API_T(void, Release, _In_ OrtEpProfilerImpl* this_ptr);

  /** \brief Called when profiling starts.
   *
   * Allows the EP profiler to initialize profiling utilities and record the profiling start time.
   *
   * An EP profiler should record its own clock's current time when this function is called. This allows the EP to
   * later compute ORT-relative event timestamps by combining `ep_profiling_start_offset_ns` with the EP's own
   * elapsed time since this call. The formula is:
   *
   *   event_timestamp_us = (ep_profiling_start_offset_ns + (ep_event_time_ns - ep_profiling_start_time_ns)) / 1000
   *
   * where `ep_event_time_ns` and `ep_profiling_start_time_ns` are measured using the EP's own clock.
   *
   * \param[in] this_ptr Pointer to the OrtEpProfilerImpl instance.
   * \param[in] ep_profiling_start_offset_ns The elapsed time in nanoseconds (using ORT's profiling clock) between
   *                                         ORT's profiling start and this call to StartProfiling.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note An error OrtStatus returned from this function is logged by ORT (does not end execution).
   * \note Implementation of this function is required.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(StartProfiling, _In_ OrtEpProfilerImpl* this_ptr, _In_ int64_t ep_profiling_start_offset_ns);

  /** \brief Called when an ORT event (e.g., session initialization, node kernel execution, etc.) begins.
   *
   * ORT pairs every StartEvent call with a corresponding call to StopEvent with the same ORT event correlation ID.
   * EP profiler implementations may use the calls to StartEvent and StopEvent to maintain a stack of ORT event
   * correlation IDs that can be correlated with EP events (e.g., GPU kernel events). For example:
   *
   *   OrtEpProfilerImpl::StartEvent(x) -> EP ort event stack: [x]    <- top of stack
   *       [EP events are tagged with 'x']
   *   OrtEpProfilerImpl::StartEvent(y) -> EP ort event stack: [x, y] <- top of stack
   *       [EP events are tagged with 'y']
   *   OrtEpProfilerImpl::StopEvent(y) ->  EP ort event stack: [x]    <- top of stack
   *   OrtEpProfilerImpl::StartEvent(z) -> EP ort event stack: [x, z] <- top of stack
   *       [EP events are tagged with 'z']
   *   OrtEpProfilerImpl::StopEvent(z) ->  EP ort event stack: [x]    <- top of stack
   *   OrtEpProfilerImpl::StopEvent(x) ->  EP ort event stack: [ ]    <- top of stack
   *
   * Tagging EP events with the ORT event correlation ID enables the EP to annotate its own events
   * with metadata from the parent ORT event (e.g., operator name).
   *
   * \note **Threading:** For a given ORT event, StartEvent, the kernel's Compute() entry point, and StopEvent
   *       are all invoked on the same CPU thread (Compute() may asynchronously launch device work).
   *       Different ORT events may run on different threads (e.g., inter-op parallelism), so correlation
   *       stacks (e.g., for CUPTI) should use thread-local storage.
   *
   * \note The ORT event correlation ID is an absolute, epoch-based timestamp in microseconds. It is computed
   *       from the ORT event's start time using std::chrono::high_resolution_clock (platform-defined epoch).
   *       Because it is absolute rather than relative to profiling start, it is practically unique across
   *       concurrent profiling sessions within the same process (collisions require sub-microsecond event
   *       concurrency) and can be used directly as a correlation ID for EP profiling utilities
   *       (e.g., CUPTI or ROCTracer).
   *
   * \param[in] this_ptr Pointer to the OrtEpProfilerImpl instance.
   * \param[in] ort_event_correlation_id Absolute, epoch-based correlation ID for the ORT event that is starting.
   *                                     The same value is passed to the corresponding StopEvent call.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note An error OrtStatus returned from this function is logged by ORT (does not end execution).
   * \note Implementation of this function is optional. If set to NULL, it is not called.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(StartEvent, _In_ OrtEpProfilerImpl* this_ptr, _In_ uint64_t ort_event_correlation_id);

  /** \brief Called when a profiled ORT event (e.g., session initialization, node kernel execution, etc.) ends.
   *
   * ORT pairs every StartEvent call with a corresponding call to StopEvent with the same ORT event correlation ID.
   * EP profiler implementations may use the calls to StartEvent and StopEvent to maintain a stack of ORT event
   * correlation IDs that can be correlated with EP events (e.g., GPU kernel events). For example:
   *
   *   OrtEpProfilerImpl::StartEvent(x) -> EP ort event stack: [x]    <- top of stack
   *       [EP events are tagged with 'x']
   *   OrtEpProfilerImpl::StartEvent(y) -> EP ort event stack: [x, y] <- top of stack
   *       [EP events are tagged with 'y']
   *   OrtEpProfilerImpl::StopEvent(y) ->  EP ort event stack: [x]    <- top of stack
   *   OrtEpProfilerImpl::StartEvent(z) -> EP ort event stack: [x, z] <- top of stack
   *       [EP events are tagged with 'z']
   *   OrtEpProfilerImpl::StopEvent(z) ->  EP ort event stack: [x]    <- top of stack
   *   OrtEpProfilerImpl::StopEvent(x) ->  EP ort event stack: [ ]    <- top of stack
   *
   * Tagging EP events with the ORT event correlation ID enables the EP to annotate its own events
   * with metadata from the parent ORT event (e.g., operator name).
   *
   * \note **Threading:** For a given ORT event, StartEvent, the kernel's Compute() entry point, and StopEvent
   *       are all invoked on the same CPU thread (Compute() may asynchronously launch device work).
   *       Different ORT events may run on different threads (e.g., inter-op parallelism), so correlation
   *       stacks (e.g., for CUPTI) should use thread-local storage.
   *
   * \note The ORT event correlation ID is an absolute, epoch-based timestamp in microseconds. It is computed
   *       from the ORT event's start time using std::chrono::high_resolution_clock (platform-defined epoch).
   *       Because it is absolute rather than relative to profiling start, it is practically unique across
   *       concurrent profiling sessions within the same process (collisions require sub-microsecond event
   *       concurrency) and can be used directly as a correlation ID for EP profiling utilities
   *       (e.g., CUPTI or ROCTracer).
   *
   * \param[in] this_ptr Pointer to the OrtEpProfilerImpl instance.
   * \param[in] ort_event_correlation_id Absolute, epoch-based correlation ID for the ORT event that is ending.
   *                                     The same value was passed to the corresponding StartEvent call.
   * \param[in] ort_event Opaque pointer to the ORT profiling event. Valid only during this call.
   *                      Use OrtEpApi accessor functions to read event fields.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note An error OrtStatus returned from this function is logged by ORT (does not end execution).
   * \note Implementation of this function is optional. If set to NULL, it is not called.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(StopEvent, _In_ OrtEpProfilerImpl* this_ptr, _In_ uint64_t ort_event_correlation_id,
                  _In_ const OrtProfilingEvent* ort_event);

  /** \brief Called when profiling ends to collect the EP's new profiling events since the call to StartProfiling.
   *
   * An EP profiler converts its events to OrtProfilingEvent instances and adds them into the provided
   * OrtProfilingEventsContainer container. Call OrtEpApi::CreateProfilingEvent to create a new
   * OrtProfilingEvent instance. Then call OrtEpApi::ProfilingEventsContainer_AddEvents to add one or more
   * events to the container.
   *
   * After this function returns, ORT appends the EP's events to the profiling timeline.
   *
   * \param[in] this_ptr The OrtEpProfilerImpl instance.
   * \param[in] events_container Event container to which the EP profiler adds its new events.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note An error OrtStatus returned from this function is logged by ORT (does not end execution).
   * \note Implementation of this function is required.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(EndProfiling, _In_ OrtEpProfilerImpl* this_ptr,
                  _In_ OrtProfilingEventsContainer* events_container);
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

struct OrtKernelImpl;
typedef struct OrtKernelImpl OrtKernelImpl;

/**
 * \brief Contains functions that an OrtEp implements to specify the computation for an operator kernel.
 * \since Version 1.24.
 */
struct OrtKernelImpl {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION
  uint32_t flags;                  ///< EP must initialize to 0. Used internally by ORT.

  /** \brief Computation function called to execute the kernel on an EP.
   *
   * \note Implementation of this function is required.
   *
   * \param[in] this_ptr The OrtKernelImpl instance.
   * \param[in] context The OrtKernelContext instance that provides access to the inputs and outputs.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(Compute, _In_ OrtKernelImpl* this_ptr, _In_ OrtKernelContext* context);

  /** \brief Called by ORT to release the OrtKernelImpl instance and its resources.
   *
   * \note Implementation of this function is required.
   *
   * \param[in] this_ptr The OrtKernelImpl instance.
   *
   * \since Version 1.24.
   */
  ORT_API_T(void, Release, _In_ OrtKernelImpl* this_ptr);

  /** \brief Optional function to pre-pack a constant tensor (i.e., a weight) to the kernel's preferred data layout.
   *
   * For example, a Conv kernel can define this function to pack input W to the channel-last data layout
   * before inference.
   *
   * Pre-packing can operate in three different modes: no pre-packing mode, sharing mode, and non-sharing mode.
   *    1) No pre-packing mode: The kernel can forgo any weight pre-packing for the given `input_index` by setting
   *                            `is_packed` to false and returning a successful OrtStatus. In this mode, the kernel's
   *                            OrtKernelImpl::SetSharedPrePackedWeight() function is not called for that specific
   *                            `input_index`.
   *    2) Sharing mode: Sharing is allowed if the `prepacked_weight_cache` argument is not NULL and the EP stores
   *                     weight data in CPU-accessible memory. In this case, the kernel can optionally choose
   *                     to share the packed weight with other kernels that use the same weight
   *                     (compared by content hash). To do so, the kernel must allocate the packed weight with the
   *                     provided `allocator`, then it stores the packed weight data into `prepacked_weight_cache`
   *                     via SharedPrePackedWeightCache_StoreWeightData(), sets `is_packed` to true, and returns a
   *                     successful OrtStatus. ORT will subsequently call OrtKernelImpl::SetSharedPrePackedWeight()
   *                     to provide this kernel with the actual shared weight data, whose memory location could
   *                     differ (i.e., if shared data was allocated by a previously processed kernel).
   *    3) Non-sharing mode: In non-sharing mode, the `prepacked_weight_cache` argument is ignored. In this mode,
   *                         the implementation allocates the packed data with the provided `allocator`, sets
   *                         `is_packed` to true, and returns a successful OrtStatus. The kernel is ultimately
   *                         responsible for releasing the packed data for the weight with `allocator`.
   *                         ORT may release the original (unpacked) weight, which must not be accessed in
   *                         OrtKernelImpl::Compute(). Note that in this mode, the kernel's
   *                         OrtKernelImpl::SetSharedPrePackedWeight() function is not called by ORT for that specific
   *                         `input_index`.
   *
   * \note This function is based on the internal OpKernel::PrePack() virtual function used within ORT.
   *
   * \param[in] this_ptr The OrtKernelImpl instance.
   * \param[in] tensor The OrtValue instance representing the constant tensor (weight). Do not cache in the kernel.
   * \param[in] input_index The input index of the tensor in this kernel.
   * \param[in] allocator Allocator for allocating the pre-packed data. Its use is required in sharing mode and
   *                      recommended, but not required, in the non-sharing mode. This will be an allocator set by
   *                      the application for the session/environment (e.g., via CreateAndRegisterAllocator[V2]
   *                      or RegisterAllocator), or an allocator on the OrtEpDevice (read-only or default) otherwise.
   *                      The allocator remains valid throughout the lifetime of the OrtKernelImpl instance.
   * \param[in] prepacked_weight_cache May be NULL. If not NULL, the kernel may choose to share a packed weight by
   *                                   first storing it in the OrtSharedPrePackedWeightCache instance and then
   *                                   receiving the actual shared weight data in the call to
   *                                   OrtKernelImpl::SetSharedPrePackedWeight(). See the above description for
   *                                   "sharing mode".
   * \param[out] is_packed Output parameter that the implementation sets to true if the kernel packed the tensor data.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional. If not implemented (set to NULL), ORT assumes the kernel
   *       does not pre-pack weight data (i.e., `is_packed` defaults to false).
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(PrePackWeight, _In_ OrtKernelImpl* this_ptr, _In_ const OrtValue* tensor,
                  _In_ int input_index, _Inout_ OrtAllocator* allocator,
                  _In_opt_ OrtSharedPrePackedWeightCache* prepacked_weight_cache, _Out_ bool* is_packed);

  /** \brief Optional function that receives data for a shared pre-packed weight from ORT.
   *
   * ORT calls this function after calling OrtKernelImpl::PrePackWeight for a specific `input_index` if:
   *   - OrtKernelImpl::PrePackWeight set the output parameter `is_packed` to true.
   *   - OrtKernelImpl::PrePackWeight stored weight data to share into the provided OrtSharedPrePackedWeightCache
   *     parameter (`prepacked_weight_cache`) via the API SharedPrePackedWeightCache_StoreWeightData.
   *
   * Refer to the description of the "sharing-mode" in the documentation for OrtKernelImpl::PrePackWeight().
   *
   * \note ORT will not call this function for an `input_index` that a previous call to
   *       OrtKernelImpl::PrePackWeight() did not elect to pre-pack and share.
   *
   * \note This function is based on the internal OpKernel::UseSharedPrePackedBuffers() virtual function used
   *       within ORT.
   *
   * \param[in] this_ptr The OrtKernelImpl instance.
   * \param[in] buffer_data_ptrs An array of buffer data pointers that collectively hold the pre-packed data for a
   *                             single shared weight. The buffers are provided in the same order and with the same
   *                             contents (in a potentially different memory location) as the buffers
   *                             passed into SharedPrePackedWeightCache_StoreWeightData() within the
   *                             OrtKernelImpl::PrePackWeight() call for the same `input_index`.
   * \param[in] buffer_data_sizes An array of buffer byte sizes, one per element in `buffer_data_ptrs`.
   * \param[in] num_buffers The number of buffers used to store the data for the shared pre-packed weight.
   *                        Specifies the number of elements in the `buffer_data_ptrs` and `buffer_data_sizes` arrays.
   * \param[in] input_index The input index of the tensor in this kernel. This index identifies the identity of
   *                        the weight.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is generally optional. It is only required if OrtKernelImpl::PrePack()
   *       elects to share pre-packed weights.
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(SetSharedPrePackedWeight, _In_ OrtKernelImpl* this_ptr,
                  _In_reads_(num_buffers) const void* const* buffer_data_ptrs,
                  _In_reads_(num_buffers) const size_t* buffer_data_sizes,
                  _In_ size_t num_buffers, _In_ int input_index);
};

/** \brief Type definition for a function that creates an OrtKernelImpl instance for an operator kernel.
 *
 * \param[in] kernel_create_func_state Opaque state initially provided by the EP that registered the kernel.
 *                                     Refer to OrtEpApi::KernelRegistry_AddKernel(). May be null.
 * \param[in] info The OrtKernelInfo instance that provides access to the kernel's input and output characteristics.
 * \param[out] kernel_out Output parameter set to the new OrtKernelImpl instance. On success, ownership of this
 *                        OrtKernelImpl instance transfers to ORT, which will call OrtKernelImpl::Release() to
 *                        release the instance when it is no longer used.
 *
 * \snippet{doc} snippets.dox OrtStatus Return Value
 *
 * \since Version 1.24.
 */
typedef OrtStatus*(ORT_API_CALL* OrtKernelCreateFunc)(_In_ void* kernel_create_func_state,
                                                      _In_ const OrtKernelInfo* info,
                                                      _Outptr_result_maybenull_ OrtKernelImpl** kernel_out);

struct OrtLoopKernelHelper;
typedef struct OrtLoopKernelHelper OrtLoopKernelHelper;

/**
 * \brief Contains helper functions for a Loop OrtKernelImpl created via OrtEpApi::CreateLoopKernel.
 * \since Version 1.24.
 */
struct OrtLoopKernelHelper {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION

  /** \brief Called by ORT to release the OrtLoopKernelHelper instance and its resources.
   *
   * \param[in] this_ptr The OrtLoopKernelHelper instance.
   *
   * \since Version 1.24.
   */
  ORT_API_T(void, Release, _In_ OrtLoopKernelHelper* this_ptr);

  /** \brief Helper function that concatenates OrtValue instances from each loop iteration into a single
   *         pre-allocated output buffer.
   *
   * \note Implementing this function is required for all Loop opset versions.
   *
   * \param[in] this_ptr The OrtLoopKernelHelper instance.
   * \param[in] stream_handle Optional native stream handle that enables asynchronous operations. May be NULL.
   * \param[in] per_iteration_outputs Array of OrtValue instances from each iteration. All OrtValue elements have the
   *                                  same shape.
   * \param[in] num_per_iteration_outputs The number of OrtValue* elements in the `per_iteration_outputs` array.
   * \param[out] output The pre-allocated output buffer. Memory is allocated on the device for the EP running the
   *                    Loop node.
   * \param[in] output_size_in_bytes The size in bytes of the `output` buffer. It is guaranteed to be large enough
   *                                 to hold the concatenated data of each element in `per_iteration_outputs`.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(ConcatOutput, _In_ OrtLoopKernelHelper* this_ptr, _In_opt_ void* stream_handle,
                  _In_reads_(num_per_iteration_outputs) const OrtValue* const* per_iteration_outputs,
                  _In_ size_t num_per_iteration_outputs, _Out_writes_bytes_all_(output_size_in_bytes) void* output,
                  _In_ size_t output_size_in_bytes);
};

struct OrtScanKernelHelper;
typedef struct OrtScanKernelHelper OrtScanKernelHelper;

/**
 * \brief Contains helper functions for a Scan OrtKernelImpl created via OrtEpApi::CreateScanKernel.
 * \since Version 1.24.
 */
struct OrtScanKernelHelper {
  uint32_t ort_version_supported;  ///< Must be initialized to ORT_API_VERSION

  /** \brief Called by ORT to release the OrtScanKernelHelper instance and its resources.
   *
   * \param[in] this_ptr The OrtScanKernelHelper instance.
   *
   * \since Version 1.24.
   */
  ORT_API_T(void, Release, _In_ OrtScanKernelHelper* this_ptr);

  /** \brief Helper function that transposes an OrtValue instance during execution of a Scan kernel.
   *
   * \note Called for Scan (opset >= 9) when the 'scan_input_axes' or 'scan_output_axes' attributes contain
   *       non-zero values. Implementing this function is required for Scan opset versions >= 9.
   *
   * \param[in] this_ptr The OrtScanKernelHelper instance.
   * \param[in] permutation An array of integers that defines how the input tensor's axes should be permuted.
   * \param[in] num_permutation_elems The number of integer elements in the `permutation` array.
   * \param[in] input The input OrtValue tensor to transpose.
   * \param[in] stream An optional OrtSyncStream instance to be used for asynchronous operations. May be NULL.
   * \param[out] output The pre-allocated output OrtValue instance into which to store the results of the
   *                    transpose operation. Must not be released as it is owned by ORT.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(Transpose, _In_ OrtScanKernelHelper* this_ptr,
                  _In_reads_(num_permutation_elems) const size_t* permutation, _In_ size_t num_permutation_elems,
                  _In_ const OrtValue* input, _In_opt_ OrtSyncStream* stream, _Inout_ OrtValue* output);
};

/**
 * \brief Discriminator for the resource count type stored in an OrtResourceCount.
 *
 * New resource accounting types can be added by appending new enum values.
 * The OrtResourceCount union storage is large enough to hold all current and future types.
 *
 * \since Version 1.26.
 */
typedef enum OrtResourceCountKind {
  OrtResourceCountKind_None = 0,        ///< Unset / zero-cost sentinel.
  OrtResourceCountKind_TotalBytes = 1,  ///< Single uint64_t: byte count (cost or budget).
} OrtResourceCountKind;

/**
 * \brief ABI-stable tagged union representing a resource cost or budget.
 *
 * This struct is a C-safe variant that can be passed by value across the plugin DLL boundary.
 * The `kind` field selects which member of the `value` union is active. The
 * `value.reserved_words` storage reserves space for future resource types without changing
 * the struct layout.
 *
 * Adding new resource types requires only: (a) a new OrtResourceCountKind enum value,
 * (b) a new union member. No new C API functions are needed.
 *
 * \since Version 1.26.
 */
typedef struct OrtResourceCount {
  uint32_t kind;     /**< OrtResourceCountKind discriminator. */
  uint32_t reserved; /**< Must be zero. Ensures natural alignment for the value union. */

  union {
    uint64_t total_bytes;       /**< Active when kind == OrtResourceCountKind_TotalBytes. */
    uint64_t reserved_words[6]; /**< 48 bytes fixed storage for future resource types. */
  } value;

#ifdef __cplusplus
  /** Default-construct a None (unset) resource count. */
  OrtResourceCount() noexcept : kind{OrtResourceCountKind_None}, reserved{0}, value{} {}

  /** Construct a zero/unset resource count. */
  static OrtResourceCount None() noexcept {
    return OrtResourceCount{};
  }

  /** Construct a resource count representing total bytes. */
  static OrtResourceCount FromTotalBytes(uint64_t bytes) noexcept {
    OrtResourceCount rc{};
    rc.kind = OrtResourceCountKind_TotalBytes;
    rc.value.total_bytes = bytes;
    return rc;
  }

  /** Read the total_bytes value (caller must check kind first). */
  uint64_t AsTotalBytes() const noexcept {
    return value.total_bytes;
  }
#endif
} OrtResourceCount;

#ifdef __cplusplus
static_assert(sizeof(OrtResourceCount) == 56, "OrtResourceCount size must not change to maintain ABI stability");
#endif

/**
 * \brief The OrtEpApi struct provides functions that are relevant to the implementation of an execution provider.
 *
 * \since Version 1.22.
 */
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

  /** \brief Create an OrtHardwareDevice.
   *
   * \note Called within OrtEpFactory::GetSupportedDevices to create a new hardware device (e.g., virtual).
   *
   * \param[in] type The hardware device type.
   * \param[in] vendor_id The hardware device's vendor identifier.
   * \param[in] device_id The hardware device's identifier.
   * \param[in] vendor_name The hardware device's vendor name as a null-terminated string. Copied by ORT.
   * \param[in] metadata Optional OrtKeyValuePairs instance for hardware device metadata that may be queried by
   *                     applications via OrtApi::GetEpDevices().
   *                     Refer to onnxruntime_ep_device_ep_metadata_keys.h for common OrtHardwareDevice metadata keys.
   * \param[out] hardware_device Output parameter set to the new OrtHardwareDevice instance that is created.
   *                             Must be release with ReleaseHardwareDevice().
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(CreateHardwareDevice, _In_ OrtHardwareDeviceType type,
                  _In_ uint32_t vendor_id,
                  _In_ uint32_t device_id,
                  _In_ const char* vendor_name,
                  _In_opt_ const OrtKeyValuePairs* metadata,
                  _Out_ OrtHardwareDevice** hardware_device);

  ORT_CLASS_RELEASE(HardwareDevice);

  /** \brief Creates an empty kernel registry. A kernel registry contains kernel creation information for
   * every operator kernel supported by an EP.
   *
   * \remarks Refer to OrtEp::GetKernelRegistry, which returns an EP's kernel registry to ORT.
   *
   * \param[out] kernel_registry Output parameter set to the new OrtKernelRegistry instance.
   *                             Must be released with OrtEpApi::ReleaseKernelRegistry.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(CreateKernelRegistry, _Outptr_ OrtKernelRegistry** kernel_registry);

  ORT_CLASS_RELEASE(KernelRegistry);

  /** \brief Adds kernel creation information for a supported operator kernel to the given kernel registry.
   *
   * \remarks Refer to OrtEp::GetKernelRegistry, which returns an EP's kernel registry to ORT.
   *
   * \param[in] kernel_registry The OrtKernelRegistry instance.
   * \param[in] kernel_def The kernel definition, which includes operator type, version, EP name, type constraints, etc.
   * \param[in] kernel_create_func Function that creates an instance of the operator kernel as a OrtKernelImpl instance.
   * \param[in] kernel_create_func_state Custom state passed to the kernel creation function. Can be null.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelRegistry_AddKernel, _In_ OrtKernelRegistry* kernel_registry,
                  _In_ const OrtKernelDef* kernel_def, _In_ OrtKernelCreateFunc kernel_create_func,
                  _In_ void* kernel_create_func_state);

  /** \brief Creates a kernel definition builder used to create instances of OrtKernelDef.
   *
   * \param[out] kernel_def_builder_out Output parameter set to the new OrtKernelDefBuilder instance.
   *                                    Must be released with OrtEpApi::ReleaseKernelDefBuilder().
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(CreateKernelDefBuilder, _Outptr_ OrtKernelDefBuilder** kernel_def_builder_out);

  ORT_CLASS_RELEASE(KernelDefBuilder);

  /** \brief Sets the kernel's operator type.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] op_type A null-terminated string representing the operator type.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_SetOperatorType, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_ const char* op_type);

  /** \brief Sets the kernel's domain.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] domain A null-terminated string representing the operator's domain.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_SetDomain, _In_ OrtKernelDefBuilder* kernel_def_builder, _In_ const char* domain);

  /** \brief Sets the kernel's opset version range that is supported.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] since_version_start The starting opset version that is supported.
   * \param[in] since_version_end The ending opset version (inclusive) that is supported.
   *                              Can be set equal to the starting version to indicate that only one
   *                              version is supported.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_SetSinceVersion, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_ int since_version_start, _In_ int since_version_end);

  /** \brief Sets the name of the kernel's intended execution provider.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] ep_name A null-terminated string representing the execution provider's name.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_SetExecutionProvider, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_ const char* ep_name);

  /** \brief Sets the memory type for a kernel input.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] input_index The index of the input.
   * \param[in] mem_type The input's memory type.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_SetInputMemType, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_ size_t input_index, _In_ OrtMemType mem_type);

  /** \brief Sets the memory type for a kernel output.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] output_index The index of the output.
   * \param[in] mem_type The output's memory type.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_SetOutputMemType, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_ size_t output_index, _In_ OrtMemType mem_type);

  /** \brief Adds type constraints for a kernel argument represented as a string (e.g., "T").
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] arg_name A null-terminated string representing the argument to constrain (e.g., "T").
   * \param[in] types Array of OrtDataType instances representing allowed types for the argument.
   *                  Must contain `num_types` elements.
   * \param[in] num_types The number of OrtDataType elements in the `types` array.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_AddTypeConstraint, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_ const char* arg_name, _In_reads_(num_types) const OrtDataType* const* types,
                  _In_ size_t num_types);

  /** \brief Adds aliases for the given input and output pairs.
   *
   * \note Used for operators like Identity and Reshape to allow ORT to reuse the input buffer for the output
   *       without modification.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] input_indices Array of input indices. Array must contain `num_io_indices` elements.
   * \param[in] output_indices Array of output indices. Each output index is aliased with a corresponding
   *                           input index in `input_indices`. Array must contain `num_io_indices` elements.
   * \param[in] num_io_indices The number of input/output index pairs to alias.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_AddInputOutputAliases, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_reads_(num_io_indices) int const* input_indices,
                  _In_reads_(num_io_indices) int const* output_indices,
                  _In_ size_t num_io_indices);

  /** \brief Adds mutable aliases for the given input and output pairs.
   *
   * \note Allows ORT to reuse and *modify* an input buffer (in-place) for the output buffer.
   *       This is also known as "MayInplace" within the ORT codebase.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[in] input_indices Array of input indices. Array must contain `num_io_indices` elements.
   * \param[in] output_indices Array of output indices. Each output index is aliased with a corresponding
   *                           input index in `input_indices`. Array must contain `num_io_indices` elements.
   * \param[in] num_io_indices The number of input/output index pairs to alias.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_AddInputOutputMutableAliases, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _In_reads_(num_io_indices) int const* input_indices,
                  _In_reads_(num_io_indices) int const* output_indices,
                  _In_ size_t num_io_indices);

  /** \brief Creates a OrtKernelDef instance from the given kernel definition builder.
   *
   * \param[in] kernel_def_builder The OrtKernelDefBuilder instance.
   * \param[out] kernel_def_out The new OrtKernelDef instance.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDefBuilder_Build, _In_ OrtKernelDefBuilder* kernel_def_builder,
                  _Outptr_ OrtKernelDef** kernel_def_out);

  ORT_CLASS_RELEASE(KernelDef);

  /** \brief Returns the operator type from the kernel definition.
   *
   * \param[in] kernel_def The OrtKernelDef instance.
   * \return A null-terminated string representing the operator type.
   *
   * \since Version 1.24.
   */
  ORT_API_T(const char*, KernelDef_GetOperatorType, _In_ const OrtKernelDef* kernel_def);

  /** \brief Returns the operator's domain from the kernel definition.
   *
   * \param[in] kernel_def The OrtKernelDef instance.
   * \return A null-terminated string representing the operator's domain.
   *
   * \since Version 1.24.
   */
  ORT_API_T(const char*, KernelDef_GetDomain, _In_ const OrtKernelDef* kernel_def);

  /** \brief Gets the kernel's opset version range that is supported.
   *
   * \param[in] kernel_def The OrtKernelDef instance.
   * \param[out] start_version Output parameter set to the starting opset version that is supported.
   * \param[out] end_version Output parameter set to the ending opset version (inclusive) that is supported.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDef_GetSinceVersion, _In_ const OrtKernelDef* kernel_def,
                  _Out_ int* start_version, _Out_ int* end_version);

  /** \brief Returns the name of the kernel's intended execution provider.
   *
   * \param[in] kernel_def The OrtKernelDef instance.
   * \return A null-terminated string representing the name of the execution provider.
   *
   * \since Version 1.24.
   */
  ORT_API_T(const char*, KernelDef_GetExecutionProvider, _In_ const OrtKernelDef* kernel_def);

  /** \brief Gets the memory type for a kernel input.
   *
   * \param[in] kernel_def The OrtKernelDef instance.
   * \param[in] input_index The index of the input.
   * \param[out] mem_type Output parameter set to the input's memory type.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDef_GetInputMemType, _In_ const OrtKernelDef* kernel_def,
                  _In_ size_t input_index, _Out_ OrtMemType* mem_type);

  /** \brief Gets the memory type for a kernel output.
   *
   * \param[in] kernel_def The OrtKernelDef instance.
   * \param[in] output_index The index of the output.
   * \param[out] mem_type Output parameter set to the output's memory type.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(KernelDef_GetOutputMemType, _In_ const OrtKernelDef* kernel_def,
                  _In_ size_t output_index, _Out_ OrtMemType* mem_type);

  /** \brief Gets the OrtDataType that represents the data type for a tensor of the given element type.
   *
   * \param[in] elem_type The tensor's element type.
   * \param[out] out Output parameter set to the OrtDataType. Owned by ORT and must not be released.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(GetTensorDataType, _In_ ONNXTensorElementDataType elem_type,
                  _Outptr_ const OrtDataType** out);

  /** \brief Gets the kernel definition for a given node, if any exists for the calling execution provider.
   *
   * Used within OrtEp::GetCapability() to get the registered kernel definition for the given node.
   * The kernel definition is set to NULL if there is no registered kernel definition for the node
   * and execution provider.
   *
   * \param[in] graph_support_info The OrtEpGraphSupportInfo instance to query.
   * \param[in] node The node for which to look up a kernel definition.
   * \param[out] out_kernel_def Output parameter set to the OrtKernelDef or NULL.
   *                            Owned by ORT and must not be released.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(EpGraphSupportInfo_LookUpKernel, _In_ OrtEpGraphSupportInfo* graph_support_info,
                  _In_ const OrtNode* node, _Outptr_result_maybenull_ const OrtKernelDef** out_kernel_def);

  /** \brief Sets one or more data buffers that collectively hold the pre-packed data for a single shared weight.
   *
   * \note Used within the implementation of OrtKernelImpl::PrePackWeight() when the kernel wants to share pre-packed
   *       weight data with other kernels. The buffer data MUST be allocated with the OrtAllocator provided to
   *       OrtKernelImpl::PrePack.
   *
   * \note Ownership of weight data transfers to the OrtSharedPrePackedWeightCache instance on success.
   *       If this function returns an error status, the caller retains ownership of the weight data.
   *
   * \note Subsequent calls with the same OrtSharedPrePackedWeightCache instance release and replace the old data.
   *
   * \param[in] prepacked_weight_cache The OrtSharedPrePackedWeightCache instance.
   * \param[in] buffer_data_ptrs An array of buffer data pointers that collectively hold the pre-packed data for a
   *                             single shared weight. Note that sometimes a single weight may have multiple pre-packed
   *                             buffers and it is up to the kernel implementation to determine how to split the data
   *                             into multiple buffers (if desired).
   * \param[in] buffer_data_sizes An array of buffer byte sizes, one per element in `buffer_data_ptrs`.
   * \param[in] num_buffers The number of buffers used to store the data for the shared pre-packed weight.
   *                        Specifies the number of elements in the `buffer_data_ptrs` and `buffer_data_sizes` arrays.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(SharedPrePackedWeightCache_StoreWeightData,
                  _In_ OrtSharedPrePackedWeightCache* prepacked_weight_cache,
                  _In_reads_(num_buffers) void** buffer_data_ptrs, _In_reads_(num_buffers) size_t* buffer_data_sizes,
                  _In_ size_t num_buffers);

  /** \brief Get the OrtEp instance to which the node is assigned from the OrtKernelInfo.
   *
   * \note Used within OrtKernelImpl implementations to obtain a reference to the OrtEp.
   *
   * \param[in] info The ::OrtKernelInfo instance.
   * \param[out] ep Output parameter set to the OrtEp instance associated with the OrtKernelInfo.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   * \since Version 1.24
   */
  ORT_API2_STATUS(KernelInfo_GetEp, _In_ const OrtKernelInfo* info, _Outptr_ const OrtEp** ep);

  /** \brief Set the details of an OrtDeviceEpIncompatibilityDetails instance.
   *
   * Used by execution provider factories to set incompatibility details in their
   * GetHardwareDeviceIncompatibilityDetails implementation. ORT creates and initializes the object
   * before passing it to the EP, so calling this function is optional. The EP uses this function
   * to set incompatibility information when the device is not compatible.
   *
   * \param[in,out] details The OrtDeviceEpIncompatibilityDetails instance to update.
   * \param[in] reasons_bitmask Bitmask of OrtDeviceEpIncompatibilityReason values. (0 = no incompatibility).
   * \param[in] error_code Optional EP-specific error code (0 = no error).
   * \param[in] notes Optional human-readable notes. Can be null.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(DeviceEpIncompatibilityDetails_SetDetails, _Inout_ OrtDeviceEpIncompatibilityDetails* details,
                  _In_ uint32_t reasons_bitmask,
                  _In_ int32_t error_code,
                  _In_opt_z_ const char* notes);

  /** \brief Creates an OrtKernelImpl instance for an If operator.
   *
   * Control flow operators require access to ORT session internals to orchestrate subgraph operations.
   * This function allows an EP to create a properly configured OrtKernelImpl with access to ORT internals that
   * the EP can add to its kernel registry.
   *
   * An EP is required to create an OrtKernelDef that keeps input[0] ('cond') on the CPU (i.e., OrtMemTypeCPUInput)
   * as this input is used by CPU logic. The output should remain on the device (i.e., OrtMemTypeDefault), which is
   * the default setting, to avoid copying to/from CPU.
   *
   * Example kernel definition (CXX API):
   *     Ort::KernelDef kernel_def = Ort::KernelDefBuilder()
   *                                     .SetDomain("").SetOperatorType("If").SetSinceVersion(21, 22)
   *                                     .SetExecutionProvider("MyEp")
   *                                     .SetInputMemType(0, OrtMemTypeCPUInput) // 'cond' on CPU
   *                                     .SetOutputMemType(0, OrtMemTypeDefault) // output on EP device
   *                                     .AddTypeConstraint("B", ...)
   *                                     .AddTypeConstraint("V", ...).Build();
   *
   * \param[in] kernel_info The ::OrtKernelInfo instance for an If node. This function returns error ORT_FAIL
   *                        if the opset version specified by `kernel_info` is unsupported.
   * \param[out] kernel_out Output parameter set to the OrtKernelImpl instance for the If node.
   *                        Must be released via OrtEpApi::ReleaseKernelImpl, unless ownership is transferred
   *                        to ORT (see OrtKernelCreateFunc and OrtEpApi::KernelRegistry_AddKernel).
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   * \since Version 1.24
   */
  ORT_API2_STATUS(CreateIfKernel, _In_ const OrtKernelInfo* kernel_info, _Outptr_ OrtKernelImpl** kernel_out);

  /** \brief Creates an OrtKernelImpl instance for a Loop operator.
   *
   * Control flow operators require access to ORT session internals to orchestrate subgraph operations.
   * This function allows an EP to create a properly configured OrtKernelImpl with access to ORT internals that
   * the EP can add to its kernel registry.
   *
   * An EP is required to create an OrtKernelDef that keeps input[0] ('M') and input[1] ('cond') on the CPU
   * (i.e., OrtMemTypeCPUInput) as these inputs are used by CPU logic. Input[2] ('v_initial') and the output should
   * remain on the device (i.e., OrtMemTypeDefault), which is the default setting, to avoid copying to/from CPU.
   *
   * Example kernel definition (CXX API):
   *     Ort::KernelDef kernel_def = Ort::KernelDefBuilder()
   *                                     .SetDomain("").SetOperatorType("Loop").SetSinceVersion(21, 22)
   *                                     .SetExecutionProvider("MyEp")
   *                                     .SetInputMemType(0, OrtMemTypeCPUInput) // 'M' on CPU
   *                                     .SetInputMemType(1, OrtMemTypeCPUInput) // 'cond' on CPU
   *                                     .SetInputMemType(2, OrtMemTypeDefault) // 'v_initial' on EP device
   *                                     .SetOutputMemType(0, OrtMemTypeDefault) // output on EP device
   *                                     .AddTypeConstraint("I", ...)
   *                                     .AddTypeConstraint("B", ...)
   *                                     .AddTypeConstraint("V", ...).Build();
   *
   * \param[in] kernel_info The ::OrtKernelInfo instance for a Loop node. This function returns error ORT_FAIL
   *                        if the opset version specified by `kernel_info` is unsupported.
   * \param[in] helper A OrtLoopKernelHelper instance that contains helper functions that ORT calls during kernel
   *                   execution to operate on tensors allocated with the EP's device memory.
   *                   ORT will call OrtLoopKernelHelper::Release() to release the helper and its resources.
   * \param[out] kernel_out Output parameter set to the OrtKernelImpl instance for the Loop node.
   *                        Must be released via OrtEpApi::ReleaseKernelImpl, unless ownership is transferred
   *                        to ORT (see OrtKernelCreateFunc and OrtEpApi::KernelRegistry_AddKernel).
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   * \since Version 1.24
   */
  ORT_API2_STATUS(CreateLoopKernel, _In_ const OrtKernelInfo* kernel_info, _In_ OrtLoopKernelHelper* helper,
                  _Outptr_ OrtKernelImpl** kernel_out);

  /** \brief Creates an OrtKernelImpl instance for a Scan operator. Does not support opset versions older than 9.
   *
   * Control flow operators require access to ORT session internals to orchestrate subgraph operations.
   * This function allows an EP to create a properly configured OrtKernelImpl with access to ORT internals that
   * the EP can add to its kernel registry.
   *
   * It is recommended that an EP create an OrtKernelDef that keeps the inputs and outputs on the EP's
   * device (i.e., OrtMemTypeDefault), which is the default setting, to avoid copying to/from CPU.
   *
   * Example kernel definition (CXX API):
   *     Ort::KernelDef kernel_def = Ort::KernelDefBuilder()
   *                                     .SetDomain("").SetOperatorType("Scan").SetSinceVersion(21, 22)
   *                                     .SetExecutionProvider("MyEp")
   *                                     .SetInputMemType(0, OrtMemTypeDefault) // input[0] on EP device
   *                                     .SetOutputMemType(0, OrtMemTypeDefault) // output[0] on EP device
   *                                     .AddTypeConstraint("V", ...).Build();
   *
   * \param[in] kernel_info The ::OrtKernelInfo instance for a Scan node. This function returns error ORT_FAIL
   *                        if the opset version specified by `kernel_info` is unsupported.
   * \param[in] helper A OrtScanKernelHelper instance that contains helper functions that ORT calls during kernel
   *                   execution to operate on tensors allocated with the EP's device memory.
   *                   ORT will call OrtScanKernelHelper::Release() to release the helper and its resources.
   * \param[out] kernel_out Output parameter set to the OrtKernelImpl instance for the Scan node.
   *                        Must be released via OrtEpApi::ReleaseKernelImpl, unless ownership is transferred
   *                        to ORT (see OrtKernelCreateFunc and OrtEpApi::KernelRegistry_AddKernel).
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   * \since Version 1.24
   */
  ORT_API2_STATUS(CreateScanKernel, _In_ const OrtKernelInfo* kernel_info, _In_ OrtScanKernelHelper* helper,
                  _Outptr_ OrtKernelImpl** kernel_out);

  ORT_CLASS_RELEASE(KernelImpl);

  /** \brief Gets a new OrtKeyValuePairs instance containing a copy of all configuration entries set on the environment.
   *
   * \note An application provides environment-level configuration options for execution provider libraries by
   *       using keys with the prefix 'ep_factory.\\<ep_name\\>.'. Ex: the key 'ep_factory.my_ep.some_ep_key' represents
   *       a key named 'some_ep_key' that is meant to be consumed by an execution provider named 'my_ep'. Refer to
   *       the specific execution provider's documentation for valid keys and values.
   *
   * \note Refer to onnxruntime_env_config_keys.h for common configuration entry keys and their supported values.
   *
   * \param[out] config_entries Output parameter set to the OrtKeyValuePairs instance containing all configuration entries.
   *                 Must be released via OrtApi::ReleaseKeyValuePairs.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   * \since Version 1.24
   */
  ORT_API2_STATUS(GetEnvConfigEntries, _Outptr_ OrtKeyValuePairs** config_entries);

  /** \brief Get an operator schema from the global schema registry.
   *
   * Looks up a schema by name, maximum inclusive version, and domain.
   * The returned pointer is owned by the caller and must be released via ReleaseOpSchema.
   * If the schema is not found, *out_schema is set to nullptr (no allocation occurs).
   *
   * Available schemas include standard ONNX operators (domain "" or "ai.onnx"), ONNX ML operators
   * (domain "ai.onnx.ml"), and ORT contrib operators (domain "com.microsoft").
   *
   * \param[in] name A null-terminated string for the operator name.
   * \param[in] max_inclusive_version The maximum inclusive opset version.
   * \param[in] domain A null-terminated string for the operator domain.
   * \param[out] out_schema Output parameter set to the schema pointer, or nullptr if not found.
   *                        Must be released via OrtEpApi::ReleaseOpSchema.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(GetOpSchema, _In_ const char* name, _In_ int max_inclusive_version,
                  _In_ const char* domain, _Outptr_result_maybenull_ OrtOpSchema** out_schema);

  ORT_CLASS_RELEASE(OpSchema);

  /** \brief Get the first ONNX opset version that introduced this operator schema.
   *
   * If an operator has had no changes that break backwards compatibility, the `since_version` is
   * just the first opset version that introduced the operator. However, if the operator has had breaking changes,
   * then `since_version` corresponds to the opset version that introduced the breaking change.
   *
   * For example, suppose operator "Foo" was added in version 3 and had a breaking change in version 6.
   * Then, there will be an operator schema entry for "Foo" with a since_version of 3 and another updated
   * operator schema entry for "Foo" with a since_version of 6.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[out] out Output parameter set to the ONNX opset version.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetSinceVersion, _In_ const OrtOpSchema* schema, _Out_ int* out);

  /** \brief Get the number of inputs defined by the operator schema.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[out] out Output parameter set to the number of inputs.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetNumInputs, _In_ const OrtOpSchema* schema, _Out_ size_t* out);

  /** \brief Get the name of the i-th input formal parameter from an operator schema.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[in] index Zero-based index of the input parameter.
   * \param[out] out Output parameter set to the name of the input parameter (null-terminated UTF8 string).
   *                 Valid as long as the OrtOpSchema exists.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetInputName, _In_ const OrtOpSchema* schema, _In_ size_t index,
                  _Outptr_ const char** out);

  /** \brief Get the type constraint for the i-th input formal parameter from an operator schema.
   *
   * Returns a non-owning pointer to the OrtOpSchemaTypeConstraint associated with the given input.
   * The returned pointer is valid as long as the parent OrtOpSchema is alive.
   * If the input has no type constraint, *out is set to nullptr.
   *
   * Multiple inputs sharing the same type constraint (e.g., both using "T") return the same pointer.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[in] index Zero-based index of the input parameter.
   * \param[out] out Output parameter set to the type constraint, or NULL if the input has no type constraint.
   *                 Valid as long as the OrtOpSchema exists.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetInputTypeConstraint, _In_ const OrtOpSchema* schema, _In_ size_t index,
                  _Outptr_result_maybenull_ const OrtOpSchemaTypeConstraint** out);

  /** \brief Get the number of outputs defined by the operator schema.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[out] out Output parameter set to the number of outputs.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetNumOutputs, _In_ const OrtOpSchema* schema, _Out_ size_t* out);

  /** \brief Get the name of the i-th output formal parameter from an operator schema.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[in] index Zero-based index of the output parameter.
   * \param[out] out Output parameter set to the name of the output parameter (null-terminated UTF8 string).
   *                 Valid as long as the OrtOpSchema exists.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetOutputName, _In_ const OrtOpSchema* schema, _In_ size_t index,
                  _Outptr_ const char** out);

  /** \brief Get the type constraint for the i-th output formal parameter from an operator schema.
   *
   * Returns a non-owning pointer to the OrtOpSchemaTypeConstraint associated with the given output.
   * The returned pointer is valid as long as the parent OrtOpSchema is alive.
   * If the output has no type constraint, *out is set to nullptr.
   *
   * Multiple outputs sharing the same type constraint return the same pointer.
   * Pointer equality can be used to check if two outputs share a type constraint.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[in] index Zero-based index of the output parameter.
   * \param[out] out Output parameter set to the type constraint, or NULL if the output has no type constraint.
   *                 Valid as long as the OrtOpSchema exists.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetOutputTypeConstraint, _In_ const OrtOpSchema* schema, _In_ size_t index,
                  _Outptr_result_maybenull_ const OrtOpSchemaTypeConstraint** out);

  /** \brief Get the number of unique type constraints in the operator schema.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[out] out Output set to the number of type constraints.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetTypeConstraintCount, _In_ const OrtOpSchema* schema, _Out_ size_t* out);

  /** \brief Get the i-th type constraint from the operator schema.
   *
   * Returns a non-owning pointer to the OrtOpSchemaTypeConstraint at the given index.
   * The returned pointer is valid as long as the parent OrtOpSchema is alive.
   *
   * Constraints are returned in the order they are declared in the ONNX operator schema
   * definition. The order is stable but has no semantic significance.
   *
   * Use this API to iterate all type constraints (e.g., to register allowed types for
   * each constraint). Use OpSchema_GetInputTypeConstraint / OpSchema_GetOutputTypeConstraint
   * to look up the constraint for a specific input or output.
   *
   * \param[in] schema The OrtOpSchema instance.
   * \param[in] index Zero-based index of the type constraint.
   * \param[out] out Output parameter set to the type constraint.
   *                 Valid as long as the OrtOpSchema exists.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchema_GetTypeConstraint, _In_ const OrtOpSchema* schema, _In_ size_t index,
                  _Outptr_ const OrtOpSchemaTypeConstraint** out);

  /** \brief Get the type parameter name of a type constraint (e.g., "T", "T1").
   *
   * \param[in] type_constraint The OrtOpSchemaTypeConstraint instance.
   * \param[out] out Output parameter set to the type parameter name.
   *                 Valid as long as the parent OrtOpSchema exists.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchemaTypeConstraint_GetTypeParamName, _In_ const OrtOpSchemaTypeConstraint* type_constraint,
                  _Outptr_ const char** out);

  /** \brief Get the allowed type strings for a type constraint.
   *
   * Returns an array of null-terminated strings representing the allowed data types
   * (e.g., "tensor(float)", "tensor(double)"). The array and its contents are valid
   * as long as the parent OrtOpSchema exists.
   *
   * \param[in] type_constraint The OrtOpSchemaTypeConstraint instance.
   * \param[out] out_types Output parameter set to the output array of type strings.
   *                       Valid as long as the parent OrtOpSchema exists.
   * \param[out] num_types Output parameter set to the number of elements in the output array.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchemaTypeConstraint_GetAllowedTypes, _In_ const OrtOpSchemaTypeConstraint* type_constraint,
                  _Outptr_ const char* const** out_types, _Out_ size_t* num_types);

  /** \brief Get the input indices that use a type constraint.
   *
   * Returns an array of zero-based input indices whose formal parameter type string
   * matches this type constraint. The array is valid as long as the parent OrtOpSchema exists.
   *
   * \param[in] type_constraint The OrtOpSchemaTypeConstraint instance.
   * \param[out] out_indices Output parameter set to the output array of input indices.
   * \param[out] count Output parameter set to the number of elements in the output array.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchemaTypeConstraint_GetInputIndices, _In_ const OrtOpSchemaTypeConstraint* type_constraint,
                  _Outptr_ const size_t** out_indices, _Out_ size_t* count);

  /** \brief Get the output indices that use a type constraint.
   *
   * Returns an array of zero-based output indices whose formal parameter type string
   * matches this type constraint. The array is valid as long as the parent OrtOpSchema exists.
   *
   * \param[in] type_constraint The OrtOpSchemaTypeConstraint instance.
   * \param[out] out_indices Output parameter set to the output array of output indices.
   * \param[out] count Output parameter set to the number of elements in the output array.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(OpSchemaTypeConstraint_GetOutputIndices, _In_ const OrtOpSchemaTypeConstraint* type_constraint,
                  _Outptr_ const size_t** out_indices, _Out_ size_t* count);

  /** \brief Create a profiling event.
   *
   * An EP profiler calls this to create an event to pass to OrtEpApi::ProfilingEventsContainer_AddEvents.
   * The returned event must be released via OrtEpApi::ReleaseProfilingEvent after it has been added.
   *
   * \param[in] category The event category (e.g., session, node, kernel, or API).
   * \param[in] process_id Process ID. Set to -1 if does not apply.
   * \param[in] thread_id Thread ID. Set to -1 if does not apply.
   * \param[in] event_name Null-terminated string representing the event name. ORT copies this string.
   * \param[in] timestamp_us Starting timestamp in microseconds relative to the profiling start time.
   *                         An OrtEpProfilerImpl should record its own clock's profiling start time and
   *                         use the `ep_profiling_start_offset_ns` value passed to OrtEpProfilerImpl::StartProfiling
   *                         to compute this value as:
   *                           timestamp_us = (ep_profiling_start_offset_ns +
   *                                           (ep_event_time_ns - ep_profiling_start_time_ns)) / 1000
   * \param[in] duration_us Duration in microseconds.
   * \param[in] arg_keys Array of null-terminated argument key strings. Can be NULL if num_args is 0.
   *                     ORT copies these strings.
   * \param[in] arg_values Array of null-terminated argument value strings. Can be NULL if num_args is 0.
   *                       ORT copies these strings.
   * \param[in] num_args Number of key-value argument pairs.
   * \param[out] out Output parameter set to the created profiling event.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(CreateProfilingEvent,
                  _In_ OrtProfilingEventCategory category,
                  _In_ int32_t process_id,
                  _In_ int32_t thread_id,
                  _In_ const char* event_name,
                  _In_ int64_t timestamp_us,
                  _In_ int64_t duration_us,
                  _In_reads_(num_args) const char* const* arg_keys,
                  _In_reads_(num_args) const char* const* arg_values,
                  _In_ size_t num_args,
                  _Outptr_ OrtProfilingEvent** out);

  /** \brief Release an opaque profiling event created via CreateProfilingEvent.
   *
   * \since Version 1.25.
   */
  ORT_CLASS_RELEASE(ProfilingEvent);

  /** \brief Get the event category of a profiling event.
   *
   * \param[in] event The OrtProfilingEvent instance.
   * \param[out] out Output parameter set to the event category.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(ProfilingEvent_GetCategory, _In_ const OrtProfilingEvent* event,
                  _Out_ OrtProfilingEventCategory* out);

  /** \brief Get the event name of a profiling event.
   *
   * \param[in] event The OrtProfilingEvent instance.
   * \param[out] out Output parameter set to the event name as a null-terminated UTF-8 string.
   *                 Do not free as it is owned by the OrtProfilingEvent instance.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(ProfilingEvent_GetName, _In_ const OrtProfilingEvent* event,
                  _Outptr_ const char** out);

  /** \brief Get the start timestamp of a profiling event in microseconds.
   *
   * \param[in] event The OrtProfilingEvent instance.
   * \param[out] out Output parameter set to the start timestamp of the profiling event in microseconds relative to
   *                 the profiling start time.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(ProfilingEvent_GetTimestampUs, _In_ const OrtProfilingEvent* event,
                  _Out_ int64_t* out);

  /** \brief Get the duration of a profiling event in microseconds.
   *
   * \param[in] event The OrtProfilingEvent instance.
   * \param[out] out Output parameter set to the event duration in microseconds.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(ProfilingEvent_GetDurationUs, _In_ const OrtProfilingEvent* event,
                  _Out_ int64_t* out);

  /** \brief Get the value of an event argument by its key.
   *
   * The value is set to NULL if the key is not found.
   *
   * \param[in] event The OrtProfilingEvent instance.
   * \param[in] key Null-terminated argument key to look up.
   * \param[out] out Output parameter set to the argument value string, or NULL if not found.
   *                 The value is a null-terminated UTF-8 string.
   *                 Do not free as the string is owned by the OrtProfilingEvent instance.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   * \since Version 1.25.
   */
  ORT_API2_STATUS(ProfilingEvent_GetArgValue, _In_ const OrtProfilingEvent* event, _In_ const char* key,
                  _Outptr_result_maybenull_ const char** out);

  /** \brief Add EP profiling events to an events container.
   *
   * An EP profiler calls this function to report new EP profiling events (e.g., GPU kernel timings) during
   * OrtEpProfilerImpl::EndProfiling(). ORT copies the EP event data during this call. The EP retains ownership of the
   * OrtProfilingEvent instances and must release them via ReleaseProfilingEvent after this call returns.
   * This function may be called multiple times within a single EndProfiling call to add EP events in batches.
   *
   * \param[in] events_container The OrtProfilingEventsContainer instance provided by ORT
   *                             to OrtEpProfilerImpl::EndProfiling().
   * \param[in] events Array of pointers to opaque OrtProfilingEvent instances.
   * \param[in] num_events Number of events in the `events` array. Must be greater than 0.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(ProfilingEventsContainer_AddEvents, _In_ OrtProfilingEventsContainer* events_container,
                  _In_reads_(num_events) const OrtProfilingEvent* const* events,
                  _In_ size_t num_events);
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
 * \brief Node assignment policies for graph capture validation.
 *
 * When graph capture is enabled, ORT validates that nodes are assigned to EPs in a way that is
 * compatible with graph capture. An EP can specify which validation policy ORT should apply.
 *
 * \since Version 1.26.
 */
typedef enum OrtGraphCaptureNodeAssignmentPolicy {
  /** All nodes in the main graph must be assigned to this EP. No CPU fallback is allowed. */
  OrtGraphCaptureNodeAssignmentPolicy_ALL_NODES_ON_EP = 0,

  /** Compute nodes must be on this EP. CPU nodes are allowed for shape computation as long as
   *  no memory copy nodes exist. */
  OrtGraphCaptureNodeAssignmentPolicy_ALLOW_CPU_FOR_SHAPES = 1,
} OrtGraphCaptureNodeAssignmentPolicy;

/**
 * \brief The OrtEp struct provides functions to implement for an execution provider.
 * \since Version 1.22.
 */
struct OrtEp {
  /** \brief The ONNX Runtime API version the execution provider was compiled with.
   *
   * Implementation should set this to ORT_API_VERSION.
   * ORT uses this to avoid calling functions that were not available when the EP was compiled.
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
   * \note As of version 1.24, implementation of this function is optional if the EP does not compile nodes and
   *       uses a kernel registry instead.
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
   * \note As of version 1.24, implementation of this function is optional if the EP does not compile nodes and
   *       uses a kernel registry instead.
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
   *       When graph capture/replay is enabled and a graph has already been captured, ORT skips
   *       normal execution and calls ReplayGraph() directly, so this callback is not invoked for replay runs.
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
   *       When graph capture/replay is enabled and a graph has already been captured, ORT skips
   *       normal execution and calls ReplayGraph() directly, so this callback is not invoked for replay runs.
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

  /** \brief Gets the execution provider's kernel registry, if any.
   *
   * A kernel registry contains kernel creation information for operator kernels supported by an EP.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[out] kernel_registry Output parameter set to the EP's kernel registry, which must remain valid throughout
   *                             the lifetime of the EP. Can be NULL if the EP doesn't use a kernel registry.
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional. If set to NULL, ORT assumes the EP compiles nodes.
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(GetKernelRegistry, _In_ OrtEp* this_ptr,
                  _Outptr_result_maybenull_ const OrtKernelRegistry** kernel_registry);

  /** \brief Gets whether the execution provider supports concurrent run calls made on the session.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[out] is_supported Whether concurrent runs are supported.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional and it may be set to NULL.
   *       If not implemented, ORT assumes that concurrent runs are supported.
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(IsConcurrentRunSupported, _In_ OrtEp* this_ptr, _Outptr_ bool* is_supported);

  /** \brief Called by ORT to block until the device has completed all preceding requested tasks.
   *
   * Currently this is primarily used by the IOBinding object to ensure that all inputs have been copied
   * to the device before execution begins.
   *
   * \param[in] this_ptr The OrtEp instance.
   *
   * \note Implementation of this function is optional.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(Sync, _In_ OrtEp* this_ptr);

  /** \brief Return a new profiler for the execution provider.
   *
   * If the EP supports profiling, it should create and return an OrtEpProfilerImpl instance.
   * ORT takes ownership of each non-NULL instance returned and will call OrtEpProfilerImpl::Release when
   * it is no longer needed.
   *
   * ORT may call this function multiple times over the lifetime of a single OrtEp instance, for example
   * during EP registration and again per run if run-level profiling is enabled. Each call is independent and
   * the EP must return a new profiler instance (or NULL if profiling is not supported).
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[out] profiler Output parameter set to a new OrtEpProfilerImpl instance created by the EP.
   *                      Set to NULL if the EP does not support profiling.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional. If set to NULL, ORT assumes the EP does not
   *       support profiling.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(CreateProfiler, _In_ OrtEp* this_ptr,
                  _Outptr_result_maybenull_ OrtEpProfilerImpl** profiler);

  /** \brief Indicate whether the graph capturing mode (e.g., CUDA graph) is enabled for the provider.
   *
   * Graph capture allows an EP to record a sequence of device (e.g., GPU) operations during an initial run and replay
   * them on subsequent runs, bypassing per-kernel CPU launch overhead.
   *
   * Applications enable graph capture via EP-specific provider options (e.g., `enable_cuda_graph=1`
   * for the CUDA EP). An EP should return true from this function if it has been configured to enable
   * graph capture/replay.
   *
   * **ORT graph capture/replay summary:**
   * During OrtSession initialization, ORT calls OrtEp::IsGraphCaptureEnabled() on each EP in the order specified during
   * provider registration with the session. If an EP returns true, ORT validates that the graph is suitable for
   * graph capture, and if so, caches the EP for graph capture during the next run. The graph validation ensures
   * that there are no control flow nodes and that node-to-EP assignments are compatible with the policy specified
   * by the EP via OrtEp::GetGraphCaptureNodeAssignmentPolicy().
   * Note that an OrtSession only supports graph capture for one EP (i.e., the first EP to claim support).
   *
   * During the first call to OrtApi::Run() for the OrtSession, ORT performs multiple internal runs of the model
   * until the EP indicates that the graph has been captured by returning `true` from `OrtEp::IsGraphCaptured()`.
   * If the EP is unable to capture the graph within 8 runs, the call to OrtApi::Run() returns an error OrtStatus.
   * Each internal run invokes `OrtEp::OnRunStart()`, normal execution, and `OrtEp::OnRunEnd()`. EPs should use
   * these run callbacks to track the number of necessary warm-up runs and begin/end graph capture when ready.
   *
   * After successful graph capture, subsequent calls to OrtApi::Run() skip normal execution and ORT instead calls
   * `OrtEp::ReplayGraph()` directly.
   *
   * Applications can capture and replay multiple graphs (e.g., one per distinct input shape) by setting the
   * `"gpu_graph_id"` run config entry via `OrtApi::AddRunConfigEntry()` to different integer values. ORT passes
   * the value as the `graph_annotation_id` parameter to `OrtEp::IsGraphCaptured()` and `OrtEp::ReplayGraph()`.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \return true if graph capture mode is enabled, false otherwise.
   *
   * \note Implementation of this function is optional. If set to NULL, ORT assumes graph capture is not enabled.
   * \note If this function returns true, `OrtEp::IsGraphCaptured` and `OrtEp::ReplayGraph` must also be implemented.
   *       If either is NULL, ORT will log a warning and ignore this EP for graph capture.
   *
   * \since Version 1.26.
   */
  ORT_API_T(bool, IsGraphCaptureEnabled, _In_ const OrtEp* this_ptr);

  /** \brief Indicate whether a graph has been captured and instantiated.
   *
   * ORT calls this before each `Session::Run()`. If true, ORT calls `ReplayGraph()` instead of
   * normal execution. After a run where this returns false, ORT automatically retries until it
   * returns true (handling warm-up runs transparently).
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] graph_annotation_id Identifies which captured graph to query.
   *            Applications can set this value via `OrtApi::AddRunConfigEntry()` with the key `"gpu_graph_id"`.
   *            The default value is 0 when the run config entry is not set.
   *            Setting different IDs allows the EP to capture and manage multiple graphs (e.g., one per
   *            distinct input shape). A value of -1 means graph capture/replay should be skipped for this run.
   * \return true if the graph has been captured, false otherwise.
   *
   * \note This function must be implemented if `OrtEp::IsGraphCaptureEnabled` is implemented and may return true.
   *
   * \since Version 1.26.
   */
  ORT_API_T(bool, IsGraphCaptured, _In_ const OrtEp* this_ptr, _In_ int graph_annotation_id);

  /** \brief Run the instantiated (captured) graph.
   *
   * Called by ORT instead of normal execution when `IsGraphCaptured()` returns true.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[in] graph_annotation_id Identifies which captured graph to replay.
   *            Applications can set this value via `OrtApi::AddRunConfigEntry()` with the key `"gpu_graph_id"`.
   *            The default value is 0 when the run config entry is not set.
   *            A value of -1 means graph replay should be skipped for this run.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note This function must be implemented if `OrtEp::IsGraphCaptureEnabled` is implemented and may return true.
   *
   * \since Version 1.26.
   */
  ORT_API2_STATUS(ReplayGraph, _In_ OrtEp* this_ptr, _In_ int graph_annotation_id);

  /** \brief Get the node assignment validation policy for graph capture.
   *
   * When graph capture is enabled, ORT validates that nodes are assigned to EPs in a way that is
   * compatible with graph capture. This function tells ORT which validation policy to apply.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \return The node assignment policy for graph capture.
   *
   * \note Implementation of this function is optional. If set to NULL, ORT uses
   *       OrtGraphCaptureNodeAssignmentPolicy_ALL_NODES_ON_EP (strictest validation).
   *
   * \since Version 1.26.
   */
  ORT_API_T(OrtGraphCaptureNodeAssignmentPolicy, GetGraphCaptureNodeAssignmentPolicy,
            _In_ const OrtEp* this_ptr);

  /** \brief Query the available device resource for partitioning budget.
   *
   * Called by ORT during graph partitioning when no explicit resource budget threshold
   * has been configured via session options. The EP should query its device for the
   * currently available resource (e.g., free GPU memory) and return it as an OrtResourceCount.
   *
   * If the EP does not support resource querying, set this function pointer to NULL.
   * ORT will skip threshold-based budget enforcement in that case.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[out] available The available device resource.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional. If set to NULL, no automatic
   *       resource threshold is established and budget enforcement requires an explicit
   *       threshold from session options.
   *
   * \since Version 1.26.
   */
  ORT_API2_STATUS(GetAvailableResource, _In_ const OrtEp* this_ptr, _Out_ OrtResourceCount* available);

  /** \brief Called by ORT when session initialization is complete.
   *
   * This provides an opportunity for execution providers to optionally synchronize and
   * clean up temporary resources to reduce memory usage and ensure the first inference run is fast.
   *
   * \param[in] this_ptr The OrtEp instance.
   *
   * \note Implementation of this function is optional. If set to NULL, ORT assumes no
   *       post-initialization work is needed and treats it as a no-op success.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.27.
   */
  ORT_API2_STATUS(OnSessionInitializationEnd, _In_ OrtEp* this_ptr);

  /** \brief Get the EP's default memory device.
   *
   * The EP's default memory device identifies the hardware the EP operates on. ORT uses it to:
   * - Determine if data copies are needed between EPs (inserting memcpy nodes at EP boundaries)
   * - Determine if the EP is CPU-based (which affects synchronization and data transfer decisions)
   * - Bind execution streams to the correct device
   *
   * If the implementation allows an EP to be created with multiple EpDevices this should return the OrtMemoryDevice
   * that ORT should consider as default for this EP instance.
   *
   * An OrtMemoryDevice is obtained from an OrtMemoryInfo via `OrtEpApi::MemoryInfo_GetMemoryDevice()`.
   * Typically, an EP creates OrtMemoryInfo instances and registers them with its OrtEpDevice(s) via
   * `OrtEpApi::EpDevice_AddAllocatorInfo()`. The OrtMemoryDevice returned here must correspond to an
   * OrtMemoryInfo registered as an `OrtDeviceAllocator` entry (either `OrtDeviceMemoryType_DEFAULT` or
   * `OrtDeviceMemoryType_HOST_ACCESSIBLE`). An OrtMemoryDevice from an `OrtReadOnlyAllocator` entry is
   * not accepted as the EP's default/identity device.
   *
   * The returned pointer must remain valid for the lifetime of the OrtEp instance
   * (typically by storing the parent OrtMemoryInfo as a member of the EP).
   *
   * If this function is not implemented (NULL), or if it sets `device` to NULL, ORT infers
   * the default memory device from the first OrtEpDevice's `OrtDeviceAllocator` entry with
   * `OrtDeviceMemoryType_DEFAULT` registered via `EpDevice_AddAllocatorInfo`. EPs created against
   * multiple OrtEpDevices whose default memory devices differ should implement this function to
   * disambiguate; otherwise the first OrtEpDevice's default memory device is used and the others
   * are ignored for identity purposes. If no such allocator entry is registered, the EP defaults
   * to a CPU memory device.
   *
   * \param[in] this_ptr The OrtEp instance.
   * \param[out] device Set to the EP's default OrtMemoryDevice, or NULL to use the default behavior (described above).
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional. If set to NULL (not implemented), ORT
   *       infers the default memory device using the default behavior described above.
   *
   * \since Version 1.27.
   */
  ORT_API2_STATUS(GetDefaultMemoryDevice, _In_ const OrtEp* this_ptr,
                  _Outptr_result_maybenull_ const OrtMemoryDevice** device);

  /** \brief Release a previously captured graph and its associated resources.
   *
   * Called when the caller no longer needs the captured graph for the given annotation ID.
   * This allows the EP to free buffers and other resources tied to this graph.
   *
   * \param[in] this_ptr The EP instance.
   * \param[in] graph_annotation_id The annotation ID of the graph to release.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional. If set to NULL, ORT assumes
   *       no captured graph release is needed and treats it as a no-op success.
   *
   * \note Thread safety: For EPs that support concurrent Run() calls, this method may be
   *       called concurrently with Run(). The EP is responsible for ensuring thread safety
   *       of its own state in that case. For non-concurrent EPs, the session serializes
   *       calls via its internal mutex.
   *
   * \since Version 1.27.
   */
  ORT_API2_STATUS(ReleaseCapturedGraph, _In_ OrtEp* this_ptr, _In_ int graph_annotation_id);
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

  /** \brief Check for known incompatibility reasons between a hardware device and this execution provider.
   *
   * This function allows an execution provider to check if a specific hardware device is compatible
   * with the execution provider. The EP can set specific incompatibility reasons via the
   * OrtDeviceEpIncompatibilityDetails parameter using OrtEpApi::DeviceEpIncompatibilityDetails_SetDetails.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] hw The hardware device to check for incompatibility.
   * \param[in,out] details Pre-allocated incompatibility details object created and initialized by ORT.
   *                        The EP can use OrtEpApi::DeviceEpIncompatibilityDetails_SetDetails to set
   *                        incompatibility information. If the device is compatible, the EP can
   *                        leave the object unchanged (it defaults to no incompatibility).
   *
   * \note Implementation of this function is optional.
   *       If not implemented, ORT will assume the device is compatible with this EP.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(GetHardwareDeviceIncompatibilityDetails, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtHardwareDevice* hw,
                  _Inout_ OrtDeviceEpIncompatibilityDetails* details);

  /** \brief Create an OrtExternalResourceImporterImpl for external resource import.
   *
   * This is used to create an external resource importer that enables zero-copy import of
   * external GPU memory (e.g., D3D12 shared resources) and synchronization primitives
   * (e.g., D3D12 timeline fences).
   *
   * EPs that support external resource import (via CUDA, HIP, Vulkan, or D3D12 APIs) can
   * implement this to allow applications to share GPU resources without copies.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] ep_device The OrtEpDevice to create the external resource importer for.
   * \param[out] out_importer The created OrtExternalResourceImporterImpl instance.
   *                          Set to nullptr if external resource import is not supported.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional.
   *       An EP factory should only implement this if it supports external resource import.
   *       If not implemented or not supported, return ORT_NOT_IMPLEMENTED or set out_importer to nullptr.
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(CreateExternalResourceImporterForDevice, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtEpDevice* ep_device,
                  _Outptr_result_maybenull_ OrtExternalResourceImporterImpl** out_importer);

  /** \brief Returns the number of OrtCustomOpDomains that this factory provides.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[out] num_domains Output parameter set to the number of provided OrtCustomOpDomain instances.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(GetNumCustomOpDomains, _In_ OrtEpFactory* this_ptr, _Out_ size_t* num_domains);

  /** \brief Gets the EP-specific OrtCustomOpDomains.
   *
   * This function is used when running inference on a model that contains EP-specific custom operations.
   *
   * Workflow:
   * 1. The EP factory implements this function to supply a list of OrtCustomOpDomain instances.
   * 2. The application either 1) calls SessionOptionsAppendExecutionProvider_V2() with an OrtEpDevice containing
   *    the plugin EP's factory or 2) enables auto ep selection.
   * 3. 1) SessionOptionsAppendExecutionProvider_V2() appends the provided OrtCustomOpDomains to the
   *    session options or 2) ORT registers the OrtCustomOpDomains provided by the EP devices
   *    that could be potentially selected.
   *
   * As a result, any session created from these session options will have these custom op domains registered
   * in ORT, ensuring that the custom ops are properly recognized and validated when the model is loaded.
   *
   * Plugin EPs can provide two types of custom ops:
   *  1. A full OrtCustomOp with a concrete kernel implementation
   *    - A Plugin EP can supply an OrtCustomOp and a corresponding CustomKernel::Compute() implementation.
   *    - In GetCapability(), it calls EpGraphSupportInfo_AddSingleNode() to inform ORT
   *      that the custom node should NOT be fused or compiled. Instead, ORT should invoke
   *      the custom node's Compute() function at runtime.
   *
   *  2. A "placeholder" OrtCustomOp with an empty kernel implementation
   *    - A compile-based Plugin EP can supply an OrtCustomOp whose CustomKernel::Compute()
   *      does nothing. The purpose is to satisfy model validation during model loading by
   *      registering the custom op as a valid operator in the session.
   *    - In GetCapability(), the EP should call EpGraphSupportInfo_AddNodesToFuse() to
   *      notify ORT that this custom node should be fused and compiled by the EP.
   *    - In Compile(), the EP executes its compiled bits to perform inference for
   *      the fused custom node.
   *
   * Note: The OrtCustomOpDomain instances must be valid while any session is using them.
           EP factory has the responsibility to release OrtCustomOpDomain instances it creates. It happens
   *       automatically if using the C++ Ort::CustomOpDomain class.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[out] domains Array of `num_domains` elements pre-allocated by ORT that should be filled with
                         OrtCustomOpDomain instances created by the EP. The `num_domains` is the value returned by
                         GetNumCustomOpDomains().
   * \param[in] num_domains The size of the `domains` array pre-allocated by ORT.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.24.
   */
  ORT_API2_STATUS(GetCustomOpDomains, _In_ OrtEpFactory* this_ptr,
                  _Out_writes_all_(num_domains) OrtCustomOpDomain** domains, _In_ size_t num_domains);

  /** \brief Initialize graphics interop for the EP factory.
   *
   * This function sets up graphics interop context that enables synchronization between
   * external graphics API workloads (D3D12, Vulkan) and ONNX Runtime inference.
   *
   * The factory stores the graphics context configuration and uses it when creating
   * synchronization streams via CreateSyncStreamForDevice. This approach
   * is more graceful than passing the command queue directly during stream creation.
   *
   * The implementation is EP-specific. EPs may create a specialized interop context using
   * platform-specific APIs to enable GPU-GPU synchronization.
   *
   * Key design points:
   * - Single init function with all required params (avoids multiple init signatures)
   * - Factory stores context and uses it in stream creation
   * - Paired with DeinitGraphicsInterop for cleanup
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] ep_device The OrtEpDevice to initialize graphics interop for.
   * \param[in] config Configuration specifying the graphics API and required handles.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional.
   *       EPs that don't support graphics interop should set this to nullptr or return ORT_NOT_IMPLEMENTED.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(InitGraphicsInterop, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtEpDevice* ep_device,
                  _In_ const OrtGraphicsInteropConfig* config);

  /** \brief Deinitialize graphics interop for the EP factory.
   *
   * This function cleans up any graphics interop context that was set up by InitGraphicsInterop.
   * Should be called when graphics interop is no longer needed.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] ep_device The OrtEpDevice to deinitialize graphics interop for.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \note Implementation of this function is optional.
   *       EPs that don't support graphics interop should set this to nullptr or return ORT_NOT_IMPLEMENTED.
   *
   * \since Version 1.25.
   */
  ORT_API2_STATUS(DeinitGraphicsInterop, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtEpDevice* ep_device);

  /** \brief Select the best model variant candidate from metadata.
   *
   * Evaluates each candidate's metadata against the given hardware device and optional session options,
   * and returns the index of the best match.
   *
   * Each candidate is an OrtKeyValuePairs representing one model variant. The KVP uses indexed keys
   * so that the EP can inspect each model's metadata independently. A variant always has num_models >= 1.
   *
   * Required and optional keys:
   *   - "num_models"                 — number of models in this variant (>= 1) (required)
   *   - "<i>.ep_compatibility_info"  — compatibility string for model i (required per model)
   *   - "<i>.role"                   — role/purpose of model i (e.g., "prefill", "decode") (optional)
   *   - "<i>.future_meaningful_info" — additional EP-meaningful metadata for model i (optional)
   *
   * where <i> is a zero-based index (e.g., "0.ep_compatibility_info", "1.ep_compatibility_info").
   *
   * The implementer should loop from 0 to num_models - 1 and validate each "<i>.ep_compatibility_info" entry.
   * An advanced implementation may additionally consider "role" or other metadata when ranking candidates.
   *
   * **Why this function exists:**
   *
   * The existing ValidateCompiledModelCompatibilityInfo() alone is not sufficient for some EPs to determine the best
   * compatible model when there are multiple candidates. For example, an EP may support multiple compilation modes
   * (e.g., "speed optimized" vs "memory optimized") that produce different compatibility strings. The EP can implement
   * this function to evaluate the candidate metadata and select the best compatible variant based on its own criteria,
   * the target device, and the session options.
   *
   * If all candidates are unsupported, this function succeeds and sets `selected_index` to SIZE_MAX.
   *
   * \note The implementer should validate each "<i>.ep_compatibility_info" in the candidate (e.g., by calling
   * ValidateCompiledModelCompatibilityInfo for each one) before determining the best match.
   *
   * \param[in] this_ptr The OrtEpFactory instance.
   * \param[in] device The target hardware device that the EP would run on. Must map to this EP.
   * \param[in] candidates Array of OrtKeyValuePairs pointers (one per model variant).
   * \param[in] num_candidates Number of candidates (i.e., number of model variants to evaluate).
   * \param[in] session_options Optional session options to consider when selecting the best candidate.
   *                            May be nullptr if no session-level preferences are relevant.
   * \param[out] selected_index Selected candidate index, or SIZE_MAX if all unsupported.
   *
   * \snippet{doc} snippets.dox OrtStatus Return Value
   *
   * \since Version 1.28.
   */
  ORT_API2_STATUS(SelectBestModelCandidate, _In_ OrtEpFactory* this_ptr,
                  _In_ const OrtHardwareDevice* device,
                  _In_reads_(num_candidates) const OrtKeyValuePairs* const* candidates,
                  _In_ size_t num_candidates,
                  _In_opt_ const OrtSessionOptions* session_options,
                  _Out_ size_t* selected_index);
};

#ifdef __cplusplus
}
#endif
