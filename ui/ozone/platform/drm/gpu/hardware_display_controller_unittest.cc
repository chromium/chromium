// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode = {0, 6, 0, 0, 0, 0, 4,     0,
                                      0, 0, 0, 0, 0, 0, {'\0'}};

constexpr uint32_t kCrtcIdBase = 100;
constexpr uint32_t kPrimaryCrtc = kCrtcIdBase;
constexpr uint32_t kSecondaryCrtc = kCrtcIdBase + 1;
constexpr uint32_t kConnectorIdBase = 200;
constexpr uint32_t kPlaneOffset = 300;
constexpr uint32_t kInFormatsBlobPropId = 400;

constexpr uint32_t kActivePropId = 1000;
constexpr uint32_t kModePropId = 1001;
constexpr uint32_t kCrtcIdPropId = 2000;
constexpr uint32_t kLinkStatusPropId = 2001;

constexpr uint32_t kPlaneCrtcId = 3001;
constexpr uint32_t kCrtcX = 3002;
constexpr uint32_t kCrtcY = 3003;
constexpr uint32_t kCrtcW = 3004;
constexpr uint32_t kCrtcH = 3005;
constexpr uint32_t kPlaneFbId = 3006;
constexpr uint32_t kSrcX = 3007;
constexpr uint32_t kSrcY = 3008;
constexpr uint32_t kSrcW = 3009;
constexpr uint32_t kSrcH = 3010;
constexpr uint32_t kFenceFdPropId = 3011;
constexpr uint32_t kTypePropId = 3012;
constexpr uint32_t kInFormatsPropId = 3013;

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);
const gfx::Size kOverlaySize(kDefaultMode.hdisplay / 2,
                             kDefaultMode.vdisplay / 2);
const gfx::SizeF kDefaultModeSizeF(1.0, 1.0);

const std::string kGpuCrashLogTimeout =
    "Failed to modeset within " +
    base::NumberToString(ui::kWaitForModesetTimeout.InSeconds()) +
    " s of the first page flip failure. Crashing GPU process.";

}  // namespace

class FakeFenceFD {
 public:
  FakeFenceFD();

  std::unique_ptr<gfx::GpuFence> GetGpuFence() const;
  void Signal() const;

 private:
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
};

FakeFenceFD::FakeFenceFD() {
  int fds[2];
  base::CreateLocalNonBlockingPipe(fds);
  read_fd = base::ScopedFD(fds[0]);
  write_fd = base::ScopedFD(fds[1]);
}

std::unique_ptr<gfx::GpuFence> FakeFenceFD::GetGpuFence() const {
  gfx::GpuFenceHandle handle;
  handle.owned_fd = base::ScopedFD(HANDLE_EINTR(dup(read_fd.get())));
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

  void InitializeDrmDevice(bool use_atomic);
  void SchedulePageFlip(ui::DrmOverlayPlaneList planes);
  void OnSubmission(gfx::SwapResult swap_result,
                    gfx::GpuFenceHandle release_fence);
  void OnPresentation(const gfx::PresentationFeedback& feedback);
  uint64_t GetPlanePropertyValue(uint32_t plane,
                                 const std::string& property_name);

  scoped_refptr<ui::DrmFramebuffer> CreateBuffer() {
    std::unique_ptr<ui::GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, kDefaultModeSize, GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(drm_, buffer.get(),
                                              kDefaultModeSize);
  }

  scoped_refptr<ui::DrmFramebuffer> CreateOverlayBuffer() {
    std::unique_ptr<ui::GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, kOverlaySize, GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), kOverlaySize);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

 protected:
  bool ModesetWithPlanes(const ui::DrmOverlayPlaneList& modeset_planes);
  bool DisableController();

  std::unique_ptr<ui::HardwareDisplayController> controller_;
  scoped_refptr<ui::MockDrmDevice> drm_;

  int successful_page_flips_count_ = 0;
  gfx::SwapResult last_swap_result_;
  gfx::PresentationFeedback last_presentation_feedback_;
};

void HardwareDisplayControllerTest::SetUp() {
  successful_page_flips_count_ = 0;
  last_swap_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  drm_ = new ui::MockDrmDevice(std::move(gbm_device));
  InitializeDrmDevice(/* use_atomic= */ true);
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_ = nullptr;
}

