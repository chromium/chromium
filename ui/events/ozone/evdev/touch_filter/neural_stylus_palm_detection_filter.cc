// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"
#include "ui/events/ozone/features.h"

namespace ui {
namespace {
// Returns the Euclidean distance between two points.
float EuclideanDistance(const gfx::PointF& a, const gfx::PointF& b) {
  return (a - b).Length();
}

}  // namespace

NeuralStylusPalmDetectionFilter::NeuralStylusPalmDetectionFilter(
    const EventDeviceInfo& devinfo,
    std::unique_ptr<NeuralStylusPalmDetectionFilterModel> palm_model,
    SharedPalmDetectionFilterState* shared_palm_state)
    : PalmDetectionFilter(shared_palm_state),
      tracking_ids_count_within_session_(0),
      palm_filter_dev_info_(CreatePalmFilterDeviceInfo(devinfo)),
      model_(std::move(palm_model)) {
  DCHECK(CompatibleWithNeuralStylusPalmDetectionFilter(devinfo))
      << "One should run compatible check before instantiation.";
}

NeuralStylusPalmDetectionFilter::~NeuralStylusPalmDetectionFilter() {}

void NeuralStylusPalmDetectionFilter::FindBiggestNeighborsWithin(
    int neighbor_count,
    unsigned long min_sample_count,
    float max_distance,
    const PalmFilterStroke& stroke,
    std::vector<std::pair<float, int>>* biggest_strokes) const {
  if (neighbor_count <= 0) {
    return;
  }
  // Tuple of {size, distance, stroke_id.}
  std::priority_queue<std::tuple<float, float, int>> biggest_strokes_queue;
  for (const auto& lookup : strokes_) {
    const PalmFilterStroke& neighbor = lookup.second;
    if (neighbor.tracking_id() == stroke.tracking_id()) {
      continue;
    }
    if (neighbor.samples().size() < min_sample_count) {
      continue;
    }
    float distance =
        EuclideanDistance(neighbor.GetCentroid(), stroke.GetCentroid());
    if (distance > max_distance) {
      continue;
    }
    float size = neighbor.BiggestSize();
    biggest_strokes_queue.push(
        std::make_tuple(size, distance, neighbor.tracking_id()));
  }
  for (int i = 0; i < neighbor_count && !biggest_strokes_queue.empty(); ++i) {
    const auto big_stroke = biggest_strokes_queue.top();
    biggest_strokes_queue.pop();
    biggest_strokes->emplace_back(std::get<1>(big_stroke),
                                  std::get<2>(big_stroke));
  }
}

void NeuralStylusPalmDetectionFilter::FindNearestNeighborsWithin(
    int neighbor_count,
    float max_distance,
    const PalmFilterStroke& stroke,
    std::vector<std::pair<float, int>>* nearest_strokes) const {
  using StrokeId = int;
  using Distance = float;
  using DistanceWithStrokeId = std::pair<Distance, StrokeId>;
  std::priority_queue<DistanceWithStrokeId, std::vector<DistanceWithStrokeId>,
                      std::greater<DistanceWithStrokeId>>
      queue;
  if (neighbor_count <= 0) {
    return;
  }
  for (const auto& lookup : strokes_) {
    const PalmFilterStroke& neighbor = lookup.second;
    if (neighbor.tracking_id() == stroke.tracking_id()) {
      continue;
    }
    if (neighbor.samples().size() < model_->config().min_sample_count) {
      continue;
    }
    float distance =
        EuclideanDistance(neighbor.GetCentroid(), stroke.GetCentroid());
    if (distance < max_distance) {
      queue.push(std::make_pair(distance, neighbor.tracking_id()));
    }
  }
  for (int i = 0; i < neighbor_count && !queue.empty(); ++i) {
    nearest_strokes->push_back(queue.top());
    queue.pop();
  }
}

void NeuralStylusPalmDetectionFilter::Filter(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time,
    std::bitset<kNumTouchEvdevSlots>* slots_to_hold,
    std::bitset<kNumTouchEvdevSlots>* slots_to_suppress) {
  EraseOldStrokes(time);
  slots_to_hold->reset();
  slots_to_suppress->reset();
  std::unordered_set<int> slots_to_decide;
  std::vector<int> ended_tracking_ids;
  uint32_t total_finger_touching = 0;
  for (const auto& touch : touches) {
    if (touch.touching && touch.tool_code != BTN_TOOL_PEN) {
      total_finger_touching++;
      if (!touch.was_touching) {
        shared_palm_state_->latest_finger_touch_time = time;
      }
    }
    // Ignore touch events that are not touches.
    if (!touch.touching && !touch.was_touching) {
      continue;
    }
    int tracking_id = touch.tracking_id;
    const size_t slot = touch.slot;
    if (!touch.was_touching) {
      // New stroke, so add the new stroke to the stroke list.
      DCHECK_NE(tracking_id, -1);
      DCHECK(strokes_.count(tracking_id) == 0)
          << " Tracking id " << tracking_id;
      // Stroke new_stroke(model_->config().max_sample_count);  // TODO:save the
      // constant here.
      strokes_.emplace(std::make_pair(
          tracking_id, PalmFilterStroke(model_->config().max_sample_count)));
      strokes_.find(tracking_id)->second.SetTrackingId(tracking_id);
      tracking_ids_[slot] = tracking_id;
      is_palm_.set(slot, false);
      is_delay_.set(slot, false);
    }

    const bool end_of_stroke = tracking_id == -1;
    if (end_of_stroke) {
      // Recover the tracking ID.
      tracking_id = tracking_ids_[slot];
    }

    DCHECK_NE(tracking_id, -1);

    auto insert_result = active_tracking_ids_.insert(tracking_id);
    // New tracking_id.
    if (insert_result.second)
      tracking_ids_count_within_session_++;

    // Find the stroke in the stroke list.
    auto stroke_it = strokes_.find(tracking_id);

    if (stroke_it == strokes_.end()) {
      LOG(DFATAL) << "No stroke found, continue.";
      continue;
    }

    const NeuralStylusPalmDetectionFilterModelConfig& config = model_->config();

    PalmFilterStroke& stroke = stroke_it->second;
    if (end_of_stroke) {
      // This is a stroke that hasn't had a decision yet, so we force decide.
      if (stroke.samples().size() < config.max_sample_count) {
        slots_to_decide.insert(slot);
      }

      ended_tracking_ids.push_back(tracking_id);
      continue;
    }

    // Add the sample to the stroke.
    stroke.AddSample(CreatePalmFilterSample(touch, time, model_->config(),
                                            palm_filter_dev_info_));
    if (!is_palm_.test(slot) && ShouldDecideStroke(stroke)) {
      slots_to_decide.insert(slot);
    }

    if (config.heuristic_delay_start_if_palm && !end_of_stroke &&
        stroke.samples_seen() < config.max_sample_count &&
        IsHeuristicPalmStroke(stroke)) {
      //  A stroke that we _think_ may be a palm, but is too short to decide
      //  yet. So we mark for delay for now.
      is_delay_.set(slot, true);
    }
  }

  for (const int slot : slots_to_decide) {
    is_delay_.set(slot, false);
    is_palm_.set(slot, false);
    int tracking_id = tracking_ids_[slot];
    auto lookup = strokes_.find(tracking_id);
    if (lookup == strokes_.end()) {
      LOG(DFATAL) << "Unable to find marked stroke.";
      continue;
    }
    auto& stroke = lookup->second;
    if (stroke.samples_seen() < model_->config().min_sample_count) {
      // in very short strokes: we use a heuristic.
      is_palm_.set(slot, IsHeuristicPalmStroke(stroke));
      continue;
    }
    is_palm_.set(slot, DetectSpuriousStroke(ExtractFeatures(tracking_id),
                                            tracking_id, 0.0));
    if (is_palm_.test(slot)) {
      shared_palm_state_->latest_palm_touch_time = time;
    }
  }

  for (const int tracking_id : ended_tracking_ids) {
    active_tracking_ids_.erase(tracking_id);
  }

  *slots_to_suppress |= is_palm_;
  *slots_to_hold |= is_delay_;

  shared_palm_state_->active_palm_touches = is_palm_.count();
  shared_palm_state_->active_finger_touches =
      total_finger_touching - is_palm_.count();
}

bool NeuralStylusPalmDetectionFilter::ShouldDecideStroke(
    const PalmFilterStroke& stroke) const {
  const NeuralStylusPalmDetectionFilterModelConfig& config = model_->config();
  // Perform inference at most every |max_sample_count| samples.
  if (stroke.samples_seen() % config.max_sample_count != 0)
    return false;

  // Only inference at start.
  if (stroke.samples_seen() >
      config.max_sequence_start_count_for_inference + config.max_sample_count)
    return false;
  return true;
}

bool NeuralStylusPalmDetectionFilter::IsHeuristicPalmStroke(
    const PalmFilterStroke& stroke) const {
  if (stroke.samples().size() >= model_->config().max_sample_count) {
    LOG(DFATAL) << "Should not call this method on long strokes.";
    return false;
  }
  const auto& config = model_->config();
  if (config.heuristic_palm_touch_limit > 0.0) {
    if (stroke.MaxMajorRadius() >= config.heuristic_palm_touch_limit) {
      return true;
    }
  }
  if (config.heuristic_palm_area_limit > 0.0) {
    if (stroke.BiggestSize() >= config.heuristic_palm_area_limit) {
      return true;
    }
    std::vector<std::pair<float, int>> biggest_strokes;
    FindBiggestNeighborsWithin(1 /* neighbors */, 1 /* min sample count */,
                               model_->config().max_neighbor_distance_in_mm,
                               stroke, &biggest_strokes);
    if (!biggest_strokes.empty() &&
        strokes_.find(biggest_strokes[0].second)->second.BiggestSize() >=
            config.heuristic_palm_area_limit) {
      return true;
    }
  }
  return false;
}

bool NeuralStylusPalmDetectionFilter::DetectSpuriousStroke(
    const std::vector<float>& features,
    int tracking_id,
    float threshold) const {
  return model_->Inference(features) >= threshold;
}

std::vector<float> NeuralStylusPalmDetectionFilter::ExtractFeatures(
    int tracking_id) const {
  std::vector<float> features;
  const PalmFilterStroke& stroke = strokes_.at(tracking_id);
  AppendFeatures(stroke, &features);
  const int features_per_stroke = features.size();
  std::vector<std::pair<float, int>> nearest_strokes, biggest_strokes;
  const NeuralStylusPalmDetectionFilterModelConfig& config = model_->config();
  FindNearestNeighborsWithin(config.nearest_neighbor_count,
                             config.max_neighbor_distance_in_mm, stroke,
                             &nearest_strokes);
  FindBiggestNeighborsWithin(
      config.biggest_near_neighbor_count,
      model_->config().min_sample_count /* min sample count */,
      config.max_neighbor_distance_in_mm, stroke, &biggest_strokes);
  for (uint32_t i = 0; i < config.nearest_neighbor_count; ++i) {
    if (i < nearest_strokes.size()) {
      const auto& nearest_stroke = nearest_strokes[i];
      AppendFeaturesAsNeighbor(strokes_.at(nearest_stroke.second),
                               nearest_stroke.first, &features);
    } else {
      features.resize(features.size() + features_per_stroke + 2, 0);
    }
  }

  for (uint32_t i = 0; i < config.biggest_near_neighbor_count; ++i) {
    if (i < biggest_strokes.size()) {
      const auto& biggest_stroke = biggest_strokes[i];
      AppendFeaturesAsNeighbor(strokes_.at(biggest_stroke.second),
                               biggest_stroke.first, &features);
    } else {
      features.resize(
          features.size() + features_per_stroke + kExtraFeaturesForNeighbor, 0);
    }
  }

  if (config.use_tracking_id_count) {
    features.push_back(tracking_ids_count_within_session_);
  }

  if (config.use_active_tracking_id_count) {
    features.push_back(active_tracking_ids_.size());
  }

  return features;
}

void NeuralStylusPalmDetectionFilter::AppendFeatures(
    const PalmFilterStroke& stroke,
    std::vector<float>* features) const {
  const int size = stroke.samples().size();
  for (int i = 0; i < size; ++i) {
    const auto& sample = stroke.samples()[i];
    features->push_back(sample.major_radius);
    features->push_back(sample.minor_radius <= 0.0 ? sample.major_radius
                                                   : sample.minor_radius);
    float distance;
    if (i == 0) {
      distance = 0;

    } else {
      distance = EuclideanDistance(stroke.samples()[i - 1].point, sample.point);
    }
    features->push_back(distance);
    features->push_back(sample.edge);
    features->push_back(1.0);  // existence.
  }
  const int padding = model_->config().max_sample_count - size;
  DCHECK_GE(padding, 0);

  for (int i = 0; i < padding * kFeaturesPerSample; ++i) {
    features->push_back(0.0);
  }
  // "fill proportion."
  features->push_back(static_cast<float>(size) /
                      model_->config().max_sample_count);
  features->push_back(EuclideanDistance(stroke.samples().front().point,
                                        stroke.samples().back().point));

  // Start sequence number. 0 is min.
  uint32_t samples_seen = stroke.samples_seen();
  if (samples_seen < model_->config().max_sample_count) {
    features->push_back(0);
  } else {
    features->push_back(samples_seen - model_->config().max_sample_count);
  }
}
void NeuralStylusPalmDetectionFilter::AppendFeaturesAsNeighbor(
    const PalmFilterStroke& stroke,
    float distance,
    std::vector<float>* features) const {
  features->push_back(1);  // existence.
  features->push_back(distance);
  AppendFeatures(stroke, features);
}

const int NeuralStylusPalmDetectionFilter::kExtraFeaturesForNeighbor = 2;
const int NeuralStylusPalmDetectionFilter::kFeaturesPerSample = 5;

const char NeuralStylusPalmDetectionFilter::kFilterName[] =
    "NeuralStylusPalmDetectionFilter";
std::string NeuralStylusPalmDetectionFilter::FilterNameForTesting() const {
  return kFilterName;
}

bool NeuralStylusPalmDetectionFilter::
    CompatibleWithNeuralStylusPalmDetectionFilter(
        const EventDeviceInfo& devinfo) {
  return NeuralStylusPalmDetectionFilter::
      CompatibleWithNeuralStylusPalmDetectionFilter(
          devinfo, base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                       kOzoneNNPalmSwitchName));
}

