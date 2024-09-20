// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_modifiers_filter.h"
#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace ui {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr uint32_t kNoModesConnectorId = 404;

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode = {0, 6, 0, 0, 0, 0, 4,     0,
                                      0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);
const gfx::Size kOverlaySize(kDefaultMode.hdisplay / 2,
                             kDefaultMode.vdisplay / 2);
const gfx::SizeF kDefaultModeSizeF(1.0, 1.0);

const std::string kGpuCrashLogTimeout =
    "Failed to modeset within " +
    base::NumberToString(kWaitForModesetTimeout.InSeconds()) +
    " s of the first page flip failure. Crashing GPU process.";

SkBitmap CreateBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  return bitmap;
}

class FakeFenceFD {
 public:
  FakeFenceFD();

  std::unique_ptr<gfx::GpuFence> GetGpuFence() const;
  void Signal() const;

 private:
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
};

// TODO(b/364634013): Create a test util file for ozone/drm and de-deuplicate
// EqTileProperty().
testing::Matcher<TileProperty> EqTileProperty(const TileProperty& expected) {
  return AllOf(Field(&TileProperty::group_id, Eq(expected.group_id)),
               Field(&TileProperty::scale_to_fit_display,
                     Eq(expected.scale_to_fit_display)),
               Field(&TileProperty::tile_size, Eq(expected.tile_size)),
               Field(&TileProperty::tile_layout, Eq(expected.tile_layout)),
               Field(&TileProperty::location, Eq(expected.location)));
}
}  // namespace

FakeFenceFD::FakeFenceFD() {
  int fds[2];
  base::CreateLocalNonBlockingPipe(fds);
  read_fd = base::ScopedFD(fds[0]);
  write_fd = base::ScopedFD(fds[1]);
}

std::unique_ptr<gfx::GpuFence> FakeFenceFD::GetGpuFence() const {
  gfx::GpuFenceHandle handle;
  handle.Adopt(base::ScopedFD(HANDLE_EINTR(dup(read_fd.get()))));
  return std::make_unique<gfx::GpuFence>(std::move(handle));
}

void FakeFenceFD::Signal() const {
  base::WriteFileDescriptor(write_fd.get(), "a");
}

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() = default;

  HardwareDisplayControllerTest(const HardwareDisplayControllerTest&) = delete;
  HardwareDisplayControllerTest& operator=(
      const HardwareDisplayControllerTest&) = delete;

  ~HardwareDisplayControllerTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void InitializeDrmDevice(
      bool use_atomic,
      size_t movable_planes = 0,
      const std::vector<uint64_t>& supported_modifiers = {},
      std::unique_ptr<DrmModifiersFilter> modifiers_filter = nullptr,
      bool has_size_hints = false);
  void SchedulePageFlip(DrmOverlayPlaneList planes);
  void OnSubmission(gfx::SwapResult swap_result,
                    gfx::GpuFenceHandle release_fence);
  void OnPresentation(const gfx::PresentationFeedback& feedback);
  uint64_t GetPlanePropertyValue(uint32_t plane,
                                 const std::string& property_name);

  scoped_refptr<DrmFramebuffer> CreateBuffer() {
    std::unique_ptr<GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, kDefaultModeSize, GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), kDefaultModeSize);
  }

  scoped_refptr<DrmFramebuffer> CreateOverlayBuffer() {
    std::unique_ptr<GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, kOverlaySize, GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), kOverlaySize);
  }

  std::vector<HardwareDisplayPlane*> GetMovableOverlays() {
    std::vector<HardwareDisplayPlane*> out;
    for (const auto& plane : drm_->plane_manager()->planes()) {
      if (plane->GetCompatibleCrtcIds().size() > 1) {
        out.push_back(plane.get());
      }
    }
    return out;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

 protected:
  bool ModesetWithPlanes(const DrmOverlayPlaneList& modeset_planes,
                         const drmModeModeInfo& mode = kDefaultMode);
  bool DisableController();

  std::unique_ptr<HardwareDisplayController> controller_;
  scoped_refptr<FakeDrmDevice> drm_;
  std::unique_ptr<DrmModifiersFilter> modifiers_filter_;

  int successful_page_flips_count_ = 0;
  gfx::SwapResult last_swap_result_;
  gfx::PresentationFeedback last_presentation_feedback_;

  uint32_t primary_crtc_ = 0;
  uint32_t secondary_crtc_ = 0;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void HardwareDisplayControllerTest::SetUp() {
  // Enable the |kUseDynamicCursorSize| feature for test.
  scoped_feature_list_.InitAndEnableFeature(ui::kUseDynamicCursorSize);
  successful_page_flips_count_ = 0;
  last_swap_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm_device = std::make_unique<MockGbmDevice>();
  drm_ = new FakeDrmDevice(std::move(gbm_device));
  InitializeDrmDevice(/* use_atomic= */ true);
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_->ResetPlaneManagerForTesting();
  drm_->ClearCallbacks();
  drm_ = nullptr;
}

void HardwareDisplayControllerTest::InitializeDrmDevice(
    bool use_atomic,
    size_t movable_planes,
    const std::vector<uint64_t>& supported_modifiers,
    std::unique_ptr<DrmModifiersFilter> modifiers_filter,
    bool has_size_hints) {
  // This will change the plane_manager of the drm.
  // HardwareDisplayController is tied to the plane_manager CRTC states.
  // Destruct the controller before destructing the plane manager its CRTC
  // controllers are tied to.
  controller_ = nullptr;
  modifiers_filter_ = std::move(modifiers_filter);

  // Set up the default property blob for in formats:
  std::vector<drm_format_modifier> drm_format_modifiers;
  for (const auto modifier : supported_modifiers) {
    drm_format_modifiers.push_back(
        {.formats = 1, .offset = 0, .pad = 0, .modifier = modifier});
  }

  drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc*/ 2, movable_planes,
      {DRM_FORMAT_XRGB8888}, drm_format_modifiers);

  if (has_size_hints) {
    std::vector<gfx::Size> supported_cursor_sizes = {
        gfx::Size(64, 64), gfx::Size(128, 128), gfx::Size(256, 256)};
    for (FakeDrmDevice::PlaneProperties plane : drm_->plane_properties()) {
      for (DrmWrapper::Property property : plane.properties) {
        if (property.id == kTypePropId &&
            property.value == DRM_PLANE_TYPE_CURSOR) {
          ScopedDrmPropertyBlob size_hints_blob =
              drm_->CreateSizeHintsBlob(supported_cursor_sizes);
          drm_->AddProperty(plane.id, {.id = kSizeHintsPropId,
                                       .value = size_hints_blob->id()});
        }
      }
    }
  }

  // Add one connected connector with no modes (sterile).
  auto& connector_props = drm_->AddConnector();
  connector_props.id = kNoModesConnectorId;
  connector_props.connection = true;

  drm_->AddProperty(drm_->crtc_property(0).id,
                    {.id = kVrrEnabledPropId, .value = 0});
  drm_->InitializeState(use_atomic);
  primary_crtc_ = drm_->crtc_property(0).id;
  secondary_crtc_ = drm_->crtc_property(1).id;

  // Initialize a new HardwareDisplayController with the new Plane Manager of
  // the DRM.
  controller_ = std::make_unique<HardwareDisplayController>(
      std::make_unique<CrtcController>(drm_.get(), primary_crtc_,
                                       kConnectorIdBase),
      gfx::Point(), modifiers_filter_.get());
}

