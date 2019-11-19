// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/android_surface_control_compat.h"

#include <dlfcn.h>
#include <android/ndk-version.h>
#if __NDK_MAJOR__ >= 18
#include <android/data_space.h>
#endif

#include "base/android/build_info.h"
#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/color_space.h"

extern "C" {
typedef struct ASurfaceTransactionStats ASurfaceTransactionStats;
typedef void (*ASurfaceTransaction_OnComplete)(void* context,
                                               ASurfaceTransactionStats* stats);

// ASurface
using pASurfaceControl_createFromWindow =
    ASurfaceControl* (*)(ANativeWindow* parent, const char* name);
using pASurfaceControl_create = ASurfaceControl* (*)(ASurfaceControl* parent,
                                                     const char* name);
using pASurfaceControl_release = void (*)(ASurfaceControl*);

// ASurfaceTransaction enums
enum {
  ASURFACE_TRANSACTION_TRANSPARENCY_TRANSPARENT = 0,
  ASURFACE_TRANSACTION_TRANSPARENCY_TRANSLUCENT = 1,
  ASURFACE_TRANSACTION_TRANSPARENCY_OPAQUE = 2,
};

#if __NDK_MAJOR__ < 18
enum {
  ADATASPACE_UNKNOWN = 0,
  ADATASPACE_SCRGB_LINEAR = 406913024,
  ADATASPACE_SRGB = 142671872,
  ADATASPACE_DISPLAY_P3 = 143261696,
  ADATASPACE_BT2020_PQ = 163971072,
};
#endif

#if __NDK_MAJOR__ < 20
enum {
  AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY = 1ULL << 11,
};
#endif

// ASurfaceTransaction
using pASurfaceTransaction_create = ASurfaceTransaction* (*)(void);
using pASurfaceTransaction_delete = void (*)(ASurfaceTransaction*);
using pASurfaceTransaction_apply = int64_t (*)(ASurfaceTransaction*);
using pASurfaceTransaction_setOnComplete =
    void (*)(ASurfaceTransaction*, void* ctx, ASurfaceTransaction_OnComplete);
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
}

namespace gl {
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

struct SurfaceControlMethods {
 public:
  static const SurfaceControlMethods& Get() {
    static const base::NoDestructor<SurfaceControlMethods> instance;
    return *instance;
  }

  SurfaceControlMethods() {
    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load android so";
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_createFromWindow);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_create);
    LOAD_FUNCTION(main_dl_handle, ASurfaceControl_release);

    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_create);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_delete);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_apply);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setOnComplete);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setVisibility);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setZOrder);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBuffer);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setGeometry);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBufferTransparency);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setDamageRegion);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransaction_setBufferDataSpace);

    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getPresentFenceFd);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getLatchTime);
    LOAD_FUNCTION(main_dl_handle, ASurfaceTransactionStats_getASurfaceControls);
    LOAD_FUNCTION(main_dl_handle,
                  ASurfaceTransactionStats_releaseASurfaceControls);
    LOAD_FUNCTION(main_dl_handle,
                  ASurfaceTransactionStats_getPreviousReleaseFenceFd);
  }

  ~SurfaceControlMethods() = default;

  bool supported = true;
  // Surface methods.
  pASurfaceControl_createFromWindow ASurfaceControl_createFromWindowFn;
  pASurfaceControl_create ASurfaceControl_createFn;
  pASurfaceControl_release ASurfaceControl_releaseFn;

  // Transaction methods.
  pASurfaceTransaction_create ASurfaceTransaction_createFn;
  pASurfaceTransaction_delete ASurfaceTransaction_deleteFn;
  pASurfaceTransaction_apply ASurfaceTransaction_applyFn;
  pASurfaceTransaction_setOnComplete ASurfaceTransaction_setOnCompleteFn;
  pASurfaceTransaction_setVisibility ASurfaceTransaction_setVisibilityFn;
  pASurfaceTransaction_setZOrder ASurfaceTransaction_setZOrderFn;
  pASurfaceTransaction_setBuffer ASurfaceTransaction_setBufferFn;
  pASurfaceTransaction_setGeometry ASurfaceTransaction_setGeometryFn;
  pASurfaceTransaction_setBufferTransparency
      ASurfaceTransaction_setBufferTransparencyFn;
  pASurfaceTransaction_setDamageRegion ASurfaceTransaction_setDamageRegionFn;
  pASurfaceTransaction_setBufferDataSpace
      ASurfaceTransaction_setBufferDataSpaceFn;

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

uint64_t ColorSpaceToADataSpace(const gfx::ColorSpace& color_space) {
  if (!color_space.IsValid() || color_space == gfx::ColorSpace::CreateSRGB())
    return ADATASPACE_SRGB;

  if (color_space == gfx::ColorSpace::CreateSCRGBLinear())
    return ADATASPACE_SCRGB_LINEAR;

  if (color_space == gfx::ColorSpace::CreateDisplayP3D65())
    return ADATASPACE_DISPLAY_P3;

  // TODO(khushalsagar): Check if we can support BT2020 using
  // ADATASPACE_BT2020_PQ.
  return ADATASPACE_UNKNOWN;
}

