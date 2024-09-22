// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_controller.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/shadow_controller_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

using std::make_pair;

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::Shadow*)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::Shadow, kShadowLayerKey, nullptr)

namespace wm {

namespace {

int GetShadowElevationForActiveState(aura::Window* window) {
  int elevation = window->GetProperty(kShadowElevationKey);
  if (elevation != kShadowElevationDefault)
    return elevation;

  if (IsActiveWindow(window))
    return kShadowElevationActiveWindow;

  return GetDefaultShadowElevationForWindow(window);
}

// Returns the shadow style to be applied to |losing_active| when it is losing
// active to |gaining_active|. |gaining_active| may be of a type that hides when
// inactive, and as such we do not want to render |losing_active| as inactive.
int GetShadowElevationForWindowLosingActive(aura::Window* losing_active,
                                            aura::Window* gaining_active) {
  if (gaining_active && GetHideOnDeactivate(gaining_active)) {
    if (base::Contains(GetTransientChildren(losing_active), gaining_active))
      return kShadowElevationActiveWindow;
  }
  return kShadowElevationInactiveWindow;
}

}  // namespace

// ShadowController::Impl ------------------------------------------------------

// Real implementation of the ShadowController. ShadowController observes
// ActivationChangeObserver, which are per ActivationClient, where as there is
// only a single Impl (as it observes all window creation by way of an
// EnvObserver).
class ShadowController::Impl :
      public aura::EnvObserver,
      public aura::WindowObserver,
      public base::RefCounted<Impl> {
 public:
  // Returns the singleton instance for the specified Env.
  static Impl* GetInstance(aura::Env* env);

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void set_delegate(std::unique_ptr<ShadowControllerDelegate> delegate) {
    delegate_ = std::move(delegate);
  }
  bool IsShadowVisibleForWindow(aura::Window* window);
  void UpdateShadowForWindow(aura::Window* window);

  // aura::EnvObserver override:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  friend class base::RefCounted<Impl>;
  friend class ShadowController;

  explicit Impl(aura::Env* env);
  ~Impl() override;

  static base::flat_set<Impl*>* GetInstances();

  // Forwarded from ShadowController.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active);

  // Checks if |window| is visible and contains a property requesting a shadow.
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  // Sets rounded corner on the shadow for the `window`.
  void MaybeSetShadowRadiusForWindow(aura::Window* window) const;

  // Updates the shadow for windows when activation changes.
  void HandleWindowActivationChange(aura::Window* gaining_active,
                                    aura::Window* losing_active);

  // Shows or hides |window|'s shadow as needed (creating the shadow if
  // necessary).
  void HandlePossibleShadowVisibilityChange(aura::Window* window);

  // Creates a new shadow for |window| and stores it with the |kShadowLayerKey|
  // key.
  // The shadow's bounds are initialized and it is added to the window's layer.
  void CreateShadowForWindow(aura::Window* window);

  const raw_ptr<aura::Env> env_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observation_manager_{this};

  std::unique_ptr<ShadowControllerDelegate> delegate_;
};

// static
ShadowController::Impl* ShadowController::Impl::GetInstance(aura::Env* env) {
  for (Impl* impl : *GetInstances()) {
    if (impl->env_ == env)
      return impl;
  }

  return new Impl(env);
}

bool ShadowController::Impl::IsShadowVisibleForWindow(aura::Window* window) {
  if (!observation_manager_.IsObservingSource(window))
    return false;
  ui::Shadow* shadow = GetShadowForWindow(window);
  return shadow && shadow->layer()->visible();
}

void ShadowController::Impl::UpdateShadowForWindow(aura::Window* window) {
  DCHECK(observation_manager_.IsObservingSource(window));
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::Impl::OnWindowInitialized(aura::Window* window) {
  // During initialization, the window can't reliably tell whether it will be a
  // root window. That must be checked in the first visibility change
  DCHECK(!window->parent());
  DCHECK(!window->TargetVisibility());
  observation_manager_.AddObservation(window);
}

void ShadowController::Impl::OnWindowParentChanged(aura::Window* window,
                                                   aura::Window* parent) {
  if (parent && window->IsVisible())
    HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::Impl::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  bool shadow_will_change = key == kShadowElevationKey &&
                            window->GetProperty(kShadowElevationKey) != old;

  if (key == aura::client::kShowStateKey) {
    shadow_will_change = window->GetProperty(aura::client::kShowStateKey) !=
                         static_cast<ui::mojom::WindowShowState>(old);
  }

  if (key == aura::client::kWindowCornerRadiusKey) {
    shadow_will_change =
        window->GetProperty(aura::client::kWindowCornerRadiusKey) !=
        static_cast<int>(old);
  }

  shadow_will_change |=
      delegate_ &&
      delegate_->ShouldUpdateShadowOnWindowPropertyChange(window, key, old);

  // Check the target visibility. IsVisible() may return false if a parent layer
  // is hidden, but |this| only observes calls to Show()/Hide() on |window|.
  if (shadow_will_change && window->TargetVisibility()) {
    HandlePossibleShadowVisibilityChange(window);
  }
}

void ShadowController::Impl::OnWindowVisibilityChanging(aura::Window* window,
                                                        bool visible) {
  // At the time of the first visibility change, |window| will give a correct
  // answer for whether or not it is a root window. If it is, don't bother
  // observing: a shadow should never be added. Root windows can only have
  // shadows in the WindowServer (where a corresponding aura::Window may no
  // longer be a root window). Without this check, a second shadow is added,
  // which clips to the root window bounds; filling any rounded corners the
  // window may have.
  if (window->IsRootWindow()) {
    observation_manager_.RemoveObservation(window);
    return;
  }

  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::Impl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  ui::Shadow* shadow = GetShadowForWindow(window);
  if (shadow && window->GetProperty(aura::client::kUseWindowBoundsForShadow))
    shadow->SetContentBounds(gfx::Rect(new_bounds.size()));
}

void ShadowController::Impl::OnWindowDestroyed(aura::Window* window) {
  window->ClearProperty(kShadowLayerKey);
  observation_manager_.RemoveObservation(window);
}

void ShadowController::Impl::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  if (gained_active) {
    ui::Shadow* shadow = GetShadowForWindow(gained_active);
    if (shadow)
      shadow->SetElevation(GetShadowElevationForActiveState(gained_active));
  }
  if (lost_active) {
    ui::Shadow* shadow = GetShadowForWindow(lost_active);
    if (shadow && GetShadowElevationConvertDefault(lost_active) ==
                      kShadowElevationInactiveWindow) {
      shadow->SetElevation(
          GetShadowElevationForWindowLosingActive(lost_active, gained_active));
    }
  }
}

bool ShadowController::Impl::ShouldShowShadowForWindow(
    aura::Window* window) const {
  if (delegate_) {
    const bool should_show = delegate_->ShouldShowShadowForWindow(window);
    if (should_show)
      DCHECK(GetShadowElevationConvertDefault(window) > 0);
    return should_show;
  }

  ui::mojom::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  if (show_state == ui::mojom::WindowShowState::kFullscreen ||
      show_state == ui::mojom::WindowShowState::kMaximized) {
    return false;
  }

  return GetShadowElevationConvertDefault(window) > 0;
}

void ShadowController::Impl::MaybeSetShadowRadiusForWindow(
    aura::Window* window) const {
  ui::Shadow* shadow = GetShadowForWindow(window);
  CHECK(shadow);

  const int corner_radius =
      window->GetProperty(aura::client::kWindowCornerRadiusKey);

  // `aura::client::kWindowCornerRadiusKey` default value is -1, meaning
  // unspecified radius. i.e window server may want to apply rounded corners
  // implicitly.
  if (corner_radius >= 0) {
    shadow->SetRoundedCornerRadius(corner_radius);
  }
}

void ShadowController::Impl::HandlePossibleShadowVisibilityChange(
    aura::Window* window) {
  const bool should_show = ShouldShowShadowForWindow(window);
  ui::Shadow* shadow = GetShadowForWindow(window);
  if (shadow) {
    shadow->SetElevation(GetShadowElevationForActiveState(window));
    MaybeSetShadowRadiusForWindow(window);
    shadow->layer()->SetVisible(should_show);
  } else if (should_show) {
    CreateShadowForWindow(window);
  }
}

void ShadowController::Impl::CreateShadowForWindow(aura::Window* window) {
  DCHECK(!window->IsRootWindow());
  ui::Shadow* shadow =
      window->SetProperty(kShadowLayerKey, std::make_unique<ui::Shadow>());

  MaybeSetShadowRadiusForWindow(window);
  shadow->Init(GetShadowElevationForActiveState(window));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  shadow->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
#endif
  shadow->SetContentBounds(gfx::Rect(window->bounds().size()));
  shadow->layer()->SetVisible(ShouldShowShadowForWindow(window));
  window->layer()->Add(shadow->layer());
  window->layer()->StackAtBottom(shadow->layer());

  if (delegate_) {
    delegate_->ApplyColorThemeToWindowShadow(window);
  }
}

ShadowController::Impl::Impl(aura::Env* env)
    : env_(env), observation_manager_(this) {
  GetInstances()->insert(this);
  env_->AddObserver(this);
}

ShadowController::Impl::~Impl() {
  env_->RemoveObserver(this);
  GetInstances()->erase(this);
}

// static
base::flat_set<ShadowController::Impl*>*
ShadowController::Impl::GetInstances() {
  static base::NoDestructor<base::flat_set<Impl*>> impls;
  return impls.get();
}

// ShadowController ------------------------------------------------------------

ui::Shadow* ShadowController::GetShadowForWindow(aura::Window* window) {
  return window->GetProperty(kShadowLayerKey);
}

ui::Shadow::ElevationToColorsMap ShadowController::GenerateShadowColorsMap(
    const ui::ColorProvider* color_provider) {
  ui::Shadow::ElevationToColorsMap color_map;
  color_map[kShadowElevationPopup] = std::make_pair(
      color_provider->GetColor(ui::kColorShadowValueKeyShadowElevationFour),
      color_provider->GetColor(
          ui::kColorShadowValueAmbientShadowElevationFour));
  color_map[kShadowElevationInactiveWindow] = std::make_pair(
      color_provider->GetColor(ui::kColorShadowValueKeyShadowElevationTwelve),
      color_provider->GetColor(
          ui::kColorShadowValueAmbientShadowElevationTwelve));
  color_map[kShadowElevationActiveWindow] = std::make_pair(
      color_provider->GetColor(
          ui::kColorShadowValueKeyShadowElevationTwentyFour),
      color_provider->GetColor(
          ui::kColorShadowValueAmbientShadowElevationTwentyFour));
  return color_map;
}

ShadowController::ShadowController(
    ActivationClient* activation_client,
    std::unique_ptr<ShadowControllerDelegate> delegate,
    aura::Env* env)
    : activation_client_(activation_client),
      impl_(Impl::GetInstance(env ? env : aura::Env::GetInstance())) {
  // Watch for window activation changes.
  activation_client_->AddObserver(this);
  if (delegate)
    impl_->set_delegate(std::move(delegate));
}

ShadowController::~ShadowController() {
  activation_client_->RemoveObserver(this);
}

bool ShadowController::IsShadowVisibleForWindow(aura::Window* window) {
  return impl_->IsShadowVisibleForWindow(window);
}

void ShadowController::UpdateShadowForWindow(aura::Window* window) {
  impl_->UpdateShadowForWindow(window);
}

void ShadowController::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  impl_->OnWindowActivated(reason, gained_active, lost_active);
}

}  // namespace wm