bool HardwareDisplayControllerTest::ModesetWithPlanes(
    const DrmOverlayPlaneList& modeset_planes,
    const drmModeModeInfo& mode) {
  CommitRequest commit_request;
  controller_->GetModesetProps(&commit_request, modeset_planes, mode,
                               /*enable_vrr=*/false);
  CommitRequest request_for_update = commit_request;
  bool status = drm_->plane_manager()->Commit(std::move(commit_request),
                                              DRM_MODE_ATOMIC_ALLOW_MODESET);
  for (const CrtcCommitRequest& crtc_request : request_for_update) {
    controller_->UpdateState(crtc_request);
  }

  return status;
}

bool HardwareDisplayControllerTest::DisableController() {
  CommitRequest commit_request;
  controller_->GetDisableProps(&commit_request);
  CommitRequest request_for_update = commit_request;
  bool status = drm_->plane_manager()->Commit(std::move(commit_request),
                                              DRM_MODE_ATOMIC_ALLOW_MODESET);
  for (const CrtcCommitRequest& crtc_request : request_for_update) {
    controller_->UpdateState(crtc_request);
  }

  return status;
}

void HardwareDisplayControllerTest::SchedulePageFlip(
    DrmOverlayPlaneList planes) {
  controller_->SchedulePageFlip(
      std::move(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
}

void HardwareDisplayControllerTest::OnSubmission(
    gfx::SwapResult result,
    gfx::GpuFenceHandle release_fence) {
  last_swap_result_ = result;
}

void HardwareDisplayControllerTest::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  if (!feedback.failed()) {
    successful_page_flips_count_++;
  }
  last_presentation_feedback_ = feedback;
}

uint64_t HardwareDisplayControllerTest::GetPlanePropertyValue(
    uint32_t plane,
    const std::string& property_name) {
  DrmWrapper::Property p{};
  ScopedDrmObjectPropertyPtr properties(
      drm_->GetObjectProperties(plane, DRM_MODE_OBJECT_PLANE));
  EXPECT_TRUE(
      GetDrmPropertyForName(drm_.get(), properties.get(), property_name, &p));
  return p.value;
}

class HardwareDisplayControllerMockedDeviceTest
    : public HardwareDisplayControllerTest {
  void SetUp() override {
    successful_page_flips_count_ = 0;
    last_swap_result_ = gfx::SwapResult::SWAP_FAILED;

    auto gbm_device = std::make_unique<MockGbmDevice>();
    drm_ = new testing::NiceMock<MockDrmDevice>(base::FilePath(),
                                                std::move(gbm_device), true);
    InitializeDrmDevice(/* use_atomic= */ true);
  }
};

TEST_F(HardwareDisplayControllerTest, CheckModesettingResult) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_FALSE(
      DrmOverlayPlane::GetPrimaryPlane(modeset_planes)->buffer->HasOneRef());
}

TEST_F(HardwareDisplayControllerTest, CrtcPropsAfterModeset) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ScopedDrmObjectPropertyPtr crtc_props =
      drm_->GetObjectProperties(primary_crtc_, DRM_MODE_OBJECT_CRTC);
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), crtc_props.get(), "ACTIVE", &prop);
    EXPECT_EQ(kActivePropId, prop.id);
    EXPECT_EQ(1U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), crtc_props.get(), "MODE_ID", &prop);
    EXPECT_EQ(kModePropId, prop.id);
    EXPECT_GT(prop.value, 0U);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), crtc_props.get(), "VRR_ENABLED", &prop);
    EXPECT_EQ(kVrrEnabledPropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
}

TEST_F(HardwareDisplayControllerTest, ConnectorPropsAfterModeset) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ScopedDrmObjectPropertyPtr connector_props =
      drm_->GetObjectProperties(kConnectorIdBase, DRM_MODE_OBJECT_CONNECTOR);

  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), connector_props.get(), "CRTC_ID", &prop);
    EXPECT_EQ(kCrtcIdPropId, prop.id);
    EXPECT_EQ(kCrtcIdBase, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), connector_props.get(), "link-status",
                          &prop);
    EXPECT_EQ(kLinkStatusPropId, prop.id);
    EXPECT_EQ(static_cast<uint64_t>(DRM_MODE_LINK_STATUS_GOOD), prop.value);
  }
}

TEST_F(HardwareDisplayControllerTest, BadLinkStatusConnectorPropsAfterModeset) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ScopedDrmObjectPropertyPtr bad_link_connector_props =
      drm_->GetObjectProperties(kNoModesConnectorId, DRM_MODE_OBJECT_CONNECTOR);
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), bad_link_connector_props.get(),
                          "link-status", &prop);
    EXPECT_EQ(kLinkStatusPropId, prop.id);
    EXPECT_EQ(static_cast<uint64_t>(DRM_MODE_LINK_STATUS_BAD), prop.value);
  }
}

