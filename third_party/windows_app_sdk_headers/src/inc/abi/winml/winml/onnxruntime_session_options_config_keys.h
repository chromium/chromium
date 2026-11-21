// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

/*
 * This file defines SessionOptions Config Keys and format of the Config Values.
 *
 * The Naming Convention for a SessionOptions Config Key,
 * "[Area][.[SubArea1].[SubArea2]...].[Keyname]"
 * Such as "ep.cuda.use_arena"
 * The Config Key cannot be empty
 * The maximum length of the Config Key is 1024
 *
 * The string format of a SessionOptions Config Value is defined individually for each Config.
 * The maximum length of the Config Value is 8192
 */

// Key for disable PrePacking,
// If the config value is set to "1" then the prepacking is disabled, otherwise prepacking is enabled (default value)
static const char* const kOrtSessionOptionsConfigDisablePrepacking = "session.disable_prepacking";

// A value of "1" means allocators registered in the env will be used. "0" means the allocators created in the session
// will be used. Use this to override the usage of env allocators on a per session level.
static const char* const kOrtSessionOptionsConfigUseEnvAllocators = "session.use_env_allocators";

// Set to 'ORT' (case sensitive) to load an ORT format model.
// If unset, model type will default to ONNX unless inferred from filename ('.ort' == ORT format) or bytes to be ORT
static const char* const kOrtSessionOptionsConfigLoadModelFormat = "session.load_model_format";

// Set to 'ORT' (case sensitive) to save optimized model in ORT format when SessionOptions.optimized_model_path is set.
// If unset, format will default to ONNX unless optimized_model_filepath ends in '.ort'.
static const char* const kOrtSessionOptionsConfigSaveModelFormat = "session.save_model_format";

// If a value is "1", flush-to-zero and denormal-as-zero are applied. The default is "0".
// When multiple sessions are created, a main thread doesn't override changes from succeeding session options,
// but threads in session thread pools follow option changes.
// When ORT runs with OpenMP, the same rule is applied, i.e. the first session option to flush-to-zero and
// denormal-as-zero is only applied to global OpenMP thread pool, which doesn't support per-session thread pool.
// Note that an alternative way not using this option at runtime is to train and export a model without denormals
// and that's recommended because turning this option on may hurt model accuracy.
static const char* const kOrtSessionOptionsConfigSetDenormalAsZero = "session.set_denormal_as_zero";

// It controls to run quantization model in QDQ (QuantizelinearDeQuantizelinear) format or not.
// "0": enable. ORT does fusion logic for QDQ format.
// "1": disable. ORT doesn't do fusion logic for QDQ format.
// Its default value is "0" unless the DirectML execution provider is registered, in which case it defaults to "1".
static const char* const kOrtSessionOptionsDisableQuantQDQ = "session.disable_quant_qdq";

// This controls whether to prevent constant folding from folding DequantizeLinear nodes:
// "0": (default) DequantizeLinear constant folding is determined solely by session.disable_quant_qdq.
// "1": DequantizeLinear nodes are never individually constant folded.
// When session.disable_quant_qdq is "0" (default), DequantizeLinear nodes are already protected from
// constant folding to preserve QDQ node units for downstream QDQ fusion optimizers.
// When session.disable_quant_qdq is "1", then DequantizeLinear nodes are normally allowed to be
// constant folded, but setting this option to "1" still preserves DequantizeLinear nodes.
// This is useful for execution providers like WebNN that disable QDQ fusion, but which
// still need the original DQ/Q nodes to be preserved for their own quantization handling.

static const char* const kOrtSessionOptionsDisableQDQConstantFolding =
    "session.disable_qdq_constant_folding";

// It controls whether to enable Double QDQ remover and Identical Children Consolidation
// "0": not to disable. ORT does remove the middle 2 Nodes from a Q->(QD->Q)->QD pairs
// "1": disable. ORT doesn't remove the middle 2 Nodes from a Q->(QD->Q)->QD pairs
// Its default value is "0"
static const char* const kOrtSessionOptionsDisableDoubleQDQRemover = "session.disable_double_qdq_remover";

// If set to "1", enables the removal of QuantizeLinear/DequantizeLinear node pairs once all QDQ handling has been
// completed. e.g. If after all QDQ handling has completed and we have -> FloatOp -> Q -> DQ -> FloatOp -> the
// Q -> DQ could potentially be removed. This will provide a performance benefit by avoiding going from float to
// 8-bit and back to float, but could impact accuracy. The impact on accuracy will be model specific and depend on
// other factors like whether the model was created using Quantization Aware Training or Post Training Quantization.
// As such, it's best to test to determine if enabling this works well for your scenario.
// The default value is "0"
// Available since version 1.11.
static const char* const kOrtSessionOptionsEnableQuantQDQCleanup = "session.enable_quant_qdq_cleanup";

