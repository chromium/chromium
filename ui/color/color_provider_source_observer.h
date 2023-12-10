// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_SOURCE_OBSERVER_H_
#define UI_COLOR_COLOR_PROVIDER_SOURCE_OBSERVER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/color/color_provider_source.h"

namespace ui {

// Implemented by classes wanting access to the ColorProviderSource's current
// ColorProvider instance and receives updates on changes to the instance
// supplied. Can only observe a single ColorProviderSource at a time.
class COMPONENT_EXPORT(COLOR) ColorProviderSourceObserver
    : public base::CheckedObserver {
 public:
  explicit ColorProviderSourceObserver(ColorProviderSource* source = nullptr);
  ~ColorProviderSourceObserver() override;

  // Called when the source's ColorProvider instance has changed.
  virtual void OnColorProviderChanged() = 0;

  // Called by the ColorProviderSource during destruction. Avoids situations
  // where we could be left with a dangling pointer should the observer outlive
  // the source.
  void OnColorProviderSourceDestroying();

  const ui::ColorProviderSource* GetColorProviderSourceForTesting() const;

 protected:
  // Starts observing the new `source`. Clears the current observation if
  // already observing a ColorProviderSource.
  void Observe(ColorProviderSource* source);

  // Gets the ColorProviderSource currently under observation, if it exists.
  const ui::ColorProviderSource* GetColorProviderSource() const;

 private:
  // The currently observed source.
  raw_ptr<const ui::ColorProviderSource> source_ = nullptr;

  // Ensure references to the observer are removed from the source should the
  // source outlive the observer.
  base::ScopedObservation<ColorProviderSource, ColorProviderSourceObserver>
      color_provider_source_observation_{this};
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_SOURCE_OBSERVER_H_