void HardwareDisplayControllerTest::InitializeDrmDevice(bool use_atomic) {
  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties(2);
  std::map<uint32_t, std::string> crtc_property_names = {
      {1000, "ACTIVE"},
      {1001, "MODE_ID"},
  };

  std::vector<ui::MockDrmDevice::ConnectorProperties> connector_properties(2);
  std::map<uint32_t, std::string> connector_property_names = {
      {kCrtcIdPropId, "CRTC_ID"},
      {kLinkStatusPropId, "link-status"},
  };
  for (size_t i = 0; i < connector_properties.size(); ++i) {
    connector_properties[i].id = kConnectorIdBase + i;
    for (const auto& pair : connector_property_names) {
      connector_properties[i].properties.push_back(
          {.id = pair.first, .value = 0});
    }
  }

  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties;
  std::map<uint32_t, std::string> plane_property_names = {
      // Add all required properties.
      {kPlaneCrtcId, "CRTC_ID"},
      {kCrtcX, "CRTC_X"},
      {kCrtcY, "CRTC_Y"},
      {kCrtcW, "CRTC_W"},
      {kCrtcH, "CRTC_H"},
      {kPlaneFbId, "FB_ID"},
      {kSrcX, "SRC_X"},
      {kSrcY, "SRC_Y"},
      {kSrcW, "SRC_W"},
      {kSrcH, "SRC_H"},
      {kFenceFdPropId, "IN_FENCE_FD"},
      // Add some optional properties we use for convenience.
      {kTypePropId, "type"},
      {kInFormatsPropId, "IN_FORMATS"},
  };

  for (size_t i = 0; i < crtc_properties.size(); ++i) {
    crtc_properties[i].id = kCrtcIdBase + i;
    for (const auto& pair : crtc_property_names) {
      crtc_properties[i].properties.push_back({.id = pair.first, .value = 0});
    }

    for (size_t j = 0; j < 2; ++j) {
      const uint32_t offset = plane_properties.size();

      ui::MockDrmDevice::PlaneProperties plane;
      plane.id = kPlaneOffset + offset;
      plane.crtc_mask = 1 << i;
      for (const auto& pair : plane_property_names) {
        uint32_t value = 0;
        if (pair.first == kTypePropId)
          value = j == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
        else if (pair.first == kInFormatsPropId)
          value = kInFormatsBlobPropId;

        plane.properties.push_back({.id = pair.first, .value = value});
      }

      drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
          kInFormatsBlobPropId, {DRM_FORMAT_XRGB8888}, {}));

      plane_properties.emplace_back(std::move(plane));
    }
  }

  std::map<uint32_t, std::string> property_names;
  property_names.insert(crtc_property_names.begin(), crtc_property_names.end());
  property_names.insert(connector_property_names.begin(),
                        connector_property_names.end());
  property_names.insert(plane_property_names.begin(),
                        plane_property_names.end());

  // This will change the plane_manager of the drm.
  // HardwareDisplayController is tied to the plane_manager CRTC states.
  // Destruct the controller before destructing the plane manager its CRTC
  // controllers are tied to.
  controller_ = nullptr;
  drm_->InitializeState(crtc_properties, connector_properties, plane_properties,
                        property_names, use_atomic);
  // Initialize a new HardwareDisplayController with the new Plane Manager of
  // the DRM.
  controller_ = std::make_unique<ui::HardwareDisplayController>(
      std::make_unique<ui::CrtcController>(drm_.get(), kPrimaryCrtc,
                                           kConnectorIdBase),
      gfx::Point());
}

bool HardwareDisplayControllerTest::ModesetWithPlanes(
    const ui::DrmOverlayPlaneList& modeset_planes) {
  ui::CommitRequest commit_request;
  controller_->GetModesetProps(&commit_request, modeset_planes, kDefaultMode);
  ui::CommitRequest request_for_update = commit_request;
  bool status = drm_->plane_manager()->Commit(std::move(commit_request),
                                              DRM_MODE_ATOMIC_ALLOW_MODESET);
  for (const ui::CrtcCommitRequest& crtc_request : request_for_update)
    controller_->UpdateState(crtc_request);

  return status;
}