// Enable or disable gelu approximation in graph optimization. "0": disable; "1": enable. The default is "0".
// GeluApproximation has side effects which may change the inference results. It is disabled by default due to this.
static const char* const kOrtSessionOptionsEnableGeluApproximation = "optimization.enable_gelu_approximation";

// Enable or disable Cast chain elimination in graph optimization. "0": disable; "1": enable. The default is "0".
// CastElimination with chain elimination has side effects which may change the inference results. It is disabled by default due to this.
static const char* const kOrtSessionOptionsEnableCastChainElimination = "optimization.enable_cast_chain_elimination";

// This setting controls whether to enable AheadOfTime function inlining.
// AOT function inlining examines the graph and attempts to inline as many locally defined functions in the model
// as possible with the help of enabled execution providers.
// This can reduce the number of function calls and improve performance because it is done before
// Level1 optimizers and constant folding. However, under some circumstances, when the EPs are not available,
// one can disable the AOT inlining, produce an optimized model and postpone AOT until run time.
// "0": enable; "1": disable.
// Its default value is "0".
static const char* const kOrtSessionOptionsDisableAheadOfTimeFunctionInlining = "session.disable_aot_function_inlining";

#ifdef ENABLE_TRAINING
// Specifies a path of the file containing a list of memory optimization configurations.
// The value should be a string indicating the file path of the config file.
// The content of the config file is a JSON struct like this:
// [
//   "Gelu+Cast+:1:0",
//   "Dropout+:1:1"
// ]
// Taking the example of "Gelu+Cast+:1:0",
// > "Gelu+Cast+" is the subgraph string, a valid "subgraph string" should be one subgraph representation
//    output by ORT graph transformations.
// > "1" is "optimization strategy", valid values: 0 - disabled, 1 - recompute.
// > "0" is "number of subgraph to apply" which is used to control how many subgraphs to apply optimization,
//    to avoid "oversaving" the memory.
static const char* const kOrtSessionOptionsMemoryOptimizerApplyConfig = "optimization.memory_optimizer_config";

// Specifies the config for detecting subgraphs for memory footprint reduction.
// The value should be a string contains int separated using commas. The default value is "0:0".
static const char* const kOrtSessionOptionsMemoryOptimizerProbeConfig = "optimization.enable_memory_probe_recompute_config";
#endif

// This setting if set should contain a comma separated list of optimizers names that should be disabled.
// Optimizers may take time to execute and affect model loading time. If you feel that a specific optimizer
// does not provider runtime benefits, but affects your model loading time you may disable it using this config
// entry. This option is not enabled in ORT_MINIMAL_BUILD build.
// A list of optimizes is available in onnxruntime/core/optimizer/graph_transformer_utils.cc
//
// Default is an empty string which means no optimizers are disabled.
static const char* const kOrtSessionOptionsDisableSpecifiedOptimizers = "optimization.disable_specified_optimizers";

// Maximum total output size in bytes that the constant folding optimizer is allowed to produce per node.
// Prevents malicious models from causing excessive memory allocation during optimization.
// If the estimated or actual output size of a constant-foldable node exceeds this limit, the node will
// not be constant folded and will instead be executed at runtime.
//
// Option values:
// - A positive integer (as string): Maximum allowed output size in bytes per constant-folded node.
//   Default is "1073741824" (1 GB).
// - "0": Disable the size limit (not recommended for untrusted models).
static const char* const kOrtSessionOptionsConstantFoldingMaxOutputSizeInBytes =
    "optimization.constant_folding_max_output_size_in_bytes";

// It controls whether to run graph optimizations in loop or not.
//
// "0": disable. Graph Optimization Loop is disabled.
// ```
// Level 2 --> Level 3 --> InsertCastTransforms --> Level 4
//   ^                                                 |
//   |                 "No Loop"                       |
//   |                                                 |
//   X                xxxxxxxxxxx                      X
// ```
// "1": enable. Graph Optimization Loop is enabled, such that, if optimizations at Level 4 are applied then
// the loop will check for any other valid optimization that can happen.
// ```
// Level 2 --> Level 3 --> InsertCastTransforms --> Level 4
//   ^                                                 |
//   |        "Loop only depending on Level 4"         |
//   |                                                 |
//   ---------------------------------------------------
// ```
// "2": enable. Graph Optimization Loop is enabled, such that, if optimizations at Level 2 or above are applied then
// The loop will check for any other valid optimization that can happen.
// ```
// Level 2 --> Level 3 --> InsertCastTransforms --> Level 4
//   ^                                                 |
//   |                    "Loop"                       |
//   |                                                 |
//   ---------------------------------------------------
// ```
// Default value is set to "1".
static const char* const kOrtSessionOptionsGraphOptimizationsLoopLevel = "session.graph_optimizations_loop_level";

