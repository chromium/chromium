// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/focus_controller.h"

#include <map>

#include "base/macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

// EXPECT_DCHECK executes statement and expects a DCHECK death when DCHECK is
// enabled.
#if DCHECK_IS_ON()
#define EXPECT_DCHECK(statement, regex) \
  EXPECT_DEATH_IF_SUPPORTED(statement, regex)
#else
#define EXPECT_DCHECK(statement, regex) \
  { statement; }
#endif

namespace wm {

class FocusNotificationObserver : public ActivationChangeObserver,
                                  public aura::client::FocusChangeObserver {
 public:
  FocusNotificationObserver()
      : last_activation_reason_(ActivationReason::ACTIVATION_CLIENT),
        activation_changed_count_(0),
        focus_changed_count_(0),
        reactivation_count_(0),
        reactivation_requested_window_(NULL),
        reactivation_actual_window_(NULL) {}
  ~FocusNotificationObserver() override {}

  void ExpectCounts(int activation_changed_count, int focus_changed_count) {
    EXPECT_EQ(activation_changed_count, activation_changed_count_);
    EXPECT_EQ(focus_changed_count, focus_changed_count_);
  }
  ActivationReason last_activation_reason() const {
    return last_activation_reason_;
  }
  int reactivation_count() const {
    return reactivation_count_;
  }
  aura::Window* reactivation_requested_window() const {
    return reactivation_requested_window_;
  }
  aura::Window* reactivation_actual_window() const {
    return reactivation_actual_window_;
  }

 private:
  // Overridden from ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    last_activation_reason_ = reason;
    ++activation_changed_count_;
  }
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override {
    ++reactivation_count_;
    reactivation_requested_window_ = request_active;
    reactivation_actual_window_ = actual_active;
  }

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    ++focus_changed_count_;
  }

  ActivationReason last_activation_reason_;
  int activation_changed_count_;
  int focus_changed_count_;
  int reactivation_count_;
  aura::Window* reactivation_requested_window_;
  aura::Window* reactivation_actual_window_;

  DISALLOW_COPY_AND_ASSIGN(FocusNotificationObserver);
};

class WindowDeleter {
 public:
  virtual aura::Window* GetDeletedWindow() = 0;

 protected:
  virtual ~WindowDeleter() {}
};

// ActivationChangeObserver and FocusChangeObserver that keeps track of whether
// it was notified about activation changes or focus changes with a deleted
// window.
class RecordingActivationAndFocusChangeObserver
    : public ActivationChangeObserver,
      public aura::client::FocusChangeObserver {
 public:
  RecordingActivationAndFocusChangeObserver(aura::Window* root,
                                            WindowDeleter* deleter)
      : root_(root),
        deleter_(deleter),
        was_notified_with_deleted_window_(false) {
    GetActivationClient(root_)->AddObserver(this);
    aura::client::GetFocusClient(root_)->AddObserver(this);
  }
  ~RecordingActivationAndFocusChangeObserver() override {
    GetActivationClient(root_)->RemoveObserver(this);
    aura::client::GetFocusClient(root_)->RemoveObserver(this);
  }

  bool was_notified_with_deleted_window() const {
    return was_notified_with_deleted_window_;
  }

  // Overridden from ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (lost_active && lost_active == deleter_->GetDeletedWindow())
      was_notified_with_deleted_window_ = true;
  }

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    if (lost_focus && lost_focus == deleter_->GetDeletedWindow())
      was_notified_with_deleted_window_ = true;
  }

 private:
  aura::Window* root_;

  // Not owned.
  WindowDeleter* deleter_;

  // Whether the observer was notified about the loss of activation or the
  // loss of focus with a window already deleted by |deleter_| as the
  // |lost_active| or |lost_focus| parameter.
  bool was_notified_with_deleted_window_;

  DISALLOW_COPY_AND_ASSIGN(RecordingActivationAndFocusChangeObserver);
};

// Hides a window when activation changes.
class HideOnLoseActivationChangeObserver : public ActivationChangeObserver {
 public:
  explicit HideOnLoseActivationChangeObserver(aura::Window* window_to_hide)
      : root_(window_to_hide->GetRootWindow()),
        window_to_hide_(window_to_hide) {
    GetActivationClient(root_)->AddObserver(this);
  }

  ~HideOnLoseActivationChangeObserver() override {
    GetActivationClient(root_)->RemoveObserver(this);
  }

  aura::Window* window_to_hide() { return window_to_hide_; }

 private:
  // Overridden from ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (window_to_hide_) {
      aura::Window* window_to_hide = window_to_hide_;
      window_to_hide_ = nullptr;
      window_to_hide->Hide();
    }
  }

  aura::Window* root_;
  aura::Window* window_to_hide_;

  DISALLOW_COPY_AND_ASSIGN(HideOnLoseActivationChangeObserver);
};

// ActivationChangeObserver that deletes the window losing activation.
class DeleteOnLoseActivationChangeObserver : public ActivationChangeObserver,
                                             public WindowDeleter {
 public:
  explicit DeleteOnLoseActivationChangeObserver(aura::Window* window)
      : root_(window->GetRootWindow()),
        window_(window),
        did_delete_(false) {
    GetActivationClient(root_)->AddObserver(this);
  }
  ~DeleteOnLoseActivationChangeObserver() override {
    GetActivationClient(root_)->RemoveObserver(this);
  }

  // Overridden from ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (window_ && lost_active == window_) {
      delete lost_active;
      did_delete_ = true;
    }
  }

  // Overridden from WindowDeleter:
  aura::Window* GetDeletedWindow() override {
    return did_delete_ ? window_ : NULL;
  }

 private:
  aura::Window* root_;
  aura::Window* window_;
  bool did_delete_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnLoseActivationChangeObserver);
};

