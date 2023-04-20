// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/android_surface_control_compat.h"

#include <android/data_space.h>
#include <android/hdr_metadata.h>
#include <dlfcn.h>

#include "base/android/build_info.h"
#include "base/atomic_sequence_num.h"
#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/hash/md5_constexpr.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/color_space.h"

extern "C" {
typedef struct ASurfaceTransactionStats ASurfaceTransactionStats;
typedef void (*ASurfaceTransaction_OnComplete)(void* context,
                                               ASurfaceTransactionStats* stats);
typedef void (*ASurfaceTransaction_OnCommit)(void* context,
                                             ASurfaceTransactionStats* stats);

// ASurface
using pASurfaceControl_createFromWindow =
    ASurfaceControl* (*)(ANativeWindow* parent, const char* name);
using pASurfaceControl_create = ASurfaceControl* (*)(ASurfaceControl* parent,
                                                     const char* name);
using pASurfaceControl_fromJava = ASurfaceControl* (*)(JNIEnv*, jobject);
using pASurfaceControl_release = void (*)(ASurfaceControl*);

// ASurfaceTransaction enums
enum {
  ASURFACE_TRANSACTION_TRANSPARENCY_TRANSPARENT = 0,
  ASURFACE_TRANSACTION_TRANSPARENCY_TRANSLUCENT = 1,
  ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE = 2,
};

// ASurfaceTransaction
using pASurfaceTransaction_create = ASurfaceTransaction* (*)(void);
using pASurfaceTransaction_delete = void (*)(ASurfaceTransaction*);
using pASurfaceTransaction_apply = int64_t (*)(ASurfaceTransaction*);
using pASurfaceTransaction_setOnComplete =
    void (*)(ASurfaceTransaction*, void* ctx, ASurfaceTransaction_OnComplete);
using pASurfaceTransaction_setOnCommit = void (*)(ASurfaceTransaction*,
                                                  void* ctx,
                                                  ASurfaceTransaction_OnCommit);
using pASurfaceTransaction_setVisibility = void (*)(ASurfaceTransaction*,
                                                    ASurfaceControl*,
                                                    int8_t visibility);
using pASurfaceTransaction_setZOrder =
    void (*)(ASurfaceTransaction* transaction, ASurfaceControl*, int32_t z);
using pASurfaceTransaction_setBuffer =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl*,
             AHardwareBuffer*,
             int32_t fence_fd);
using pASurfaceTransaction_setGeometry =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             const ARect& src,
             const ARect& dst,
             int32_t transform);
using pASurfaceTransaction_setPosition =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             int32_t x,
             int32_t y);
using pASurfaceTransaction_setScale = void (*)(ASurfaceTransaction* transaction,
                                               ASurfaceControl* surface,
                                               float x_scale,
                                               float y_scale);
using pASurfaceTransaction_setCrop = void (*)(ASurfaceTransaction* transaction,
                                              ASurfaceControl* surface,
                                              const ARect& src);
using pASurfaceTransaction_setBufferTransparency =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             int8_t transparency);
using pASurfaceTransaction_setDamageRegion =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             const ARect rects[],
             uint32_t count);
using pASurfaceTransaction_setBufferDataSpace =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             uint64_t data_space);
using pASurfaceTransaction_setHdrMetadata_cta861_3 =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             struct AHdrMetadata_cta861_3* metadata);
using pASurfaceTransaction_setHdrMetadata_smpte2086 =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface,
             struct AHdrMetadata_smpte2086* metadata);
using pASurfaceTransaction_setExtendedRangeBrightness =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface_control,
             float currentBufferRatio,
             float desiredRatio);
using pASurfaceTransaction_setFrameRate =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface_control,
             float frameRate,
             int8_t compatibility);
using pASurfaceTransaction_setFrameTimeline =
    void (*)(ASurfaceTransaction* transaction, int64_t vsync_id);
using pASurfaceTransaction_reparent = void (*)(ASurfaceTransaction*,
                                               ASurfaceControl* surface_control,
                                               ASurfaceControl* new_parent);
// ASurfaceTransactionStats
using pASurfaceTransactionStats_getPresentFenceFd =
    int (*)(ASurfaceTransactionStats* stats);
using pASurfaceTransactionStats_getLatchTime =
    int64_t (*)(ASurfaceTransactionStats* stats);
using pASurfaceTransactionStats_getASurfaceControls =
    void (*)(ASurfaceTransactionStats* stats,
             ASurfaceControl*** surface_controls,
             size_t* size);
using pASurfaceTransactionStats_releaseASurfaceControls =
    void (*)(ASurfaceControl** surface_controls);
using pASurfaceTransactionStats_getPreviousReleaseFenceFd =
    int (*)(ASurfaceTransactionStats* stats, ASurfaceControl* surface_control);