bool NeuralStylusPalmDetectionFilter::
    CompatibleWithNeuralStylusPalmDetectionFilter(
        const EventDeviceInfo& devinfo,
        const std::string& ozone_params_switch_string) {
  if (devinfo.HasStylus()) {
    return false;
  }
  // Though we like having abs_mt_touch_minor, we don't need it.
  auto code_check = [&devinfo](int code) {
    if (!devinfo.HasAbsEvent(code)) {
      return false;
    }
    const auto absinfo = devinfo.GetAbsInfoByCode(code);
    // Ensure minimum is 0, maximum is greater than the minimum.
    if (absinfo.minimum != 0 || absinfo.maximum <= absinfo.minimum) {
      return false;
    }
    return true;
  };

  static constexpr int kRequiredAbsMtCodes[] = {
      ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TOUCH_MAJOR};
  if (!std::all_of(std::begin(kRequiredAbsMtCodes),
                   std::end(kRequiredAbsMtCodes), code_check)) {
    return false;
  }

  // Optionally, we use touch_minor if it's around, so check it's good if it is
  // present.
  if (devinfo.HasAbsEvent(ABS_MT_TOUCH_MINOR) &&
      !code_check(ABS_MT_TOUCH_MINOR)) {
    return false;
  }
  // Only work with internal touchscreens.
  if (devinfo.device_type() != INPUT_DEVICE_INTERNAL) {
    return false;
  }

  // Check the switch string.

  base::Optional<base::Value> value =
      base::JSONReader::Read(ozone_params_switch_string);
  if (value != base::nullopt && !ozone_params_switch_string.empty()) {
    if (!value->is_dict()) {
      return false;
    }
    // If the key isn't set, default to false.
    if (value->FindKey(kOzoneNNPalmTouchCompatibleProperty) == nullptr) {
      return false;
    }
    std::string* touch_string_val =
        value->FindStringKey(kOzoneNNPalmTouchCompatibleProperty);
    if (touch_string_val != nullptr) {
      if (*touch_string_val == "false") {
        return false;
      } else if (*touch_string_val == "true") {
        return true;
      } else {
        LOG(DFATAL) << "Unexpected value for nnpalm touch compatible. expected "
                       "\"true\" or \"false\" . Got: "
                    << *touch_string_val;
      }
    }
  }
  return true;
}

void NeuralStylusPalmDetectionFilter::EraseOldStrokes(base::TimeTicks time) {
  const base::TimeDelta max_age = model_->config().max_dead_neighbor_time;
  for (auto it = strokes_.begin(); it != strokes_.end();) {
    DCHECK(!it->second.samples().empty());
    const base::TimeTicks most_recent_sample_time =
        it->second.samples().back().time;
    const auto age = time - most_recent_sample_time;
    if (age > max_age) {
      it = strokes_.erase(it);
    } else {
      ++it;
    }
  }

  // If the blank time is more than max_blank_time, starts a new session.
  if (time - previous_report_time_ > model_->config().max_blank_time) {
    tracking_ids_count_within_session_ = 0;
    active_tracking_ids_.clear();
  }
  previous_report_time_ = time;
}
}  // namespace ui