// FocusChangeObserver that deletes the window losing focus.
class DeleteOnLoseFocusChangeObserver
    : public aura::client::FocusChangeObserver,
      public WindowDeleter {
 public:
  explicit DeleteOnLoseFocusChangeObserver(aura::Window* window)
      : root_(window->GetRootWindow()),
        window_(window),
        did_delete_(false) {
    aura::client::GetFocusClient(root_)->AddObserver(this);
  }
  ~DeleteOnLoseFocusChangeObserver() override {
    aura::client::GetFocusClient(root_)->RemoveObserver(this);
  }

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    if (window_ && lost_focus == window_) {
      delete lost_focus;
      did_delete_ = true;
    }
  }

  // Overridden from WindowDeleter:
  aura::Window* GetDeletedWindow() override {
    return did_delete_ ? window_ : NULL;
  }

 private:
  aura::Window* root_;
  aura::Window* window_;
  bool did_delete_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnLoseFocusChangeObserver);
};

class ScopedFocusNotificationObserver : public FocusNotificationObserver {
 public:
  ScopedFocusNotificationObserver(aura::Window* root_window)
      : root_window_(root_window) {
    GetActivationClient(root_window_)->AddObserver(this);
    aura::client::GetFocusClient(root_window_)->AddObserver(this);
  }
  ~ScopedFocusNotificationObserver() override {
    GetActivationClient(root_window_)->RemoveObserver(this);
    aura::client::GetFocusClient(root_window_)->RemoveObserver(this);
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFocusNotificationObserver);
};

class ScopedTargetFocusNotificationObserver : public FocusNotificationObserver {
 public:
  ScopedTargetFocusNotificationObserver(aura::Window* root_window, int id)
      : target_(root_window->GetChildById(id)) {
    SetActivationChangeObserver(target_, this);
    aura::client::SetFocusChangeObserver(target_, this);
    tracker_.Add(target_);
  }
  ~ScopedTargetFocusNotificationObserver() override {
    if (tracker_.Contains(target_)) {
      SetActivationChangeObserver(target_, NULL);
      aura::client::SetFocusChangeObserver(target_, NULL);
    }
  }

 private:
  aura::Window* target_;
  aura::WindowTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTargetFocusNotificationObserver);
};

// Used to fake the handling of events in the pre-target phase.
class SimpleEventHandler : public ui::EventHandler {
 public:
  SimpleEventHandler() {}
  ~SimpleEventHandler() override {}

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override { event->SetHandled(); }
  void OnGestureEvent(ui::GestureEvent* event) override { event->SetHandled(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleEventHandler);
};

class FocusShiftingActivationObserver : public ActivationChangeObserver {
 public:
  explicit FocusShiftingActivationObserver(aura::Window* activated_window)
      : activated_window_(activated_window),
        shift_focus_to_(NULL) {}
  ~FocusShiftingActivationObserver() override {}

  void set_shift_focus_to(aura::Window* shift_focus_to) {
    shift_focus_to_ = shift_focus_to;
  }

 private:
  // Overridden from ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    // Shift focus to a child. This should prevent the default focusing from
    // occurring in FocusController::FocusWindow().
    if (gained_active == activated_window_) {
      aura::client::FocusClient* client =
          aura::client::GetFocusClient(gained_active);
      client->FocusWindow(shift_focus_to_);
    }
  }

  aura::Window* activated_window_;
  aura::Window* shift_focus_to_;

  DISALLOW_COPY_AND_ASSIGN(FocusShiftingActivationObserver);
};

class ActivateWhileActivatingObserver : public ActivationChangeObserver {
 public:
  ActivateWhileActivatingObserver(aura::Window* to_observe,
                                  aura::Window* to_activate,
                                  aura::Window* to_focus)
      : to_observe_(to_observe),
        to_activate_(to_activate),
        to_focus_(to_focus) {
    GetActivationClient(to_observe_->GetRootWindow())->AddObserver(this);
  }
  ~ActivateWhileActivatingObserver() override {
    GetActivationClient(to_observe_->GetRootWindow())->RemoveObserver(this);
  }

 private:
  // Overridden from ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override {
    if (gaining_active != to_observe_)
      return;

    if (to_activate_)
      ActivateWindow(to_activate_);
    if (to_focus_)
      FocusWindow(to_focus_);
  }
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}

  void ActivateWindow(aura::Window* window) {
    GetActivationClient(to_observe_->GetRootWindow())->ActivateWindow(window);
  }

  void FocusWindow(aura::Window* window) {
    aura::client::GetFocusClient(to_observe_->GetRootWindow())
        ->FocusWindow(window);
  }

  aura::Window* to_observe_;
  aura::Window* to_activate_;
  aura::Window* to_focus_;

  DISALLOW_COPY_AND_ASSIGN(ActivateWhileActivatingObserver);
};

// BaseFocusRules subclass that allows basic overrides of focus/activation to
// be tested. This is intended more as a test that the override system works at
// all, rather than as an exhaustive set of use cases, those should be covered
// in tests for those FocusRules implementations.
class TestFocusRules : public BaseFocusRules {
 public:
  TestFocusRules() : focus_restriction_(NULL) {}

  // Restricts focus and activation to this window and its child hierarchy.
  void set_focus_restriction(aura::Window* focus_restriction) {
    focus_restriction_ = focus_restriction;
  }

  // Overridden from BaseFocusRules:
  bool SupportsChildActivation(const aura::Window* window) const override {
    // In FocusControllerTests, only the RootWindow has activatable children.
    return window->GetRootWindow() == window;
  }
  bool CanActivateWindow(const aura::Window* window) const override {
    // Restricting focus to a non-activatable child window means the activatable
    // parent outside the focus restriction is activatable.
    bool can_activate =
        CanFocusOrActivate(window) || window->Contains(focus_restriction_);
    return can_activate ? BaseFocusRules::CanActivateWindow(window) : false;
  }
  bool CanFocusWindow(const aura::Window* window,
                      const ui::Event* event) const override {
    return CanFocusOrActivate(window)
               ? BaseFocusRules::CanFocusWindow(window, event)
               : false;
  }
  aura::Window* GetActivatableWindow(aura::Window* window) const override {
    return BaseFocusRules::GetActivatableWindow(
        CanFocusOrActivate(window) ? window : focus_restriction_);
  }
  aura::Window* GetFocusableWindow(aura::Window* window) const override {
    return BaseFocusRules::GetFocusableWindow(
        CanFocusOrActivate(window) ? window : focus_restriction_);
  }
  aura::Window* GetNextActivatableWindow(aura::Window* ignore) const override {
    aura::Window* next_activatable =
        BaseFocusRules::GetNextActivatableWindow(ignore);
    return CanFocusOrActivate(next_activatable) ?
        next_activatable : GetActivatableWindow(focus_restriction_);
  }

