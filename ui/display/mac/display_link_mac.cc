// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"

namespace base::apple {

template <>
struct ScopedTypeRefTraits<CVDisplayLinkRef> {
  static CVDisplayLinkRef InvalidValue() { return nullptr; }
  static CVDisplayLinkRef Retain(CVDisplayLinkRef object) {
    return CVDisplayLinkRetain(object);
  }
  static void Release(CVDisplayLinkRef object) { CVDisplayLinkRelease(object); }
};

}  // namespace base::apple

namespace ui {

namespace {
// To prevent constantly switching VSync on and off, allow this max number of
// extra CVDisplayLink VSync running before stopping CVDisplayLink.
constexpr int kMaxExtraVSyncs = 12;

struct DisplayLinkGlobals {
  // |map| maybe accessed on anythread but only modified on the main thread..
  std::map<CGDirectDisplayID, std::unique_ptr<DisplayLinkMacSharedState>>
      GUARDED_BY(lock) map;

  // Making any calls to the CVDisplayLink API while `lock` is held can
  // result in deadlock, because `lock` is taken inside the CVDisplayLink
  // system callback.
  // https://crbug.com/1427235#c2
  base::Lock lock;

  void AssertLockHeldByCurrentThread() const { lock.AssertAcquired(); }

  void AssertLockNotHeldByCurrentThread() const {
    // The function base::Lock::AssertNotHeld asserts that no thread holds
    // the specified lock, not that the current thread does not hold it.
    // TODO(https://crbug.com/1427235): Make this be a real DCHECK.
  }

