#ifndef THIRD_PARTY_OPENXR_DEV_XR_ANDROID_H_
#define THIRD_PARTY_OPENXR_DEV_XR_ANDROID_H_

/*
** Copyright 2017-2022 The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0 OR MIT
*/

#include <stdint.h>

#include "third_party/openxr/src/include/openxr/openxr.h"

#include "third_party/openxr/src/include/openxr/openxr_platform.h"
#include "third_party/openxr/src/include/openxr/openxr_platform_defines.h"

#ifndef XR_ANDROID_trackables
#define XR_ANDROID_trackables 1
#define XR_ANDROID_trackables_SPEC_VERSION 1
#define XR_ANDROID_TRACKABLES_EXTENSION_NAME "XR_ANDROID_trackables"

#define XR_TYPE_TRACKABLE_GET_INFO_ANDROID ((XrStructureType)1000455000U)
#define XR_TYPE_TRACKABLE_PLANE_ANDROID ((XrStructureType)1000455003U)
#define XR_TYPE_TRACKABLE_TRACKER_CREATE_INFO_ANDROID \
  ((XrStructureType)1000455004U)
#define XR_TYPE_ANCHOR_SPACE_CREATE_INFO_ANDROID ((XrStructureType)1000455001U)

#define XR_ERROR_MISMATCHING_TRACKABLE_TYPE_ANDROID ((XrResult)-1000455000U)

#define XR_OBJECT_TYPE_TRACKABLE_TRACKER_ANDROID ((XrObjectType)1000455001U)

XR_DEFINE_ATOM(XrTrackableANDROID)
#define XR_NULL_TRACKABLE_ANDROID 0

XR_DEFINE_HANDLE(XrTrackableTrackerANDROID)

enum XrTrackingStateANDROID {
  XR_TRACKING_STATE_PAUSED_ANDROID = 0,
  XR_TRACKING_STATE_STOPPED_ANDROID = 1,
  XR_TRACKING_STATE_TRACKING_ANDROID = 2,
};

enum XrTrackableTypeANDROID {
  XR_TRACKABLE_TYPE_NOT_VALID_ANDROID = 0,
  XR_TRACKABLE_TYPE_PLANE_ANDROID = 1,
  XR_TRACKABLE_TYPE_DEPTH_ANDROID = 2,
};

enum XrPlaneTypeANDROID {
  XR_PLANE_TYPE_HORIZONTAL_DOWNWARD_FACING_ANDROID = 0,
  XR_PLANE_TYPE_HORIZONTAL_UPWARD_FACING_ANDROID = 1,
  XR_PLANE_TYPE_VERTICAL_ANDROID = 2,
  XR_PLANE_TYPE_ARBITRARY_ANDROID = 3,
};

typedef struct XrTrackableTrackerCreateInfoANDROID {
  XrStructureType type;
  const void* next;
  XrTrackableTypeANDROID
      trackableType;
} XrTrackableTrackerCreateInfoANDROID;

typedef struct XrTrackableGetInfoANDROID {
  XrStructureType type;
  void* next;
  XrTrackableANDROID trackable;
  XrSpace baseSpace;
  XrTime time;
} XrTrackableGetInfoANDROID;

typedef struct XrTrackablePlaneANDROID {
  XrStructureType type;
  void* next;
  XrTrackingStateANDROID
      trackingState;
  XrPosef centerPose;
  XrExtent2Df extents;
  XrPlaneTypeANDROID
      planeType;
  uint32_t
      deprecated;
  XrTrackableANDROID
      subsumedByPlane;
  XrTime lastUpdatedTime;
  uint32_t
      vertexCapacityInput;
  uint32_t*
      vertexCountOutput;
  XrVector2f* vertices;
} XrTrackablePlaneANDROID;

typedef XrResult(XRAPI_PTR* PFN_xrCreateTrackableTrackerANDROID)(
    XrSession session, const XrTrackableTrackerCreateInfoANDROID* createInfo,
    XrTrackableTrackerANDROID* trackableTracker);

typedef XrResult(XRAPI_PTR* PFN_xrDestroyTrackableTrackerANDROID)(
    XrTrackableTrackerANDROID trackableTracker);

