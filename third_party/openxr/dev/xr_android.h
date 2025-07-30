#ifndef THIRD_PARTY_OPENXR_DEV_XR_ANDROID_H_
#define THIRD_PARTY_OPENXR_DEV_XR_ANDROID_H_

/*
** Copyright 2017-2022 The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0 OR MIT
*/

#include <stdint.h>

#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform_defines.h"

#ifndef XR_ANDROID_unbounded_reference_space
#define XR_ANDROID_unbounded_reference_space 1
#define XR_ANDROID_unbounded_reference_space_SPEC_VERSION 1
#define XR_ANDROID_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME \
  "XR_ANDROID_unbounded_reference_space"
#define XR_REFERENCE_SPACE_TYPE_UNBOUNDED_ANDROID \
  ((XrReferenceSpaceType)1000467000U)
#endif /* XR_ANDROID_unbounded_reference_space */

#ifndef XR_ANDROID_light_estimation
#define XR_ANDROID_light_estimation 1
XR_DEFINE_HANDLE(XrLightEstimatorANDROID)
#define XR_ANDROID_light_estimation_SPEC_VERSION 1
#define XR_ANDROID_LIGHT_ESTIMATION_EXTENSION_NAME "XR_ANDROID_light_estimation"
#define XR_TYPE_LIGHT_ESTIMATOR_CREATE_INFO_ANDROID \
  ((XrStructureType)1000700000U)
#define XR_TYPE_LIGHT_ESTIMATE_GET_INFO_ANDROID ((XrStructureType)1000700001U)
#define XR_TYPE_LIGHT_ESTIMATE_ANDROID ((XrStructureType)1000700002U)
#define XR_TYPE_DIRECTIONAL_LIGHT_ANDROID ((XrStructureType)1000700003U)
#define XR_TYPE_SPHERICAL_HARMONICS_ANDROID ((XrStructureType)1000700004U)
#define XR_TYPE_SYSTEM_LIGHT_ESTIMATION_PROPERTIES_ANDROID \
  ((XrStructureType)1000700006U)
#define XR_TYPE_AMBIENT_LIGHT_ANDROID ((XrStructureType)1000700005U)
#define XR_OBJECT_TYPE_LIGHT_ESTIMATOR_ANDROID ((XrObjectType)1000700000U)

typedef enum XrLightEstimateStateANDROID {
  XR_LIGHT_ESTIMATE_STATE_VALID_ANDROID = 0,
  XR_LIGHT_ESTIMATE_STATE_INVALID_ANDROID = 1,
  XR_LIGHT_ESTIMATE_STATE_MAX_ENUM_ANDROID = 0x7FFFFFFF
} XrLightEstimateStateANDROID;

typedef enum XrSphericalHarmonicsKindANDROID {
  XR_SPHERICAL_HARMONICS_KIND_TOTAL_ANDROID = 0,
  XR_SPHERICAL_HARMONICS_KIND_AMBIENT_ANDROID = 1,
  XR_SPHERICAL_HARMONICS_KIND_MAX_ENUM_ANDROID = 0x7FFFFFFF
} XrSphericalHarmonicsKindANDROID;

typedef struct XrSystemLightEstimationPropertiesANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  XrBool32 supportsLightEstimation;
} XrSystemLightEstimationPropertiesANDROID;

typedef struct XrLightEstimatorCreateInfoANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
} XrLightEstimatorCreateInfoANDROID;

typedef struct XrLightEstimateGetInfoANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  XrSpace space;
  XrTime time;
} XrLightEstimateGetInfoANDROID;

typedef struct XrLightEstimateANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  XrLightEstimateStateANDROID state;
  XrTime lastUpdatedTime;
} XrLightEstimateANDROID;

// XrDirectionalLightANDROID extends XrLightEstimateANDROID
typedef struct XrDirectionalLightANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  XrLightEstimateStateANDROID state;
  XrVector3f intensity;
  XrVector3f direction;
} XrDirectionalLightANDROID;

// XrAmbientLightANDROID extends XrLightEstimateANDROID
typedef struct XrAmbientLightANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  XrLightEstimateStateANDROID state;
  XrVector3f intensity;
  XrVector3f colorCorrection;
} XrAmbientLightANDROID;

// XrSphericalHarmonicsANDROID extends XrLightEstimateANDROID
typedef struct XrSphericalHarmonicsANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  XrLightEstimateStateANDROID state;
  XrSphericalHarmonicsKindANDROID kind;
  float coefficients[9][3];
} XrSphericalHarmonicsANDROID;

typedef XrResult(XRAPI_PTR* PFN_xrCreateLightEstimatorANDROID)(
    XrSession session,
    XrLightEstimatorCreateInfoANDROID* createInfo,
    XrLightEstimatorANDROID* outHandle);
typedef XrResult(XRAPI_PTR* PFN_xrDestroyLightEstimatorANDROID)(
    XrLightEstimatorANDROID estimator);
typedef XrResult(XRAPI_PTR* PFN_xrGetLightEstimateANDROID)(
    XrLightEstimatorANDROID estimator,
    const XrLightEstimateGetInfoANDROID* input,
    XrLightEstimateANDROID* output);
#endif  // XR_ANDROID_light_estimation

#ifndef XR_ANDROID_depth_texture
#define XR_ANDROID_depth_texture 1
XR_DEFINE_HANDLE(XrDepthSwapchainANDROID)
#define XR_ANDROID_depth_texture_SPEC_VERSION 1
#define XR_ANDROID_DEPTH_TEXTURE_EXTENSION_NAME "XR_ANDROID_depth_texture"
#define XR_TYPE_DEPTH_SWAPCHAIN_CREATE_INFO_ANDROID \
  ((XrStructureType)1000702000U)
