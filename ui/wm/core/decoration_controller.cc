// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/decoration_controller.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
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
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow_with_highlight_border.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/wm/core/decoration_controller_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::Decoration*)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::Decoration, kDecorationKey)

namespace wm {

namespace {

constexpr SkColor kDefaultHighlightColor = SkColorSetA(SK_ColorWHITE, 0x28);
constexpr SkColor kDefaultBorderColor = SkColorSetA(SK_ColorBLACK, 0x14);

int GetShadowElevationForActiveState(aura::Window* window) {
  int elevation = window->GetProperty(kShadowElevationKey);
  if (elevation != kShadowElevationDefault) {
    return elevation;
  }

  if (IsActiveWindow(window)) {
    return kShadowElevationActiveWindow;
  }

  return GetDefaultShadowElevationForWindow(window);
}

// Returns the shadow style to be applied to |losing_active| when it is losing
// active to |gaining_active|. |gaining_active| may be of a type that hides when
// inactive, and as such we do not want to render |losing_active| as inactive.
int GetShadowElevationForWindowLosingActive(aura::Window* losing_active,
                                            aura::Window* gaining_active) {
  if (gaining_active && GetHideOnDeactivate(gaining_active)) {
    if (std::ranges::contains(GetTransientChildren(losing_active),
                              gaining_active)) {
      return kShadowElevationActiveWindow;
    }
  }
  return kShadowElevationInactiveWindow;
}

bool IsFullscreenOrMaximized(const aura::Window* window) {
  const ui::mojom::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  return show_state == ui::mojom::WindowShowState::kFullscreen ||
         show_state == ui::mojom::WindowShowState::kMaximized;
}

}  // namespace

// DecorationController::Impl --------------------------------------------------

// Real implementation of the DecorationController. DecorationController
// observes ActivationChangeObserver, which are per ActivationClient, whereas
// there is only a single Impl (as it observes all window creation by way of an
// EnvObserver).
class DecorationController::Impl : public aura::EnvObserver,
                                   public aura::WindowObserver,
                                   public base::RefCounted<Impl> {
 public:
  // Returns the singleton instance for the specified Env.
  static Impl* GetInstance(aura::Env* env);

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void set_delegate(std::unique_ptr<DecorationControllerDelegate> delegate) {
    delegate_ = std::move(delegate);
  }
  bool IsShadowVisibleForWindow(aura::Window* window);
  bool IsHighlightBorderVisibleForWindow(aura::Window* window);
  void UpdateDecorationForWindow(aura::Window* window);

  // aura::EnvObserver override:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowOcclusionChanged(aura::Window* window) override;

 private:
  friend class base::RefCounted<Impl>;
  friend class DecorationController;

  explicit Impl(aura::Env* env);
  ~Impl() override;

  static base::flat_set<Impl*>* GetInstances();

  // Forwarded from DecorationController.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active);

  // Checks if |window| is visible and contains a property requesting a shadow.
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  // Checks if `window` should display a highlight border.
  bool ShouldShowHighlightBorderForWindow(aura::Window* window) const;

  // Sets rounded corner on the decoration for the `window`.
  void MaybeSetDecorationRadiusForWindow(aura::Window* window) const;

  // Shows or hides |window|'s decoration as needed (creating the decoration if
  // necessary).
  void HandlePossibleDecorationVisibilityChange(aura::Window* window);

  // Creates a new decoration for |window| and stores it with the
  // |kDecorationKey| key. The decoration's bounds are initialized and it is
  // added to the window's layer.
  void CreateDecorationForWindow(aura::Window* window);

  bool IsObservingWindowForTest(aura::Window* window) const;  // IN-TEST

  const raw_ptr<aura::Env> env_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observation_manager_{this};

  std::unique_ptr<DecorationControllerDelegate> delegate_;
};

// static
DecorationController::Impl* DecorationController::Impl::GetInstance(
    aura::Env* env) {
  for (Impl* impl : *GetInstances()) {
    if (impl->env_ == env) {
      return impl;
    }
  }

  return new Impl(env);
}

bool DecorationController::Impl::IsShadowVisibleForWindow(
    aura::Window* window) {
  if (!observation_manager_.IsObservingSource(window)) {
    return false;
  }
  ui::Decoration* decoration = GetDecorationForWindow(window);
  return decoration && decoration->layer()->visible() &&
         GetShadowForWindow(window)->elevation() > 0;
}

bool DecorationController::Impl::IsHighlightBorderVisibleForWindow(
    aura::Window* window) {
  if (!observation_manager_.IsObservingSource(window)) {
    return false;
  }
  ui::Decoration* decoration = GetDecorationForWindow(window);
  return decoration && decoration->layer()->visible() &&
         GetHighlightBorderForWindow(window) != nullptr;
}

void DecorationController::Impl::UpdateDecorationForWindow(
    aura::Window* window) {
  CHECK(observation_manager_.IsObservingSource(window),
        base::NotFatalUntil::M163);
  HandlePossibleDecorationVisibilityChange(window);
}

void DecorationController::Impl::OnWindowInitialized(aura::Window* window) {
  if (delegate_ && !delegate_->ShouldObserveWindow(window)) {
    return;
  }
  CHECK(!window->parent(), base::NotFatalUntil::M163);
  CHECK(!window->TargetVisibility(), base::NotFatalUntil::M163);
  observation_manager_.AddObservation(window);
}

void DecorationController::Impl::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Skip if the parent is null there is no need to update it during
  // destruction.
  if (!params.new_parent) {
    return;
  }
  // If a child window was added to an observed window with a highlight border,
  // keep the decoration layer stacked on top of the child's layer.
  if (params.new_parent == params.receiver &&
      params.target != params.receiver) {
    if (GetHighlightBorderForWindow(params.receiver)) {
      ui::Decoration* decoration = GetDecorationForWindow(params.receiver);
      params.receiver->layer()->StackAtTop(decoration->layer());
    }
    return;
  }
  // Update the decoration if the observing window is visible and its parent has
  // changed.
  const bool parent_changed = params.target == params.receiver &&
                              params.new_parent != params.old_parent;
  if (parent_changed && params.target->IsVisible()) {
    HandlePossibleDecorationVisibilityChange(params.target);
  }
}