bool HardwareDisplayControllerTest::DisableController() {
  ui::CommitRequest commit_request;
  controller_->GetDisableProps(&commit_request);
  ui::CommitRequest request_for_update = commit_request;
  bool status = drm_->plane_manager()->Commit(std::move(commit_request),
                                              DRM_MODE_ATOMIC_ALLOW_MODESET);
  for (const ui::CrtcCommitRequest& crtc_request : request_for_update)
    controller_->UpdateState(crtc_request);

  return status;
}

void HardwareDisplayControllerTest::SchedulePageFlip(
    ui::DrmOverlayPlaneList planes) {
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
  if (!feedback.failed())
    successful_page_flips_count_++;
  last_presentation_feedback_ = feedback;
}

uint64_t HardwareDisplayControllerTest::GetPlanePropertyValue(
    uint32_t plane,
    const std::string& property_name) {
  ui::DrmDevice::Property p{};
  ui::ScopedDrmObjectPropertyPtr properties(
      drm_->GetObjectProperties(plane, DRM_MODE_OBJECT_PLANE));
  EXPECT_TRUE(ui::GetDrmPropertyForName(drm_.get(), properties.get(),
                                        property_name, &p));
  return p.value;
}

TEST_F(HardwareDisplayControllerTest, CheckModesettingResult) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);

  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_FALSE(ui::DrmOverlayPlane::GetPrimaryPlane(modeset_planes)
                   ->buffer->HasOneRef());
}

TEST_F(HardwareDisplayControllerTest, CrtcPropsAfterModeset) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ui::ScopedDrmObjectPropertyPtr crtc_props =
      drm_->GetObjectProperties(kPrimaryCrtc, DRM_MODE_OBJECT_CRTC);
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), crtc_props.get(), "ACTIVE", &prop);
    EXPECT_EQ(kActivePropId, prop.id);
    EXPECT_EQ(1U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), crtc_props.get(), "MODE_ID", &prop);
    EXPECT_EQ(kModePropId, prop.id);
    EXPECT_GT(prop.value, 0U);
  }
}

TEST_F(HardwareDisplayControllerTest, ConnectorPropsAfterModeset) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ui::ScopedDrmObjectPropertyPtr connector_props =
      drm_->GetObjectProperties(kConnectorIdBase, DRM_MODE_OBJECT_CONNECTOR);

  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), connector_props.get(), "CRTC_ID",
                              &prop);
    EXPECT_EQ(kCrtcIdPropId, prop.id);
    EXPECT_EQ(kCrtcIdBase, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), connector_props.get(), "link-status",
                              &prop);
    EXPECT_EQ(kLinkStatusPropId, prop.id);
    EXPECT_EQ(static_cast<uint64_t>(DRM_MODE_LINK_STATUS_GOOD), prop.value);
  }
}