TEST_F(HardwareDisplayControllerTest, PlanePropsAfterModeset) {
  const FakeFenceFD fake_fence_fd;
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(), gfx::ColorSpace::CreateSRGB(),
                                 fake_fence_fd.GetGpuFence()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ScopedDrmObjectPropertyPtr plane_props =
      drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
  const DrmOverlayPlane* primary_plane =
      DrmOverlayPlane::GetPrimaryPlane(modeset_planes);

  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_ID", &prop);
    EXPECT_EQ(kPlaneCrtcId, prop.id);
    EXPECT_EQ(kCrtcIdBase, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_X", &prop);
    EXPECT_EQ(kCrtcX, prop.id);
    EXPECT_EQ(primary_plane->display_bounds.x(), static_cast<int>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_Y", &prop);
    EXPECT_EQ(kCrtcY, prop.id);
    EXPECT_EQ(primary_plane->display_bounds.y(), static_cast<int>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_W", &prop);
    EXPECT_EQ(kCrtcW, prop.id);
    EXPECT_EQ(kDefaultModeSize.width(), static_cast<int>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_H", &prop);
    EXPECT_EQ(kCrtcH, prop.id);
    EXPECT_EQ(kDefaultModeSize.height(), static_cast<int>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "FB_ID", &prop);
    EXPECT_EQ(kPlaneFbId, prop.id);
    EXPECT_EQ(primary_plane->buffer->opaque_framebuffer_id(),
              static_cast<uint32_t>(prop.value));
  }

  gfx::RectF crop_rectf = primary_plane->crop_rect;
  crop_rectf.Scale(primary_plane->buffer->size().width(),
                   primary_plane->buffer->size().height());
  gfx::Rect crop_rect = gfx::ToNearestRect(crop_rectf);
  gfx::Rect fixed_point_rect =
      gfx::Rect(crop_rect.x() << 16, crop_rect.y() << 16,
                crop_rect.width() << 16, crop_rect.height() << 16);
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_X", &prop);
    EXPECT_EQ(kSrcX, prop.id);
    EXPECT_EQ(fixed_point_rect.x(), static_cast<float>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_Y", &prop);
    EXPECT_EQ(kSrcY, prop.id);
    EXPECT_EQ(fixed_point_rect.y(), static_cast<float>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_W", &prop);
    EXPECT_EQ(kSrcW, prop.id);
    EXPECT_EQ(fixed_point_rect.width(), static_cast<int>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_H", &prop);
    EXPECT_EQ(kSrcH, prop.id);
    EXPECT_EQ(fixed_point_rect.height(), static_cast<int>(prop.value));
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD", &prop);
    EXPECT_EQ(kInFencePropId, prop.id);
    EXPECT_GT(static_cast<int>(prop.value), base::kInvalidPlatformFile);
  }
}

TEST_F(HardwareDisplayControllerTest, FenceFdValueChange) {
  DrmOverlayPlaneList modeset_planes;
  DrmOverlayPlane plane(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(plane.Clone());
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  // Test invalid fence fd
  {
    DrmWrapper::Property fence_fd_prop = {};
    ScopedDrmObjectPropertyPtr plane_props =
        drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                          &fence_fd_prop);
    EXPECT_EQ(kInFencePropId, fence_fd_prop.id);
    EXPECT_EQ(base::kInvalidPlatformFile,
              static_cast<int>(fence_fd_prop.value));
  }

  const FakeFenceFD fake_fence_fd;
  plane.gpu_fence = fake_fence_fd.GetGpuFence();
  std::vector<DrmOverlayPlane> planes = {};
  planes.push_back(plane.Clone());
  SchedulePageFlip(std::move(planes));

  // Verify fence FD after a GPU Fence is added to the plane.
  {
    DrmWrapper::Property fence_fd_prop = {};
    ScopedDrmObjectPropertyPtr plane_props =
        drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                          &fence_fd_prop);
    EXPECT_EQ(kInFencePropId, fence_fd_prop.id);
    EXPECT_LT(base::kInvalidPlatformFile,
              static_cast<int>(fence_fd_prop.value));
  }

  plane.gpu_fence = nullptr;
  modeset_planes.clear();
  modeset_planes.push_back(plane.Clone());
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  // Test an invalid FD again after the fence is removed.
  {
    DrmWrapper::Property fence_fd_prop = {};
    ScopedDrmObjectPropertyPtr plane_props =
        drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                          &fence_fd_prop);
    EXPECT_EQ(kInFencePropId, fence_fd_prop.id);
    EXPECT_EQ(base::kInvalidPlatformFile,
              static_cast<int>(fence_fd_prop.value));
  }
}

TEST_F(HardwareDisplayControllerTest, CheckDisableResetsProps) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  // Test props values after disabling.
  DisableController();

  ScopedDrmObjectPropertyPtr crtc_props =
      drm_->GetObjectProperties(primary_crtc_, DRM_MODE_OBJECT_CRTC);
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), crtc_props.get(), "ACTIVE", &prop);
    EXPECT_EQ(kActivePropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), crtc_props.get(), "MODE_ID", &prop);
    EXPECT_EQ(kModePropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), crtc_props.get(), "VRR_ENABLED", &prop);
    EXPECT_EQ(kVrrEnabledPropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }

  ScopedDrmObjectPropertyPtr connector_props =
      drm_->GetObjectProperties(kConnectorIdBase, DRM_MODE_OBJECT_CONNECTOR);
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), connector_props.get(), "CRTC_ID", &prop);
    EXPECT_EQ(kCrtcIdPropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }

  ScopedDrmObjectPropertyPtr plane_props =
      drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_ID", &prop);
    EXPECT_EQ(kPlaneCrtcId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_X", &prop);
    EXPECT_EQ(kCrtcX, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_Y", &prop);
    EXPECT_EQ(kCrtcY, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_W", &prop);
    EXPECT_EQ(kCrtcW, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_H", &prop);
    EXPECT_EQ(kCrtcH, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "FB_ID", &prop);
    EXPECT_EQ(kPlaneFbId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_X", &prop);
    EXPECT_EQ(kSrcX, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_Y", &prop);
    EXPECT_EQ(kSrcY, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_W", &prop);
    EXPECT_EQ(kSrcW, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_H", &prop);
    EXPECT_EQ(kSrcH, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    DrmWrapper::Property prop = {};
    GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD", &prop);
    EXPECT_EQ(kInFencePropId, prop.id);
    EXPECT_EQ(base::kInvalidPlatformFile, static_cast<int>(prop.value));
  }
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  DrmOverlayPlane page_flip_plane(DrmOverlayPlane::TestPlane(CreateBuffer()));
  std::vector<DrmOverlayPlane> page_flip_planes;
  page_flip_planes.push_back(page_flip_plane.Clone());

  SchedulePageFlip(std::move(page_flip_planes));

  drm_->RunCallbacks();
  EXPECT_TRUE(
      DrmOverlayPlane::GetPrimaryPlane(modeset_planes)->buffer->HasOneRef());
  EXPECT_FALSE(page_flip_plane.buffer->HasOneRef());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_EQ(2, drm_->get_commit_count());
  // Verify only the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  InitializeDrmDevice(/* use_atomic */ false);
  drm_->set_set_crtc_expectation(false);

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_FALSE(ModesetWithPlanes(modeset_planes));
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayPresent) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  planes.emplace_back(CreateOverlayBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
                      gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF),
                      true, nullptr);

  EXPECT_TRUE(ModesetWithPlanes(planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_EQ(2, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayTestMode) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  planes.emplace_back(CreateOverlayBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
                      gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF),
                      true, nullptr);

  EXPECT_TRUE(ModesetWithPlanes(planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  SchedulePageFlip(DrmOverlayPlane::Clone(planes));
  EXPECT_EQ(2, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));

  // A test call shouldn't cause new flips, but should succeed.
  EXPECT_TRUE(controller_->TestPageFlip(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_EQ(3, drm_->get_commit_count());

  // Regular flips should continue on normally.
  SchedulePageFlip(DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, successful_page_flips_count_);
  EXPECT_EQ(4, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, AcceptUnderlays) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  planes.emplace_back(CreateBuffer(), gfx::ColorSpace::CreateSRGB(), -1,
                      gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                      gfx::Rect(kDefaultModeSize),
                      gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, PageflipMirroredControllers) {
  controller_->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id));

  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  EXPECT_TRUE(ModesetWithPlanes(planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_EQ(2, drm_->get_commit_count());

  // Verify only the displays have a valid framebuffer on the primary plane.
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->type() == DRM_PLANE_TYPE_PRIMARY) {
      EXPECT_NE(0u, GetPlanePropertyValue(plane->id(), "FB_ID"));
    } else {
      EXPECT_EQ(0u, GetPlanePropertyValue(plane->id(), "FB_ID"));
    }
  }
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterRemoveCrtc) {
  controller_->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id));

  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);

  const HardwareDisplayPlane* primary_crtc_plane = nullptr;
  const HardwareDisplayPlane* secondary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && plane->owning_crtc() == primary_crtc_) {
      primary_crtc_plane = plane.get();
    }
    if (plane->in_use() && plane->owning_crtc() == secondary_crtc_) {
      secondary_crtc_plane = plane.get();
    }
  }

  ASSERT_NE(nullptr, primary_crtc_plane);
  ASSERT_NE(nullptr, secondary_crtc_plane);
  EXPECT_EQ(primary_crtc_, primary_crtc_plane->owning_crtc());
  EXPECT_EQ(secondary_crtc_, secondary_crtc_plane->owning_crtc());

  // Removing the crtc should free the plane.
  std::unique_ptr<CrtcController> crtc =
      controller_->RemoveCrtc(drm_, primary_crtc_);
  EXPECT_FALSE(primary_crtc_plane->in_use());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(secondary_crtc_, secondary_crtc_plane->owning_crtc());

  // Check that controller doesn't affect the state of removed plane in
  // subsequent page flip.
  SchedulePageFlip(DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, successful_page_flips_count_);
  EXPECT_FALSE(primary_crtc_plane->in_use());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(secondary_crtc_, secondary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterDestroyingCrtc) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);

  const HardwareDisplayPlane* owned_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use()) {
      owned_plane = plane.get();
    }
  }
  ASSERT_TRUE(owned_plane != nullptr);
  EXPECT_EQ(primary_crtc_, owned_plane->owning_crtc());
  std::unique_ptr<CrtcController> crtc =
      controller_->RemoveCrtc(drm_, primary_crtc_);
  // Destroying crtc should free the plane.
  crtc.reset();
  uint32_t crtc_nullid = 0;
  EXPECT_FALSE(owned_plane->in_use());
  EXPECT_EQ(crtc_nullid, owned_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterAddCrtc) {
  controller_->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id));

  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);

  HardwareDisplayPlane* primary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && primary_crtc_ == plane->owning_crtc()) {
      primary_crtc_plane = plane.get();
    }
  }

  ASSERT_TRUE(primary_crtc_plane != nullptr);

  auto hdc_controller = std::make_unique<HardwareDisplayController>(
      controller_->RemoveCrtc(drm_, primary_crtc_), controller_->origin(),
      nullptr);
  SchedulePageFlip(DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, successful_page_flips_count_);
  EXPECT_FALSE(primary_crtc_plane->in_use());

  // We reset state of plane here to test that the plane was actually added to
  // hdc_controller. In which case, the right state should be set to plane
  // after page flip call is handled by the controller.
  primary_crtc_plane->set_in_use(false);
  primary_crtc_plane->set_owning_crtc(0);
  hdc_controller->SchedulePageFlip(
      DrmOverlayPlane::Clone(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(3, successful_page_flips_count_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(primary_crtc_, primary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, ModesetWhilePageFlipping) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(DrmOverlayPlane::Clone(planes));

  EXPECT_TRUE(ModesetWithPlanes(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, FailPageFlippingWithNoSavingModeset) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  std::vector<DrmOverlayPlane> page_flip_planes;
  page_flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  // Page flip fails, so a GPU process self-destruct sequence is initiated.
  drm_->set_commit_expectation(false);
  SchedulePageFlip(std::move(page_flip_planes));

  // Since no modeset event was detected, death occurs after
  // |kWaitForModesetTimeout| seconds.
  EXPECT_DEATH_IF_SUPPORTED(
      task_environment_.FastForwardBy(kWaitForModesetTimeout),
      kGpuCrashLogTimeout);
}

TEST_F(HardwareDisplayControllerTest, FailPageFlippingWithSavingModeset) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  std::vector<DrmOverlayPlane> page_flip_planes;
  page_flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  // Page flip fails, so a GPU process self-destruct sequence is initiated.
  drm_->set_commit_expectation(false);
  SchedulePageFlip(std::move(page_flip_planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_FAILED, last_swap_result_);
  EXPECT_EQ(0, successful_page_flips_count_);

  // Some time passes.
  task_environment_.FastForwardBy(base::Milliseconds(1623));

  // A modeset event occurs and prevents the GPU process from crashing.
  modeset_planes.clear();
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  // Ensure self-destruct time runs out without process death.
  task_environment_.FastForwardBy(kWaitForModesetTimeout);
}

TEST_F(HardwareDisplayControllerTest,
       RecreateBuffersOnOldPlanesPageFlipFailure) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(planes));

  // Page flip fails due to planes being allocated prior to the last modeset.
  drm_->set_commit_expectation(false);
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  // We recreate the buffers.
  EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
  EXPECT_EQ(0, successful_page_flips_count_);

  // Next page flip passes, so the GPU process is safe.
  drm_->set_commit_expectation(true);
  planes.clear();
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  SchedulePageFlip(std::move(planes));

  // Ensure self-destruct time runs out without process death.
  task_environment_.FastForwardBy(kWaitForModesetTimeout);

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, CheckNoPrimaryPlaneOnFlip) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  std::vector<DrmOverlayPlane> page_flip_planes;
  page_flip_planes.emplace_back(CreateBuffer(), gfx::ColorSpace::CreateSRGB(),
                                1, gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                                gfx::Rect(kDefaultModeSize),
                                gfx::RectF(0, 0, 1, 1), true, nullptr);
  SchedulePageFlip(std::move(page_flip_planes));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, PageFlipWithUnassignablePlanes) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  {
    std::vector<DrmOverlayPlane> page_flip_planes;
    page_flip_planes.emplace_back(CreateBuffer(), gfx::ColorSpace::CreateSRGB(),
                                  1, gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                                  gfx::Rect(kDefaultModeSize),
                                  gfx::RectF(0, 0, 1, 1), true, nullptr);
    page_flip_planes.emplace_back(CreateBuffer(), gfx::ColorSpace::CreateSRGB(),
                                  1, gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                                  gfx::Rect(kDefaultModeSize),
                                  gfx::RectF(0, 0, 1, 1), true, nullptr);
    page_flip_planes.emplace_back(CreateBuffer(), gfx::ColorSpace::CreateSRGB(),
                                  1, gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                                  gfx::Rect(kDefaultModeSize),
                                  gfx::RectF(0, 0, 1, 1), true, nullptr);
    SchedulePageFlip(std::move(page_flip_planes));
  }

  drm_->RunCallbacks();

  // It's important we don't do any real DRM flips here, since we know
  // we can't allocate any planes, we avoid sending bad commits to the
  // drivers.
  EXPECT_EQ(0, drm_->get_page_flip_call_count());
  EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
}