  static DisplayLinkGlobals& Get() {
    static base::NoDestructor<DisplayLinkGlobals> instance;
    return *instance;
  }
};

bool ComputeVSyncParameters(const CVTimeStamp& cv_time,
                            base::TimeTicks* timebase,
                            base::TimeDelta* interval) {
  // Verify that videoRefreshPeriod fits in 32 bits.
  DCHECK((cv_time.videoRefreshPeriod & ~0xFFFF'FFFFull) == 0ull);

  // Verify that the numerator and denominator make some sense.
  uint32_t numerator = static_cast<uint32_t>(cv_time.videoRefreshPeriod);
  uint32_t denominator = cv_time.videoTimeScale;
  if (numerator == 0 || denominator == 0) {
    LOG(WARNING) << "Unexpected numerator or denominator, bailing.";
    return false;
  }

  base::CheckedNumeric<int64_t> interval_us(base::Time::kMicrosecondsPerSecond);
  interval_us *= numerator;
  interval_us /= denominator;
  if (!interval_us.IsValid()) {
    LOG(DFATAL) << "Bailing due to overflow: "
                << base::Time::kMicrosecondsPerSecond << " * " << numerator
                << " / " << denominator;
    return false;
  }

  *timebase = base::TimeTicks::FromMachAbsoluteTime(cv_time.hostTime);
  *interval = base::Microseconds(int64_t{interval_us.ValueOrDie()});
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayLinkMacSharedState

// For each CGDirectDisplayID there is only one DisplayLinkMacSharedState
// structure, which is shared_state between all DisplayLinkMacs that have that
// CGDirectDisplayID.
class DisplayLinkMacSharedState {
 public:
  static std::unique_ptr<DisplayLinkMacSharedState> Create(
      CGDirectDisplayID display_id);

  ~DisplayLinkMacSharedState() {
    // The destructor will call into the CVDisplayLink API to delete
    // `display_link_`. Ensure that we do not hold the globals' lock.
    DisplayLinkGlobals::Get().AssertLockNotHeldByCurrentThread();
  }

  // Increment the refcount for `this`. Caller must hold the global' lock.
  void Retain();

  // Decrement the refcount for `this` and potentially delete `this`. Caller
  // must not hold the globals' lock.
  void Release();

  double GetRefreshRate() const;
  base::TimeTicks GetCurrentTime() const;

  // Run all callbacks. This is called during the CVDisplayLink callback.
  void RunCallbacks(const VSyncParamsMac& params) const;

  // The interval over which CVDisplayLinkStart and CVDisplayLinkStop are
  // called. The EnsureDisplayLinkRunning call will return false if
  // CVDisplayLinkStart fails.
  bool EnsureDisplayLinkRunning();
  void StopDisplayLinkIfNeeded();

  // Register and unregister a callback. Note that the callback itself will keep
  // `this` alive (because it holds a reference to a DisplayLinkMac, which holds
  // a reference to `this`).
  void RegisterCallback(VSyncCallbackMac* callback);
  void UnregisterCallback(VSyncCallbackMac* callback);

 private:
  DisplayLinkMacSharedState(
      CGDirectDisplayID display_id,
      base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link)
      : display_id_(display_id), display_link_(std::move(display_link)) {
    DisplayLinkGlobals::Get().AssertLockNotHeldByCurrentThread();
  }

  // Reference count that controls the lifetime of `this`.
  uint32_t refcount_ = 0;

  // The display that this display link is attached to.
  CGDirectDisplayID display_id_;

  // CVDisplayLink for querying VSync timing info.
  base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link_;

  // Each VSyncCallbackMac holds a reference to `this`. This member may only be
  // accessed or modified while holding `globals.lock`.
  std::set<VSyncCallbackMac*> callbacks_;

  // The number of consecutive DisplayLink VSyncs received after zero
  // |callbacks_|. CVDisplayLink will be stopped after |kMaxExtraVSyncs| is
  // reached. It's guarded by |globals.lock|.
  int consecutive_vsyncs_with_no_callbacks_ = 0;

  // The status whether CVDisplayLinkStart or CVDisplayLinkStop is called.
  bool display_link_is_running_ GUARDED_BY(display_link_running_lock_) = false;

  // The CVDisplayLink API is called while holding `display_link_running_lock_`.
  base::Lock display_link_running_lock_;
};

namespace {

// Called by the system on the display link thread, and posts a call to
// the thread indicated in DisplayLinkMac::RegisterCallback().
CVReturn DisplayLinkCallback(CVDisplayLinkRef display_link,
                             const CVTimeStamp* now,
                             const CVTimeStamp* output_time,
                             CVOptionFlags flags_in,
                             CVOptionFlags* flags_out,
                             void* context) {
  // This function is called on the system CVDisplayLink thread.
  TRACE_EVENT0("ui", "DisplayLinkCallback");

  // Convert the time parameters to our VSync parameters.
  VSyncParamsMac params;
  params.callback_times_valid = ComputeVSyncParameters(
      *now, &params.callback_timebase, &params.callback_interval);
  params.display_times_valid = ComputeVSyncParameters(
      *output_time, &params.display_timebase, &params.display_interval);

  // Take `globals.lock`. There exists a lock internal to the CVDisplayLink,
  // and that lock is held during this callback. We cannot call the
  // CVDisplayLink API while holding `globals.lock`, because that would take
  // the locks in the opposite order, potentially deadlocking.
  // https://crbug.com/1427235#c2
  auto& globals = DisplayLinkGlobals::Get();
  base::AutoLock lock(globals.lock);

  // Locate the DisplayLinkMac for this display.
  CGDirectDisplayID display_id =
      static_cast<CGDirectDisplayID>(reinterpret_cast<uintptr_t>(context));
  auto found = globals.map.find(display_id);
  if (found == globals.map.end()) {
    return kCVReturnSuccess;
  }

  // Issue all of its callbacks.
  auto* shared_state = found->second.get();
  shared_state->RunCallbacks(params);
  shared_state->StopDisplayLinkIfNeeded();

  return kCVReturnSuccess;
}

}  // namespace

std::unique_ptr<DisplayLinkMacSharedState> DisplayLinkMacSharedState::Create(
    CGDirectDisplayID display_id) {
  // Create a new DisplayLink, outside of the lock. Creating the CVDisplayLink
  // wll take a OS-internal lock which is also held during the CVDisplayLink
  // callback.
  DisplayLinkGlobals::Get().AssertLockNotHeldByCurrentThread();

  CVReturn ret = kCVReturnSuccess;
  base::apple::ScopedTypeRef<CVDisplayLinkRef> display_link;
  ret = CVDisplayLinkCreateWithCGDisplay(display_id,
                                         display_link.InitializeInto());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithCGDisplay failed. CVReturn: " << ret;
    return nullptr;
  }

  // Workaround for bug https://crbug.com/1218720. According to
  // https://hg.mozilla.org/releases/mozilla-esr68/rev/db0628eadb86,
  // CVDisplayLinkCreateWithCGDisplays()
  // (called by CVDisplayLinkCreateWithCGDisplay()) sometimes
  // creates a CVDisplayLinkRef with an uninitialized (nulled) internal
  // pointer. If we continue to use this CVDisplayLinkRef, we will
  // eventually crash in CVCGDisplayLink::getDisplayTimes(), where the
  // internal pointer is dereferenced. Fortunately, when this happens
  // another internal variable is also left uninitialized (zeroed),
  // which is accessible via CVDisplayLinkGetCurrentCGDisplay(). In
  // normal conditions the current display is never zero.
  if ((ret == kCVReturnSuccess) &&
      (CVDisplayLinkGetCurrentCGDisplay(display_link) == 0)) {
    LOG(ERROR)
        << "CVDisplayLinkCreateWithCGDisplay failed (no current display)";
    return nullptr;
  }

  ret = CVDisplayLinkSetOutputCallback(display_link, &DisplayLinkCallback,
                                       reinterpret_cast<void*>(display_id));
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkSetOutputCallback failed. CVReturn: " << ret;
    return nullptr;
  }

  return base::WrapUnique(
      new DisplayLinkMacSharedState(display_id, std::move(display_link)));
}

// Functions to call CVDisplayLinkStart and CVDisplayLinkStop. This is
// reference counted, and takes `display_link_running_lock_`.
bool DisplayLinkMacSharedState::EnsureDisplayLinkRunning() {
  base::AutoLock lock(display_link_running_lock_);

  if (!display_link_is_running_) {
    DCHECK(!CVDisplayLinkIsRunning(display_link_));
    CVReturn ret = CVDisplayLinkStart(display_link_);
    if (ret != kCVReturnSuccess) {
      LOG(ERROR) << "CVDisplayLinkStart failed. CVReturn: " << ret;
      return false;
    }
    display_link_is_running_ = true;
  }

  return true;
}

// Called on the system CVDisplayLink thread.
void DisplayLinkMacSharedState::StopDisplayLinkIfNeeded() {
  DisplayLinkGlobals::Get().AssertLockHeldByCurrentThread();

  if (!callbacks_.empty()) {
    consecutive_vsyncs_with_no_callbacks_ = 0;
    return;
  }
  consecutive_vsyncs_with_no_callbacks_ += 1;
  if (consecutive_vsyncs_with_no_callbacks_ < kMaxExtraVSyncs) {
    return;
  }

  base::AutoLock lock(display_link_running_lock_);
  if (!display_link_is_running_) {
    return;
  }
  CVReturn ret = CVDisplayLinkStop(display_link_);
  if (ret == kCVReturnSuccess) {
    display_link_is_running_ = false;
    consecutive_vsyncs_with_no_callbacks_ = 0;
  } else {
    LOG(ERROR) << "CVDisplayLinkStop failed. CVReturn: " << ret;
  }
}

double DisplayLinkMacSharedState::GetRefreshRate() const {
  double refresh_rate = 0;
  CVTime cv_time =
      CVDisplayLinkGetNominalOutputVideoRefreshPeriod(display_link_);
  if (!(cv_time.flags & kCVTimeIsIndefinite)) {
    refresh_rate = (static_cast<double>(cv_time.timeScale) /
                    static_cast<double>(cv_time.timeValue));
  }

  return refresh_rate;
}

base::TimeTicks DisplayLinkMacSharedState::GetCurrentTime() const {
  CVTimeStamp out_time;
  CVReturn ret = CVDisplayLinkGetCurrentTime(display_link_, &out_time);
  if (ret == kCVReturnSuccess) {
    return base::TimeTicks::FromMachAbsoluteTime(out_time.hostTime);
  } else {
    return base::TimeTicks();
  }
}

void DisplayLinkMacSharedState::RunCallbacks(
    const VSyncParamsMac& params) const {
  DisplayLinkGlobals::Get().AssertLockHeldByCurrentThread();
  for (auto* callback : callbacks_) {
    callback->callback_for_cvdisplaylink_thread_.Run(params);
  }
}

void DisplayLinkMacSharedState::RegisterCallback(VSyncCallbackMac* callback) {
  base::AutoLock lock(DisplayLinkGlobals::Get().lock);
  callbacks_.insert(callback);
}

void DisplayLinkMacSharedState::UnregisterCallback(VSyncCallbackMac* callback) {
  base::AutoLock lock(DisplayLinkGlobals::Get().lock);
  callbacks_.erase(callback);
}

void DisplayLinkMacSharedState::Retain() {
  DisplayLinkGlobals::Get().AssertLockHeldByCurrentThread();
  refcount_ += 1;
}

void DisplayLinkMacSharedState::Release() {
  auto& globals = DisplayLinkGlobals::Get();
  globals.AssertLockNotHeldByCurrentThread();

  // If the reference count drops to zero, the populate `scoped_this` with
  // the std::unique_ptr holding `this`, so that it can be deleted after
  // `globals.lock` is released.
  std::unique_ptr<DisplayLinkMacSharedState> scoped_this;

  // While holding `globals.lock`, decrement `refcount_`, and potentially
  // remove `this` from `globals.map`.
  {
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find(display_id_);
    DCHECK(found != globals.map.end());
    DCHECK(found->second.get() == this);

    DCHECK(refcount_ > 0);
    refcount_ -= 1;
    if (refcount_ == 0) {
      scoped_this = std::move(found->second);
      globals.map.erase(found);
    }
  }

  // Let `scoped_this` be destroyed now that `globals.lock` is no longer held.
  scoped_this = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayLinkMac

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
  if (!display_id) {
    return nullptr;
  }

  // Take `globals.lock` and check if there exists DisplayLinkMacSharedState for
  // this id.
  auto& globals = DisplayLinkGlobals::Get();
  {
    base::AutoLock lock(globals.lock);
    auto found = globals.map.find(display_id);
    if (found != globals.map.end()) {
      return new DisplayLinkMac(found->second.get());
    }
  }

  // Create a new CVDisplayLink while not holding `globals.lock`.
  auto shared_state = DisplayLinkMacSharedState::Create(display_id);
  if (!shared_state) {
    return nullptr;
  }

  // Take `globals.lock` again and store `shared_state` in the map (or use an
  // existing DisplayLinkMacSharedState, if another thread created one).
  scoped_refptr<DisplayLinkMac> result;
  {
    base::AutoLock lock(globals.lock);

    auto found = globals.map.find(display_id);
    if (found == globals.map.end()) {
      found = globals.map.emplace(display_id, std::move(shared_state)).first;
    }
    result = new DisplayLinkMac(found->second.get());
  }

  // If we didn't need `shared_state`, because another thread created one and
  // won the race to put it in `globals.map`, delete it, now that we no longer
  // hold `globals.lock`.
  shared_state = nullptr;

  return result;
}

double DisplayLinkMac::GetRefreshRate() const {
  return shared_state_->GetRefreshRate();
}

base::TimeTicks DisplayLinkMac::GetCurrentTime() const {
  return shared_state_->GetCurrentTime();
}

DisplayLinkMac::DisplayLinkMac(DisplayLinkMacSharedState* shared_state)
    : shared_state_(shared_state) {
  DisplayLinkGlobals::Get().AssertLockHeldByCurrentThread();
  shared_state_->Retain();
}

DisplayLinkMac::~DisplayLinkMac() {
  DisplayLinkGlobals::Get().AssertLockNotHeldByCurrentThread();

  // `shared_state_` may be deleted by the call to Release. Avoid dangling
  // raw_ptr warnings by setting `shared_state_` to nullptr prior to calling
  // Release.
  DisplayLinkMacSharedState* shared_state_to_release = shared_state_;
  shared_state_ = nullptr;

  shared_state_to_release->Release();
  shared_state_to_release = nullptr;
}

std::unique_ptr<VSyncCallbackMac> DisplayLinkMac::RegisterCallback(
    VSyncCallbackMac::Callback callback,
    bool do_callback_on_register_thread) {
  // Make add the new callback. Register first before calling
  // EnsureDisplayLinkRunning() to ensure the callback function is available.
  std::unique_ptr<VSyncCallbackMac> new_callback(new VSyncCallbackMac(
      base::BindOnce(&DisplayLinkMac::UnregisterCallback, this),
      std::move(callback), do_callback_on_register_thread));
  shared_state_->RegisterCallback(new_callback.get());

  // Ensure that the CVDisplayLink is running. If something goes wrong, return
  // nullptr.
  if (!shared_state_->EnsureDisplayLinkRunning()) {
    new_callback.reset();
  }

  return new_callback;
}

void DisplayLinkMac::UnregisterCallback(VSyncCallbackMac* callback) {
  shared_state_->UnregisterCallback(callback);
}

////////////////////////////////////////////////////////////////////////////////
// VSyncCallbackMac

VSyncCallbackMac::VSyncCallbackMac(UnregisterCallback unregister_callback,
                                   Callback callback,
                                   bool do_callback_on_ctor_thread)
    : unregister_callback_(std::move(unregister_callback)),
      weak_factory_(this) {
  if (do_callback_on_ctor_thread) {
    auto lambda = [](base::WeakPtr<VSyncCallbackMac> weak_this,
                     Callback callback, VSyncParamsMac params) {
      if (weak_this) {
        callback.Run(params);
      }
    };
    auto callback_for_current_thread =
        base::BindRepeating(lambda, weak_factory_.GetWeakPtr(), callback);
    callback_for_cvdisplaylink_thread_ =
        base::BindPostTaskToCurrentDefault(callback_for_current_thread);
  } else {
    callback_for_cvdisplaylink_thread_ = std::move(callback);
  }
}

VSyncCallbackMac::~VSyncCallbackMac() {
  std::move(unregister_callback_).Run(this);
}

}  // namespace ui