TEST_F(HardwareDisplayControllerTest, PlanePropsAfterModeset) {
  const FakeFenceFD fake_fence_fd;
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), fake_fence_fd.GetGpuFence());
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  ui::ScopedDrmObjectPropertyPtr plane_props =
      drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
  const ui::DrmOverlayPlane* primary_plane =
      ui::DrmOverlayPlane::GetPrimaryPlane(modeset_planes);

  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_ID", &prop);
    EXPECT_EQ(kPlaneCrtcId, prop.id);
    EXPECT_EQ(kCrtcIdBase, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_X", &prop);
    EXPECT_EQ(kCrtcX, prop.id);
    EXPECT_EQ(primary_plane->display_bounds.x(), static_cast<int>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_Y", &prop);
    EXPECT_EQ(kCrtcY, prop.id);
    EXPECT_EQ(primary_plane->display_bounds.y(), static_cast<int>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_W", &prop);
    EXPECT_EQ(kCrtcW, prop.id);
    EXPECT_EQ(kDefaultModeSize.width(), static_cast<int>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_H", &prop);
    EXPECT_EQ(kCrtcH, prop.id);
    EXPECT_EQ(kDefaultModeSize.height(), static_cast<int>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "FB_ID", &prop);
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
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_X", &prop);
    EXPECT_EQ(kSrcX, prop.id);
    EXPECT_EQ(fixed_point_rect.x(), static_cast<float>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_Y", &prop);
    EXPECT_EQ(kSrcY, prop.id);
    EXPECT_EQ(fixed_point_rect.y(), static_cast<float>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_W", &prop);
    EXPECT_EQ(kSrcW, prop.id);
    EXPECT_EQ(fixed_point_rect.width(), static_cast<int>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_H", &prop);
    EXPECT_EQ(kSrcH, prop.id);
    EXPECT_EQ(fixed_point_rect.height(), static_cast<int>(prop.value));
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                              &prop);
    EXPECT_EQ(kFenceFdPropId, prop.id);
    EXPECT_GT(static_cast<int>(prop.value), base::kInvalidPlatformFile);
  }
}

TEST_F(HardwareDisplayControllerTest, FenceFdValueChange) {
  ui::DrmOverlayPlaneList modeset_planes;
  ui::DrmOverlayPlane plane(CreateBuffer(), nullptr);
  modeset_planes.push_back(plane.Clone());
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  // Test invalid fence fd
  {
    ui::DrmDevice::Property fence_fd_prop = {};
    ui::ScopedDrmObjectPropertyPtr plane_props =
        drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                              &fence_fd_prop);
    EXPECT_EQ(kFenceFdPropId, fence_fd_prop.id);
    EXPECT_EQ(base::kInvalidPlatformFile,
              static_cast<int>(fence_fd_prop.value));
  }

  const FakeFenceFD fake_fence_fd;
  plane.gpu_fence = fake_fence_fd.GetGpuFence();
  std::vector<ui::DrmOverlayPlane> planes = {};
  planes.push_back(plane.Clone());
  SchedulePageFlip(std::move(planes));

  // Verify fence FD after a GPU Fence is added to the plane.
  {
    ui::DrmDevice::Property fence_fd_prop = {};
    ui::ScopedDrmObjectPropertyPtr plane_props =
        drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                              &fence_fd_prop);
    EXPECT_EQ(kFenceFdPropId, fence_fd_prop.id);
    EXPECT_LT(base::kInvalidPlatformFile,
              static_cast<int>(fence_fd_prop.value));
  }

  plane.gpu_fence = nullptr;
  modeset_planes.clear();
  modeset_planes.push_back(plane.Clone());
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  // Test an invalid FD again after the fence is removed.
  {
    ui::DrmDevice::Property fence_fd_prop = {};
    ui::ScopedDrmObjectPropertyPtr plane_props =
        drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                              &fence_fd_prop);
    EXPECT_EQ(kFenceFdPropId, fence_fd_prop.id);
    EXPECT_EQ(base::kInvalidPlatformFile,
              static_cast<int>(fence_fd_prop.value));
  }
}

TEST_F(HardwareDisplayControllerTest, CheckDisableResetsProps) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  // Test props values after disabling.
  DisableController();

  ui::ScopedDrmObjectPropertyPtr crtc_props =
      drm_->GetObjectProperties(kPrimaryCrtc, DRM_MODE_OBJECT_CRTC);
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), crtc_props.get(), "ACTIVE", &prop);
    EXPECT_EQ(kActivePropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), crtc_props.get(), "MODE_ID", &prop);
    EXPECT_EQ(kModePropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }

  ui::ScopedDrmObjectPropertyPtr connector_props =
      drm_->GetObjectProperties(kConnectorIdBase, DRM_MODE_OBJECT_CONNECTOR);
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), connector_props.get(), "CRTC_ID",
                              &prop);
    EXPECT_EQ(kCrtcIdPropId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }

  ui::ScopedDrmObjectPropertyPtr plane_props =
      drm_->GetObjectProperties(kPlaneOffset, DRM_MODE_OBJECT_PLANE);
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_ID", &prop);
    EXPECT_EQ(kPlaneCrtcId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_X", &prop);
    EXPECT_EQ(kCrtcX, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_Y", &prop);
    EXPECT_EQ(kCrtcY, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_W", &prop);
    EXPECT_EQ(kCrtcW, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "CRTC_H", &prop);
    EXPECT_EQ(kCrtcH, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "FB_ID", &prop);
    EXPECT_EQ(kPlaneFbId, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_X", &prop);
    EXPECT_EQ(kSrcX, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_Y", &prop);
    EXPECT_EQ(kSrcY, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_W", &prop);
    EXPECT_EQ(kSrcW, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "SRC_H", &prop);
    EXPECT_EQ(kSrcH, prop.id);
    EXPECT_EQ(0U, prop.value);
  }
  {
    ui::DrmDevice::Property prop = {};
    ui::GetDrmPropertyForName(drm_.get(), plane_props.get(), "IN_FENCE_FD",
                              &prop);
    EXPECT_EQ(kFenceFdPropId, prop.id);
    EXPECT_EQ(base::kInvalidPlatformFile, static_cast<int>(prop.value));
  }
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  ui::DrmOverlayPlane page_flip_plane(CreateBuffer(), nullptr);
  std::vector<ui::DrmOverlayPlane> page_flip_planes;
  page_flip_planes.push_back(page_flip_plane.Clone());

  SchedulePageFlip(std::move(page_flip_planes));

  drm_->RunCallbacks();
  EXPECT_TRUE(ui::DrmOverlayPlane::GetPrimaryPlane(modeset_planes)
                  ->buffer->HasOneRef());
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

  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_FALSE(ModesetWithPlanes(modeset_planes));
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayPresent) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  planes.emplace_back(CreateOverlayBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
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
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  planes.emplace_back(CreateOverlayBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
                      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF),
                      true, nullptr);

  EXPECT_TRUE(ModesetWithPlanes(planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
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
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, successful_page_flips_count_);
  EXPECT_EQ(4, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, AcceptUnderlays) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  planes.emplace_back(CreateBuffer(), -1, gfx::OVERLAY_TRANSFORM_NONE,
                      gfx::Rect(kDefaultModeSize),
                      gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, PageflipMirroredControllers) {
  controller_->AddCrtc(std::make_unique<ui::CrtcController>(
      drm_.get(), kSecondaryCrtc, kConnectorIdBase + 1));

  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);

  EXPECT_TRUE(ModesetWithPlanes(planes));
  EXPECT_EQ(1, drm_->get_commit_count());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
  EXPECT_EQ(2, drm_->get_commit_count());
  // Verify only the displays have a valid framebuffer on the primary plane.
  // First display:
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
  // Second display:
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 2, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 3, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterRemoveCrtc) {
  controller_->AddCrtc(std::make_unique<ui::CrtcController>(
      drm_.get(), kSecondaryCrtc, kConnectorIdBase + 1));

  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);

  const ui::HardwareDisplayPlane* primary_crtc_plane = nullptr;
  const ui::HardwareDisplayPlane* secondary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && plane->owning_crtc() == kPrimaryCrtc)
      primary_crtc_plane = plane.get();
    if (plane->in_use() && plane->owning_crtc() == kSecondaryCrtc)
      secondary_crtc_plane = plane.get();
  }

  ASSERT_NE(nullptr, primary_crtc_plane);
  ASSERT_NE(nullptr, secondary_crtc_plane);
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());

  // Removing the crtc should free the plane.
  std::unique_ptr<ui::CrtcController> crtc =
      controller_->RemoveCrtc(drm_, kPrimaryCrtc);
  EXPECT_FALSE(primary_crtc_plane->in_use());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());

  // Check that controller doesn't affect the state of removed plane in
  // subsequent page flip.
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, successful_page_flips_count_);
  EXPECT_FALSE(primary_crtc_plane->in_use());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterDestroyingCrtc) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);

  const ui::HardwareDisplayPlane* owned_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes())
    if (plane->in_use())
      owned_plane = plane.get();
  ASSERT_TRUE(owned_plane != nullptr);
  EXPECT_EQ(kPrimaryCrtc, owned_plane->owning_crtc());
  std::unique_ptr<ui::CrtcController> crtc =
      controller_->RemoveCrtc(drm_, kPrimaryCrtc);
  // Destroying crtc should free the plane.
  crtc.reset();
  uint32_t crtc_nullid = 0;
  EXPECT_FALSE(owned_plane->in_use());
  EXPECT_EQ(crtc_nullid, owned_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterAddCrtc) {
  controller_->AddCrtc(std::make_unique<ui::CrtcController>(
      drm_.get(), kSecondaryCrtc, kConnectorIdBase + 1));

  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);

  ui::HardwareDisplayPlane* primary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && kPrimaryCrtc == plane->owning_crtc())
      primary_crtc_plane = plane.get();
  }

  ASSERT_TRUE(primary_crtc_plane != nullptr);

  auto hdc_controller = std::make_unique<ui::HardwareDisplayController>(
      controller_->RemoveCrtc(drm_, kPrimaryCrtc), controller_->origin());
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
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
      ui::DrmOverlayPlane::Clone(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(3, successful_page_flips_count_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, ModesetWhilePageFlipping) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));

  EXPECT_TRUE(ModesetWithPlanes(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, FailPageFlippingWithNoSavingModeset) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  std::vector<ui::DrmOverlayPlane> page_flip_planes;
  page_flip_planes.emplace_back(CreateBuffer(), nullptr);

  // Page flip fails, so a GPU process self-destruct sequence is initiated.
  drm_->set_commit_expectation(false);
  SchedulePageFlip(std::move(page_flip_planes));

  // Since no modeset event was detected, death occurs after
  // |kWaitForModesetTimeout| seconds.
  EXPECT_DEATH_IF_SUPPORTED(
      task_environment_.FastForwardBy(ui::kWaitForModesetTimeout),
      kGpuCrashLogTimeout);
}