TEST_F(HardwareDisplayControllerTest, SomePlaneAssignmentFailuresAreOk) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  constexpr int kUnassignableFlips = 3;

  for (size_t i = 0; i < kUnassignableFlips; ++i) {
    {
      std::vector<DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(0, successful_page_flips_count_);
    EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
  }

  for (size_t i = 0; i < kPageFlipWatcherHistorySize - kUnassignableFlips;
       ++i) {
    drm_->set_commit_expectation(true);
    {
      std::vector<DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    // +1 because we're comparing an index with a count.
    EXPECT_EQ(i + 1, static_cast<size_t>(successful_page_flips_count_));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  }

  // We should still be alive since we didn't submit too many unassignable page
  // flips.
  task_environment_.FastForwardBy(kWaitForModesetTimeout);
}

TEST_F(HardwareDisplayControllerTest, CrashOnTooManyFlakyPlaneAssignments) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  auto do_successful_flip = [&]() {
    {
      std::vector<DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
    EXPECT_FALSE(last_presentation_feedback_.failed());
  };

  auto do_failed_flip = [&]() {
    {
      std::vector<DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
    EXPECT_TRUE(last_presentation_feedback_.failed());
  };

  auto do_flake = [&]() {
    do_successful_flip();
    do_failed_flip();
  };

  auto flakes = kPlaneAssignmentFlakeThreshold;
  ASSERT_GT(kPageFlipWatcherHistorySize, flakes)
      << "Page flip history is too small to account for the maximum number of "
         "flakes";
  auto successes = kPageFlipWatcherHistorySize - (2 * flakes);

  for (size_t i = 0; i < successes; ++i) {
    do_successful_flip();
  }
  for (size_t i = 0; i < flakes; ++i) {
    do_flake();
  }

  EXPECT_DEATH_IF_SUPPORTED(
      do_flake(),
      base::StringPrintf("Plane assignment has flaked %d times, but the "
                         "threshold is %d. Crashing the GPU process.",
                         flakes, kPlaneAssignmentFlakeThreshold));
}

TEST_F(HardwareDisplayControllerTest, CrashOnTooManyFailedPlaneAssignments) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  auto do_successful_flip = [&]() {
    {
      std::vector<DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
    EXPECT_FALSE(last_presentation_feedback_.failed());
  };

  auto do_failed_flip = [&]() {
    {
      std::vector<DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
          gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(), gfx::Rect(kDefaultModeSize),
          gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
    EXPECT_TRUE(last_presentation_feedback_.failed());
  };

  auto failures = kPlaneAssignmentMaximumFailures;
  auto successes = kPageFlipWatcherHistorySize - failures;

  for (size_t i = 0; i < successes; ++i) {
    do_successful_flip();
  }
  for (size_t i = 0; i < (failures - 1); ++i) {
    do_failed_flip();
  }

  EXPECT_DEATH_IF_SUPPORTED(
      do_failed_flip(),
      base::StringPrintf("Plane assignment has failed %d/%d times, but the "
                         "threshold is %d. Crashing the GPU process.",
                         failures, kPageFlipWatcherHistorySize,
                         kPlaneAssignmentMaximumFailures));
}

TEST_F(HardwareDisplayControllerTest, AddCrtcMidPageFlip) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));

  controller_->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, kConnectorIdBase + 1));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, RemoveCrtcMidPageFlip) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));

  controller_->RemoveCrtc(drm_, primary_crtc_);

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, Disable) {
  // Page flipping overlays is only supported on atomic configurations.
  InitializeDrmDevice(/* use_atomic= */ true);

  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  planes.emplace_back(CreateOverlayBuffer(), gfx::ColorSpace::CreateSRGB(), 1,
                      gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF),
                      true, nullptr);
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);

  EXPECT_TRUE(DisableController());

  int planes_in_use = 0;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use()) {
      planes_in_use++;
    }
  }
  // No plane should be in use.
  ASSERT_EQ(0, planes_in_use);
}