typedef XrResult(XRAPI_PTR* PFN_xrGetAllTrackablesANDROID)(
    XrTrackableTrackerANDROID trackableTracker, uint32_t trackableCapacityInput,
    uint32_t* trackableCountOutput, XrTrackableANDROID* trackablesOutput);

typedef XrResult(XRAPI_PTR* PFN_xrGetTrackablePlaneANDROID)(
    XrTrackableTrackerANDROID trackableTracker,
    const XrTrackableGetInfoANDROID* getInfo,
    XrTrackablePlaneANDROID* planeOutput);

typedef struct XrAnchorSpaceCreateInfoANDROID {
  XrStructureType type;
  void* next;
  XrSpace space;
  XrTime time;
  XrPosef pose;
  XrTrackableANDROID trackable;
} XrAnchorSpaceCreateInfoANDROID;

typedef XrResult(XRAPI_PTR* PFN_xrCreateAnchorSpaceANDROID)(
    XrSession session, const XrAnchorSpaceCreateInfoANDROID* createInfo,
    XrSpace* anchorOutput);
#endif // XR_ANDROID_trackables

#ifndef XR_ANDROID_raycast
#define XR_ANDROID_raycast 1
#define XR_ANDROID_raycast_SPEC_VERSION 1
#define XR_ANDROID_RAYCAST_EXTENSION_NAME "XR_ANDROID_raycast"

#define XR_TYPE_RAYCAST_INFO_ANDROID ((XrStructureType)1000463000U)
#define XR_TYPE_RAYCAST_HIT_RESULTS_ANDROID ((XrStructureType)1000463001U)

typedef struct XrRaycastInfoANDROID {
  XrStructureType type;
  void* next;
  uint32_t maxResults;
  uint32_t trackerCount;
  const XrTrackableTrackerANDROID* trackers;
  XrVector3f origin;
  XrVector3f trajectory;
  XrSpace space;
  XrTime time;
} XrRaycastInfoANDROID;

typedef struct XrRaycastHitResultANDROID {
  XrTrackableTypeANDROID type;
  XrTrackableANDROID trackable;
  XrPosef pose;
} XrRaycastHitResultANDROID;

typedef struct XrRaycastHitResultsANDROID {
  XrStructureType type;
  void* next;
  uint32_t
      resultsCapacityInput;
  uint32_t
      resultsCountOutput;
  XrRaycastHitResultANDROID* results;
} XrRaycastHitResultsANDROID;

typedef XrResult(XRAPI_PTR* PFN_xrRaycastANDROID)(
    XrSession session, const XrRaycastInfoANDROID* rayInfo,
    XrRaycastHitResultsANDROID* results);
#endif // XR_ANDROID_raycast

#ifndef XR_ANDROID_unbounded_reference_space
#define XR_ANDROID_unbounded_reference_space 1
#define XR_ANDROID_unbounded_reference_space_SPEC_VERSION 1
#define XR_ANDROID_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME "XR_ANDROID_unbounded_reference_space"
#define XR_REFERENCE_SPACE_TYPE_UNBOUNDED_ANDROID ((XrReferenceSpaceType) 1000467000U)
#endif /* XR_ANDROID_unbounded_reference_space */

#ifndef XR_ANDROID_reference_space_bounds_polygon
#define XR_ANDROID_reference_space_bounds_polygon 1
#define XR_ANDROID_reference_space_bounds_polygon_SPEC_VERSION 1
#define XR_ANDROID_REFERENCE_SPACE_BOUNDS_POLYGON_EXTENSION_NAME "XR_ANDROID_reference_space_bounds_polygon"
typedef XrResult (XRAPI_PTR *PFN_xrGetReferenceSpaceBoundsPolygonANDROID)(XrSession session, XrReferenceSpaceType referenceSpaceType, uint32_t boundaryVerticesCapacityInput, uint32_t* boundaryVerticesCountOutput, XrVector2f* boundaryVertices);
#endif /* XR_ANDROID_reference_space_bounds_polygon */

#endif // THIRD_PARTY_OPENXR_DEV_XR_ANDROID_H_