TEST_F(HardwareDisplayControllerTest, FailPageFlippingWithSavingModeset) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  std::vector<ui::DrmOverlayPlane> page_flip_planes;
  page_flip_planes.emplace_back(CreateBuffer(), nullptr);

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
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  // Ensure self-destruct time runs out without process death.
  task_environment_.FastForwardBy(ui::kWaitForModesetTimeout);
}

TEST_F(HardwareDisplayControllerTest,
       RecreateBuffersOnOldPlanesPageFlipFailure) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
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
  planes.emplace_back(CreateBuffer(), nullptr);
  SchedulePageFlip(std::move(planes));

  // Ensure self-destruct time runs out without process death.
  task_environment_.FastForwardBy(ui::kWaitForModesetTimeout);

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, CheckNoPrimaryPlaneOnFlip) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(modeset_planes));

  std::vector<ui::DrmOverlayPlane> page_flip_planes;
  page_flip_planes.emplace_back(CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
                                gfx::Rect(kDefaultModeSize),
                                gfx::RectF(0, 0, 1, 1), true, nullptr);
  SchedulePageFlip(std::move(page_flip_planes));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, PageFlipWithUnassignablePlanes) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  {
    std::vector<ui::DrmOverlayPlane> page_flip_planes;
    page_flip_planes.emplace_back(
        CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
        gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
    page_flip_planes.emplace_back(
        CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
        gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
    page_flip_planes.emplace_back(
        CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
        gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
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
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  constexpr int kUnassignableFlips = 3;

  for (size_t i = 0; i < kUnassignableFlips; ++i) {
    {
      std::vector<ui::DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(0, successful_page_flips_count_);
    EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
  }

  for (size_t i = 0; i < ui::kPageFlipWatcherHistorySize - kUnassignableFlips;
       ++i) {
    drm_->set_commit_expectation(true);
    {
      std::vector<ui::DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    // +1 because we're comparing an index with a count.
    EXPECT_EQ(i + 1, static_cast<size_t>(successful_page_flips_count_));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  }

  // We should still be alive since we didn't submit too many unassignable page
  // flips.
  task_environment_.FastForwardBy(ui::kWaitForModesetTimeout);
}

TEST_F(HardwareDisplayControllerTest, CrashOnTooManyFlakyPlaneAssignments) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  auto do_successful_flip = [&]() {
    {
      std::vector<ui::DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
    EXPECT_FALSE(last_presentation_feedback_.failed());
  };

  auto do_failed_flip = [&]() {
    {
      std::vector<ui::DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
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

  auto flakes = ui::kPlaneAssignmentFlakeThreshold;
  ASSERT_GT(ui::kPageFlipWatcherHistorySize, flakes)
      << "Page flip history is too small to account for the maximum number of "
         "flakes";
  auto successes = ui::kPageFlipWatcherHistorySize - (2 * flakes);

  for (size_t i = 0; i < successes; ++i)
    do_successful_flip();
  for (size_t i = 0; i < flakes; ++i)
    do_flake();

  EXPECT_DEATH_IF_SUPPORTED(
      do_flake(),
      base::StringPrintf("Plane assignment has flaked %d times, but the "
                         "threshold is %d. Crashing the GPU process.",
                         flakes, ui::kPlaneAssignmentFlakeThreshold));
}

TEST_F(HardwareDisplayControllerTest, CrashOnTooManyFailedPlaneAssignments) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));

  auto do_successful_flip = [&]() {
    {
      std::vector<ui::DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
    EXPECT_FALSE(last_presentation_feedback_.failed());
  };

  auto do_failed_flip = [&]() {
    {
      std::vector<ui::DrmOverlayPlane> page_flip_planes;
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      page_flip_planes.emplace_back(
          CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(kDefaultModeSize), gfx::RectF(0, 0, 1, 1), true, nullptr);
      SchedulePageFlip(std::move(page_flip_planes));
    }
    drm_->RunCallbacks();

    EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, last_swap_result_);
    EXPECT_TRUE(last_presentation_feedback_.failed());
  };

  auto failures = ui::kPlaneAssignmentMaximumFailures;
  auto successes = ui::kPageFlipWatcherHistorySize - failures;

  for (size_t i = 0; i < successes; ++i)
    do_successful_flip();
  for (size_t i = 0; i < (failures - 1); ++i)
    do_failed_flip();

  EXPECT_DEATH_IF_SUPPORTED(
      do_failed_flip(),
      base::StringPrintf("Plane assignment has failed %d/%d times, but the "
                         "threshold is %d. Crashing the GPU process.",
                         failures, ui::kPageFlipWatcherHistorySize,
                         ui::kPlaneAssignmentMaximumFailures));
}

TEST_F(HardwareDisplayControllerTest, AddCrtcMidPageFlip) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));

  controller_->AddCrtc(std::make_unique<ui::CrtcController>(
      drm_.get(), kSecondaryCrtc, kConnectorIdBase + 1));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, RemoveCrtcMidPageFlip) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(std::move(planes));

  controller_->RemoveCrtc(drm_, kPrimaryCrtc);

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, successful_page_flips_count_);
}