TEST_F(HardwareDisplayControllerTest, PageflipAfterModeset) {
  DrmOverlayPlaneList planes;
  scoped_refptr<DrmFramebuffer> buffer = CreateBuffer();
  planes.push_back(DrmOverlayPlane::TestPlane(buffer));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  for (const auto& plane : planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(primary_crtc_)
                                   .modeset_framebuffers,
                               plane.buffer));
  }

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();

  // modeset_framebuffers should be cleared after the pageflip is complete.
  EXPECT_TRUE(drm_->plane_manager()
                  ->GetCrtcStateForCrtcId(primary_crtc_)
                  .modeset_framebuffers.empty());
}

TEST_F(HardwareDisplayControllerTest, PageflipBeforeModeset) {
  DrmOverlayPlaneList planes;
  scoped_refptr<DrmFramebuffer> buffer = CreateBuffer();
  planes.push_back(DrmOverlayPlane::TestPlane(buffer));
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(DrmOverlayPlane::Clone(planes));

  EXPECT_TRUE(ModesetWithPlanes(planes));
  for (const auto& plane : planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(primary_crtc_)
                                   .modeset_framebuffers,
                               plane.buffer));
  }

  // modeset_framebuffers should not be cleared when a pageflip callback is run
  // after a modeset
  drm_->RunCallbacks();
  EXPECT_FALSE(drm_->plane_manager()
                   ->GetCrtcStateForCrtcId(primary_crtc_)
                   .modeset_framebuffers.empty());
  for (const auto& plane : planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(primary_crtc_)
                                   .modeset_framebuffers,
                               plane.buffer));
  }
}