SurfaceControl::TransactionStats ToTransactionStats(
    ASurfaceTransactionStats* stats) {
  SurfaceControl::TransactionStats transaction_stats;
  transaction_stats.present_fence = base::ScopedFD(
      SurfaceControlMethods::Get().ASurfaceTransactionStats_getPresentFenceFdFn(
          stats));
  transaction_stats.latch_time =
      base::TimeTicks() +
      base::TimeDelta::FromNanoseconds(
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
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  SurfaceControl::Transaction::OnCompleteCb callback;
};

// Note that the framework API states that this callback can be dispatched on
// any thread (in practice it should be the binder thread).
void OnTransactionCompletedOnAnyThread(void* context,
                                       ASurfaceTransactionStats* stats) {
  auto* ack_ctx = static_cast<TransactionAckCtx*>(context);
  auto transaction_stats = ToTransactionStats(stats);
  TRACE_EVENT_ASYNC_END0("gpu,benchmark", "SurfaceControlTransaction",
                         ack_ctx->id);

  if (ack_ctx->task_runner) {
    ack_ctx->task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(ack_ctx->callback),
                                  std::move(transaction_stats)));
  } else {
    std::move(ack_ctx->callback).Run(std::move(transaction_stats));
  }

  delete ack_ctx;
}
}  // namespace

// static
bool SurfaceControl::IsSupported() {
  if (!base::android::BuildInfo::GetInstance()->is_at_least_q())
    return false;
  return SurfaceControlMethods::Get().supported;
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

SurfaceControl::Surface::Surface() = default;

SurfaceControl::Surface::Surface(const Surface& parent, const char* name) {
  surface_ = SurfaceControlMethods::Get().ASurfaceControl_createFn(
      parent.surface(), name);
  if (!surface_)
    LOG(ERROR) << "Failed to create ASurfaceControl : " << name;
}

SurfaceControl::Surface::Surface(ANativeWindow* parent, const char* name) {
  surface_ = SurfaceControlMethods::Get().ASurfaceControl_createFromWindowFn(
      parent, name);
  if (!surface_)
    LOG(ERROR) << "Failed to create ASurfaceControl : " << name;
}

SurfaceControl::Surface::~Surface() {
  if (surface_)
    SurfaceControlMethods::Get().ASurfaceControl_releaseFn(surface_);
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
  if (transaction_)
    SurfaceControlMethods::Get().ASurfaceTransaction_deleteFn(transaction_);
}

SurfaceControl::Transaction::Transaction(Transaction&& other)
    : id_(other.id_), transaction_(other.transaction_) {
  other.transaction_ = nullptr;
  other.id_ = 0;
}

SurfaceControl::Transaction& SurfaceControl::Transaction::operator=(
    Transaction&& other) {
  if (transaction_)
    SurfaceControlMethods::Get().ASurfaceTransaction_deleteFn(transaction_);

  transaction_ = other.transaction_;
  id_ = other.id_;

  other.transaction_ = nullptr;
  other.id_ = 0;
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
}

void SurfaceControl::Transaction::SetGeometry(const Surface& surface,
                                              const gfx::Rect& src,
                                              const gfx::Rect& dst,
                                              gfx::OverlayTransform transform) {
  SurfaceControlMethods::Get().ASurfaceTransaction_setGeometryFn(
      transaction_, surface.surface(), RectToARect(src), RectToARect(dst),
      OverlayTransformToWindowTransform(transform));
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
    const gfx::ColorSpace& color_space) {
  auto data_space = ColorSpaceToADataSpace(color_space);

  // Log the data space in crash keys for debugging crbug.com/997592.
  static auto* kCrashKey = base::debug::AllocateCrashKeyString(
      "data_space_for_buffer", base::debug::CrashKeySize::Size256);
  auto crash_key_value = base::NumberToString(data_space);
  base::debug::ScopedCrashKeyString scoped_crash_key(kCrashKey,
                                                     crash_key_value);

  SurfaceControlMethods::Get().ASurfaceTransaction_setBufferDataSpaceFn(
      transaction_, surface.surface(), data_space);
}

void SurfaceControl::Transaction::SetOnCompleteCb(
    OnCompleteCb cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  TransactionAckCtx* ack_ctx = new TransactionAckCtx;
  ack_ctx->callback = std::move(cb);
  ack_ctx->task_runner = std::move(task_runner);
  ack_ctx->id = id_;

  SurfaceControlMethods::Get().ASurfaceTransaction_setOnCompleteFn(
      transaction_, ack_ctx, &OnTransactionCompletedOnAnyThread);
}

void SurfaceControl::Transaction::Apply() {
  TRACE_EVENT_ASYNC_BEGIN0("gpu,benchmark", "SurfaceControlTransaction", id_);
  SurfaceControlMethods::Get().ASurfaceTransaction_applyFn(transaction_);
}

}  // namespace gl