void DecorationController::Impl::OnWindowPropertyChanged(aura::Window* window,
                                                         const void* key,
                                                         intptr_t old) {
  bool decoration_will_change = false;
  if (key == kShadowElevationKey) {
    decoration_will_change = window->GetProperty(kShadowElevationKey) != old;
  } else if (key == kWindowHasHighlightBorderKey) {
    decoration_will_change =
        window->GetProperty(kWindowHasHighlightBorderKey) !=
        static_cast<bool>(old);
  } else if (key == aura::client::kShowStateKey) {
    decoration_will_change = window->GetProperty(aura::client::kShowStateKey) !=
                             static_cast<ui::mojom::WindowShowState>(old);
  } else if (key == aura::client::kWindowRoundedCornersKey) {
    decoration_will_change =
        *window->GetProperty(aura::client::kWindowRoundedCornersKey) !=
        static_cast<gfx::RoundedCornersF>(old);
  } else if (delegate_) {
    decoration_will_change =
        delegate_->ShouldUpdateDecorationOnWindowPropertyChange(window, key,
                                                                old);
  }

  // Check the target visibility. IsVisible() may return false if a parent layer
  // is hidden, but |this| only observes calls to Show()/Hide() on |window|.
  if (decoration_will_change && window->TargetVisibility()) {
    HandlePossibleDecorationVisibilityChange(window);
  }
}

void DecorationController::Impl::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  // At the time of the first visibility change, |window| will give a correct
  // answer for whether or not it is a root window. If it is, don't bother
  // observing: a decoration should never be added. Root windows can only have
  // shadows in the WindowServer (where a corresponding aura::Window may no
  // longer be a root window). Without this check, a second shadow is added,
  // which clips to the root window bounds; filling any rounded corners the
  // window may have.
  if (window->IsRootWindow()) {
    observation_manager_.RemoveObservation(window);
    return;
  }

  HandlePossibleDecorationVisibilityChange(window);
}

void DecorationController::Impl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  ui::Decoration* decoration = GetDecorationForWindow(window);
  if (decoration &&
      window->GetProperty(aura::client::kUseWindowBoundsForShadow)) {
    decoration->SetContentBounds(gfx::Rect(new_bounds.size()));
  }
}

void DecorationController::Impl::OnWindowDestroyed(aura::Window* window) {
  window->ClearProperty(kDecorationKey);
  observation_manager_.RemoveObservation(window);
}

void DecorationController::Impl::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  if (gained_active) {
    if (auto* shadow = GetShadowForWindow(gained_active);
        shadow && ShouldShowShadowForWindow(gained_active)) {
      shadow->SetElevation(GetShadowElevationForActiveState(gained_active));
    }
  }
  if (lost_active) {
    if (auto* shadow = GetShadowForWindow(lost_active);
        shadow && ShouldShowShadowForWindow(lost_active) &&
        GetShadowElevationConvertDefault(lost_active) ==
            kShadowElevationInactiveWindow) {
      shadow->SetElevation(
          GetShadowElevationForWindowLosingActive(lost_active, gained_active));
    }
  }
}