TEST_F(HardwareDisplayControllerTest, MultiplePlanesModeset) {
  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_EQ(drm_->plane_manager()
                ->GetCrtcStateForCrtcId(primary_crtc_)
                .modeset_framebuffers.size(),
            2UL);
  for (const auto& plane : modeset_planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(primary_crtc_)
                                   .modeset_framebuffers,
                               plane.buffer));
  }
}

TEST_F(HardwareDisplayControllerTest, CheckPinningAfterPageFlip) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/1);

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  DrmOverlayPlane page_flip_plane(DrmOverlayPlane::TestPlane(CreateBuffer()));
  std::vector<DrmOverlayPlane> page_flip_planes;
  page_flip_planes.push_back(page_flip_plane.Clone());
  page_flip_planes.push_back(page_flip_plane.Clone());
  page_flip_planes.push_back(page_flip_plane.Clone());

  SchedulePageFlip((std::move(page_flip_planes)));
  drm_->RunCallbacks();
  EXPECT_EQ(1, successful_page_flips_count_);

  size_t in_use_planes = 0;
  for (auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use()) {
      EXPECT_EQ(controller_->crtc_controllers()[0]->crtc(),
                plane->owning_crtc());
      in_use_planes++;
    }
  }
  EXPECT_EQ(3u, in_use_planes);
}

TEST_F(HardwareDisplayControllerTest, CheckPinningAfterFailedPageFlip) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/1);

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  // InitializeDrmDevice created 2 crtcs with 2 planes, plus a movable plane.
  // Try to fill 'em up:
  auto flip_all_planes = [&]() {
    DrmOverlayPlane page_flip_plane(DrmOverlayPlane::TestPlane(CreateBuffer()));

    std::vector<DrmOverlayPlane> page_flip_planes;
    page_flip_planes.push_back(page_flip_plane.Clone());
    page_flip_planes.push_back(page_flip_plane.Clone());
    page_flip_planes.push_back(page_flip_plane.Clone());

    SchedulePageFlip((std::move(page_flip_planes)));
    drm_->RunCallbacks();
  };

  flip_all_planes();
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_FALSE(last_presentation_feedback_.failed());

  drm_->set_commit_expectation(false);
  flip_all_planes();
  EXPECT_TRUE(last_presentation_feedback_.failed());

  size_t in_use_planes =
      base::ranges::count_if(drm_->plane_manager()->planes(),
                             [](const auto& plane) { return plane->in_use(); });
  EXPECT_EQ(0u, in_use_planes) << "Planes, including pinned planes, should not "
                                  "be in use after a failed flip.";
}

TEST_F(HardwareDisplayControllerTest,
       PinnedPlanesAreRespectedDuringModesetting) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/1);

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  HardwareDisplayPlane* movable_plane = GetMovableOverlays()[0];
  movable_plane->set_in_use(true);
  movable_plane->set_owning_crtc(drm_->crtc_property(1).id);

  ASSERT_FALSE(controller_->HasCrtc(drm_, movable_plane->owning_crtc()));
  EXPECT_FALSE(ModesetWithPlanes(modeset_planes))
      << "Modesetting should fail if it requires a movable plane that is "
         "already pinned to a different CRTC.";
  EXPECT_EQ(0, drm_->get_commit_count());

  movable_plane->set_in_use(false);
  movable_plane->set_owning_crtc(0);

  EXPECT_TRUE(ModesetWithPlanes(modeset_planes))
      << "Modesetting with movable planes should work once those movable "
         "planes are available to use.";
  EXPECT_EQ(1, drm_->get_commit_count());
}

TEST_F(HardwareDisplayControllerTest, AddingAndRemovingCrtcsWithMovablePlanes) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/1);

  controller_->AddCrtc(std::make_unique<CrtcController>(
      drm_, secondary_crtc_, drm_->connector_property(1).id));

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  EXPECT_FALSE(ModesetWithPlanes(modeset_planes))
      << "Should not modeset when two CRTCs both need the movable overlay "
         "plane.";

  modeset_planes.pop_back();
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes))
      << "Modesetting should work when neither CRTC needs the movable overlay "
         "plane";

  {
    DrmOverlayPlaneList flip_planes;
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    SchedulePageFlip(std::move(flip_planes));
    drm_->RunCallbacks();
    EXPECT_TRUE(last_presentation_feedback_.failed())
        << "Only one of the CRTCs should be able to use an additional plane.";
  }

  {
    DrmOverlayPlaneList flip_planes;
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    SchedulePageFlip(std::move(flip_planes));
    drm_->RunCallbacks();
    EXPECT_FALSE(last_presentation_feedback_.failed())
        << "Both CRTCs should be able to flip with their own overlays.";
  }

  auto removed_crtc = controller_->RemoveCrtc(drm_, secondary_crtc_);
  EXPECT_TRUE(removed_crtc);
  {
    DrmOverlayPlaneList flip_planes;
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    flip_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
    SchedulePageFlip(std::move(flip_planes));
    drm_->RunCallbacks();
    EXPECT_FALSE(last_presentation_feedback_.failed())
        << "With only one CRTC to flip, we should be able to use the movable "
           "plane again.";
  }
}