// Enable or disable using device allocator for allocating initialized tensor memory. "1": enable; "0": disable. The default is "0".
// Using device allocators means the memory allocation is made using malloc/new.
static const char* const kOrtSessionOptionsUseDeviceAllocatorForInitializers = "session.use_device_allocator_for_initializers";

// Configure whether to allow the inter_op/intra_op threads spinning a number of times before blocking
// "0": thread will block if found no job to run
// "1": thread will spin a number of times before blocking
// The default is "0" when ORT is built with "ORT_CLIENT_PACKAGE_BUILD" and "1" otherwise.
// Thread spinning is disabled by default for client/on-device workloads to reduce cpu utilization and improve power efficiency.
static const char* const kOrtSessionOptionsConfigAllowInterOpSpinning = "session.inter_op.allow_spinning";
static const char* const kOrtSessionOptionsConfigAllowIntraOpSpinning = "session.intra_op.allow_spinning";

// Configure the duration in microseconds that threads spin waiting for work before blocking.
// This setting is subordinate to the allow_spinning flags (session.intra_op.allow_spinning /
// session.inter_op.allow_spinning). When allow_spinning is "0", spinning is disabled and
// the spin duration is forced to 0 regardless of this setting.
// By default (when this option is not set), the thread pool uses an iteration-count-based spin loop
// whose wall-clock duration varies by CPU architecture and pause instruction latency. This provides
// the best throughput but may result in high CPU utilization.
// Setting a positive value switches to calibrated iteration-based spinning that targets
// the specified duration. The actual spin time is a best-effort approximation based on a
// one-time measurement of the pause instruction latency; it may vary with CPU frequency
// changes. Recommended for power-sensitive or client/on-device workloads.
// Common values: 500-2000 (0.5-2ms).
// Setting to "0" with spinning enabled effectively disables spinning (equivalent to allow_spinning = false).
static const char* const kOrtSessionOptionsConfigIntraOpSpinDurationUs = "session.intra_op.spin_duration_us";
static const char* const kOrtSessionOptionsConfigInterOpSpinDurationUs = "session.inter_op.spin_duration_us";

// Configure the maximum exponential-backoff cap for the thread pool spin loop.
// When > 1, each idle spin iteration emits a growing number of SpinPause() calls
// (1, 2, 4, ..., up to this cap), which reduces the density of pause instructions
// during the spin window and lowers CPU/power usage compared to emitting one
// SpinPause() per iteration. The total wall-clock spin duration targeted by
// session.{intra,inter}_op.spin_duration_us is preserved by scaling the iteration
// count against the backoff cap.
//   "1" (default) = no backoff, one SpinPause() per iteration (original behavior).
//   ">= 2"        = enable exponential backoff capped at this value. Typical
//                   values: 4 (hybrid/E-core friendly) or 8 (desktop/server).
// Values above 64 are clamped to 64.
// This setting is subordinate to allow_spinning and spin_duration_us: when
// spinning is disabled or spin_duration_us forces zero iterations, this value
// has no effect.
static const char* const kOrtSessionOptionsConfigIntraOpSpinBackoffMax = "session.intra_op.spin_backoff_max";
static const char* const kOrtSessionOptionsConfigInterOpSpinBackoffMax = "session.inter_op.spin_backoff_max";

// Key for using model bytes directly for ORT format
// If a session is created using an input byte array contains the ORT format model data,
// By default we will copy the model bytes at the time of session creation to ensure the model bytes
// buffer is valid.
// Setting this option to "1" will disable copy the model bytes, and use the model bytes directly. The caller
// has to guarantee that the model bytes are valid until the ORT session using the model bytes is destroyed.
static const char* const kOrtSessionOptionsConfigUseORTModelBytesDirectly = "session.use_ort_model_bytes_directly";

/// <summary>
/// Key for using the ORT format model flatbuffer bytes directly for initializers.
/// This avoids copying the bytes and reduces peak memory usage during model loading and initialization.
/// Requires `session.use_ort_model_bytes_directly` or `session.use_memory_mapped_ort_model` to be true.
/// If set, the flatbuffer bytes provided when creating the InferenceSession MUST remain valid for the entire
/// duration of the InferenceSession.
/// </summary>
static const char* const kOrtSessionOptionsConfigUseORTModelBytesForInitializers =
    "session.use_ort_model_bytes_for_initializers";

/// <summary>
/// Key for using memory-mapped I/O to load ORT format model files.
/// When set to "1" and the session is created from a file path, ORT will use memory-mapped I/O
/// to load the .ort model file instead of reading it into a heap-allocated buffer.
/// Usage with session.use_ort_model_bytes_for_initializers will ensure Tensors point directly to the mapped bytes,
/// although the mapping must remain valid and model weights will be immutable.
/// The model load will fail if the mapping fails; fallbacks should be caller-handled.
/// </summary>
static const char* const kOrtSessionOptionsConfigUseMemoryMappedOrtModel = "session.use_memory_mapped_ort_model";

