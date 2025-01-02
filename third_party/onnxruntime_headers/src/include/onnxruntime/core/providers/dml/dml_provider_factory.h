// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)  // nonstandard extension used: nameless struct/union
#ifdef _GAMING_XBOX_SCARLETT
#include <d3d12_xs.h>
#elif defined(_GAMING_XBOX_XBOXONE)
#include <d3d12_x.h>
#else
#include <d3d12.h>
#endif
#pragma warning(pop)

#ifdef __cplusplus
#include <DirectML.h>
#else
struct IDMLDevice;
typedef struct IDMLDevice IDMLDevice;
#endif

// Windows pollutes the macro space, causing a build break in constants.h.
#undef OPTIONAL

#include "onnxruntime_c_api.h"

#ifdef __cplusplus

extern "C" {

enum OrtDmlDeviceFilter : uint32_t {
#ifdef ENABLE_NPU_ADAPTER_ENUMERATION
  Any = 0xffffffff,
  Gpu = 1 << 0,
  Npu = 1 << 1,
#else
  Gpu = 1 << 0,
#endif
};

inline OrtDmlDeviceFilter operator~(OrtDmlDeviceFilter a) { return (OrtDmlDeviceFilter) ~(int)a; }
inline OrtDmlDeviceFilter operator|(OrtDmlDeviceFilter a, OrtDmlDeviceFilter b) { return (OrtDmlDeviceFilter)((int)a | (int)b); }
inline OrtDmlDeviceFilter operator&(OrtDmlDeviceFilter a, OrtDmlDeviceFilter b) { return (OrtDmlDeviceFilter)((int)a & (int)b); }
inline OrtDmlDeviceFilter operator^(OrtDmlDeviceFilter a, OrtDmlDeviceFilter b) { return (OrtDmlDeviceFilter)((int)a ^ (int)b); }
inline OrtDmlDeviceFilter& operator|=(OrtDmlDeviceFilter& a, OrtDmlDeviceFilter b) { return (OrtDmlDeviceFilter&)((int&)a |= (int)b); }
inline OrtDmlDeviceFilter& operator&=(OrtDmlDeviceFilter& a, OrtDmlDeviceFilter b) { return (OrtDmlDeviceFilter&)((int&)a &= (int)b); }
inline OrtDmlDeviceFilter& operator^=(OrtDmlDeviceFilter& a, OrtDmlDeviceFilter b) { return (OrtDmlDeviceFilter&)((int&)a ^= (int)b); }

#else

typedef enum OrtDmlDeviceFilter {
#ifdef ENABLE_NPU_ADAPTER_ENUMERATION
  Any = 0xffffffff,
  Gpu = 1 << 0,
  Npu = 1 << 1,
#else
  Gpu = 1 << 0,
#endif
} OrtDmlDeviceFilter;

#endif

typedef enum OrtDmlPerformancePreference {
  Default = 0,
  HighPerformance = 1,
  MinimumPower = 2
} OrtDmlPerformancePreference;

struct OrtDmlDeviceOptions {
  OrtDmlPerformancePreference Preference;
  OrtDmlDeviceFilter Filter;
};

typedef struct OrtDmlDeviceOptions OrtDmlDeviceOptions;

/**
 * [[deprecated]]
 * This export is deprecated.
 * The OrtSessionOptionsAppendExecutionProvider_DML export on the OrtDmlApi should be used instead.
 *
 * Creates a DirectML Execution Provider which executes on the hardware adapter with the given device_id, also known as
 * the adapter index. The device ID corresponds to the enumeration order of hardware adapters as given by
 * IDXGIFactory::EnumAdapters. A device_id of 0 always corresponds to the default adapter, which is typically the
 * primary display GPU installed on the system. A negative device_id is invalid.
 */
ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_DML, _In_ OrtSessionOptions* options, int device_id);

/**
 * [[deprecated]]
 * This export is deprecated.
 * The OrtSessionOptionsAppendExecutionProvider_DML1 export on the OrtDmlApi should be used instead.
 *
 * Creates a DirectML Execution Provider using the given DirectML device, and which executes work on the supplied D3D12
 * command queue. The DirectML device and D3D12 command queue must have the same parent ID3D12Device, or an error will
 * be returned. The D3D12 command queue must be of type DIRECT or COMPUTE (see D3D12_COMMAND_LIST_TYPE). If this
 * function succeeds, the inference session maintains a strong reference on both the dml_device and the command_queue
 * objects.
 * See also: DMLCreateDevice
 * See also: ID3D12Device::CreateCommandQueue
 */
ORT_API_STATUS(OrtSessionOptionsAppendExecutionProviderEx_DML, _In_ OrtSessionOptions* options,
               _In_ IDMLDevice* dml_device, _In_ ID3D12CommandQueue* cmd_queue);

struct OrtDmlApi;
typedef struct OrtDmlApi OrtDmlApi;

struct OrtDmlApi {
  /**
   * Creates a DirectML Execution Provider which executes on the hardware adapter with the given device_id, also known as
   * the adapter index. The device ID corresponds to the enumeration order of hardware adapters as given by
   * IDXGIFactory::EnumAdapters. A device_id of 0 always corresponds to the default adapter, which is typically the
   * primary display GPU installed on the system. A negative device_id is invalid.
   */
  ORT_API2_STATUS(SessionOptionsAppendExecutionProvider_DML, _In_ OrtSessionOptions* options, int device_id);

  /**
   * Creates a DirectML Execution Provider using the given DirectML device, and which executes work on the supplied D3D12
   * command queue. The DirectML device and D3D12 command queue must have the same parent ID3D12Device, or an error will
   * be returned. The D3D12 command queue must be of type DIRECT or COMPUTE (see D3D12_COMMAND_LIST_TYPE). If this
   * function succeeds, the inference session maintains a strong reference on both the dml_device and the command_queue
   * objects.
   * See also: DMLCreateDevice
   * See also: ID3D12Device::CreateCommandQueue
   */
  ORT_API2_STATUS(SessionOptionsAppendExecutionProvider_DML1, _In_ OrtSessionOptions* options,
                  _In_ IDMLDevice* dml_device, _In_ ID3D12CommandQueue* cmd_queue);

  /**
   * CreateGPUAllocationFromD3DResource
   * This API creates a DML EP resource based on a user-specified D3D12 resource.
   */
  ORT_API2_STATUS(CreateGPUAllocationFromD3DResource, _In_ ID3D12Resource* d3d_resource, _Out_ void** dml_resource);

  /**
   * FreeGPUAllocation
   * This API frees the DML EP resource created by CreateGPUAllocationFromD3DResource.
   */
  ORT_API2_STATUS(FreeGPUAllocation, _In_ void* dml_resource);

  /**
   * GetD3D12ResourceFromAllocation
   * This API gets the D3D12 resource when an OrtValue has been allocated by the DML EP.
   */
  ORT_API2_STATUS(GetD3D12ResourceFromAllocation, _In_ OrtAllocator* provider, _In_ void* dml_resource, _Out_ ID3D12Resource** d3d_resource);

  /**
   * SessionOptionsAppendExecutionProvider_DML2
   * Creates a DirectML Execution Provider given the supplied device options that contain a performance preference
   * (high power, low power, or default) and a device filter (None, GPU, or NPU).
   */
  ORT_API2_STATUS(SessionOptionsAppendExecutionProvider_DML2, _In_ OrtSessionOptions* options, OrtDmlDeviceOptions* device_opts);
};

#ifdef __cplusplus
}
#endif