 private:
  bool CanFocusOrActivate(const aura::Window* window) const {
    return !focus_restriction_ || focus_restriction_->Contains(window);
  }

  aura::Window* focus_restriction_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

// Common infrastructure shared by all FocusController test types.
class FocusControllerTestBase : public aura::test::AuraTestBase {
 protected:
  FocusControllerTestBase() {}

  // Overridden from aura::test::AuraTestBase:
  void SetUp() override {
    // FocusController registers itself as an Env observer so it can catch all
    // window initializations, including the root_window()'s, so we create it
    // before allowing the base setup.
    test_focus_rules_ = new TestFocusRules;
    focus_controller_ = std::make_unique<FocusController>(test_focus_rules_);
    aura::test::AuraTestBase::SetUp();
    root_window()->AddPreTargetHandler(focus_controller_.get());
    aura::client::SetFocusClient(root_window(), focus_controller_.get());
    SetActivationClient(root_window(), focus_controller_.get());

    // Hierarchy used by all tests:
    // root_window
    //       +-- w1
    //       |    +-- w11
    //       |    +-- w12
    //       +-- w2
    //       |    +-- w21
    //       |         +-- w211
    //       +-- w3
    aura::Window* w1 = aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 1,
        gfx::Rect(0, 0, 50, 50), root_window());
    aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 11,
        gfx::Rect(5, 5, 10, 10), w1);
    aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 12,
        gfx::Rect(15, 15, 10, 10), w1);
    aura::Window* w2 = aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 2,
        gfx::Rect(75, 75, 50, 50), root_window());
    aura::Window* w21 = aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 21,
        gfx::Rect(5, 5, 10, 10), w2);
    aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 211,
        gfx::Rect(1, 1, 5, 5), w21);
    aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 3,
        gfx::Rect(125, 125, 50, 50), root_window());
  }
  void TearDown() override {
    root_window()->RemovePreTargetHandler(focus_controller_.get());
    aura::test::AuraTestBase::TearDown();
    test_focus_rules_ = NULL;  // Owned by FocusController.
    focus_controller_.reset();
  }

  void FocusWindow(aura::Window* window) {
    aura::client::GetFocusClient(root_window())->FocusWindow(window);
  }
  aura::Window* GetFocusedWindow() {
    return aura::client::GetFocusClient(root_window())->GetFocusedWindow();
  }
  int GetFocusedWindowId() {
    aura::Window* focused_window = GetFocusedWindow();
    return focused_window ? focused_window->id() : -1;
  }
  void ActivateWindow(aura::Window* window) {
    GetActivationClient(root_window())->ActivateWindow(window);
  }
  void DeactivateWindow(aura::Window* window) {
    GetActivationClient(root_window())->DeactivateWindow(window);
  }
  aura::Window* GetActiveWindow() {
    return GetActivationClient(root_window())->GetActiveWindow();
  }
  int GetActiveWindowId() {
    aura::Window* active_window = GetActiveWindow();
    return active_window ? active_window->id() : -1;
  }

  TestFocusRules* test_focus_rules() { return test_focus_rules_; }

  // Test functions.
  virtual void BasicFocus() = 0;
  virtual void BasicActivation() = 0;
  virtual void FocusEvents() = 0;
  virtual void DuplicateFocusEvents() {}
  virtual void ActivationEvents() = 0;
  virtual void ReactivationEvents() {}
  virtual void DuplicateActivationEvents() {}
  virtual void ShiftFocusWithinActiveWindow() {}
  virtual void ShiftFocusToChildOfInactiveWindow() {}
  virtual void ShiftFocusToParentOfFocusedWindow() {}
  virtual void FocusRulesOverride() = 0;
  virtual void ActivationRulesOverride() = 0;
  virtual void ShiftFocusOnActivation() {}
  virtual void ShiftFocusOnActivationDueToHide() {}
  virtual void NoShiftActiveOnActivation() {}
  virtual void FocusChangeDuringDrag() {}
  virtual void ChangeFocusWhenNothingFocusedAndCaptured() {}
  virtual void DontPassDeletedWindow() {}
  virtual void StackWindowAtTopOnActivation() {}
  virtual void HideFocusedWindowDuringActivationLoss() {}
  virtual void ActivateWhileActivating() {}

 private:
  std::unique_ptr<FocusController> focus_controller_;
  TestFocusRules* test_focus_rules_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerTestBase);
};

// Test base for tests where focus is directly set to a target window.
class FocusControllerDirectTestBase : public FocusControllerTestBase {
 protected:
  FocusControllerDirectTestBase() {}

  // Different test types shift focus in different ways.
  virtual void FocusWindowDirect(aura::Window* window) = 0;
  virtual void ActivateWindowDirect(aura::Window* window) = 0;
  virtual void DeactivateWindowDirect(aura::Window* window) = 0;

  // Input events do not change focus if the window can not be focused.
  virtual bool IsInputEvent() = 0;

  // Returns the expected ActivationReason caused by calling the
  // ActivatedWindowDirect(...) or DeactivateWindowDirect(...) methods.
  virtual ActivationChangeObserver::ActivationReason
  GetExpectedActivationReason() const = 0;

  void FocusWindowById(int id) {
    aura::Window* window = root_window()->GetChildById(id);
    DCHECK(window);
    FocusWindowDirect(window);
  }
  void ActivateWindowById(int id) {
    aura::Window* window = root_window()->GetChildById(id);
    DCHECK(window);
    ActivateWindowDirect(window);
  }