// This should only be specified when exporting an ORT format model for use on a different platform.
// If the ORT format model will be used on ARM platforms set to "1". For other platforms set to "0"
// Available since version 1.11.
static const char* const kOrtSessionOptionsQDQIsInt8Allowed = "session.qdqisint8allowed";

// x64 SSE4.1/AVX2/AVX512(with no VNNI) has overflow problem with quantizied matrix multiplication with U8S8.
// To avoid this we need to use slower U8U8 matrix multiplication instead. This option, if
// turned on, use slower U8U8 matrix multiplications. Only effective with AVX2 or AVX512
// platforms.
static const char* const kOrtSessionOptionsAvx2PrecisionMode = "session.x64quantprecision";

// Specifies how minimal build graph optimizations are handled in a full build.
// These optimizations are at the extended level or higher.
// Possible values and their effects are:
// "save": Save runtime optimizations when saving an ORT format model.
// "apply": Only apply optimizations available in a minimal build.
// ""/<unspecified>: Apply optimizations available in a full build.
// Available since version 1.11.
static const char* const kOrtSessionOptionsConfigMinimalBuildOptimizations =
    "optimization.minimal_build_optimizations";

// Note: The options specific to an EP should be specified prior to appending that EP to the session options object in
// order for them to take effect.

// Specifies a list of stop op types. Nodes of a type in the stop op types and nodes downstream from them will not be
// run by the NNAPI EP.
// The value should be a ","-delimited list of op types. For example, "Add,Sub".
// If not specified, the default set of stop ops is used. To specify an empty stop ops types list and disable stop op
// exclusion, set the value to "".
static const char* const kOrtSessionOptionsConfigNnapiEpPartitioningStopOps = "ep.nnapi.partitioning_stop_ops";

// Enabling dynamic block-sizing for multithreading.
// With a positive value, thread pool will split a task of N iterations to blocks of size starting from:
// N / (num_of_threads * dynamic_block_base)
// As execution progresses, the size will decrease according to the diminishing residual of N,
// meaning the task will be distributed in smaller granularity for better parallelism.
// For some models, it helps to reduce the variance of E2E inference latency and boost performance.
// The feature will not function by default, specify any positive integer, e.g. "4", to enable it.
// Available since version 1.11.
static const char* const kOrtSessionOptionsConfigDynamicBlockBase = "session.dynamic_block_base";

// This option allows to decrease CPU usage between infrequent
// requests and forces any TP threads spinning stop immediately when the last of
// concurrent Run() call returns.
// Spinning is restarted on the next Run() call.
// Applies only to internal thread-pools
static const char* const kOrtSessionOptionsConfigForceSpinningStop = "session.force_spinning_stop";

// "1": all inconsistencies encountered during shape and type inference
// will result in failures.
// "0": in some cases warnings will be logged but processing will continue. The default.
// May be useful to expose bugs in models.
static const char* const kOrtSessionOptionsConfigStrictShapeTypeInference = "session.strict_shape_type_inference";

// "1": every model using a more recent opset than the latest released one will fail
// "0": the model may or may not work if onnxruntime cannot find an implementation, this option
// is used for development purpose.
static const char* const kOrtSessionOptionsConfigStrictAllowReleasedOpsetsOnly = "session.allow_released_opsets_only";

// The file saves configuration for partitioning node among logic streams
static const char* const kNodePartitionConfigFile = "session.node_partition_config_file";

// This Option allows setting affinities for intra op threads.
// Affinity string follows format:
// logical_processor_id,logical_processor_id;logical_processor_id,logical_processor_id
// Semicolon isolates configurations among threads, while comma split processors where ith thread expected to attach to.
// e.g.1,2,3;4,5
// specifies affinities for two threads, with the 1st thread attach to the 1st, 2nd, and 3rd processor, and 2nd thread to the 4th and 5th.
// To ease the configuration, an "interval" is also allowed:
// e.g. 1-8;8-16;17-24
// orders that the 1st thread runs on first eight processors, 2nd thread runs on next eight processors, and so forth.
// Note:
// 1. Once set, the number of thread affinities must equal to intra_op_num_threads - 1, since ort does not set affinity on the main thread which
//    is started and managed by the calling app;
// 2. For windows, ort will infer the group id from a logical processor id, for example, assuming there are two groups with each has 64 logical processors,
//    an id of 64 will be inferred as the last processor of the 1st group, while 65 will be interpreted as the 1st processor of the second group.
//    Hence 64-65 is an invalid configuration, because a windows thread cannot be attached to processors across group boundary.
static const char* const kOrtSessionOptionsConfigIntraOpThreadAffinities = "session.intra_op_thread_affinities";

