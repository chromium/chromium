// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wp_color_manager.h"

#include <color-management-v1-server-protocol.h>

#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

constexpr uint32_t kTestWpColorManagerVersion = 1;

// An image description. `params` is set for descriptions created by the client
// from a parametric creator, and unset for descriptions created by the server
// (e.g. for outputs or surface feedback).
class TestImageDescription : public ServerObject {
 public:
  TestImageDescription(wl_resource* resource,
                       std::optional<TestImageDescriptionParams> params)
      : ServerObject(resource), params_(std::move(params)) {}
  ~TestImageDescription() override = default;

  const std::optional<TestImageDescriptionParams>& params() const {
    return params_;
  }

 private:
  const std::optional<TestImageDescriptionParams> params_;
};

void ImageDescriptionGetInformation(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id) {
  wl_resource* info =
      wl_resource_create(client, &wp_image_description_info_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!info) {
    wl_client_post_no_memory(client);
    return;
  }
  // Describe an SDR sRGB display.
  wp_image_description_info_v1_send_primaries_named(
      info, WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
  wp_image_description_info_v1_send_tf_named(
      info, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
  wp_image_description_info_v1_send_luminances(info, 2000, 80, 80);
  wp_image_description_info_v1_send_done(info);
  // `done` is a destructor event.
  wl_resource_destroy(info);
}

constexpr struct wp_image_description_v1_interface kImageDescriptionImpl = {
    .destroy = DestroyResource,
    .get_information = ImageDescriptionGetInformation,
};

// Creates an image description resource and sends `ready`.
void CreateImageDescription(wl_client* client,
                            wl_resource* parent,
                            uint32_t id,
                            TestWpColorManager* manager,
                            std::optional<TestImageDescriptionParams> params) {
  wl_resource* description = CreateResourceWithImpl<TestImageDescription>(
      client, &wp_image_description_v1_interface,
      wl_resource_get_version(parent), &kImageDescriptionImpl, id,
      base::DoNothing(), std::move(params));
  if (!description) {
    return;
  }
  wp_image_description_v1_send_ready(description, manager->NextIdentity());
}

// wp_image_description_creator_params_v1
class TestImageDescriptionCreatorParams : public ServerObject {
 public:
  TestImageDescriptionCreatorParams(wl_resource* resource,
                                    TestWpColorManager* manager)
      : ServerObject(resource), manager_(manager) {}
  ~TestImageDescriptionCreatorParams() override = default;

  TestWpColorManager* manager() { return manager_; }
  TestImageDescriptionParams& params() { return params_; }

 private:
  const raw_ptr<TestWpColorManager> manager_;
  TestImageDescriptionParams params_;
};

TestImageDescriptionParams& GetParams(wl_resource* resource) {
  return GetUserDataAs<TestImageDescriptionCreatorParams>(resource)->params();
}

void CreatorCreate(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* creator = GetUserDataAs<TestImageDescriptionCreatorParams>(resource);
  TestWpColorManager* manager = creator->manager();
  const TestImageDescriptionParams params = creator->params();
  // Like mutter, reject a reference luminance above the max luminance (the
  // protocol allows it with extended_target_volume, which isn't advertised).
  if (params.luminances &&
      params.luminances->reference > params.luminances->max) {
    wl_resource_post_error(
        resource,
        WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
        "reference luminance above max luminance");
    return;
  }
  // Like Weston, reject max_cll or max_fall above the mastering peak.
  if (params.mastering_luminance &&
      ((params.max_cll && *params.max_cll > params.mastering_luminance->max) ||
       (params.max_fall &&
        *params.max_fall > params.mastering_luminance->max))) {
    wl_resource_post_error(
        resource,
        WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
        "max_cll or max_fall above mastering max luminance");
    return;
  }
  manager->OnDescriptionCreated(params);
  CreateImageDescription(client, resource, id, manager, params);
  // `create` is a destructor request.
  wl_resource_destroy(resource);
}

void CreatorSetTfNamed(wl_client* client, wl_resource* resource, uint32_t tf) {
  GetParams(resource).tf_named = tf;
}

void CreatorSetTfPower(wl_client* client,
                       wl_resource* resource,
                       uint32_t eexp) {
  GetParams(resource).tf_power = eexp;
}

void CreatorSetPrimariesNamed(wl_client* client,
                              wl_resource* resource,
                              uint32_t primaries) {
  GetParams(resource).primaries_named = primaries;
}

void CreatorSetPrimaries(wl_client* client,
                         wl_resource* resource,
                         int32_t r_x,
                         int32_t r_y,
                         int32_t g_x,
                         int32_t g_y,
                         int32_t b_x,
                         int32_t b_y,
                         int32_t w_x,
                         int32_t w_y) {
  GetParams(resource).primaries =
      std::array<int32_t, 8>{r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y};
}

void CreatorSetLuminances(wl_client* client,
                          wl_resource* resource,
                          uint32_t min_lum,
                          uint32_t max_lum,
                          uint32_t reference_lum) {
  if (max_lum * 10000.0 <= min_lum || reference_lum * 10000.0 <= min_lum) {
    wl_resource_post_error(
        resource,
        WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
        "invalid luminance");
    return;
  }
  GetParams(resource).luminances = TestImageDescriptionParams::Luminances{
      .min = min_lum, .max = max_lum, .reference = reference_lum};
}

void CreatorSetMasteringDisplayPrimaries(wl_client* client,
                                         wl_resource* resource,
                                         int32_t r_x,
                                         int32_t r_y,
                                         int32_t g_x,
                                         int32_t g_y,
                                         int32_t b_x,
                                         int32_t b_y,
                                         int32_t w_x,
                                         int32_t w_y) {
  GetParams(resource).mastering_display_primaries =
      std::array<int32_t, 8>{r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y};
}

void CreatorSetMasteringLuminance(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t min_lum,
                                  uint32_t max_lum) {
  if (max_lum * 10000.0 <= min_lum) {
    wl_resource_post_error(
        resource,
        WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
        "invalid mastering luminance");
    return;
  }
  GetParams(resource).mastering_luminance =
      TestImageDescriptionParams::MasteringLuminance{.min = min_lum,
                                                     .max = max_lum};
}

void CreatorSetMaxCll(wl_client* client,
                      wl_resource* resource,
                      uint32_t max_cll) {
  GetParams(resource).max_cll = max_cll;
}

void CreatorSetMaxFall(wl_client* client,
                       wl_resource* resource,
                       uint32_t max_fall) {
  GetParams(resource).max_fall = max_fall;
}

constexpr struct wp_image_description_creator_params_v1_interface
    kCreatorParamsImpl = {
        .create = CreatorCreate,
        .set_tf_named = CreatorSetTfNamed,
        .set_tf_power = CreatorSetTfPower,
        .set_primaries_named = CreatorSetPrimariesNamed,
        .set_primaries = CreatorSetPrimaries,
        .set_luminances = CreatorSetLuminances,
        .set_mastering_display_primaries = CreatorSetMasteringDisplayPrimaries,
        .set_mastering_luminance = CreatorSetMasteringLuminance,
        .set_max_cll = CreatorSetMaxCll,
        .set_max_fall = CreatorSetMaxFall,
};

// wp_color_management_output_v1
class TestColorManagementOutput : public ServerObject {
 public:
  TestColorManagementOutput(wl_resource* resource, TestWpColorManager* manager)
      : ServerObject(resource), manager_(manager) {}
  ~TestColorManagementOutput() override = default;

  TestWpColorManager* manager() { return manager_; }

 private:
  const raw_ptr<TestWpColorManager> manager_;
};

void OutputGetImageDescription(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  CreateImageDescription(
      client, resource, id,
      GetUserDataAs<TestColorManagementOutput>(resource)->manager(),
      std::nullopt);
}

constexpr struct wp_color_management_output_v1_interface kOutputImpl = {
    .destroy = DestroyResource,
    .get_image_description = OutputGetImageDescription,
};

// wp_color_management_surface_v1
class TestColorManagementSurface : public ServerObject {
 public:
  TestColorManagementSurface(wl_resource* resource,
                             TestWpColorManager* manager,
                             uint32_t surface_id)
      : ServerObject(resource), manager_(manager), surface_id_(surface_id) {}
  ~TestColorManagementSurface() override = default;

  TestWpColorManager* manager() { return manager_; }
  uint32_t surface_id() const { return surface_id_; }

 private:
  const raw_ptr<TestWpColorManager> manager_;
  const uint32_t surface_id_;
};

void SurfaceSetImageDescription(wl_client* client,
                                wl_resource* resource,
                                wl_resource* image_description,
                                uint32_t render_intent) {
  auto* surface = GetUserDataAs<TestColorManagementSurface>(resource);
  auto* description = GetUserDataAs<TestImageDescription>(image_description);
  surface->manager()->OnSurfaceImageDescriptionSet(surface->surface_id(),
                                                   description->params());
}

void SurfaceUnsetImageDescription(wl_client* client, wl_resource* resource) {
  auto* surface = GetUserDataAs<TestColorManagementSurface>(resource);
  surface->manager()->OnSurfaceImageDescriptionSet(surface->surface_id(),
                                                   std::nullopt);
}

constexpr struct wp_color_management_surface_v1_interface kSurfaceImpl = {
    .destroy = DestroyResource,
    .set_image_description = SurfaceSetImageDescription,
    .unset_image_description = SurfaceUnsetImageDescription,
};

// wp_color_management_surface_feedback_v1
void FeedbackGetPreferred(wl_client* client,
                          wl_resource* resource,
                          uint32_t id) {
  CreateImageDescription(
      client, resource, id,
      GetUserDataAs<TestColorManagementOutput>(resource)->manager(),
      std::nullopt);
}

constexpr struct wp_color_management_surface_feedback_v1_interface
    kFeedbackImpl = {
        .destroy = DestroyResource,
        .get_preferred = FeedbackGetPreferred,
        .get_preferred_parametric = FeedbackGetPreferred,
};

// wp_color_manager_v1
TestWpColorManager* GetManager(wl_resource* resource) {
  return GetUserDataAs<TestWpColorManager>(resource);
}

void ManagerGetOutput(wl_client* client,
                      wl_resource* resource,
                      uint32_t id,
                      wl_resource* output) {
  CreateResourceWithImpl<TestColorManagementOutput>(
      client, &wp_color_management_output_v1_interface,
      wl_resource_get_version(resource), &kOutputImpl, id, base::DoNothing(),
      GetManager(resource));
}

void ManagerGetSurface(wl_client* client,
                       wl_resource* resource,
                       uint32_t id,
                       wl_resource* surface) {
  CreateResourceWithImpl<TestColorManagementSurface>(
      client, &wp_color_management_surface_v1_interface,
      wl_resource_get_version(resource), &kSurfaceImpl, id, base::DoNothing(),
      GetManager(resource), wl_resource_get_id(surface));
}

void ManagerGetSurfaceFeedback(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* surface) {
  // The feedback object only needs access to the manager, so reuse
  // TestColorManagementOutput as its user data.
  CreateResourceWithImpl<TestColorManagementOutput>(
      client, &wp_color_management_surface_feedback_v1_interface,
      wl_resource_get_version(resource), &kFeedbackImpl, id, base::DoNothing(),
      GetManager(resource));
}

void ManagerCreateIccCreator(wl_client* client,
                             wl_resource* resource,
                             uint32_t id) {
  wl_resource_post_error(resource,
                         WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
                         "ICC is not supported");
}

void ManagerCreateParametricCreator(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id) {
  CreateResourceWithImpl<TestImageDescriptionCreatorParams>(
      client, &wp_image_description_creator_params_v1_interface,
      wl_resource_get_version(resource), &kCreatorParamsImpl, id,
      base::DoNothing(), GetManager(resource));
}

void ManagerCreateWindowsScrgb(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  wl_resource_post_error(resource,
                         WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
                         "Windows scRGB is not supported");
}

void ManagerGetImageDescription(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* reference) {
  wl_resource_post_error(resource,
                         WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
                         "get_image_description is not supported");
}

void ManagerCreateWindowsBt2100(wl_client* client,
                                wl_resource* resource,
                                uint32_t id) {
  wl_resource_post_error(resource,
                         WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
                         "Windows BT.2100 is not supported");
}

constexpr struct wp_color_manager_v1_interface kTestWpColorManagerImpl = {
    .destroy = DestroyResource,
    .get_output = ManagerGetOutput,
    .get_surface = ManagerGetSurface,
    .get_surface_feedback = ManagerGetSurfaceFeedback,
    .create_icc_creator = ManagerCreateIccCreator,
    .create_parametric_creator = ManagerCreateParametricCreator,
    .create_windows_scrgb = ManagerCreateWindowsScrgb,
    .get_image_description = ManagerGetImageDescription,
    .create_windows_bt2100 = ManagerCreateWindowsBt2100,
};

}  // namespace

TestImageDescriptionParams::TestImageDescriptionParams() = default;
TestImageDescriptionParams::TestImageDescriptionParams(
    const TestImageDescriptionParams&) = default;
TestImageDescriptionParams& TestImageDescriptionParams::operator=(
    const TestImageDescriptionParams&) = default;
TestImageDescriptionParams::~TestImageDescriptionParams() = default;

TestWpColorManager::TestWpColorManager()
    : GlobalObject(&wp_color_manager_v1_interface,
                   &kTestWpColorManagerImpl,
                   kTestWpColorManagerVersion),
      supported_features_({
          WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC,
          WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES,
          WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES,
          WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES,
      }) {}

TestWpColorManager::~TestWpColorManager() = default;

void TestWpColorManager::OnBind() {
  wp_color_manager_v1_send_supported_intent(
      resource(), WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
  for (uint32_t feature : supported_features_) {
    wp_color_manager_v1_send_supported_feature(resource(), feature);
  }
  for (uint32_t tf : {WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB,
                      WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22,
                      WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886,
                      WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ,
                      WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG,
                      WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR}) {
    wp_color_manager_v1_send_supported_tf_named(resource(), tf);
  }
  for (uint32_t primaries : {WP_COLOR_MANAGER_V1_PRIMARIES_SRGB,
                             WP_COLOR_MANAGER_V1_PRIMARIES_BT2020,
                             WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3}) {
    wp_color_manager_v1_send_supported_primaries_named(resource(), primaries);
  }
  wp_color_manager_v1_send_done(resource());
}

std::optional<TestImageDescriptionParams>
TestWpColorManager::GetSurfaceImageDescription(wl_resource* surface) const {
  auto it = surface_descriptions_.find(wl_resource_get_id(surface));
  if (it == surface_descriptions_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void TestWpColorManager::OnDescriptionCreated(
    const TestImageDescriptionParams& params) {
  created_descriptions_.push_back(params);
}

void TestWpColorManager::OnSurfaceImageDescriptionSet(
    uint32_t surface_id,
    std::optional<TestImageDescriptionParams> params) {
  surface_descriptions_[surface_id] = std::move(params);
}

}  // namespace wl