using pASurfaceTransaction_setEnableBackPressure =
    void (*)(ASurfaceTransaction* transaction,
             ASurfaceControl* surface_control,
             bool enable_back_pressure);
}

namespace gfx {
namespace {

base::AtomicSequenceNumber g_next_transaction_id;

uint64_t g_agb_required_usage_bits = AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;

#define LOAD_FUNCTION(lib, func)                             \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##Fn) {                                         \
      supported = false;                                     \
      LOG(ERROR) << "Unable to load function " << #func;     \
    }                                                        \
  } while (0)

#define LOAD_FUNCTION_MAYBE(lib, func)                       \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
  } while (0)

struct SurfaceControlMethods {
 public:
  static SurfaceControlMethods& GetImpl(bool load_functions) {
    static SurfaceControlMethods instance(load_functions);
    return instance;
  }

  static const SurfaceControlMethods& Get() {
    return GetImpl(/*load_functions=*/true);
  }

  void InitWithStubs() {
    struct TransactionStub {
      ASurfaceTransaction_OnComplete on_complete = nullptr;
      raw_ptr<void> on_complete_ctx = nullptr;
      ASurfaceTransaction_OnCommit on_commit = nullptr;
      raw_ptr<void> on_commit_ctx = nullptr;
    };

    ASurfaceTransaction_createFn = []() {
      return reinterpret_cast<ASurfaceTransaction*>(new TransactionStub);
    };
    ASurfaceTransaction_deleteFn = [](ASurfaceTransaction* transaction) {
      delete reinterpret_cast<TransactionStub*>(transaction);
    };
    ASurfaceTransaction_applyFn = [](ASurfaceTransaction* transaction) {
      auto* stub = reinterpret_cast<TransactionStub*>(transaction);

      if (stub->on_commit)
        stub->on_commit(stub->on_commit_ctx, nullptr);
      stub->on_commit = nullptr;
      stub->on_commit_ctx = nullptr;

      if (stub->on_complete)
        stub->on_complete(stub->on_complete_ctx, nullptr);
      stub->on_complete = nullptr;
      stub->on_complete_ctx = nullptr;

      return static_cast<int64_t>(0);
    };

    ASurfaceTransaction_setOnCompleteFn =
        [](ASurfaceTransaction* transaction, void* ctx,
           ASurfaceTransaction_OnComplete callback) {
          auto* stub = reinterpret_cast<TransactionStub*>(transaction);
          stub->on_complete = callback;
          stub->on_complete_ctx = ctx;
        };

    ASurfaceTransaction_setOnCommitFn =
        [](ASurfaceTransaction* transaction, void* ctx,
           ASurfaceTransaction_OnCommit callback) {
          auto* stub = reinterpret_cast<TransactionStub*>(transaction);
          stub->on_commit = callback;
          stub->on_commit_ctx = ctx;
        };
  }