// This option will dump out the model to assist debugging any issues with layout transformation,
// and is primarily intended for developer usage. It is only relevant if an execution provider that requests
// NHWC layout is enabled such as NNAPI, XNNPACK or QNN.
//
// Default is off. Set to "1" to enable.
//
// If modified by layout transformation the model will be dumped after these steps:
//   1) insertion of the layout transformation Transpose nodes
//   2) after those are optimized using the transpose optimizer,
//   3) after the L1 transformers are applied to the updated graph.
// The model will be saved to filename post_layout_transform_step_<step_number>.onnx.
static const char* const kDebugLayoutTransformation = "session.debug_layout_transformation";

// Graph nodes that are not supported by the execution providers (EPs) explicitly added to the session are
// assigned (i.e., "fallback") to the CPU EP by default.
//
// This option allows the user to disable the fallback of unsupported graph nodes to the CPU EP.
// If this option is set to "1", session creation will fail if the execution providers other than the CPU EP cannot
// fully support all of the nodes in the graph.
//
// It is invalid to set this option and explicitly add the CPU EP to the session. In this case, session creation
// will also fail with an error.
//
// Option values:
// - "0": CPU EP fallback is not disabled. [DEFAULT]
// - "1": CPU EP fallback is disabled.
static const char* const kOrtSessionOptionsDisableCPUEPFallback = "session.disable_cpu_ep_fallback";

// Use this config when serializing a large model after optimization to specify an external initializers file
static const char* const kOrtSessionOptionsOptimizedModelExternalInitializersFileName =
    "session.optimized_model_external_initializers_file_name";

// Use this config to control the minimum size of the initializer when externalizing it during serialization
static const char* const kOrtSessionOptionsOptimizedModelExternalInitializersMinSizeInBytes =
    "session.optimized_model_external_initializers_min_size_in_bytes";

// Specifies the folder path used to resolve a model's external initializer data files.
// When set, external initializers are loaded from this folder instead of the model's own
// directory, overriding the model directory. This applies whether the model is loaded from a
// file path or from a memory buffer/stream. All external data files must be in the same folder.
// Typical uses include loading models with external data from memory, sharing a weights file
// across models, and weightless/cache models whose weights live outside the model directory.
static const char* const kOrtSessionOptionsModelExternalInitializersFileFolderPath =
    "session.model_external_initializers_file_folder_path";

// Use this config when saving pre-packed constant initializers to an external data file.
// This allows you to memory map pre-packed initializers on model load and leave it to
// to the OS the amount of memory consumed by the pre-packed initializers. Otherwise,
// pre-packed data resides on the heap.
//
// - "0": Default is not save pre-packed initializers to a data file.
// - "1": Save pre-packed constant initializers to an external data file.
// Sample usage: sess_options.add_session_config_entry(kOrtSessionOptionsSavePrePackedConstantInitializers,  "1")
static const char* const kOrtSessionOptionsSavePrePackedConstantInitializers =
    "session.save_external_prepacked_constant_initializers";

// Use this config when you want to collect memory stats for each node in the graph.
// The file format is a CSV file with the following columns:
// The file will be created if it does not exist, and will be overwritten if it does.
//
// The content of the file can be used to estimate memory requirements at run time including
// the temporary allocations. This operation is preferably done on a CPU device, as the model may exceed
// device memory limits in constrained environments. When enabling this option, it is important to disable
// memory patterns, as they tend to allocate large blocks to avoid fragmentation and accommodate needs of multiple
// kernels. Memory patterns may make it difficult to allocate on a device with limited memory.
//
// The collected stats then can be used to partition the graph among the devices in a way that only the
// required memory is allocated on each device.
//
// node_name, initializers_memory, dynamic_outputs_sizes, temp_allocations_size
//
// - "full path to file": there is not a default for this option. If the file can not be opened for writing, an error will be returned.
static const char* const kOrtSessionOptionsCollectNodeMemoryStatsToFile = "session.collect_node_memory_stats_to_file";

/// This is a composite CSV setting formatted as "memory limit in kb,file name for collected stats"
/// "limit > 0": enables Capacity Aware Partitioning for Cuda EP. `limit` is optional and when absent
/// the provider may attempt to figure out the memory available automatically.
/// The setting with no pre-recorded stats is expected to look like: "limit > 0,".
/// In this case, the EP will calculate memory using the initializers referenced by the node.
///   This enables an ad-hoc and flexible scenarios with no pre-recorded stats, but may be less accurate.
/// The setting with no limit is expected to look like: ",file name for collected stats"
/// Finally a setting with both limit and pre-recorded stats absent can contain a single comma: ",".
///  The EP will attempt to place nodes on device (currently only CUDA is supported) :
/// this file is expected to be found at the same folder with the model. The file contains
/// pre-recorded stats collected when running with kOrtSessionOptionsCollectNodeMemoryStatsToFile enforce (see above)
static const char* const kOrtSessionOptionsResourceCudaPartitioningSettings =
    "session.resource_cuda_partitioning_settings";

