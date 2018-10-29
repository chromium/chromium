// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/android_surface_composer_compat.h"

#include "base/android/build_info.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

#include <dlfcn.h>

extern "C" {
// ASurfaceComposer
using pASurfaceComposer_Create = ASurfaceComposer* (*)(ANativeWindow*);
using pASurfaceComposer_Delete = void (*)(ASurfaceComposer*);

// ASurface
using pASurfaceComposer_CreateSurface = ASurface* (*)(ASurfaceComposer*,
                                                      int32_t content_type,
                                                      ASurface* parent,
                                                      const char* name);
using pASurfaceComposer_DeleteSurface = void (*)(ASurface*);

// ASurfaceTransaction
using pASurfaceComposer_CreateTransaction = ASurfaceTransaction* (*)(void);
using pASurfaceComposer_DeleteTransaction = void (*)(ASurfaceTransaction*);
using pASurfaceComposer_TransactionApply = int64_t (*)(ASurfaceTransaction*);
using pASurfaceComposer_TransactionSetVisibility =
    void (*)(ASurfaceTransaction*, ASurface*, bool show);
using pASurfaceComposer_TransactionSetPosition = void (*)(ASurfaceTransaction*,
                                                          ASurface*,
                                                          float x,
                                                          float y);
using pASurfaceComposer_TransactionSetZOrder =
    void (*)(ASurfaceTransaction* transaction, ASurface*, int32_t z);
using pASurfaceComposer_TransactionSetBuffer =
    void (*)(ASurfaceTransaction* transaction,
             ASurface*,
             AHardwareBuffer*,
             int32_t fence_fd);
using pASurfaceComposer_TransactionSetSize =
    void (*)(ASurfaceTransaction* transaction,
             ASurface* surface,
             uint32_t width,
             uint32_t height);
using pASurfaceComposer_TransactionSetCropRect =
    void (*)(ASurfaceTransaction* transaction,
             ASurface* surface,
             int32_t left,
             int32_t top,
             int32_t right,
             int32_t bottom);
using pASurfaceComposer_TransactionSetOpaque =
    void (*)(ASurfaceTransaction* transaction,
             ASurface* surface,
             uint32_t transform);
}

namespace gl {
namespace {

#define LOAD_FUNCTION(lib, func)                             \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##Fn) {                                         \
      supported = false;                                     \
      LOG(ERROR) << "Unable to load function " << #func;     \
    }                                                        \
  } while (0)

struct SurfaceComposerMethods {
 public:
  static const SurfaceComposerMethods& Get() {
    static const base::NoDestructor<SurfaceComposerMethods> instance;
    return *instance;
  }

  SurfaceComposerMethods() {
    void* main_dl_handle = dlopen(nullptr, RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load android so";
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_Create);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_Delete);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_CreateSurface);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_DeleteSurface);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_CreateTransaction);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_DeleteTransaction);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionApply);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetVisibility);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetPosition);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetZOrder);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetBuffer);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetSize);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetCropRect);
    LOAD_FUNCTION(main_dl_handle, ASurfaceComposer_TransactionSetOpaque);
  }

  ~SurfaceComposerMethods() = default;

  bool supported = true;
  pASurfaceComposer_Create ASurfaceComposer_CreateFn;
  pASurfaceComposer_Delete ASurfaceComposer_DeleteFn;
  pASurfaceComposer_CreateSurface ASurfaceComposer_CreateSurfaceFn;
  pASurfaceComposer_DeleteSurface ASurfaceComposer_DeleteSurfaceFn;
  pASurfaceComposer_CreateTransaction ASurfaceComposer_CreateTransactionFn;
  pASurfaceComposer_DeleteTransaction ASurfaceComposer_DeleteTransactionFn;
  pASurfaceComposer_TransactionApply ASurfaceComposer_TransactionApplyFn;
  pASurfaceComposer_TransactionSetVisibility
      ASurfaceComposer_TransactionSetVisibilityFn;
  pASurfaceComposer_TransactionSetPosition
      ASurfaceComposer_TransactionSetPositionFn;
  pASurfaceComposer_TransactionSetZOrder
      ASurfaceComposer_TransactionSetZOrderFn;
  pASurfaceComposer_TransactionSetBuffer
      ASurfaceComposer_TransactionSetBufferFn;
  pASurfaceComposer_TransactionSetSize ASurfaceComposer_TransactionSetSizeFn;
  pASurfaceComposer_TransactionSetCropRect
      ASurfaceComposer_TransactionSetCropRectFn;
  pASurfaceComposer_TransactionSetOpaque
      ASurfaceComposer_TransactionSetOpaqueFn;
};
};