  SurfaceControlMethods(bool load_functions) {
    if (!load_functions)
      return;

    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load android so";
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_createFromWindow);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_create);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceControl_fromJava);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_release);

    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_create);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_delete);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_apply);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setOnComplete);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceTransaction_setOnCommit);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_reparent);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setVisibility);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setZOrder);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBuffer);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setGeometry);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceTransaction_setPosition);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceTransaction_setScale);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceTransaction_setCrop);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBufferTransparency);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setDamageRegion);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBufferDataSpace);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setHdrMetadata_cta861_3);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setHdrMetadata_smpte2086);
    LOAD_FUNCTION_MAYBE(main_dl_handle,
                        ASurfaceTransaction_setExtendedRangeBrightness);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceTransaction_setFrameRate);
    LOAD_FUNCTION_MAYBE(main_dl_handle, ASurfaceTransaction_setFrameTimeline);

    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getPresentFenceFd);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getLatchTime);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getASurfaceControls);
    LOAD_FUNCTION(main_dl_handle,
                  ASurfaceTransactionStats_releaseASurfaceControls);
    LOAD_FUNCTION(main_dl_handle,
                  ASurfaceTransactionStats_getPreviousReleaseFenceFd);
    LOAD_FUNCTION_MAYBE(main_dl_handle,
                        ASurfaceTransaction_setEnableBackPressure);
  }

  ~SurfaceControlMethods() = default;

  bool supported = true;
  // Surface methods.
  pASurfaceControl_createFromWindow ASurfaceControl_createFromWindowFn;
  pASurfaceControl_create ASurfaceControl_createFn;
  pASurfaceControl_fromJava ASurfaceControl_fromJavaFn;
  pASurfaceControl_release ASurfaceControl_releaseFn;

  // Transaction methods.
  pASurfaceTransaction_create ASurfaceTransaction_createFn;
  pASurfaceTransaction_delete ASurfaceTransaction_deleteFn;
  pASurfaceTransaction_apply ASurfaceTransaction_applyFn;
  pASurfaceTransaction_setOnComplete ASurfaceTransaction_setOnCompleteFn;
  pASurfaceTransaction_setOnCommit ASurfaceTransaction_setOnCommitFn;
  pASurfaceTransaction_reparent ASurfaceTransaction_reparentFn;
  pASurfaceTransaction_setVisibility ASurfaceTransaction_setVisibilityFn;
  pASurfaceTransaction_setZOrder ASurfaceTransaction_setZOrderFn;
  pASurfaceTransaction_setBuffer ASurfaceTransaction_setBufferFn;
  pASurfaceTransaction_setGeometry ASurfaceTransaction_setGeometryFn;
  pASurfaceTransaction_setPosition ASurfaceTransaction_setPositionFn;
  pASurfaceTransaction_setScale ASurfaceTransaction_setScaleFn;
  pASurfaceTransaction_setCrop ASurfaceTransaction_setCropFn;
  pASurfaceTransaction_setBufferTransparency
      ASurfaceTransaction_setBufferTransparencyFn;
  pASurfaceTransaction_setDamageRegion ASurfaceTransaction_setDamageRegionFn;
  pASurfaceTransaction_setBufferDataSpace
      ASurfaceTransaction_setBufferDataSpaceFn;
  pASurfaceTransaction_setHdrMetadata_cta861_3
      ASurfaceTransaction_setHdrMetadata_cta861_3Fn;
  pASurfaceTransaction_setHdrMetadata_smpte2086
      ASurfaceTransaction_setHdrMetadata_smpte2086Fn;
  pASurfaceTransaction_setExtendedRangeBrightness
      ASurfaceTransaction_setExtendedRangeBrightnessFn;

  pASurfaceTransaction_setFrameRate ASurfaceTransaction_setFrameRateFn;
  pASurfaceTransaction_setFrameTimeline ASurfaceTransaction_setFrameTimelineFn;
  pASurfaceTransaction_setEnableBackPressure
      ASurfaceTransaction_setEnableBackPressureFn;

  // TransactionStats methods.
  pASurfaceTransactionStats_getPresentFenceFd
      ASurfaceTransactionStats_getPresentFenceFdFn;
  pASurfaceTransactionStats_getLatchTime
      ASurfaceTransactionStats_getLatchTimeFn;
  pASurfaceTransactionStats_getASurfaceControls
      ASurfaceTransactionStats_getASurfaceControlsFn;
  pASurfaceTransactionStats_releaseASurfaceControls
      ASurfaceTransactionStats_releaseASurfaceControlsFn;
  pASurfaceTransactionStats_getPreviousReleaseFenceFd
      ASurfaceTransactionStats_getPreviousReleaseFenceFdFn;
};

ARect RectToARect(const gfx::Rect& rect) {
  return ARect{rect.x(), rect.y(), rect.right(), rect.bottom()};
}

int32_t OverlayTransformToWindowTransform(gfx::OverlayTransform transform) {
  // Note that the gfx::OverlayTransform expresses rotations in anticlockwise
  // direction while the ANativeWindow rotations are in clockwise direction.
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_INVALID:
      DCHECK(false) << "Invalid Transform";
      return ANATIVEWINDOW_TRANSFORM_IDENTITY;
    case gfx::OVERLAY_TRANSFORM_NONE:
      return ANATIVEWINDOW_TRANSFORM_IDENTITY;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return ANATIVEWINDOW_TRANSFORM_MIRROR_HORIZONTAL;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return ANATIVEWINDOW_TRANSFORM_MIRROR_VERTICAL;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return ANATIVEWINDOW_TRANSFORM_ROTATE_270;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return ANATIVEWINDOW_TRANSFORM_ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return ANATIVEWINDOW_TRANSFORM_ROTATE_90;
  };
  NOTREACHED();
  return ANATIVEWINDOW_TRANSFORM_IDENTITY;
}

// Remove this and use ADataSpace when SDK will roll. Note, this doesn't define
// any new data spaces, just defines a primary(standard)/transfer/range
// separately.
enum DataSpace : uint64_t {
  // Primaries
  STANDARD_BT709 = 1 << 16,
  STANDARD_BT601_625 = 2 << 16,
  STANDARD_BT601_525 = 4 << 16,
  STANDARD_BT2020 = 6 << 16,
  // Transfer functions
  TRANSFER_LINEAR = 1 << 22,
  TRANSFER_SRGB = 2 << 22,
  TRANSFER_SMPTE_170M = 3 << 22,
  TRANSFER_ST2084 = 7 << 22,
  TRANSFER_HLG = 8 << 22,
  // Ranges;
  RANGE_FULL = 1 << 27,
  RANGE_LIMITED = 2 << 27,
  RANGE_EXTENDED = 3 << 27,
  RANGE_MASK = 7 << 27,