TEST_F(HardwareDisplayControllerTest, Disable) {
  // Page flipping overlays is only supported on atomic configurations.
  InitializeDrmDevice(/* use_atomic= */ true);

  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  planes.emplace_back(CreateOverlayBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
                      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF),
                      true, nullptr);
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);

  EXPECT_TRUE(DisableController());

  int planes_in_use = 0;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use())
      planes_in_use++;
  }
  // No plane should be in use.
  ASSERT_EQ(0, planes_in_use);
}

TEST_F(HardwareDisplayControllerTest, PageflipAfterModeset) {
  ui::DrmOverlayPlaneList planes;
  scoped_refptr<ui::DrmFramebuffer> buffer = CreateBuffer();
  planes.emplace_back(buffer, nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  for (const auto& plane : planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                   .modeset_framebuffers,
                               plane.buffer));
  }

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();

  // modeset_framebuffers should be cleared after the pageflip is complete.
  EXPECT_TRUE(drm_->plane_manager()
                  ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                  .modeset_framebuffers.empty());
}

TEST_F(HardwareDisplayControllerTest, PageflipBeforeModeset) {
  ui::DrmOverlayPlaneList planes;
  scoped_refptr<ui::DrmFramebuffer> buffer = CreateBuffer();
  planes.emplace_back(buffer, nullptr);
  EXPECT_TRUE(ModesetWithPlanes(planes));

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));

  EXPECT_TRUE(ModesetWithPlanes(planes));
  for (const auto& plane : planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                   .modeset_framebuffers,
                               plane.buffer));
  }

  // modeset_framebuffers should not be cleared when a pageflip callback is run
  // after a modeset
  drm_->RunCallbacks();
  EXPECT_FALSE(drm_->plane_manager()
                   ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                   .modeset_framebuffers.empty());
  for (const auto& plane : planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                   .modeset_framebuffers,
                               plane.buffer));
  }
}

TEST_F(HardwareDisplayControllerTest, MultiplePlanesModeset) {
  ui::DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  modeset_planes.emplace_back(CreateBuffer(), nullptr);
  ASSERT_TRUE(ModesetWithPlanes(modeset_planes));
  EXPECT_EQ(drm_->plane_manager()
                ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                .modeset_framebuffers.size(),
            2UL);
  for (const auto& plane : modeset_planes) {
    EXPECT_TRUE(base::Contains(drm_->plane_manager()
                                   ->GetCrtcStateForCrtcId(kPrimaryCrtc)
                                   .modeset_framebuffers,
                               plane.buffer));
  }
}