// static
bool SurfaceComposer::IsSupported() {
  const int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();
  if (sdk_int < 29) {
    LOG(ERROR) << "SurfaceControl not supported on sdk: " << sdk_int;
    return false;
  }
  return SurfaceComposerMethods::Get().supported;
}

// static
std::unique_ptr<SurfaceComposer> SurfaceComposer::Create(
    ANativeWindow* window) {
  DCHECK(SurfaceComposerMethods::Get().supported);
  auto* a_composer =
      SurfaceComposerMethods::Get().ASurfaceComposer_CreateFn(window);
  if (!a_composer)
    return nullptr;
  return base::WrapUnique(new SurfaceComposer(a_composer));
}

SurfaceComposer::SurfaceComposer(ASurfaceComposer* composer)
    : composer_(composer) {}

SurfaceComposer::~SurfaceComposer() {
  SurfaceComposerMethods::Get().ASurfaceComposer_DeleteFn(composer_);
}

SurfaceComposer::Surface::Surface() = default;

SurfaceComposer::Surface::Surface(SurfaceComposer* composer,
                                  SurfaceContentType content_type,
                                  const char* name,
                                  Surface* parent) {
  surface_ = SurfaceComposerMethods::Get().ASurfaceComposer_CreateSurfaceFn(
      composer->composer_, static_cast<int32_t>(content_type),
      parent ? parent->surface() : nullptr, name);
  DCHECK(surface_);
}

SurfaceComposer::Surface::~Surface() {
  if (surface_)
    SurfaceComposerMethods::Get().ASurfaceComposer_DeleteSurfaceFn(surface_);
}

SurfaceComposer::Surface::Surface(Surface&& other) {
  surface_ = other.surface_;
  other.surface_ = nullptr;
}

SurfaceComposer::Surface& SurfaceComposer::Surface::operator=(Surface&& other) {
  if (surface_)
    SurfaceComposerMethods::Get().ASurfaceComposer_DeleteSurfaceFn(surface_);

  surface_ = other.surface_;
  other.surface_ = nullptr;
  return *this;
}

SurfaceComposer::Transaction::Transaction() {
  transaction_ =
      SurfaceComposerMethods::Get().ASurfaceComposer_CreateTransactionFn();
  DCHECK(transaction_);
}

SurfaceComposer::Transaction::~Transaction() {
  SurfaceComposerMethods::Get().ASurfaceComposer_DeleteTransactionFn(
      transaction_);
}

void SurfaceComposer::Transaction::SetVisibility(const Surface& surface,
                                                 bool show) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetVisibilityFn(
      transaction_, surface.surface(), show);
}

void SurfaceComposer::Transaction::SetPosition(const Surface& surface,
                                               float x,
                                               float y) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetPositionFn(
      transaction_, surface.surface(), x, y);
}

void SurfaceComposer::Transaction::SetZOrder(const Surface& surface,
                                             int32_t z) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetZOrderFn(
      transaction_, surface.surface(), z);
}

void SurfaceComposer::Transaction::SetBuffer(const Surface& surface,
                                             AHardwareBuffer* buffer,
                                             base::ScopedFD fence_fd) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetBufferFn(
      transaction_, surface.surface(), buffer,
      fence_fd.is_valid() ? fence_fd.release() : -1);
}

void SurfaceComposer::Transaction::SetSize(const Surface& surface,
                                           uint32_t width,
                                           uint32_t height) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetSizeFn(
      transaction_, surface.surface(), width, height);
}

void SurfaceComposer::Transaction::SetCropRect(const Surface& surface,
                                               int32_t left,
                                               int32_t top,
                                               int32_t right,
                                               int32_t bottom) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetCropRectFn(
      transaction_, surface.surface(), left, top, right, bottom);
}

void SurfaceComposer::Transaction::SetOpaque(const Surface& surface,
                                             bool opaque) {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionSetOpaqueFn(
      transaction_, surface.surface(), opaque);
}

void SurfaceComposer::Transaction::Apply() {
  SurfaceComposerMethods::Get().ASurfaceComposer_TransactionApplyFn(
      transaction_);
}

}  // namespace gl