TEST_F(HardwareDisplayControllerTest,
       ModesettingWithMirroringAndMultipleMovablePlanes) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/2);

  controller_->AddCrtc(std::make_unique<CrtcController>(
      drm_, secondary_crtc_, drm_->connector_property(1).id));

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  EXPECT_TRUE(ModesetWithPlanes(modeset_planes))
      << "Should be able modeset with two CRTCs and two movable planes.";
}

TEST_F(HardwareDisplayControllerTest, ModifiersFilter) {
  std::vector<uint64_t> filter_modifiers = {DRM_FORMAT_MOD_LINEAR,
                                            I915_FORMAT_MOD_X_TILED};
  std::unique_ptr<MockDrmModifiersFilter> filter =
      std::make_unique<MockDrmModifiersFilter>(filter_modifiers);
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/0,
                      {I915_FORMAT_MOD_X_TILED, I915_FORMAT_MOD_Y_TILED},
                      std::move(filter));

  std::vector<uint64_t> valid_modifiers =
      controller_->GetFormatModifiersForTestModeset(DRM_FORMAT_XRGB8888);

  EXPECT_EQ(1u, valid_modifiers.size());
  EXPECT_EQ(I915_FORMAT_MOD_X_TILED, valid_modifiers[0]);
}

TEST_F(HardwareDisplayControllerTest, DynamicCursorSize) {
  // The dynamic cursor size feature is currently only supported on Intel GPUs.
  drm_->SetDriverName("i915");

  // Kernel driver may or may not provide cursor "SIZE_HINTS" property.
  // Without "SIZE_HINTS", there should be only one supported size by default.
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/0,
                      /*supported_modifiers=*/{}, /*modifiers_filter=*/nullptr,
                      /*has_size_hints=*/false);
  EXPECT_EQ(1u, controller_->NumOfSupportedCursorSizesForTesting());

  // With "SIZE_HINTS", there should be 3 supported sizes.
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/0,
                      /*supported_modifiers=*/{}, /*modifiers_filter=*/nullptr,
                      /*has_size_hints=*/true);
  EXPECT_EQ(3u, controller_->NumOfSupportedCursorSizesForTesting());

  // HardwareDisplayController should select the smallest size in the supported
  // list that fits the input cursor bitmap.
  controller_->SetCursor(CreateBitmap(25, 25));
  EXPECT_EQ(gfx::Size(64, 64), controller_->CurrentCursorSizeForTesting());

  controller_->SetCursor(CreateBitmap(25, 65));
  EXPECT_EQ(gfx::Size(128, 128), controller_->CurrentCursorSizeForTesting());

  // If the input bitmap exceeds the max supported size, make sure the largest
  // size supported is used.
  controller_->SetCursor(CreateBitmap(300, 300));
  EXPECT_EQ(gfx::Size(256, 256), controller_->CurrentCursorSizeForTesting());
}

TEST_F(HardwareDisplayControllerMockedDeviceTest,
       TestSeamlessMode_MatchingSizeFailedProbe) {
  MockDrmDevice* mock_drm = static_cast<MockDrmDevice*>(drm_.get());

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  // Mode with visible size that matches the size of the currently configured
  // mode, and a different refresh rate.
  drmModeModeInfo matching_size_mode = kDefaultMode;
  matching_size_mode.vrefresh *= 2;

  // Even if the size of the mode matches, the driver may reject a
  // non-modesetting commit. This test failure should propagate up to
  // TestSeamlessMode.
  EXPECT_CALL(*mock_drm, CommitProperties(_, DRM_MODE_ATOMIC_TEST_ONLY, 1, _))
      .Times(1)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(
      controller_->TestSeamlessMode(primary_crtc_, matching_size_mode));
}

TEST_F(HardwareDisplayControllerTest, NotTiled) {
  EXPECT_FALSE(controller_->IsTiled());
}

TEST_F(HardwareDisplayControllerTest, GetTilePropertyNoTile) {
  EXPECT_EQ(controller_->GetTileProperty(), std::nullopt);
}

TEST_F(HardwareDisplayControllerTest, GetTileProperty) {
  TileProperty tile_property = {.group_id = 123,
                                .scale_to_fit_display = true,
                                .tile_size = gfx::Size(300, 200),
                                .tile_layout = gfx::Size(3, 2),
                                .location = gfx::Point(1, 2)};

  auto tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id,
      tile_property);
  auto hdc_controller = std::make_unique<HardwareDisplayController>(
      std::move(tiled_crtc_controller), gfx::Point(), nullptr);

  EXPECT_TRUE(hdc_controller->IsTiled());
  EXPECT_THAT(hdc_controller->GetTileProperty(),
              Optional(EqTileProperty(tile_property)));
}

TEST_F(HardwareDisplayControllerTest, TileCompositedModeset) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/2);
  TileProperty primary_tile_property = {.group_id = 123,
                                        .scale_to_fit_display = true,
                                        .tile_size = gfx::Size(300, 200),
                                        .tile_layout = gfx::Size(2, 1),
                                        .location = gfx::Point(0, 0)};

  auto primary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), primary_crtc_, drm_->connector_property(0).id,
      primary_tile_property);

  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);
  auto nonprimary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id,
      nonprimary_tile_property);

  controller_ = std::make_unique<HardwareDisplayController>(
      std::move(primary_tiled_crtc_controller), gfx::Point(), nullptr);
  controller_->AddCrtc(std::move(nonprimary_tiled_crtc_controller));

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  drmModeModeInfo tile_mode = {.hdisplay = 300, .vdisplay = 200};
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes, tile_mode));

  EXPECT_THAT(
      controller_->crtc_controllers(),
      testing::UnorderedElementsAre(
          Pointee(
              AllOf(Property(&CrtcController::crtc, Eq(primary_crtc_)),
                    Property(&CrtcController::CurrentModeIsTiled, Eq(true)),
                    Property(&CrtcController::is_enabled, Eq(true)),
                    Property(&CrtcController::tile_property,
                             Optional(EqTileProperty(primary_tile_property))))),
          Pointee(AllOf(
              Property(&CrtcController::crtc, Eq(secondary_crtc_)),
              Property(&CrtcController::CurrentModeIsTiled, Eq(true)),
              Property(&CrtcController::is_enabled, Eq(true)),
              Property(&CrtcController::tile_property,
                       Optional(EqTileProperty(nonprimary_tile_property)))))));
}

