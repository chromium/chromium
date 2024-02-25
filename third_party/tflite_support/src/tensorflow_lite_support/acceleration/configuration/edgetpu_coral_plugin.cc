/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <memory>
#include <unordered_map>

#include <glog/logging.h>
#include "absl/container/node_hash_map.h"  // from @com_google_absl
#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/strings/match.h"  // from @com_google_absl
#include "absl/strings/numbers.h"  // from @com_google_absl
#include "tflite/public/edgetpu_c.h"
#include "tensorflow/lite/acceleration/configuration/configuration_generated.h"
#include "tensorflow/lite/acceleration/configuration/delegate_registry.h"

namespace tflite {
namespace delegates {
namespace {

constexpr int kDEFAULT_USB_MAX_BULK_IN_QUEUE_LENGTH = 32;
constexpr char kUsb[] = "usb";
constexpr char kPci[] = "pci";

inline std::string ConvertPerformance(
    const CoralSettings_::Performance& from_performance) {
  switch (from_performance) {
    case CoralSettings_::Performance_LOW:
      return "Low";
    case CoralSettings_::Performance_MEDIUM:
      return "Medium";
    case CoralSettings_::Performance_HIGH:
      return "High";
    default:
      return "Max";
  }
}

inline std::string ConvertBool(bool from_bool) {
  return from_bool ? "True" : "False";
}

bool MatchDevice(const std::string& device, const std::string& type,
                 int* index) {
  const auto prefix(type + ":");
  if (!absl::StartsWith(device, prefix)) return false;
  if (!absl::SimpleAtoi(device.substr(prefix.size()), index)) return false;
  if (*index < 0) return false;
  return true;
}

// device_index corresponds to specific device type, e.g. "usb:0" means the
// first USB device or "pci:0" means the first PCIe device.
TfLiteDelegate* CreateEdgeTpuDelegate(
    absl::optional<edgetpu_device_type> device_type,
    absl::optional<int> device_index,
    const absl::node_hash_map<std::string, std::string>& device_options) {
  std::vector<edgetpu_option> options(device_options.size());
  size_t i = 0;
  for (auto& device_option : device_options) {
    options[i++] = {device_option.first.c_str(), device_option.second.c_str()};
  }

  size_t num_devices;
  std::unique_ptr<edgetpu_device, decltype(&edgetpu_free_devices)> devices(
      edgetpu_list_devices(&num_devices), &edgetpu_free_devices);

  if (!device_index.has_value()) {
    return CreateEdgeTpuDelegate(device_type, 0, device_options);
  } else {
    const int index = device_index.value();
    if (device_type.has_value()) {
      int type_index = 0;
      for (size_t i = 0; i < num_devices; i++) {
        const auto& device = devices.get()[i];
        if (device.type == device_type.value() && type_index++ == index)
          return edgetpu_create_delegate(device.type, device.path,
                                         options.data(), options.size());
      }
    } else {
      if (index < num_devices)
        return edgetpu_create_delegate(devices.get()[index].type,
                                       devices.get()[index].path,
                                       options.data(), options.size());
    }
    return nullptr;
  }
}

TfLiteDelegate* CreateEdgeTpuDelegate(
    const std::string& device,
    const absl::node_hash_map<std::string, std::string>& options) {
  if (device.empty()) {
    return CreateEdgeTpuDelegate(absl::nullopt, absl::nullopt, options);
  } else if (device == kUsb) {
    return CreateEdgeTpuDelegate(EDGETPU_APEX_USB, absl::nullopt, options);
  } else if (device == kPci) {
    return CreateEdgeTpuDelegate(EDGETPU_APEX_PCI, absl::nullopt, options);
  } else {
    int index;
    if (MatchDevice(device, "", &index)) {
      return CreateEdgeTpuDelegate(absl::nullopt, index, options);
    } else if (MatchDevice(device, kUsb, &index)) {
      return CreateEdgeTpuDelegate(EDGETPU_APEX_USB, index, options);
    } else if (MatchDevice(device, kPci, &index)) {
      return CreateEdgeTpuDelegate(EDGETPU_APEX_PCI, index, options);
    } else {
      LOG(ERROR) << "Cannot match the given device string (" << device
                 << ") with a Coral device.";
      return nullptr;
    }
  }
}

class EdgeTpuCoralPlugin : public DelegatePluginInterface {
 public:
  TfLiteDelegatePtr Create() override {
    return TfLiteDelegatePtr(CreateEdgeTpuDelegate(device_, options_),
                             edgetpu_free_delegate);
  }

  int GetDelegateErrno(TfLiteDelegate* from_delegate) override { return 0; }

  static std::unique_ptr<DelegatePluginInterface> New(
      const TFLiteSettings& acceleration) {
    return absl::make_unique<EdgeTpuCoralPlugin>(acceleration);
  }

  explicit EdgeTpuCoralPlugin(const TFLiteSettings& tflite_settings) {
    const auto* coral_settings = tflite_settings.coral_settings();
    if (!coral_settings) {
      return;
    }

    device_ = coral_settings->device()->str();
    options_.insert(
        {"Performance", ConvertPerformance(coral_settings->performance())});
    options_.insert(
        {"Usb.AlwaysDfu", ConvertBool(coral_settings->usb_always_dfu())});
    options_.insert(
        {"Usb.MaxBulkInQueueLength",
         std::to_string(coral_settings->usb_max_bulk_in_queue_length() == 0
                            ? kDEFAULT_USB_MAX_BULK_IN_QUEUE_LENGTH
                            : coral_settings->usb_max_bulk_in_queue_length())});
  }

 private:
  std::string device_;
  absl::node_hash_map<std::string, std::string> options_;
};
}  // namespace

TFLITE_REGISTER_DELEGATE_FACTORY_FUNCTION(EdgeTpuCoralPlugin,
                                          EdgeTpuCoralPlugin::New);
}  // namespace delegates
}  // namespace tflite