  ADATASPACE_DCI_P3 = 155844608
};

absl::optional<uint64_t> GetDataSpaceStandard(
    const gfx::ColorSpace& color_space) {
  switch (color_space.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709:
      return DataSpace::STANDARD_BT709;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      return DataSpace::STANDARD_BT601_625;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      return DataSpace::STANDARD_BT601_525;
    case gfx::ColorSpace::PrimaryID::BT2020:
      return DataSpace::STANDARD_BT2020;
    default:
      return absl::nullopt;
  }
}

absl::optional<uint64_t> GetDataSpaceTransfer(
    const gfx::ColorSpace& color_space) {
  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::SMPTE170M:
      return DataSpace::TRANSFER_SMPTE_170M;
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
      return DataSpace::TRANSFER_LINEAR;
    case gfx::ColorSpace::TransferID::PQ:
      return DataSpace::TRANSFER_ST2084;
    case gfx::ColorSpace::TransferID::HLG:
      return DataSpace::TRANSFER_HLG;
    // We use SRGB for BT709. See |ColorSpace::GetTransferFunction()| for
    // details.
    case gfx::ColorSpace::TransferID::BT709:
      return DataSpace::TRANSFER_SRGB;
    default:
      return absl::nullopt;
  }
}

absl::optional<uint64_t> GetDataSpaceRange(const gfx::ColorSpace& color_space) {
  switch (color_space.GetRangeID()) {
    case gfx::ColorSpace::RangeID::FULL:
      return DataSpace::RANGE_FULL;
    case gfx::ColorSpace::RangeID::LIMITED:
      return DataSpace::RANGE_LIMITED;
    default:
      return absl::nullopt;
  };
}

uint64_t ColorSpaceToADataSpace(const gfx::ColorSpace& color_space) {
  if (!color_space.IsValid() || color_space == gfx::ColorSpace::CreateSRGB())
    return ADATASPACE_SRGB;

  if (color_space == gfx::ColorSpace::CreateSRGBLinear())
    return ADATASPACE_SCRGB_LINEAR;

  if (color_space == gfx::ColorSpace::CreateDisplayP3D65())
    return ADATASPACE_DISPLAY_P3;

  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_S) {
    if (color_space == gfx::ColorSpace::CreateExtendedSRGB()) {
      return DataSpace::STANDARD_BT709 | DataSpace::TRANSFER_SRGB |
             DataSpace::RANGE_EXTENDED;
    }

    auto standard = GetDataSpaceStandard(color_space);
    auto transfer = GetDataSpaceTransfer(color_space);
    auto range = GetDataSpaceRange(color_space);

    // Data space is set of the flags, so check if all components are valid.
    if (standard && transfer && range)
      return standard.value() | transfer.value() | range.value();
  }

  return ADATASPACE_UNKNOWN;
}

SurfaceControl::TransactionStats ToTransactionStats(
    ASurfaceTransactionStats* stats) {
  SurfaceControl::TransactionStats transaction_stats;

  // In unit tests we don't have stats.
  if (!stats)
    return transaction_stats;

  transaction_stats.present_fence = base::ScopedFD(
      SurfaceControlMethods::Get().ASurfaceTransactionStats_getPresentFenceFdFn(
          stats));
  transaction_stats.latch_time =
      base::TimeTicks() +
      base::Nanoseconds(
          SurfaceControlMethods::Get().ASurfaceTransactionStats_getLatchTimeFn(
              stats));
  if (transaction_stats.latch_time == base::TimeTicks())
    transaction_stats.latch_time = base::TimeTicks::Now();

  ASurfaceControl** surface_controls = nullptr;
  size_t size = 0u;
  SurfaceControlMethods::Get().ASurfaceTransactionStats_getASurfaceControlsFn(
      stats, &surface_controls, &size);
  transaction_stats.surface_stats.resize(size);
  for (size_t i = 0u; i < size; ++i) {
    transaction_stats.surface_stats[i].surface = surface_controls[i];
    int fence_fd = SurfaceControlMethods::Get()
                       .ASurfaceTransactionStats_getPreviousReleaseFenceFdFn(
                           stats, surface_controls[i]);
    if (fence_fd != -1) {
      transaction_stats.surface_stats[i].fence = base::ScopedFD(fence_fd);
    }
  }
  SurfaceControlMethods::Get()
      .ASurfaceTransactionStats_releaseASurfaceControlsFn(surface_controls);

  return transaction_stats;
}

struct TransactionAckCtx {
  int id = 0;
  SurfaceControl::Transaction::OnCompleteCb callback;
  SurfaceControl::Transaction::OnCommitCb latch_callback;
};