#define XR_TYPE_DEPTH_VIEW_ANDROID ((XrStructureType)1000702001U)
#define XR_TYPE_DEPTH_ACQUIRE_INFO_ANDROID ((XrStructureType)1000702002U)
#define XR_TYPE_DEPTH_ACQUIRE_RESULT_ANDROID ((XrStructureType)1000702003U)
#define XR_TYPE_SYSTEM_DEPTH_TRACKING_PROPERTIES_ANDROID \
  ((XrStructureType)1000702004U)
#define XR_TYPE_DEPTH_SWAPCHAIN_IMAGE_ANDROID ((XrStructureType)1000702005U)
#define XR_ERROR_DEPTH_NOT_AVAILABLE_ANDROID ((XrResult)-1000702000U)
#define XR_OBJECT_TYPE_DEPTH_SWAPCHAIN_ANDROID ((XrObjectType)1000702001U)

typedef enum XrDepthCameraResolutionANDROID {
  XR_DEPTH_CAMERA_RESOLUTION_80x80_ANDROID = 0,
  XR_DEPTH_CAMERA_RESOLUTION_160x160_ANDROID = 1,
  XR_DEPTH_CAMERA_RESOLUTION_320x320_ANDROID = 2,
  XR_DEPTH_CAMERA_RESOLUTION_MAX_ENUM_ANDROID = 0x7FFFFFFF
} XrDepthCameraResolutionANDROID;
typedef XrFlags64 XrDepthSwapchainCreateFlagsANDROID;

// Flag bits for XrDepthSwapchainCreateFlagsANDROID
static const XrDepthSwapchainCreateFlagsANDROID
    XR_DEPTH_SWAPCHAIN_CREATE_SMOOTH_DEPTH_IMAGE_BIT_ANDROID = 0x00000001;
static const XrDepthSwapchainCreateFlagsANDROID
    XR_DEPTH_SWAPCHAIN_CREATE_SMOOTH_CONFIDENCE_IMAGE_BIT_ANDROID = 0x00000002;
static const XrDepthSwapchainCreateFlagsANDROID
    XR_DEPTH_SWAPCHAIN_CREATE_RAW_DEPTH_IMAGE_BIT_ANDROID = 0x00000004;
static const XrDepthSwapchainCreateFlagsANDROID
    XR_DEPTH_SWAPCHAIN_CREATE_RAW_CONFIDENCE_IMAGE_BIT_ANDROID = 0x00000008;

typedef struct XrDepthSwapchainCreateInfoANDROID {
  XrStructureType type;
  const void* XR_MAY_ALIAS next;
  XrDepthCameraResolutionANDROID resolution;
  XrDepthSwapchainCreateFlagsANDROID createFlags;
} XrDepthSwapchainCreateInfoANDROID;

typedef struct XrDepthSwapchainImageANDROID {
  XrStructureType type;
  void* XR_MAY_ALIAS next;
  const float* rawDepthImage;
  const uint8_t* rawDepthConfidenceImage;
  const float* smoothDepthImage;
  const uint8_t* smoothDepthConfidenceImage;
} XrDepthSwapchainImageANDROID;

typedef struct XrDepthAcquireInfoANDROID {
  XrStructureType type;
  const void* next;
  XrSpace space;
  XrTime displayTime;
} XrDepthAcquireInfoANDROID;

typedef struct XrDepthViewANDROID {
  XrStructureType type;
  const void* next;
  XrFovf fov;
  XrPosef pose;
} XrDepthViewANDROID;

typedef struct XrDepthAcquireResultANDROID {
  XrStructureType type;
  const void* next;
  uint32_t acquiredIndex;
  XrTime exposureTimestamp;
  XrDepthViewANDROID views[2];
} XrDepthAcquireResultANDROID;

// XrSystemDepthTrackingPropertiesANDROID extends XrSystemProperties
typedef struct XrSystemDepthTrackingPropertiesANDROID {
  XrStructureType type;
  const void* next;
  XrBool32 supportsDepthTracking;
} XrSystemDepthTrackingPropertiesANDROID;

typedef XrResult(XRAPI_PTR* PFN_xrCreateDepthSwapchainANDROID)(
    XrSession session,
    const XrDepthSwapchainCreateInfoANDROID* createInfo,
    XrDepthSwapchainANDROID* swapchain);
typedef XrResult(XRAPI_PTR* PFN_xrDestroyDepthSwapchainANDROID)(
    XrDepthSwapchainANDROID swapchain);
typedef XrResult(XRAPI_PTR* PFN_xrEnumerateDepthSwapchainImagesANDROID)(
    XrDepthSwapchainANDROID depthSwapchain,
    uint32_t depthImageCapacityInput,
    uint32_t* depthImageCountOutput,
    XrDepthSwapchainImageANDROID* depthImages);
typedef XrResult(XRAPI_PTR* PFN_xrEnumerateDepthResolutionsANDROID)(
    XrSession session,
    uint32_t resolutionCapacityInput,
    uint32_t* resolutionCountOutput,
    XrDepthCameraResolutionANDROID* resolutions);
typedef XrResult(XRAPI_PTR* PFN_xrAcquireDepthSwapchainImagesANDROID)(
    XrDepthSwapchainANDROID depthSwapchain,
    const XrDepthAcquireInfoANDROID* acquireInfo,
    XrDepthAcquireResultANDROID* acquireResult);
#endif  // XR_ANDROID_depth_texture

#endif  // THIRD_PARTY_OPENXR_DEV_XR_ANDROID_H_
