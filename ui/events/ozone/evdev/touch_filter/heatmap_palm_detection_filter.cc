// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/heatmap_palm_detection_filter.h"

#include <bitset>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

namespace ui {

HeatmapPalmFilterStroke::HeatmapPalmFilterStroke(
    const HeatmapPalmDetectionFilterModelConfig& model_config,
    int tracking_id)
    : tracking_id_(tracking_id),
      max_sample_count_(model_config.max_sample_count) {
  if (max_sample_count_ <= 0) {
    DVLOG(1) << "Max sample count " << max_sample_count_
             << " should be greater than 0";
  }
}
HeatmapPalmFilterStroke::HeatmapPalmFilterStroke(
    const HeatmapPalmFilterStroke& other) = default;
HeatmapPalmFilterStroke::HeatmapPalmFilterStroke(
    HeatmapPalmFilterStroke&& other) = default;
HeatmapPalmFilterStroke::~HeatmapPalmFilterStroke() = default;

void HeatmapPalmFilterStroke::ProcessSample(
    const HeatmapPalmFilterSample& sample) {
  if (tracking_id_ != sample.tracking_id) {
    DLOG(ERROR) << "Tracking id does not match.";
    return;
  }
  if (samples_seen_ == 0) {
    first_sample_time_ = sample.time;
  }

  AddSample(sample);

  // Prune based on number of samples.
  while (samples_.size() > max_sample_count_) {
    samples_.pop_front();
  }
}

void HeatmapPalmFilterStroke::AddSample(const HeatmapPalmFilterSample& sample) {
  samples_.push_back(sample);
  samples_seen_++;
}

base::TimeDelta HeatmapPalmFilterStroke::Duration() const {
  if (samples_.empty()) {
    DLOG(ERROR) << "No samples available.";
    return base::Milliseconds(0);
  }
  return samples_.back().time - first_sample_time_;
}

HeatmapPalmDetectionFilter::HeatmapPalmDetectionFilter(
    const EventDeviceInfo& devinfo,
    std::unique_ptr<HeatmapPalmDetectionFilterModelConfig> model_config,
    SharedPalmDetectionFilterState* shared_palm_state)
    : PalmDetectionFilter(shared_palm_state),
      model_config_(std::move(model_config)) {
  DCHECK(CompatibleWithHeatmapPalmDetectionFilter(devinfo))
      << "One should run compatible check before instantiation.";
}

HeatmapPalmDetectionFilter::~HeatmapPalmDetectionFilter() = default;

bool HasDecidedStroke(const HeatmapPalmFilterStroke& stroke,
                      const HeatmapPalmDetectionFilterModelConfig& config) {
  return stroke.samples_seen() >= config.max_sample_count;
}

HeatmapPalmFilterSample CreateHeatmapPalmFilterSample(
    const InProgressTouchEvdev& touch,
    const base::TimeTicks& time) {
  HeatmapPalmFilterSample sample;
  sample.tracking_id = touch.tracking_id;
  sample.time = time;

  return sample;
}

void HeatmapPalmDetectionFilter::Filter(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time,
    std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
    std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) {
  EraseOldStrokes(time);
  slots_to_hold->reset();
  slots_to_suppress->reset();
  std::vector<int> ended_tracking_ids;

  for (const auto& touch : touches) {
    // Ignore touch events that are not touches.
    if (!touch.touching && !touch.was_touching) {
      continue;
    }
    int tracking_id = touch.tracking_id;
    const size_t slot = touch.slot;
    if (!touch.was_touching) {
      if (tracking_id == -1) {
        DVLOG(1) << "Tracking id " << tracking_id << " has ended.";
      } else if (strokes_.count(tracking_id) != 0) {
        DVLOG(1) << "Tracking id " << tracking_id
                 << " is already in the stroke list.";
      } else {
        // New stroke, so add the new stroke to the stroke list.
        strokes_.emplace(tracking_id, HeatmapPalmFilterStroke(
                                          *model_config_.get(), tracking_id));
        tracking_ids_[slot] = tracking_id;
      }
    }

    const bool end_of_stroke = tracking_id == -1;
    if (end_of_stroke) {
      // Recover the tracking ID.
      tracking_id = tracking_ids_[slot];
    }

    if (tracking_id == -1) {
      DVLOG(1) << "Tracking id " << tracking_id << " has ended, continue.";
      continue;
    }

    // Find the stroke in the stroke list.
    auto stroke_it = strokes_.find(tracking_id);

    if (stroke_it == strokes_.end()) {
      // TODO(crbug.com/40796088): Work out why this is hit on long presses.
      DVLOG(1) << "No stroke found, continue.";
      continue;
    }

    HeatmapPalmFilterStroke& stroke = stroke_it->second;
    if (end_of_stroke) {
      ended_tracking_ids.push_back(tracking_id);
      continue;
    }

    // Add the sample to the stroke.
    stroke.ProcessSample(CreateHeatmapPalmFilterSample(touch, time));

    // This is a stroke that has had a decision, so we add it to the list.
    if (HasDecidedStroke(stroke, *model_config_.get())) {
      tracking_ids_decided_.insert(tracking_id);
    }
  }

  for (const int tracking_id : ended_tracking_ids) {
    tracking_ids_decided_.erase(tracking_id);
  }
}

// Do not listen to the model if the tracking id has had a decision.
bool HeatmapPalmDetectionFilter::ShouldRunModel(const int tracking_id) const {
  return !tracking_ids_decided_.contains(tracking_id);
}

bool HeatmapPalmDetectionFilter::CompatibleWithHeatmapPalmDetectionFilter(
    const EventDeviceInfo& devinfo) {
  return TouchEventConverterEvdev::GetHidrawModelId(devinfo) !=
         HeatmapPalmDetector::ModelId::kNotSupported;
}

std::string HeatmapPalmDetectionFilter::FilterNameForTesting() const {
  return kFilterName;
}

void HeatmapPalmDetectionFilter::EraseOldStrokes(base::TimeTicks time) {
  const base::TimeDelta max_age = model_config_->max_dead_neighbor_time;
  for (auto it = strokes_.begin(); it != strokes_.end();) {
    if (it->second.samples().empty()) {
      DVLOG(1) << "Stroke is empty, continue.";
      continue;
    }
    const base::TimeTicks most_recent_sample_time =
        it->second.samples().back().time;
    const auto age = time - most_recent_sample_time;
    if (age > max_age) {
      it = strokes_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace ui