uint64_t GetTraceIdForTransaction(int transaction_id) {
  constexpr uint64_t kMask =
      base::MD5Hash64Constexpr("SurfaceControl::Transaction");
  return kMask ^ transaction_id;
}

base::Lock& GetGlobalLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

base::flat_set<int>& GetGlobalPendingCompleteCallbackIds() {
  static base::NoDestructor<base::flat_set<int>> set;
  return *set;
}

// Note that the framework API states that this callback can be dispatched on
// any thread (in practice it should be a binder thread).
void OnTransactionCompletedOnAnyThread(void* context,
                                       ASurfaceTransactionStats* stats) {
  auto* ack_ctx = static_cast<TransactionAckCtx*>(context);
  auto transaction_stats = ToTransactionStats(stats);
  TRACE_EVENT_NESTABLE_ASYNC_END0("gpu,benchmark", "SurfaceControlTransaction",
                                  ack_ctx->id);
  TRACE_EVENT_WITH_FLOW0(
      "toplevel.flow", "gfx::SurfaceControlTransaction completed",
      GetTraceIdForTransaction(ack_ctx->id), TRACE_EVENT_FLAG_FLOW_IN);

  bool dump = false;
  {
    base::AutoLock lock(GetGlobalLock());
    size_t num_removed =
        GetGlobalPendingCompleteCallbackIds().erase(ack_ctx->id);
    dump = !num_removed;
  }
  if (dump) {
    base::debug::DumpWithoutCrashing(base::Location::Current(), base::Days(1));
  }

  std::move(ack_ctx->callback).Run(std::move(transaction_stats));
  delete ack_ctx;
}

// Note that the framework API states that this callback can be dispatched on
// any thread (in practice it should be a binder thread).
void OnTransactiOnCommittedOnAnyThread(void* context,
                                       ASurfaceTransactionStats* stats) {
  auto* ack_ctx = static_cast<TransactionAckCtx*>(context);
  TRACE_EVENT_INSTANT0("gpu,benchmark", "SurfaceControlTransaction committed",
                       TRACE_EVENT_SCOPE_THREAD);

  std::move(ack_ctx->latch_callback).Run();
  delete ack_ctx;
}

}  // namespace

// static
bool SurfaceControl::IsSupported() {
  const auto* build_info = base::android::BuildInfo::GetInstance();

  // Disabled on Samsung devices due to a platform bug fixed in R.
  int min_sdk_version = base::android::SDK_VERSION_Q;
  if (base::EqualsCaseInsensitiveASCII(build_info->manufacturer(), "samsung"))
    min_sdk_version = base::android::SDK_VERSION_R;

  if (build_info->sdk_int() < min_sdk_version)
    return false;

  CHECK(SurfaceControlMethods::Get().supported);
  return true;
}

bool SurfaceControl::SupportsColorSpace(const gfx::ColorSpace& color_space) {
  return ColorSpaceToADataSpace(color_space) != ADATASPACE_UNKNOWN;
}

uint64_t SurfaceControl::RequiredUsage() {
  if (!IsSupported())
    return 0u;
  return g_agb_required_usage_bits;
}

void SurfaceControl::EnableQualcommUBWC() {
  g_agb_required_usage_bits |= AHARDWAREBUFFER_USAGE_VENDOR_0;
}

bool SurfaceControl::SupportsSetFrameRate() {
  // TODO(khushalsagar): Assert that this function is always available on R.
  return IsSupported() &&
         SurfaceControlMethods::Get().ASurfaceTransaction_setFrameRateFn !=
             nullptr;
}

bool SurfaceControl::SupportsOnCommit() {
  return IsSupported() &&
         SurfaceControlMethods::Get().ASurfaceTransaction_setOnCommitFn !=
             nullptr;
}

bool SurfaceControl::SupportsSetFrameTimeline() {
  return IsSupported() &&
         SurfaceControlMethods::Get().ASurfaceTransaction_setFrameTimelineFn !=
             nullptr;
}

bool SurfaceControl::SupportsSurfacelessControl() {
  return IsSupported() &&
         !!SurfaceControlMethods::Get().ASurfaceControl_fromJavaFn;
}

bool SurfaceControl::SupportsSetEnableBackPressure() {
  return IsSupported() &&
         SurfaceControlMethods::Get()
                 .ASurfaceTransaction_setEnableBackPressureFn != nullptr;
}

void SurfaceControl::SetStubImplementationForTesting() {
  SurfaceControlMethods::GetImpl(/*load_functions=*/false).InitWithStubs();
}

void SurfaceControl::ApplyTransaction(ASurfaceTransaction* transaction) {
  SurfaceControlMethods::Get().ASurfaceTransaction_applyFn(transaction);
}