bool DecorationController::Impl::ShouldShowShadowForWindow(
    aura::Window* window) const {
  if (delegate_) {
    const bool should_show = delegate_->ShouldShowShadowForWindow(window);
    if (should_show) {
      CHECK(GetShadowElevationConvertDefault(window) > 0,
            base::NotFatalUntil::M163);
    }
    return should_show;
  }

  if (IsFullscreenOrMaximized(window)) {
    return false;
  }

  return GetShadowElevationConvertDefault(window) > 0;
}

bool DecorationController::Impl::ShouldShowHighlightBorderForWindow(
    aura::Window* window) const {
  if (delegate_) {
    return delegate_->ShouldShowHighlightBorderForWindow(window);
  }

  if (IsFullscreenOrMaximized(window)) {
    return false;
  }

  return window->GetProperty(kWindowHasHighlightBorderKey);
}

void DecorationController::Impl::OnWindowOcclusionChanged(
    aura::Window* window) {
  ui::Decoration* decoration = GetDecorationForWindow(window);
  if (!decoration) {
    return;
  }
  HandlePossibleDecorationVisibilityChange(window);
}

void DecorationController::Impl::MaybeSetDecorationRadiusForWindow(
    aura::Window* window) const {
  ui::Decoration* decoration = GetDecorationForWindow(window);
  CHECK(decoration);

  if (delegate_ && !delegate_->ShouldRoundDecorationForWindow(window)) {
    decoration->SetRoundedCorners(gfx::RoundedCornersF());
    return;
  }

  gfx::RoundedCornersF* rounded_corners =
      window->GetProperty(aura::client::kWindowRoundedCornersKey);

  // If `aura::client::kWindowRoundedCornersKey` is not set, it means
  // unspecified radius. i.e window server may want to apply rounded corners
  // implicitly.
  if (rounded_corners) {
    decoration->SetRoundedCorners(*rounded_corners);
  }
}

void DecorationController::Impl::HandlePossibleDecorationVisibilityChange(
    aura::Window* window) {
  const bool is_occluded =
      window->GetOcclusionState() == aura::Window::OcclusionState::OCCLUDED;
  const bool should_show_shadow = ShouldShowShadowForWindow(window);
  const bool should_show_highlight_border =
      ShouldShowHighlightBorderForWindow(window);
  const bool should_show_any =
      !is_occluded && (should_show_shadow || should_show_highlight_border);

  ui::Decoration* decoration = GetDecorationForWindow(window);
  if (!decoration) {
    if (!should_show_any) {
      return;
    }
    CreateDecorationForWindow(window);
    decoration = GetDecorationForWindow(window);
  }

  auto* source =
      decoration->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>();
  CHECK(source);

  source->shadow()->SetElevation(
      should_show_shadow ? GetShadowElevationForActiveState(window) : 0);

  if (should_show_highlight_border) {
    if (!source->highlight_border()) {
      source->SetHighlightBorder(
          std::make_unique<ui::decoration::HighlightBorder>(
              kDefaultHighlightColor, kDefaultBorderColor));
      if (delegate_) {
        delegate_->ApplyColorThemeToWindowDecoration(window);
      }
    }
  } else if (source->highlight_border()) {
    source->SetHighlightBorder(nullptr);
  }

  MaybeSetDecorationRadiusForWindow(window);

  // The highlight border draws an inner highlight stroke inside the window
  // bounds, so the decoration layer must be stacked at the top of the window's
  // layer to remain visible over the window's content layers. When only a drop
  // shadow is present, stacking at the bottom behind the window's content is
  // sufficient.
  if (source->highlight_border()) {
    window->layer()->StackAtTop(decoration->layer());
  } else {
    window->layer()->StackAtBottom(decoration->layer());
  }

  if (decoration->layer()->GetTargetVisibility() != should_show_any) {
    decoration->layer()->SetVisible(should_show_any);
  }
}

void DecorationController::Impl::CreateDecorationForWindow(
    aura::Window* window) {
  CHECK(!window->IsRootWindow(), base::NotFatalUntil::M163);
  ui::decoration::Shadow::Style style =
      ui::decoration::Shadow::Style::kMaterialDesign;
#if BUILDFLAG(IS_CHROMEOS)
  style = ui::decoration::Shadow::Style::kChromeOSSystemUI;
#endif
  ui::Decoration* decoration = window->SetProperty(
      kDecorationKey, ui::Decoration::CreateShadowWithHighlightBorder(
                          std::make_unique<ui::decoration::Shadow>(
                              ShouldShowShadowForWindow(window)
                                  ? GetShadowElevationForActiveState(window)
                                  : 0,
                              style)));
  // Historically, window shadows frame content with slightly rounded corners
  // unless `aura::client::kWindowRoundedCornersKey` specifies otherwise.
  decoration->SetRoundedCorners(gfx::RoundedCornersF(2.f));
  decoration->SetContentBounds(gfx::Rect(window->bounds().size()));
  window->layer()->Add(decoration->layer());
  window->TrackOcclusionState();

  if (delegate_) {
    delegate_->ApplyColorThemeToWindowDecoration(window);
  }
}