  // Overridden from FocusControllerTestBase:
  void BasicFocus() override {
    EXPECT_EQ(NULL, GetFocusedWindow());
    FocusWindowById(1);
    EXPECT_EQ(1, GetFocusedWindowId());
    FocusWindowById(2);
    EXPECT_EQ(2, GetFocusedWindowId());
  }
  void BasicActivation() override {
    EXPECT_EQ(NULL, GetActiveWindow());
    ActivateWindowById(1);
    EXPECT_EQ(1, GetActiveWindowId());
    ActivateWindowById(2);
    EXPECT_EQ(2, GetActiveWindowId());
    // Verify that attempting to deactivate NULL does not crash and does not
    // change activation.
    DeactivateWindow(NULL);
    EXPECT_EQ(2, GetActiveWindowId());
    DeactivateWindow(GetActiveWindow());
    EXPECT_EQ(1, GetActiveWindowId());
  }
  void FocusEvents() override {
    ScopedFocusNotificationObserver root_observer(root_window());
    ScopedTargetFocusNotificationObserver observer1(root_window(), 1);
    ScopedTargetFocusNotificationObserver observer2(root_window(), 2);

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);

    FocusWindowById(1);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(0, 0);

    FocusWindowById(2);
    root_observer.ExpectCounts(2, 2);
    observer1.ExpectCounts(2, 2);
    observer2.ExpectCounts(1, 1);
  }
  void DuplicateFocusEvents() override {
    // Focusing an existing focused window should not resend focus events.
    ScopedFocusNotificationObserver root_observer(root_window());
    ScopedTargetFocusNotificationObserver observer1(root_window(), 1);

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);

    FocusWindowById(1);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);

    FocusWindowById(1);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
  }
  void ActivationEvents() override {
    ActivateWindowById(1);

    ScopedFocusNotificationObserver root_observer(root_window());
    ScopedTargetFocusNotificationObserver observer1(root_window(), 1);
    ScopedTargetFocusNotificationObserver observer2(root_window(), 2);

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);

    ActivateWindowById(2);
    root_observer.ExpectCounts(1, 1);
    EXPECT_EQ(GetExpectedActivationReason(),
              root_observer.last_activation_reason());
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);
  }
  void ReactivationEvents() override {
    ActivateWindowById(1);
    ScopedFocusNotificationObserver root_observer(root_window());
    EXPECT_EQ(0, root_observer.reactivation_count());
    root_window()->GetChildById(2)->Hide();
    // When we attempt to activate "2", which cannot be activated because it
    // is not visible, "1" will be reactivated.
    ActivateWindowById(2);
    EXPECT_EQ(1, root_observer.reactivation_count());
    EXPECT_EQ(root_window()->GetChildById(2),
              root_observer.reactivation_requested_window());
    EXPECT_EQ(root_window()->GetChildById(1),
              root_observer.reactivation_actual_window());
  }
  void DuplicateActivationEvents() override {
    // Activating an existing active window should not resend activation events.
    ActivateWindowById(1);

    ScopedFocusNotificationObserver root_observer(root_window());
    ScopedTargetFocusNotificationObserver observer1(root_window(), 1);
    ScopedTargetFocusNotificationObserver observer2(root_window(), 2);

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);

    ActivateWindowById(2);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);

    ActivateWindowById(2);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);
  }
  void ShiftFocusWithinActiveWindow() override {
    ActivateWindowById(1);
    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());
    FocusWindowById(11);
    EXPECT_EQ(11, GetFocusedWindowId());
    FocusWindowById(12);
    EXPECT_EQ(12, GetFocusedWindowId());
  }
  void ShiftFocusToChildOfInactiveWindow() override {
    ActivateWindowById(2);
    EXPECT_EQ(2, GetActiveWindowId());
    EXPECT_EQ(2, GetFocusedWindowId());
    FocusWindowById(11);
    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(11, GetFocusedWindowId());
  }
  void ShiftFocusToParentOfFocusedWindow() override {
    ActivateWindowById(1);
    EXPECT_EQ(1, GetFocusedWindowId());
    FocusWindowById(11);
    EXPECT_EQ(11, GetFocusedWindowId());
    FocusWindowById(1);
    // Focus should _not_ shift to the parent of the already-focused window.
    EXPECT_EQ(11, GetFocusedWindowId());
  }
  void FocusRulesOverride() override {
    EXPECT_EQ(NULL, GetFocusedWindow());
    FocusWindowById(11);
    EXPECT_EQ(11, GetFocusedWindowId());

    test_focus_rules()->set_focus_restriction(root_window()->GetChildById(211));
    FocusWindowById(12);
    // Input events leave focus unchanged; direct API calls will change focus
    // to the restricted window.
    int focused_window = IsInputEvent() ? 11 : 211;
    EXPECT_EQ(focused_window, GetFocusedWindowId());

    test_focus_rules()->set_focus_restriction(NULL);
    FocusWindowById(12);
    EXPECT_EQ(12, GetFocusedWindowId());
  }
  void ActivationRulesOverride() override {
    ActivateWindowById(1);
    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());

    aura::Window* w3 = root_window()->GetChildById(3);
    test_focus_rules()->set_focus_restriction(w3);

    ActivateWindowById(2);
    // Input events leave activation unchanged; direct API calls will activate
    // the restricted window.
    int active_window = IsInputEvent() ? 1 : 3;
    EXPECT_EQ(active_window, GetActiveWindowId());
    EXPECT_EQ(active_window, GetFocusedWindowId());

    test_focus_rules()->set_focus_restriction(NULL);
    ActivateWindowById(2);
    EXPECT_EQ(2, GetActiveWindowId());
    EXPECT_EQ(2, GetFocusedWindowId());
  }
  void ShiftFocusOnActivation() override {
    // When a window is activated, by default that window is also focused.
    // An ActivationChangeObserver may shift focus to another window within the
    // same activatable window.
    ActivateWindowById(2);
    EXPECT_EQ(2, GetFocusedWindowId());
    ActivateWindowById(1);
    EXPECT_EQ(1, GetFocusedWindowId());

    ActivateWindowById(2);

    aura::Window* target = root_window()->GetChildById(1);
    ActivationClient* client = GetActivationClient(root_window());

    std::unique_ptr<FocusShiftingActivationObserver> observer(
        new FocusShiftingActivationObserver(target));
    observer->set_shift_focus_to(target->GetChildById(11));
    client->AddObserver(observer.get());

    ActivateWindowById(1);

    // w1's ActivationChangeObserver shifted focus to this child, pre-empting
    // FocusController's default setting.
    EXPECT_EQ(11, GetFocusedWindowId());

    ActivateWindowById(2);
    EXPECT_EQ(2, GetFocusedWindowId());

    // Simulate a focus reset by the ActivationChangeObserver. This should
    // trigger the default setting in FocusController.
    observer->set_shift_focus_to(NULL);
    ActivateWindowById(1);
    EXPECT_EQ(1, GetFocusedWindowId());

    client->RemoveObserver(observer.get());

    ActivateWindowById(2);
    EXPECT_EQ(2, GetFocusedWindowId());
    ActivateWindowById(1);
    EXPECT_EQ(1, GetFocusedWindowId());
  }
  void ShiftFocusOnActivationDueToHide() override {
    // Similar to ShiftFocusOnActivation except the activation change is
    // triggered by hiding the active window.
    ActivateWindowById(1);
    EXPECT_EQ(1, GetFocusedWindowId());

    // Removes window 3 as candidate for next activatable window.
    root_window()->GetChildById(3)->Hide();
    EXPECT_EQ(1, GetFocusedWindowId());

    aura::Window* target = root_window()->GetChildById(2);
    ActivationClient* client = GetActivationClient(root_window());

    std::unique_ptr<FocusShiftingActivationObserver> observer(
        new FocusShiftingActivationObserver(target));
    observer->set_shift_focus_to(target->GetChildById(21));
    client->AddObserver(observer.get());

    // Hide the active window.
    root_window()->GetChildById(1)->Hide();

    EXPECT_EQ(21, GetFocusedWindowId());

    client->RemoveObserver(observer.get());
  }
  void NoShiftActiveOnActivation() override {
    // When a window is activated, we need to prevent any change to activation
    // from being made in response to an activation change notification.
  }

  void FocusChangeDuringDrag() override {
    std::unique_ptr<aura::client::DefaultCaptureClient> capture_client(
        new aura::client::DefaultCaptureClient(root_window()));
    // Activating an inactive window during drag should activate the window.
    // This emulates the behavior of tab dragging which is merged into the
    // window below.
    ActivateWindowById(1);

    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());

    aura::Window* w2 = root_window()->GetChildById(2);
    ui::test::EventGenerator generator(root_window(), w2);
    generator.PressLeftButton();
    aura::client::GetCaptureClient(root_window())->SetCapture(w2);
    EXPECT_EQ(2, GetActiveWindowId());
    EXPECT_EQ(2, GetFocusedWindowId());
    generator.MoveMouseTo(gfx::Point(0, 0));

    // Emulate the behavior of merging a tab into an inactive window:
    // transferring the mouse capture and activate the window.
    aura::Window* w1 = root_window()->GetChildById(1);
    aura::client::GetCaptureClient(root_window())->SetCapture(w1);
    GetActivationClient(root_window())->ActivateWindow(w1);
    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());

    generator.ReleaseLeftButton();
    aura::client::GetCaptureClient(root_window())->ReleaseCapture(w1);
  }

  // Verifies focus change is honored while capture held.
  void ChangeFocusWhenNothingFocusedAndCaptured() override {
    std::unique_ptr<aura::client::DefaultCaptureClient> capture_client(
        new aura::client::DefaultCaptureClient(root_window()));
    aura::Window* w1 = root_window()->GetChildById(1);
    aura::client::GetCaptureClient(root_window())->SetCapture(w1);

    EXPECT_EQ(-1, GetActiveWindowId());
    EXPECT_EQ(-1, GetFocusedWindowId());

    FocusWindowById(1);

    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());

    aura::client::GetCaptureClient(root_window())->ReleaseCapture(w1);
  }

  // Verifies if a window that loses activation or focus is deleted during
  // observer notification we don't pass the deleted window to other observers.
  void DontPassDeletedWindow() override {
    FocusWindowById(1);

    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());

    {
      aura::Window* to_delete = root_window()->GetChildById(1);
      DeleteOnLoseActivationChangeObserver observer1(to_delete);
      RecordingActivationAndFocusChangeObserver observer2(root_window(),
                                                          &observer1);

      FocusWindowById(2);

      EXPECT_EQ(2, GetActiveWindowId());
      EXPECT_EQ(2, GetFocusedWindowId());

      EXPECT_EQ(to_delete, observer1.GetDeletedWindow());
      EXPECT_FALSE(observer2.was_notified_with_deleted_window());
    }

    {
      aura::Window* to_delete = root_window()->GetChildById(2);
      DeleteOnLoseFocusChangeObserver observer1(to_delete);
      RecordingActivationAndFocusChangeObserver observer2(root_window(),
                                                          &observer1);

      FocusWindowById(3);

      EXPECT_EQ(3, GetActiveWindowId());
      EXPECT_EQ(3, GetFocusedWindowId());

      EXPECT_EQ(to_delete, observer1.GetDeletedWindow());
      EXPECT_FALSE(observer2.was_notified_with_deleted_window());
    }
  }

  // Test that the activation of the current active window will bring the window
  // to the top of the window stack.
  void StackWindowAtTopOnActivation() override {
    // Create a window, show it and then activate it.
    std::unique_ptr<aura::Window> window1 =
        std::make_unique<aura::Window>(nullptr);
    window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window1->Init(ui::LAYER_TEXTURED);
    root_window()->AddChild(window1.get());
    window1->Show();
    ActivateWindow(window1.get());
    EXPECT_EQ(window1.get(), root_window()->children().back());
    EXPECT_EQ(window1.get(), GetActiveWindow());

    // Create another window, show it but don't activate it. The window is not
    // the active window but is placed on top of window stack.
    std::unique_ptr<aura::Window> window2 =
        std::make_unique<aura::Window>(nullptr);
    window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window2->Init(ui::LAYER_TEXTURED);
    root_window()->AddChild(window2.get());
    window2->Show();
    EXPECT_EQ(window2.get(), root_window()->children().back());
    EXPECT_EQ(window1.get(), GetActiveWindow());

    // Try to reactivate the active window. It should bring the active window
    // to the top of the window stack.
    ActivateWindow(window1.get());
    EXPECT_EQ(window1.get(), root_window()->children().back());
    EXPECT_EQ(window1.get(), GetActiveWindow());
  }

  // Verifies focus isn't left when during notification of an activation change
  // the focused window is hidden.
  void HideFocusedWindowDuringActivationLoss() override {
    aura::Window* w11 = root_window()->GetChildById(11);
    FocusWindow(w11);
    EXPECT_EQ(11, GetFocusedWindowId());
    EXPECT_EQ(1, GetActiveWindowId());
    {
      HideOnLoseActivationChangeObserver observer(w11);
      ActivateWindowById(2);
      EXPECT_EQ(nullptr, observer.window_to_hide());
      EXPECT_EQ(2, GetActiveWindowId());
      EXPECT_EQ(2, GetFocusedWindowId());
    }
  }

  // Tests that activating a window while a window is being activated is a
  // no-op.
  void ActivateWhileActivating() override {
    aura::Window* w1 = root_window()->GetChildById(1);
    aura::Window* w2 = root_window()->GetChildById(2);

    ActivateWindowById(3);
    // Activate a window while it is being activated does not DCHECK and the
    // window is made active.
    {
      ActivateWhileActivatingObserver observer(/*to_observe=*/w1,
                                               /*to_activate=*/w1,
                                               /*to_focus=*/nullptr);
      ActivateWindow(w1);
      EXPECT_EQ(1, GetActiveWindowId());
    }

    ActivateWindowById(3);
    // Focus a window while it is being activated does not DCHECK and the
    // window is made active and focused.
    {
      ActivateWhileActivatingObserver observer(/*to_observe=*/w1,
                                               /*to_activate=*/nullptr,
                                               /*to_focus=*/w1);
      ActivateWindow(w1);
      EXPECT_EQ(1, GetActiveWindowId());
      EXPECT_EQ(1, GetFocusedWindowId());
    }

    ActivateWindowById(3);
    // Shift focus while activating a window is allowed as long as it does
    // not attempt to activate a different window.
    {
      aura::Window* w11 = root_window()->GetChildById(11);
      aura::Window* w12 = root_window()->GetChildById(12);
      ActivateWhileActivatingObserver observer(/*to_observe=*/w1,
                                               /*to_activate=*/nullptr,
                                               /*to_focus=*/w12);
      FocusWindow(w11);
      EXPECT_EQ(1, GetActiveWindowId());
      EXPECT_EQ(12, GetFocusedWindowId());
    }

    ActivateWindowById(3);
    // Activate a different window while activating one fails. The window being
    // activated in the 1st activation request will be activated.
    {
      ActivateWhileActivatingObserver observer(/*to_observe=*/w2,
                                               /*to_activate=*/w1,
                                               /*to_focus=*/nullptr);
      // This should hit a DCHECK.
      EXPECT_DCHECK(
          {
            ActivateWindow(w2);
            EXPECT_EQ(2, GetActiveWindowId());
          },
          "");
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerDirectTestBase);
};