scoped_refptr<SurfaceControl::Surface> SurfaceControl::Surface::WrapUnowned(
    ASurfaceControl* surface) {
  scoped_refptr<SurfaceControl::Surface> result =
      base::MakeRefCounted<SurfaceControl::Surface>();
  result->surface_ = surface;
  return result;
}

SurfaceControl::Surface::Surface() = default;

SurfaceControl::Surface::Surface(const Surface& parent, const char* name) {
  owned_surface_ = SurfaceControlMethods::Get().ASurfaceControl_createFn(
      parent.surface(), name);
  if (!owned_surface_)
    LOG(ERROR) << "Failed to create ASurfaceControl : " << name;
  surface_ = owned_surface_;
}

SurfaceControl::Surface::Surface(ANativeWindow* parent, const char* name) {
  owned_surface_ =
      SurfaceControlMethods::Get().ASurfaceControl_createFromWindowFn(parent,
                                                                      name);
  if (!owned_surface_)
    LOG(ERROR) << "Failed to create ASurfaceControl : " << name;
  surface_ = owned_surface_;
}

SurfaceControl::Surface::Surface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_surface_control) {
  CHECK(SupportsSurfacelessControl());
  owned_surface_ = SurfaceControlMethods::Get().ASurfaceControl_fromJavaFn(
      env, j_surface_control.obj());
  if (!owned_surface_) {
    LOG(ERROR) << "Failed to obtain ASurfaceControl from java";
    return;
  }
  surface_ = owned_surface_;
}

SurfaceControl::Surface::~Surface() {
  if (owned_surface_)
    SurfaceControlMethods::Get().ASurfaceControl_releaseFn(owned_surface_);
}

SurfaceControl::SurfaceStats::SurfaceStats() = default;
SurfaceControl::SurfaceStats::~SurfaceStats() = default;

SurfaceControl::SurfaceStats::SurfaceStats(SurfaceStats&& other) = default;
SurfaceControl::SurfaceStats& SurfaceControl::SurfaceStats::operator=(
    SurfaceStats&& other) = default;

SurfaceControl::TransactionStats::TransactionStats() = default;
SurfaceControl::TransactionStats::~TransactionStats() = default;

SurfaceControl::TransactionStats::TransactionStats(TransactionStats&& other) =
    default;
SurfaceControl::TransactionStats& SurfaceControl::TransactionStats::operator=(
    TransactionStats&& other) = default;

SurfaceControl::Transaction::Transaction()
    : id_(g_next_transaction_id.GetNext()) {
  transaction_ = SurfaceControlMethods::Get().ASurfaceTransaction_createFn();
  DCHECK(transaction_);
}

SurfaceControl::Transaction::~Transaction() {
  DestroyIfNeeded();
}

void SurfaceControl::Transaction::DestroyIfNeeded() {
  if (!transaction_)
    return;
  if (need_to_apply_)
    SurfaceControlMethods::Get().ASurfaceTransaction_applyFn(transaction_);
  SurfaceControlMethods::Get().ASurfaceTransaction_deleteFn(transaction_);
  transaction_ = nullptr;
}

SurfaceControl::Transaction::Transaction(Transaction&& other)
    : id_(other.id_),
      transaction_(other.transaction_),
      on_commit_cb_(std::move(other.on_commit_cb_)),
      on_complete_cb_(std::move(other.on_complete_cb_)),
      need_to_apply_(other.need_to_apply_) {
  other.transaction_ = nullptr;
  other.id_ = 0;
  other.need_to_apply_ = false;
}

SurfaceControl::Transaction& SurfaceControl::Transaction::operator=(
    Transaction&& other) {
  if (this == &other)
    return *this;

  DestroyIfNeeded();

  transaction_ = other.transaction_;
  id_ = other.id_;
  on_commit_cb_ = std::move(other.on_commit_cb_);
  on_complete_cb_ = std::move(other.on_complete_cb_);
  need_to_apply_ = other.need_to_apply_;

  other.transaction_ = nullptr;
  other.id_ = 0;
  other.need_to_apply_ = false;
  return *this;
}

void SurfaceControl::Transaction::SetVisibility(const Surface& surface,
                                                bool show) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setVisibilityFn(
      transaction_, surface.surface(), show);
}

void SurfaceControl::Transaction::SetZOrder(const Surface& surface, int32_t z) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setZOrderFn(
      transaction_, surface.surface(), z);
}

void SurfaceControl::Transaction::SetBuffer(const Surface& surface,
                                            AHardwareBuffer* buffer,
                                            base::ScopedFD fence_fd) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferFn(
      transaction_, surface.surface(), buffer,
      fence_fd.is_valid() ? fence_fd.release() : -1);
  // In T OS, setBuffer call setOnComplete internally, so Apply() is required to
  // decrease ref count of SurfaceControl.
  // TODO(crbug.com/1395271): remove this if AOSP fix the issue
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_T) {
    need_to_apply_ = true;
  }
}