/// <summary>
/// This is a setting that contains string annotations or annotation prefixes to be matched
/// against individual nodes metadata entry 'layer_ann' to guide layer assignment during partitioning.
/// The value is a semicolon separated list of strings or string prefixes per device.
/// Format: device1(annotation1, annotation2, ...); device2(annotation1, =annotation3, ...);...
/// Where:
/// - device1, device2, ... are the recognized device names to be matched against EPs configured in
///   the given session.
/// - annotation1, annotation2, ... are annotation prefixes to be matched against node annotations. Any
///   node annotation that starts with one of these prefixes will be matched.
/// - =annotation3 indicates an exact match for annotation3. Only node annotations that are exactly
///   equal to 'annotation3' will be matched.
/// TODO: add a list of recognized devices here.
/// </summary>
static const char* const kOrtSessionOptionsLayerAssignmentSettings = "session.layer_assignment_settings";

/// <summary>
/// Name-based layer assignment. Uses the same device(pattern1, pattern2, ...); ... grammar
/// as kOrtSessionOptionsLayerAssignmentSettings but performs SUBSTRING matching against
/// Node::Name() instead of prefix/exact matching against node metadata annotations.
/// The '=' prefix (exact match) from the annotation-based grammar is rejected with an error
/// — all patterns are treated as substrings.
/// Longest matching pattern wins when multiple patterns match the same node name.
/// No subgraph inheritance is applied — each node is matched independently by its name.
///
/// MUTUALLY EXCLUSIVE with kOrtSessionOptionsLayerAssignmentSettings. Setting both returns
/// INVALID_ARGUMENT. Use annotation-based matching for models with explicit layer annotations,
/// or name-based matching for models with structured node names (HuggingFace, PyTorch exports).
/// </summary>
static const char* const kOrtSessionOptionsNameBasedLayerAssignment = "session.name_based_layer_assignment";

// Enable EP context feature to dump the partitioned graph which includes the EP context into Onnx file.
// The dumped Onnx model with EP context can be used for future inference to avoid the EP graph partitioning/compile overhead.
// "0": disable. (default)
// "1": enable.
static const char* const kOrtSessionOptionEpContextEnable = "ep.context_enable";

// Specify the file path for the Onnx model which has EP context.
// Default to original_file_name_ctx.onnx if not specified
// Folder is not a valid option
static const char* const kOrtSessionOptionEpContextFilePath = "ep.context_file_path";

// Flag to specify whether to dump the EP context into the Onnx model.
// "0": dump the EP context into separate file, keep the file name in the Onnx model. (default).
// "1": dump the EP context into the Onnx model.
static const char* const kOrtSessionOptionEpContextEmbedMode = "ep.context_embed_mode";

// Specify the EPContext node name prefix to make it unique
// in case user need to merge/connect multiple EPContext nodes in one model
static const char* const kOrtSessionOptionEpContextNodeNamePrefix = "ep.context_node_name_prefix";

// Share EP related resources across sessions
static const char* const kOrtSessionOptionShareEpContexts = "ep.share_ep_contexts";

// Stop to share EP related resources across sessions from then on
static const char* const kOrtSessionOptionStopShareEpContexts = "ep.stop_share_ep_contexts";

// Used only for context model generation.
// This configuration is used when some nodes are partitioned on the CPU EP and those nodes have external initializers.
// When generating the EP context model, the new model should not rely on the old external data file used by the source ONNX model.
// Use this setting when dumping the EP context model with an external initializers file.
// If specified, all initializers will be placed inside the external data file.
// Otherwise, all initializers will be embedded inside the generated ONNX file.
// By default, this option is not set, meaning all initializers will be included within the ONNX file.
static const char* const kOrtSessionOptionsEpContextModelExternalInitializersFileName =
    "ep.context_model_external_initializers_file_name";

// Internal-only flag set by OrtCompileAPI::CompileModel() to signal EPs that this session
// is being used for compilation only and will never be used for inference.
// EPs can use this to skip GPU deserialization and execution context creation, which would
// otherwise be wasteful since the session is destroyed immediately after compilation.
// This is NOT a user-facing option and must not be set directly by application code.
// "0": normal session (default)
// "1": compile-only session (set internally by OrtCompileAPI::CompileModel)
static const char* const kOrtSessionOptionCompileOnly = "session.compile_only";

// Gemm fastmath mode provides fp32 gemm acceleration with bfloat16 based matmul.
// Option values:
// - "0": Gemm FastMath mode is not enabled. [DEFAULT]
// - "1": Gemm FastMath mode is enabled.
static const char* const kOrtSessionOptionsMlasGemmFastMathArm64Bfloat16 = "mlas.enable_gemm_fastmath_arm64_bfloat16";