bool DecorationController::Impl::IsObservingWindowForTest(
    aura::Window* window) const {
  return observation_manager_.IsObservingSource(window);
}

DecorationController::Impl::Impl(aura::Env* env)
    : env_(env), observation_manager_(this) {
  GetInstances()->insert(this);
  env_->AddObserver(this);
}

DecorationController::Impl::~Impl() {
  env_->RemoveObserver(this);
  GetInstances()->erase(this);
}

// static
base::flat_set<DecorationController::Impl*>*
DecorationController::Impl::GetInstances() {
  static base::NoDestructor<base::flat_set<Impl*>> impls;
  return impls.get();
}

// DecorationController --------------------------------------------------------

ui::Decoration* DecorationController::GetDecorationForWindow(
    aura::Window* window) {
  return window->GetProperty(kDecorationKey);
}

ui::decoration::Shadow* DecorationController::GetShadowForWindow(
    aura::Window* window) {
  ui::Decoration* decoration = GetDecorationForWindow(window);
  if (!decoration) {
    return nullptr;
  }
  auto* source =
      decoration->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>();
  return source ? source->shadow() : nullptr;
}

ui::decoration::HighlightBorder*
DecorationController::GetHighlightBorderForWindow(aura::Window* window) {
  ui::Decoration* decoration = GetDecorationForWindow(window);
  if (!decoration) {
    return nullptr;
  }
  auto* source =
      decoration->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>();
  return source ? source->highlight_border() : nullptr;
}

ui::decoration::Shadow::ElevationToColorsMap
DecorationController::GenerateShadowColorsMap(
    const ui::ColorProvider* color_provider) {
  ui::decoration::Shadow::ElevationToColorsMap color_map;
  color_map[kShadowElevationPopup] = ui::decoration::Shadow::ElevationColors{
      .key_color =
          color_provider->GetColor(ui::kColorShadowValueKeyShadowElevationFour),
      .ambient_color = color_provider->GetColor(
          ui::kColorShadowValueAmbientShadowElevationFour)};
  color_map[kShadowElevationInactiveWindow] =
      ui::decoration::Shadow::ElevationColors{
          .key_color = color_provider->GetColor(
              ui::kColorShadowValueKeyShadowElevationTwelve),
          .ambient_color = color_provider->GetColor(
              ui::kColorShadowValueAmbientShadowElevationTwelve)};
  color_map[kShadowElevationActiveWindow] =
      ui::decoration::Shadow::ElevationColors{
          .key_color = color_provider->GetColor(
              ui::kColorShadowValueKeyShadowElevationTwentyFour),
          .ambient_color = color_provider->GetColor(
              ui::kColorShadowValueAmbientShadowElevationTwentyFour)};
  return color_map;
}

std::pair<SkColor, SkColor> DecorationController::GetHighlightBorderColors(
    const ui::ColorProvider* color_provider) {
#if BUILDFLAG(IS_CHROMEOS)
  return {color_provider->GetColor(ui::kColorCrosSystemHighlight),
          color_provider->GetColor(ui::kColorCrosSystemHighlightBorder1)};
#else
  return {kDefaultHighlightColor, kDefaultBorderColor};
#endif
}

DecorationController::DecorationController(
    ActivationClient* activation_client,
    std::unique_ptr<DecorationControllerDelegate> delegate,
    aura::Env* env)
    : activation_client_(activation_client),
      impl_(Impl::GetInstance(env ? env : aura::Env::GetInstance())) {
  // Watch for window activation changes.
  activation_client_->AddObserver(this);
  if (delegate) {
    impl_->set_delegate(std::move(delegate));
  }
}

DecorationController::~DecorationController() {
  activation_client_->RemoveObserver(this);
}

bool DecorationController::IsShadowVisibleForWindow(aura::Window* window) {
  return impl_->IsShadowVisibleForWindow(window);
}

bool DecorationController::IsHighlightBorderVisibleForWindow(
    aura::Window* window) {
  return impl_->IsHighlightBorderVisibleForWindow(window);
}

void DecorationController::UpdateDecorationForWindow(aura::Window* window) {
  impl_->UpdateDecorationForWindow(window);
}

void DecorationController::OnWindowActivated(ActivationReason reason,
                                             aura::Window* gained_active,
                                             aura::Window* lost_active) {
  impl_->OnWindowActivated(reason, gained_active, lost_active);
}

bool DecorationController::IsObservingWindowForTest(
    aura::Window* window) const {
  return impl_->IsObservingWindowForTest(window);  // IN-TEST
}

}  // namespace wm