void SurfaceControl::Transaction::SetGeometry(const Surface& surface,
                                              const gfx::Rect& src,
                                              const gfx::Rect& dst,
                                              gfx::OverlayTransform transform) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setGeometryFn(
      transaction_, surface.surface(), RectToARect(src), RectToARect(dst),
      OverlayTransformToWindowTransform(transform));
}

void SurfaceControl::Transaction::SetPosition(const Surface& surface,
                                              const gfx::Point& position) {
  CHECK(SurfaceControlMethods::Get().ASurfaceTransaction_setPositionFn);
  SurfaceControlMethods::Get().ASurfaceTransaction_setPositionFn(
      transaction_, surface.surface(), position.x(), position.y());
}

void SurfaceControl::Transaction::SetScale(const Surface& surface,
                                           const float sx,
                                           float sy) {
  CHECK(SurfaceControlMethods::Get().ASurfaceTransaction_setScaleFn);
  SurfaceControlMethods::Get().ASurfaceTransaction_setScaleFn(
      transaction_, surface.surface(), sx, sy);
}

void SurfaceControl::Transaction::SetCrop(const Surface& surface,
                                          const gfx::Rect& rect) {
  CHECK(SurfaceControlMethods::Get().ASurfaceTransaction_setCropFn);
  SurfaceControlMethods::Get().ASurfaceTransaction_setCropFn(
      transaction_, surface.surface(), RectToARect(rect));
}

void SurfaceControl::Transaction::SetFrameTimelineId(int64_t vsync_id) {
  CHECK(SurfaceControlMethods::Get().ASurfaceTransaction_setFrameTimelineFn);
  SurfaceControlMethods::Get().ASurfaceTransaction_setFrameTimelineFn(
      transaction_, vsync_id);
}

void SurfaceControl::Transaction::SetOpaque(const Surface& surface,
                                            bool opaque) {
  int8_t transparency = opaque ? ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE
                               : ASURFACE_TRANSACTION_TRANSPARENCY_TRANSLUCENT;
  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferTransparencyFn(
      transaction_, surface.surface(), transparency);
}

void SurfaceControl::Transaction::SetDamageRect(const Surface& surface,
                                                const gfx::Rect& rect) {
  auto a_rect = RectToARect(rect);
  SurfaceControlMethods::Get().ASurfaceTransaction_setDamageRegionFn(
      transaction_, surface.surface(), &a_rect, 1u);
}

void SurfaceControl::Transaction::SetColorSpace(
    const Surface& surface,
    const gfx::ColorSpace& color_space,
    const absl::optional<HDRMetadata>& metadata) {
  // Metadata shouldn't exist for SDR color spaces.
  DCHECK(!metadata || color_space.IsHDR());

  auto data_space = ColorSpaceToADataSpace(color_space);

  // Log the data space in crash keys for debugging crbug.com/997592.
  static auto* kCrashKey = base::debug::AllocateCrashKeyString(
      "data_space_for_buffer", base::debug::CrashKeySize::Size256);
  auto crash_key_value = base::NumberToString(data_space);
  base::debug::ScopedCrashKeyString scoped_crash_key(kCrashKey,
                                                     crash_key_value);

  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferDataSpaceFn(
      transaction_, surface.surface(), data_space);

  const bool extended_range =
      (data_space & DataSpace::RANGE_MASK) == DataSpace::RANGE_EXTENDED;

  // Set the HDR metadata for not extended SRGB case.
  if (metadata && !extended_range) {
    AHdrMetadata_cta861_3 cta861_3 = {
        .maxContentLightLevel =
            static_cast<float>(metadata->max_content_light_level),
        .maxFrameAverageLightLevel =
            static_cast<float>(metadata->max_frame_average_light_level)};

    const auto& primaries = metadata->color_volume_metadata.primaries;
    AHdrMetadata_smpte2086 smpte2086 = {
        .displayPrimaryRed = {.x = primaries.fRX, .y = primaries.fRY},
        .displayPrimaryGreen = {.x = primaries.fGX, .y = primaries.fGY},
        .displayPrimaryBlue = {.x = primaries.fBX, .y = primaries.fBY},
        .whitePoint = {.x = primaries.fWX, .y = primaries.fWY},
        .maxLuminance = metadata->color_volume_metadata.luminance_max,
        .minLuminance = metadata->color_volume_metadata.luminance_min};

    SurfaceControlMethods::Get().ASurfaceTransaction_setHdrMetadata_cta861_3Fn(
        transaction_, surface.surface(), &cta861_3);
    SurfaceControlMethods::Get().ASurfaceTransaction_setHdrMetadata_smpte2086Fn(
        transaction_, surface.surface(), &smpte2086);
  } else {
    SurfaceControlMethods::Get().ASurfaceTransaction_setHdrMetadata_cta861_3Fn(
        transaction_, surface.surface(), nullptr);
    SurfaceControlMethods::Get().ASurfaceTransaction_setHdrMetadata_smpte2086Fn(
        transaction_, surface.surface(), nullptr);
  }

  // Set brightness points for extended range.
  if (extended_range) {
    CHECK(metadata);
    CHECK(metadata->extended_range_brightness);
    CHECK(SurfaceControlMethods::Get()
              .ASurfaceTransaction_setExtendedRangeBrightnessFn);
    SurfaceControlMethods::Get()
        .ASurfaceTransaction_setExtendedRangeBrightnessFn(
            transaction_, surface.surface(),
            metadata->extended_range_brightness->current_buffer_ratio,
            metadata->extended_range_brightness->desired_ratio);
  } else {
    // If extended range brightness is supported, we need reset it to default
    // values.
    if (SurfaceControlMethods::Get()
            .ASurfaceTransaction_setExtendedRangeBrightnessFn) {
      SurfaceControlMethods::Get()
          .ASurfaceTransaction_setExtendedRangeBrightnessFn(
              transaction_, surface.surface(), 1.0f, 1.0f);
    }
  }
}

