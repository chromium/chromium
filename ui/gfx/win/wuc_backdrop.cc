// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/wuc_backdrop.h"

#include <DispatcherQueue.h>
#include <windows.ui.composition.core.h>
#include <wrl/client.h>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"

namespace gfx {

namespace {
using Microsoft::WRL::ComPtr;
using CreateDispatcherQueueControllerProc =
    decltype(&::CreateDispatcherQueueController);
namespace WUC = ABI::Windows::UI::Composition;

CreateDispatcherQueueControllerProc GetOrCreateDispatcherQueueControllerProc() {
  static CreateDispatcherQueueControllerProc const function =
      reinterpret_cast<CreateDispatcherQueueControllerProc>(
          WUCBackdrop::LoadCoreMessagingFunction(
              "CreateDispatcherQueueController"));
  return function;
}

HRESULT GetDispatcherQueueController(
    DispatcherQueueOptions options,
    ABI::Windows::System::IDispatcherQueueController** controller) {
  CreateDispatcherQueueControllerProc create_dispatcher_queue_controller_func =
      GetOrCreateDispatcherQueueControllerProc();
  CHECK(create_dispatcher_queue_controller_func);
  return create_dispatcher_queue_controller_func(options, controller);
}

bool EnsurePerThreadDispatcherQueueController() {
  // Maintain one DispatcherQueueController per thread.
  // DispatcherQueueController and its associated DispatcherQueue will be kept
  // alive by the OS while the event loop is running.
  static base::SequenceLocalStorageSlot<
      Microsoft::WRL::ComPtr<ABI::Windows::System::IDispatcherQueueController>>
      queue_controller_slot;

  if (queue_controller_slot) {
    return true;
  }

  ComPtr<ABI::Windows::System::IDispatcherQueueController> queue_controller;
  HRESULT hr = GetDispatcherQueueController(
      {sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE},
      &queue_controller);
  CHECK_EQ(hr, S_OK);
  queue_controller_slot.emplace(std::move(queue_controller));
  return true;
}

ComPtr<WUC::ICompositor> GetOrCreateCompositor() {
  EnsurePerThreadDispatcherQueueController();
  // Maintain one Compositor per thread. SequenceLocalStorageSlot is
  // needed because ICompositor has a non-trivial destructor.
  static base::SequenceLocalStorageSlot<ComPtr<WUC::ICompositor>>
      compositor_slot;

  if (compositor_slot) {
    return *compositor_slot;
  }

  ComPtr<WUC::ICompositor> compositor;
  HRESULT hr = base::win::RoActivateInstance(
      base::win::HStringReference(
          RuntimeClass_Windows_UI_Composition_Compositor)
          .Get(),
      &compositor);
  CHECK_EQ(hr, S_OK);
  compositor_slot.emplace(compositor);
  return compositor;
}
}  // namespace

WUCBackdrop::WUCBackdrop(HWND hwnd) {
  ComPtr<WUC::ICompositor> compositor = GetOrCreateCompositor();

  ComPtr<WUC::Desktop::ICompositorDesktopInterop> interop;
  HRESULT hr = compositor.As(&interop);
  CHECK_EQ(hr, S_OK);

  hr = interop->CreateDesktopWindowTarget(hwnd, /*isTopmost=*/false,
                                          &desktop_window_target_);
  CHECK_EQ(hr, S_OK);

  ComPtr<WUC::ICompositionTarget> wuc_composition_target;
  hr = desktop_window_target_.As(&wuc_composition_target);
  CHECK_EQ(hr, S_OK);

  hr = compositor->CreateSpriteVisual(&backdrop_sprite_visual_);
  CHECK_EQ(hr, S_OK);

  ComPtr<WUC::IVisual> sprite_visual_as_visual;
  hr = backdrop_sprite_visual_.As(&sprite_visual_as_visual);
  CHECK_EQ(hr, S_OK);
  hr = wuc_composition_target->put_Root(sprite_visual_as_visual.Get());
  CHECK_EQ(hr, S_OK);

  ComPtr<WUC::IVisual2> sprite_visual_as_visual2;
  hr = sprite_visual_as_visual.As(&sprite_visual_as_visual2);
  CHECK_EQ(hr, S_OK);
  hr = sprite_visual_as_visual2->put_RelativeSizeAdjustment({1.0f, 1.0f});
  CHECK_EQ(hr, S_OK);
}

void WUCBackdrop::UpdateBackdropColor(SkColor color) {
  if (!solid_color_brush_) {
    ComPtr<WUC::ICompositor> compositor = GetOrCreateCompositor();
    if (!compositor) {
      return;
    }

    HRESULT hr = compositor->CreateColorBrush(&solid_color_brush_);
    CHECK_EQ(hr, S_OK);

    ComPtr<WUC::ICompositionBrush> brush;
    hr = solid_color_brush_.As(&brush);
    CHECK_EQ(hr, S_OK);
    hr = backdrop_sprite_visual_->put_Brush(brush.Get());
    CHECK_EQ(hr, S_OK);
  }

  HRESULT hr =
      solid_color_brush_->put_Color({static_cast<BYTE>(SkColorGetA(color)),
                                     static_cast<BYTE>(SkColorGetR(color)),
                                     static_cast<BYTE>(SkColorGetG(color)),
                                     static_cast<BYTE>(SkColorGetB(color))});
  CHECK_EQ(hr, S_OK);
}

FARPROC WUCBackdrop::LoadCoreMessagingFunction(const char* function_name) {
  static HMODULE handle = [] {
    base::ScopedAllowBlocking scoped_allow_blocking;
    return base::LoadSystemLibrary(L"CoreMessaging.dll");
  }();
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

WUCBackdrop::~WUCBackdrop() = default;

}  // namespace gfx