// Focus and Activation changes via ActivationClient API.
class FocusControllerApiTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerApiTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  void FocusWindowDirect(aura::Window* window) override { FocusWindow(window); }
  void ActivateWindowDirect(aura::Window* window) override {
    ActivateWindow(window);
  }
  void DeactivateWindowDirect(aura::Window* window) override {
    DeactivateWindow(window);
  }
  bool IsInputEvent() override { return false; }
  // Overridden from FocusControllerDirectTestBase:
  ActivationChangeObserver::ActivationReason GetExpectedActivationReason()
      const override {
    return ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT;
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerApiTest);
};

// Focus and Activation changes via input events.
class FocusControllerMouseEventTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerMouseEventTest() {}

  // Tests that a handled mouse or gesture event does not trigger a window
  // activation.
  void IgnoreHandledEvent() {
    EXPECT_EQ(NULL, GetActiveWindow());
    aura::Window* w1 = root_window()->GetChildById(1);
    SimpleEventHandler handler;
    root_window()->AddPreTargetHandler(&handler,
                                       ui::EventTarget::Priority::kSystem);
    ui::test::EventGenerator generator(root_window(), w1);
    generator.ClickLeftButton();
    EXPECT_EQ(NULL, GetActiveWindow());
    generator.GestureTapAt(w1->bounds().CenterPoint());
    EXPECT_EQ(NULL, GetActiveWindow());
    root_window()->RemovePreTargetHandler(&handler);
    generator.ClickLeftButton();
    EXPECT_EQ(1, GetActiveWindowId());
  }

 private:
  // Overridden from FocusControllerTestBase:
  void FocusWindowDirect(aura::Window* window) override {
    ui::test::EventGenerator generator(root_window(), window);
    generator.ClickLeftButton();
  }
  void ActivateWindowDirect(aura::Window* window) override {
    ui::test::EventGenerator generator(root_window(), window);
    generator.ClickLeftButton();
  }
  void DeactivateWindowDirect(aura::Window* window) override {
    aura::Window* next_activatable =
        test_focus_rules()->GetNextActivatableWindow(window);
    ui::test::EventGenerator generator(root_window(), next_activatable);
    generator.ClickLeftButton();
  }
  // Overridden from FocusControllerDirectTestBase:
  bool IsInputEvent() override { return true; }
  ActivationChangeObserver::ActivationReason GetExpectedActivationReason()
      const override {
    return ActivationChangeObserver::ActivationReason::INPUT_EVENT;
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerMouseEventTest);
};

class FocusControllerGestureEventTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerGestureEventTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  void FocusWindowDirect(aura::Window* window) override {
    ui::test::EventGenerator generator(root_window(), window);
    generator.GestureTapAt(window->bounds().CenterPoint());
  }
  void ActivateWindowDirect(aura::Window* window) override {
    ui::test::EventGenerator generator(root_window(), window);
    generator.GestureTapAt(window->bounds().CenterPoint());
  }
  void DeactivateWindowDirect(aura::Window* window) override {
    aura::Window* next_activatable =
        test_focus_rules()->GetNextActivatableWindow(window);
    ui::test::EventGenerator generator(root_window(), next_activatable);
    generator.GestureTapAt(window->bounds().CenterPoint());
  }
  bool IsInputEvent() override { return true; }
  ActivationChangeObserver::ActivationReason GetExpectedActivationReason()
      const override {
    return ActivationChangeObserver::ActivationReason::INPUT_EVENT;
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerGestureEventTest);
};

// Test base for tests where focus is implicitly set to a window as the result
// of a disposition change to the focused window or the hierarchy that contains
// it.
class FocusControllerImplicitTestBase : public FocusControllerTestBase {
 protected:
  explicit FocusControllerImplicitTestBase(bool parent) : parent_(parent) {}

  aura::Window* GetDispositionWindow(aura::Window* window) {
    return parent_ ? window->parent() : window;
  }