TEST_F(HardwareDisplayControllerTest, ModesetTileToNonTileMode) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/2);
  TileProperty primary_tile_property = {.group_id = 123,
                                        .scale_to_fit_display = true,
                                        .tile_size = gfx::Size(300, 200),
                                        .tile_layout = gfx::Size(2, 1),
                                        .location = gfx::Point(0, 0)};

  auto primary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), primary_crtc_, drm_->connector_property(0).id,
      primary_tile_property);

  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);
  auto nonprimary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id,
      nonprimary_tile_property);

  controller_ = std::make_unique<HardwareDisplayController>(
      std::move(primary_tiled_crtc_controller), gfx::Point(), nullptr);
  controller_->AddCrtc(std::move(nonprimary_tiled_crtc_controller));

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  // Modeset to a tile mode first so that both CRTCs are enabled.
  drmModeModeInfo tile_mode = {.hdisplay = 300, .vdisplay = 200};
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes, tile_mode));

  // Modeset to non-tile mode - should disable the nonprimary tiles.
  drmModeModeInfo nontile_mode = {.hdisplay = 1920, .vdisplay = 1080};
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes, nontile_mode));

  EXPECT_THAT(
      controller_->crtc_controllers(),
      testing::UnorderedElementsAre(
          Pointee(
              AllOf(Property(&CrtcController::crtc, Eq(primary_crtc_)),
                    Property(&CrtcController::CurrentModeIsTiled, Eq(false)),
                    Property(&CrtcController::is_enabled, Eq(true)))),
          Pointee(AllOf(Property(&CrtcController::crtc, Eq(secondary_crtc_)),
                        Property(&CrtcController::is_enabled, Eq(false))))));
}

TEST_F(HardwareDisplayControllerTest, TileDisplayPageflipWithCrtcOffset) {
  InitializeDrmDevice(/*use_atomic=*/true, /*movable_planes=*/0);
  TileProperty primary_tile_property = {.group_id = 123,
                                        .scale_to_fit_display = true,
                                        .tile_size = gfx::Size(300, 200),
                                        .tile_layout = gfx::Size(2, 1),
                                        .location = gfx::Point(0, 0)};

  auto primary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), primary_crtc_, drm_->connector_property(0).id,
      primary_tile_property);

  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);
  auto nonprimary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id,
      nonprimary_tile_property);

  controller_ = std::make_unique<HardwareDisplayController>(
      std::move(primary_tiled_crtc_controller), gfx::Point(), nullptr);
  controller_->AddCrtc(std::move(nonprimary_tiled_crtc_controller));

  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  drmModeModeInfo tile_mode = {.hdisplay = 300, .vdisplay = 200};
  EXPECT_TRUE(ModesetWithPlanes(planes, tile_mode));
  EXPECT_EQ(1, drm_->get_commit_count());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_EQ(2, drm_->get_commit_count());

  std::unordered_map<uint32_t, std::vector<HardwareDisplayPlane*>>
      crtc_planes_map;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->owning_crtc() == 0) {
      continue;
    }
    crtc_planes_map[plane->owning_crtc()].push_back(plane.get());
  }
  EXPECT_THAT(crtc_planes_map, SizeIs(2));

  // Expect all planes to belong to either CRTCs and have the proper offsets.
  std::unordered_map<uint32_t, gfx::Point> crtc_offset_map = {
      {primary_crtc_, gfx::Point(0, 0)},
      {secondary_crtc_, gfx::Point(-300, 0)}};
  for (const auto& [crtc, crtc_planes] : crtc_planes_map) {
    EXPECT_TRUE(crtc_offset_map.find(crtc) != crtc_offset_map.end());

    for (const auto* plane : crtc_planes) {
      ScopedDrmObjectPropertyPtr plane_props =
          drm_->GetObjectProperties(plane->id(), DRM_MODE_OBJECT_PLANE);
      {
        DrmWrapper::Property prop;
        GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_X", &prop);
        EXPECT_EQ(static_cast<int>(prop.value), crtc_offset_map[crtc].x());
      }
      {
        DrmWrapper::Property prop;
        GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_Y", &prop);
        EXPECT_EQ(static_cast<int>(prop.value), crtc_offset_map[crtc].y());
      }
    }
  }
}

TEST_F(HardwareDisplayControllerTest, TileDisplayMoveCursor) {
  InitializeDrmDevice(/*use_atomic=*/true);
  TileProperty primary_tile_property = {.group_id = 123,
                                        .scale_to_fit_display = true,
                                        .tile_size = gfx::Size(300, 200),
                                        .tile_layout = gfx::Size(2, 1),
                                        .location = gfx::Point(0, 0)};

  auto primary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), primary_crtc_, drm_->connector_property(0).id,
      primary_tile_property);

  TileProperty nonprimary_tile_property = primary_tile_property;
  nonprimary_tile_property.location = gfx::Point(1, 0);
  auto nonprimary_tiled_crtc_controller = std::make_unique<CrtcController>(
      drm_.get(), secondary_crtc_, drm_->connector_property(1).id,
      nonprimary_tile_property);

  controller_ = std::make_unique<HardwareDisplayController>(
      std::move(primary_tiled_crtc_controller), gfx::Point(), nullptr);
  controller_->AddCrtc(std::move(nonprimary_tiled_crtc_controller));

  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  drmModeModeInfo tile_mode = {.hdisplay = 300, .vdisplay = 200};
  EXPECT_TRUE(ModesetWithPlanes(planes, tile_mode));

  const gfx::Point kCursorLocation = gfx::Point(100, 200);
  controller_->MoveCursor(kCursorLocation);

  EXPECT_EQ(drm_->get_crtc_cursor_location(primary_crtc_), kCursorLocation);
  EXPECT_EQ(drm_->get_crtc_cursor_location(secondary_crtc_),
            gfx::Point(kCursorLocation.x() - 300, kCursorLocation.y() - 0));
}

}  // namespace ui
