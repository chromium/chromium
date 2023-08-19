// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/api_object.h"

#include <cstdint>

#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

namespace ipcz {

namespace {

#if defined(LEAK_SANITIZER)
// When LSan is enabled, we keep all living API objects tracked within a global
// hash set so that they're always reachable and never detected as leaks. This
// is to work around the fact that Chromium has amassed hundreds of tests across
// more than a dozen test suites which leak Mojo handles; and prior to MojoIpcz
// the leaks were masked from LSan by Mojo's use of a global handle table.
//
// TODO(https://crbug.com/1415046): Remove this once all of the leaky tests are
// fixed.
class TrackedObjectSet {
 public:
  TrackedObjectSet() = default;
  ~TrackedObjectSet() = default;

  void Add(APIObject* object) {
    absl::MutexLock lock(&mutex_);
    tracked_objects_.insert(reinterpret_cast<uintptr_t>(object));
  }

  void Remove(APIObject* object) {
    absl::MutexLock lock(&mutex_);
    tracked_objects_.erase(reinterpret_cast<uintptr_t>(object));
  }

 private:
  absl::Mutex mutex_;

  // Use a uintptr_t to track since these aren't meant to be usable as pointers.
  absl::flat_hash_set<uintptr_t> tracked_objects_ ABSL_GUARDED_BY(mutex_);
};

TrackedObjectSet& GetTrackedObjectSet() {
  static auto* set = new TrackedObjectSet();
  return *set;
}

void TrackObject(APIObject* object) {
  GetTrackedObjectSet().Add(object);
}

void UntrackObject(APIObject* object) {
  GetTrackedObjectSet().Remove(object);
}
#else
void TrackObject(APIObject*) {}
void UntrackObject(APIObject*) {}
#endif

}  // namespace

APIObject::APIObject(ObjectType type) : type_(type) {
  TrackObject(this);
}

APIObject::~APIObject() {
  UntrackObject(this);
}

bool APIObject::CanSendFrom(Router& sender) {
  return false;
}

}  // namespace ipcz