  // Returns the expected ActivationReason caused by calling the
  // ActivatedWindowDirect(...) or DeactivateWindowDirect(...) methods.
  ActivationChangeObserver::ActivationReason GetExpectedActivationReason()
      const {
    return ActivationChangeObserver::ActivationReason::
        WINDOW_DISPOSITION_CHANGED;
  }

  // Change the disposition of |window| in such a way as it will lose focus.
  virtual void ChangeWindowDisposition(aura::Window* window) = 0;

  // Allow each disposition change test to add additional post-disposition
  // change expectations.
  virtual void PostDispositionChangeExpectations() {}

  // Overridden from FocusControllerTestBase:
  void BasicFocus() override {
    EXPECT_EQ(NULL, GetFocusedWindow());

    aura::Window* w211 = root_window()->GetChildById(211);
    FocusWindow(w211);
    EXPECT_EQ(211, GetFocusedWindowId());

    ChangeWindowDisposition(w211);
    // BasicFocusRules passes focus to the parent.
    EXPECT_EQ(parent_ ? 2 : 21, GetFocusedWindowId());
  }
  void BasicActivation() override {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    EXPECT_EQ(NULL, GetActiveWindow());

    aura::Window* w2 = root_window()->GetChildById(2);
    ActivateWindow(w2);
    EXPECT_EQ(2, GetActiveWindowId());

    ChangeWindowDisposition(w2);
    EXPECT_EQ(3, GetActiveWindowId());
    PostDispositionChangeExpectations();
  }
  void FocusEvents() override {
    aura::Window* w211 = root_window()->GetChildById(211);
    FocusWindow(w211);

    ScopedFocusNotificationObserver root_observer(root_window());
    ScopedTargetFocusNotificationObserver observer211(root_window(), 211);
    root_observer.ExpectCounts(0, 0);
    observer211.ExpectCounts(0, 0);

    ChangeWindowDisposition(w211);
    root_observer.ExpectCounts(0, 1);
    observer211.ExpectCounts(0, 1);
  }
  void ActivationEvents() override {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    aura::Window* w2 = root_window()->GetChildById(2);
    ActivateWindow(w2);

    ScopedFocusNotificationObserver root_observer(root_window());
    ScopedTargetFocusNotificationObserver observer2(root_window(), 2);
    ScopedTargetFocusNotificationObserver observer3(root_window(), 3);
    root_observer.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);
    observer3.ExpectCounts(0, 0);

    ChangeWindowDisposition(w2);
    root_observer.ExpectCounts(1, 1);
    EXPECT_EQ(GetExpectedActivationReason(),
              root_observer.last_activation_reason());
    observer2.ExpectCounts(1, 1);
    observer3.ExpectCounts(1, 1);
  }
  void FocusRulesOverride() override {
    EXPECT_EQ(NULL, GetFocusedWindow());
    aura::Window* w211 = root_window()->GetChildById(211);
    FocusWindow(w211);
    EXPECT_EQ(211, GetFocusedWindowId());

    test_focus_rules()->set_focus_restriction(root_window()->GetChildById(11));
    ChangeWindowDisposition(w211);
    // Normally, focus would shift to the parent (w21) but the override shifts
    // it to 11.
    EXPECT_EQ(11, GetFocusedWindowId());

    test_focus_rules()->set_focus_restriction(NULL);
  }
  void ActivationRulesOverride() override {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    aura::Window* w1 = root_window()->GetChildById(1);
    ActivateWindow(w1);

    EXPECT_EQ(1, GetActiveWindowId());
    EXPECT_EQ(1, GetFocusedWindowId());

    aura::Window* w3 = root_window()->GetChildById(3);
    test_focus_rules()->set_focus_restriction(w3);

    // Normally, activation/focus would move to w2, but since we have a focus
    // restriction, it should move to w3 instead.
    ChangeWindowDisposition(w1);
    EXPECT_EQ(3, GetActiveWindowId());
    EXPECT_EQ(3, GetFocusedWindowId());

    test_focus_rules()->set_focus_restriction(NULL);
    ActivateWindow(root_window()->GetChildById(2));
    EXPECT_EQ(2, GetActiveWindowId());
    EXPECT_EQ(2, GetFocusedWindowId());
  }

 private:
  // When true, the disposition change occurs to the parent of the window
  // instead of to the window. This verifies that changes occurring in the
  // hierarchy that contains the window affect the window's focus.
  bool parent_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerImplicitTestBase);
};

// Focus and Activation changes in response to window visibility changes.
class FocusControllerHideTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerHideTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerHideTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  void ChangeWindowDisposition(aura::Window* window) override {
    GetDispositionWindow(window)->Hide();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerHideTest);
};

// Focus and Activation changes in response to window parent visibility
// changes.
class FocusControllerParentHideTest : public FocusControllerHideTest {
 public:
  FocusControllerParentHideTest() : FocusControllerHideTest(true) {}

  // The parent window's visibility change should not change its transient child
  // window's modality property.
  void TransientChildWindowActivationTest() {
    aura::Window* w1 = root_window()->GetChildById(1);
    aura::Window* w11 = root_window()->GetChildById(11);
    ::wm::AddTransientChild(w1, w11);
    w11->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);

    EXPECT_EQ(ui::MODAL_TYPE_NONE, w1->GetProperty(aura::client::kModalKey));
    EXPECT_EQ(ui::MODAL_TYPE_WINDOW, w11->GetProperty(aura::client::kModalKey));

    // Hide the parent window w1 and show it again.
    w1->Hide();
    w1->Show();