// Use LUT (Lookup Table) based GEMM for quantized models when available.
// Option values:
// - "0": Do not use LUT based GEMM. [DEFAULT]
// - "1": Use LUT based GEMM when available.
static const char* const kOrtSessionOptionsMlasLutGemm = "mlas.use_lut_gemm";

// Use KleidiAI kernels in MLAS if available.
// Option values:
// - "0": Use KleidiAI kernels when available. [DEFAULT]
// - "1": Disable KleidiAI kernels even if available.
static const char* const kOrtSessionOptionsMlasDisableKleidiAi = "mlas.disable_kleidiai";

// Power-user tuning option for the Arm® KleidiAI™ SME IGEMM convolution route on Arm64.
// For 2D convolutions where both SME IGEMM and the MlasGemm SGEMM fallback are valid routes, work is estimated as:
//  output_h * output_w * input_channels * dilated_kernel_h * dilated_kernel_w * filter_count.
// Work above this threshold routes through the SGEMM fallback; work at or below it stays on IGEMM.
// "0" or unset uses the MLAS default heuristic, intended for typical workloads.
// This option exists for perf experimentation; the default may be retuned in future ORT releases.
static const char* const kOrtSessionOptionsMlasKleidiAiConvIgemmMaxWork = "mlas.kleidiai.conv_igemm_max_work";

// When converting DQ + MatMul -> MatMulNBits, the accuracy level of the MatMulNBits is controlled by this option.
// Refer to MatMulNBits op schema for more details.
// If not provided, default is 4.
static const char* const kOrtSessionOptionsQDQMatMulNBitsAccuracyLevel = "session.qdq_matmulnbits_accuracy_level";

// Block size used when converting per-tensor or per-axis DQ + MatMul to MatMulNBits.
// Only applies to DQ nodes without an existing block_size attribute (i.e., per-tensor or per-axis quantization).
// Positive value: explicit block_size (must be power-of-2 and >= 16, e.g., 16, 32, 64, 128).
// "0" or not provided: use default block_size of 32.
// "-1": heuristic - largest power-of-2 <= min(K, 256) that minimizes padding.
static const char* const kOrtSessionOptionsQDQMatMulNBitsBlockSize = "session.qdq_matmulnbits_block_size";

// Enable the DQ->MatMulNBits fusion graph transformer.
// "0": disabled (default). "1": enabled.
// This is typically set automatically by InferenceSession when the NvTensorRTRTX EP is registered.
static const char* const kOrtSessionOptionsEnableDQMatMulNBitsFusion = "session.enable_dq_matmulnbits_fusion";

// THIS OPTION IS NOT A REGULAR SESSION OPTION SINCE IT CAN BE MODIFIED AT ANY TIME
// Meant to be used with SetEpDynamicOptions
// Specify the type of workload for this session.
// "Default": OS determines the scheduling priority and processor performance to service this workload. [Default]
// "Efficient": OS treats this workload is efficiency oriented with low scheduling priority and efficient processor performance.
static const char* const kOrtEpDynamicOptionsWorkloadType = "ep.dynamic.workload_type";

// Disables model compilation during session initialization.
//
// If this option is set to "1", inference session creation will fail with error code ORT_MODEL_REQUIRES_COMPILATION
// if compilation is required to run the model on any Execution Provider added to the session.
// Only the following kinds of models are valid when this option is set to "1":
//   - Pre-compiled models that have EPContext nodes for the compiling Execution Providers in the session.
//   - Non-compiled models that run only on non-compiling Execution Providers, like CPU EP.
//
// See \href https://onnxruntime.ai/docs/execution-providers/EP-Context-Design.html for details about
// compiled models with EPContext nodes.
//
// Option values:
// - "0": EP compile is not disabled. [DEFAULT]
// - "1": EP compile is disabled.
static const char* const kOrtSessionOptionsDisableModelCompile = "session.disable_model_compile";

// Controls behavior when compiled model compatibility is SUPPORTED_PREFER_RECOMPILATION.
// "0": Allow execution with suboptimal performance. [DEFAULT]
// "1": Fail session creation to require recompilation for optimal performance.
// Note: UNSUPPORTED models always fail regardless of this setting.
static const char* const kOrtSessionOptionsFailOnSuboptimalCompiledModel =
    "session.fail_on_suboptimal_compiled_model";

// THIS OPTION IS NOT A REGULAR SESSION OPTION SINCE IT CAN BE MODIFIED AT ANY TIME
// Meant to be used with SetEpDynamicOptions
// options for HTP performance mode: "burst", "balanced", "default", "high_performance",
// "high_power_saver", "low_balanced", "extreme_power_saver", "low_power_saver", "power_saver",
// "sustained_high_performance". Default to "default".
static const char* const kOrtEpDynamicOptionsQnnHtpPerformanceMode = "ep.dynamic.qnn_htp_performance_mode";