void SurfaceControl::Transaction::SetFrameRate(const Surface& surface,
                                               float frame_rate) {
  DCHECK(SupportsSetFrameRate());

  // We always used fixed source here since a non-default value is only used for
  // videos which have a fixed playback rate.
  SurfaceControlMethods::Get().ASurfaceTransaction_setFrameRateFn(
      transaction_, surface.surface(), frame_rate,
      ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
}

void SurfaceControl::Transaction::SetParent(const Surface& surface,
                                            Surface* new_parent) {
  SurfaceControlMethods::Get().ASurfaceTransaction_reparentFn(
      transaction_, surface.surface(),
      new_parent ? new_parent->surface() : nullptr);
}

void SurfaceControl::Transaction::SetOnCompleteCb(
    OnCompleteCb cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  TRACE_EVENT_WITH_FLOW0(
      "toplevel.flow", "gfx::SurfaceControl::Transaction::SetOnCompleteCb",
      GetTraceIdForTransaction(id_), TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK(!on_complete_cb_);
  on_complete_cb_ = base::BindPostTask(std::move(task_runner), std::move(cb));
}

void SurfaceControl::Transaction::SetOnCommitCb(
    OnCommitCb cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!on_commit_cb_);
  on_commit_cb_ = base::BindPostTask(std::move(task_runner), std::move(cb));
}

void SurfaceControl::Transaction::Apply() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("gpu,benchmark",
                                    "SurfaceControlTransaction", id_);

  PrepareCallbacks();
  SurfaceControlMethods::Get().ASurfaceTransaction_applyFn(transaction_);
  need_to_apply_ = false;
}

ASurfaceTransaction* SurfaceControl::Transaction::GetTransaction() {
  PrepareCallbacks();
  need_to_apply_ = false;
  return transaction_;
}

void SurfaceControl::Transaction::PrepareCallbacks() {
  if (on_commit_cb_) {
    TransactionAckCtx* ack_ctx = new TransactionAckCtx;
    ack_ctx->latch_callback = std::move(on_commit_cb_);
    ack_ctx->id = id_;

    SurfaceControlMethods::Get().ASurfaceTransaction_setOnCommitFn(
        transaction_, ack_ctx, &OnTransactiOnCommittedOnAnyThread);
    // setOnCommit and setOnComplete increase ref count of SurfaceControl and
    // Apply() is required to decrease the ref count.
    need_to_apply_ = true;
  }

  if (on_complete_cb_) {
    TransactionAckCtx* ack_ctx = new TransactionAckCtx;
    ack_ctx->callback = std::move(on_complete_cb_);
    ack_ctx->id = id_;

    bool dump = false;
    {
      base::AutoLock lock(GetGlobalLock());
      auto result = GetGlobalPendingCompleteCallbackIds().insert(id_);
      dump = !result.second;
    }
    if (dump) {
      base::debug::DumpWithoutCrashing(base::Location::Current(),
                                       base::Days(1));
    }
    SurfaceControlMethods::Get().ASurfaceTransaction_setOnCompleteFn(
        transaction_, ack_ctx, &OnTransactionCompletedOnAnyThread);
    need_to_apply_ = true;
  }
}

void SurfaceControl::Transaction::SetEnableBackPressure(const Surface& surface,
                                                        bool enable) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setEnableBackPressureFn(
      transaction_, surface.surface(), enable);
}

}  // namespace gfx