    // Test that child window w11 doesn't change its modality property.
    EXPECT_EQ(ui::MODAL_TYPE_NONE, w1->GetProperty(aura::client::kModalKey));
    EXPECT_EQ(ui::MODAL_TYPE_WINDOW, w11->GetProperty(aura::client::kModalKey));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentHideTest);
};

// Focus and Activation changes in response to window destruction.
class FocusControllerDestructionTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerDestructionTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerDestructionTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  void ChangeWindowDisposition(aura::Window* window) override {
    delete GetDispositionWindow(window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerDestructionTest);
};

// Focus and Activation changes in response to window parent destruction.
class FocusControllerParentDestructionTest
    : public FocusControllerDestructionTest {
 public:
  FocusControllerParentDestructionTest()
      : FocusControllerDestructionTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentDestructionTest);
};

// Focus and Activation changes in response to window removal.
class FocusControllerRemovalTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerRemovalTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerRemovalTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  void ChangeWindowDisposition(aura::Window* window) override {
    aura::Window* disposition_window = GetDispositionWindow(window);
    disposition_window->parent()->RemoveChild(disposition_window);
    window_owner_.reset(disposition_window);
  }
  void TearDown() override {
    window_owner_.reset();
    FocusControllerImplicitTestBase::TearDown();
  }

 private:
  std::unique_ptr<aura::Window> window_owner_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerRemovalTest);
};

// Focus and Activation changes in response to window parent removal.
class FocusControllerParentRemovalTest : public FocusControllerRemovalTest {
 public:
  FocusControllerParentRemovalTest() : FocusControllerRemovalTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentRemovalTest);
};


#define FOCUS_CONTROLLER_TEST(TESTCLASS, TESTNAME) \
    TEST_F(TESTCLASS, TESTNAME) { TESTNAME(); }

// Runs direct focus change tests (input events and API calls).
#define DIRECT_FOCUS_CHANGE_TESTS(TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerApiTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerMouseEventTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerGestureEventTest, TESTNAME)

// Runs implicit focus change tests for disposition changes to target.
#define IMPLICIT_FOCUS_CHANGE_TARGET_TESTS(TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerDestructionTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerRemovalTest, TESTNAME)

// Runs implicit focus change tests for disposition changes to target's parent
// hierarchy.
#define IMPLICIT_FOCUS_CHANGE_PARENT_TESTS(TESTNAME) \
    /* TODO(beng): parent destruction tests are not supported at
       present due to workspace manager issues. \
    FOCUS_CONTROLLER_TEST(FocusControllerParentDestructionTest, TESTNAME) */ \
    FOCUS_CONTROLLER_TEST(FocusControllerParentHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerParentRemovalTest, TESTNAME)

// Runs all implicit focus change tests (changes to the target and target's
// parent hierarchy)
#define IMPLICIT_FOCUS_CHANGE_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_TARGET_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_PARENT_TESTS(TESTNAME)

// Runs all possible focus change tests.
#define ALL_FOCUS_TESTS(TESTNAME) \
    DIRECT_FOCUS_CHANGE_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_TESTS(TESTNAME)

// Runs focus change tests that apply only to the target. For example,
// implicit activation changes caused by window disposition changes do not
// occur when changes to the containing hierarchy happen.
#define TARGET_FOCUS_TESTS(TESTNAME) \
    DIRECT_FOCUS_CHANGE_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_TARGET_TESTS(TESTNAME)

// - Focuses a window, verifies that focus changed.
ALL_FOCUS_TESTS(BasicFocus)

// - Activates a window, verifies that activation changed.
TARGET_FOCUS_TESTS(BasicActivation)

// - Focuses a window, verifies that focus events were dispatched.
ALL_FOCUS_TESTS(FocusEvents)

// - Focuses or activates a window multiple times, verifies that events are only
//   dispatched when focus/activation actually changes.
DIRECT_FOCUS_CHANGE_TESTS(DuplicateFocusEvents)
DIRECT_FOCUS_CHANGE_TESTS(DuplicateActivationEvents)

// - Activates a window, verifies that activation events were dispatched.
TARGET_FOCUS_TESTS(ActivationEvents)

// - Attempts to active a hidden window, verifies that current window is
//   attempted to be reactivated and the appropriate event dispatched.
FOCUS_CONTROLLER_TEST(FocusControllerApiTest, ReactivationEvents)

// - Input events/API calls shift focus between focusable windows within the
//   active window.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusWithinActiveWindow)

// - Input events/API calls to a child window of an inactive window shifts
//   activation to the activatable parent and focuses the child.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusToChildOfInactiveWindow)

// - Input events/API calls to focus the parent of the focused window do not
//   shift focus away from the child.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusToParentOfFocusedWindow)

// - Verifies that FocusRules determine what can be focused.
ALL_FOCUS_TESTS(FocusRulesOverride)

// - Verifies that FocusRules determine what can be activated.
TARGET_FOCUS_TESTS(ActivationRulesOverride)

// - Verifies that attempts to change focus or activation from a focus or
//   activation change observer are ignored.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusOnActivation)
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusOnActivationDueToHide)
DIRECT_FOCUS_CHANGE_TESTS(NoShiftActiveOnActivation)

FOCUS_CONTROLLER_TEST(FocusControllerApiTest, FocusChangeDuringDrag)

FOCUS_CONTROLLER_TEST(FocusControllerApiTest,
                      ChangeFocusWhenNothingFocusedAndCaptured)

// See description above DontPassDeletedWindow() for details.
FOCUS_CONTROLLER_TEST(FocusControllerApiTest, DontPassDeletedWindow)

FOCUS_CONTROLLER_TEST(FocusControllerApiTest, StackWindowAtTopOnActivation)

FOCUS_CONTROLLER_TEST(FocusControllerApiTest,
                      HideFocusedWindowDuringActivationLoss)

FOCUS_CONTROLLER_TEST(FocusControllerApiTest, ActivateWhileActivating)

// See description above TransientChildWindowActivationTest() for details.
FOCUS_CONTROLLER_TEST(FocusControllerParentHideTest,
                      TransientChildWindowActivationTest)

// If a mouse event was handled, it should not activate a window.
FOCUS_CONTROLLER_TEST(FocusControllerMouseEventTest, IgnoreHandledEvent)

}  // namespace wm