// Enables the session to record information about the subgraphs/nodes assigned to execution providers.
// When enabled, an application may call Session_GetEpGraphAssignmentInfo() to retrieve the information.
//
// Option values:
// - "0": Recording of EP graph assignment information is disabled. [DEFAULT]
// - "1": Recording of EP graph assignment information is enabled.
static const char* const kOrtSessionOptionsRecordEpGraphAssignmentInfo = "session.record_ep_graph_assignment_info";

// An application enables this option to request that EPs create compiled models (i.e., EPContext models) with EPContext
// nodes that do not store model weights internally. Instead, the weights should be provided by ONNX Runtime as
// explicit inputs to the EPContext nodes.
//
// This option is ignored by an EP if "ep.context_enable" is not set to "1".
//
// If the weights are originally stored in an external file, this allows multiple models to share the same
// external weights file.
//
// Option values:
// - "0": disable. (default)
// - "1": enable.
static const char* const kOrtSessionOptionEpEnableWeightlessEpContextNodes = "ep.enable_weightless_ep_context_nodes";

// Controls the intra-op thread pool size for a session.
// Value should be a base-10 int32 string.
// Equivalent to OrtApi::SetIntraOpNumThreads.
static const char* const kOrtSessionOptionsConfigIntraOpNumThreads = "session.intra_op_num_threads";

// Controls the inter-op thread pool size for a session.
// Value should be a base-10 int32 string.
// Equivalent to OrtApi::SetInterOpNumThreads.
static const char* const kOrtSessionOptionsConfigInterOpNumThreads = "session.inter_op_num_threads";

// Enable or disable the CPU memory arena for a session.
// "0": disable; "1": enable.
// Equivalent to OrtApi::DisableCpuMemArena / OrtApi::EnableCpuMemArena.
static const char* const kOrtSessionOptionsConfigEnableCpuMemArena = "session.enable_cpu_mem_arena";

// Enable or disable memory pattern optimization for a session.
// "0": disable; "1": enable.
// Equivalent to OrtApi::DisableMemPattern / OrtApi::EnableMemPattern.
static const char* const kOrtSessionOptionsConfigEnableMemPattern = "session.enable_mem_pattern";

// Session log identifier.
// Value should be a UTF-8 string.
// Equivalent to OrtApi::SetSessionLogId.
static const char* const kOrtSessionOptionsConfigLogId = "session.log_id";

// Session log severity level.
// Value should be a base-10 int32 string (refer to OrtLoggingLevel values).
// Equivalent to OrtApi::SetSessionLogSeverityLevel.
static const char* const kOrtSessionOptionsConfigLogSeverityLevel = "session.log_severity_level";

// Session log verbosity level.
// Value should be a base-10 int32 string.
// Equivalent to OrtApi::SetSessionLogVerbosityLevel.
static const char* const kOrtSessionOptionsConfigLogVerbosityLevel = "session.log_verbosity_level";

// Enable or disable profiling for a session.
// Empty string: disable profiling.
// Non-empty string: enable profiling and use value as profile file prefix.
// Equivalent to OrtApi::DisableProfiling / OrtApi::EnableProfiling.
static const char* const kOrtSessionOptionsConfigEnableProfiling = "session.enable_profiling";

// Graph optimization level for a session.
// Value should be one of:
// "disable_all", "enable_basic", "enable_extended", "enable_layout", "enable_all".
// Equivalent to OrtApi::SetSessionGraphOptimizationLevel.
static const char* const kOrtSessionOptionsConfigGraphOptimizationLevel = "session.graph_optimization_level";

// File path for saving the optimized model.
// Value should be a path string.
// Equivalent to OrtApi::SetOptimizedModelFilePath.
static const char* const kOrtSessionOptionsConfigOptimizedModelFilePath = "session.optimized_model_filepath";

// Session execution mode.
// Value should be one of:
// "sequential", "parallel", "ort_sequential", "ort_parallel".
// Equivalent to OrtApi::SetSessionExecutionMode.
static const char* const kOrtSessionOptionsConfigExecutionMode = "session.execution_mode";

// Controls whether to use per-session thread pools.
// "0": disable per-session threads (use global thread pools).
// "1": keep per-session threads enabled (default behavior before disable call).
// Equivalent to OrtApi::DisablePerSessionThreads (one-way via API).
static const char* const kOrtSessionOptionsConfigUsePerSessionThreads = "session.use_per_session_threads";

// Enable or disable deterministic compute for a session.
// "0": disable; "1": enable.
// Equivalent to OrtApi::SetDeterministicCompute.
static const char* const kOrtSessionOptionsConfigUseDeterministicCompute = "session.use_deterministic_compute";
