// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/touch_device_manager.h"

#include <set>
#include <string>
#include <tuple>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace display {

namespace {

using ManagedDisplayInfoList = std::vector<ManagedDisplayInfo*>;
using DeviceList = std::vector<ui::TouchscreenDevice>;

constexpr char kFallbackTouchDeviceName[] = "fallback_touch_device_name";
constexpr char kFallbackTouchDevicePhys[] = "fallback_touch_device_phys";

// Returns true if |path| is likely a USB device.
bool IsDeviceConnectedViaUsb(const base::FilePath& path) {
  for (const auto& component : path.GetComponents()) {
    if (base::StartsWith(component, "usb",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }

    // TODO(malaykeshav): When evdi starts registering with the usb subsystem
    // in the kernel, this would no longer be needed. All evdi displays are USB
    // right now. This might change in the future however.
    // See https://crbug.com/923165 for more info.
    if (base::StartsWith(component, "evdi", base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

// Returns the USB association score between |display| and |device|. A score <=
// 0 means that there is no association.
int GetUsbAssociationScore(const ManagedDisplayInfo* display,
                           const ui::TouchscreenDevice& device) {
  // If the devices are not both connected via USB, then there cannot be a USB
  // association score.
  if (!IsDeviceConnectedViaUsb(display->sys_path()) ||
      !IsDeviceConnectedViaUsb(device.sys_path))
    return 0;

  // The association score is simply the number of prefix path components that
  // sysfs paths have in common.
  std::vector<base::FilePath::StringType> display_components =
      display->sys_path().GetComponents();
  std::vector<base::FilePath::StringType> device_components =
      device.sys_path.GetComponents();

  std::size_t largest_idx = 0;
  while (largest_idx < display_components.size() &&
         largest_idx < device_components.size() &&
         display_components[largest_idx] == device_components[largest_idx]) {
    ++largest_idx;
  }
  return largest_idx;
}

// Tries to find a USB device that best matches |display|. Returns
// |devices.end()| if one is not found.
DeviceList::const_iterator GuessBestUsbDevice(const ManagedDisplayInfo* display,
                                              const DeviceList& devices) {
  int best_score = 0;
  DeviceList::const_iterator best_device_it = devices.end();

  // TODO(malaykeshav): Migrate to std::max_element in the future.
  for (auto it = devices.begin(); it != devices.end(); it++) {
    int score = GetUsbAssociationScore(display, *it);
    if (score > best_score) {
      best_score = score;
      best_device_it = it;
    }
  }
  return best_device_it;
}

// Returns true if |display| is internal.
bool IsInternalDisplay(const ManagedDisplayInfo* display) {
  return IsInternalDisplayId(display->id());
}

// Returns true if |device| is internal.
bool IsInternalDevice(const ui::TouchscreenDevice& device) {
  return device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
}

// Returns a pointer to the internal display from the list of |displays|. Will
// return null if there is no internal display in the list.
ManagedDisplayInfo* GetInternalDisplay(ManagedDisplayInfoList* displays) {
  auto it = base::ranges::find_if(*displays, &IsInternalDisplay);
  return it == displays->end() ? nullptr : *it;
}

// Clears any calibration data from |info_map| for the display identified by
// |display_id|.
void ClearCalibrationDataInMap(TouchDeviceManager::AssociationInfoMap& info_map,
                               int64_t display_id) {
  if (info_map.find(display_id) == info_map.end())
    return;
  info_map[display_id].calibration_data = TouchCalibrationData();
}

// Returns a pointer to the ManagedDisplayInfo of the display that the device
// identified by |identifier| should be associated with. Returns |nullptr| if
// no match is found. |displays| should be the list of active displays.
ManagedDisplayInfo* GetBestMatchForDevice(
    const TouchDeviceManager::TouchAssociationMap& touch_associations,
    const TouchDeviceIdentifier& identifier,
    ManagedDisplayInfoList* displays) {
  ManagedDisplayInfo* display_info = nullptr;
  base::Time most_recent_timestamp;

  // If we have no historical information for the touch device identified by
  // |identifier|, do an early return.
  if (!base::Contains(touch_associations, identifier))
    return display_info;

  const TouchDeviceManager::AssociationInfoMap& info_map =
      touch_associations.at(identifier);
  // Iterate over each active display to see which one was most recently
  // associated with the touch device identified by |identifier|.
  for (auto* display : *displays) {
    // We do not want to match anything to the internal display.
    if (IsInternalDisplayId(display->id()))
      continue;
    if (!base::Contains(info_map, display->id()))
      continue;
    const TouchDeviceManager::TouchAssociationInfo& info =
        info_map.at(display->id());
    if (info.timestamp > most_recent_timestamp) {
      display_info = display;
      most_recent_timestamp = info.timestamp;
    }
  }
  return display_info;
}

// Returns a set of TouchDeviceIdentifiers (sans their port information) that
// are associated with more than 1 touch device from the list |devices|.
std::set<TouchDeviceIdentifier, TouchDeviceIdentifier::WeakComp>
GetCollisionSet(const DeviceList& devices) {
  std::set<TouchDeviceIdentifier, TouchDeviceIdentifier::WeakComp>
      collision_set;
  std::set<TouchDeviceIdentifier, TouchDeviceIdentifier::WeakComp> ids;
  for (const ui::TouchscreenDevice& device : devices) {
    TouchDeviceIdentifier id = TouchDeviceIdentifier::FromDevice(device);
    if (ids.find(id) != ids.end())
      collision_set.insert(id);
    else
      ids.insert(id);
  }
  return collision_set;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceIdentifier

// static
const TouchDeviceIdentifier&
TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier() {
  static const TouchDeviceIdentifier kFallTouchDeviceIdentifier(
      GenerateIdentifier(kFallbackTouchDeviceName, 0, 0),
      base::PersistentHash(kFallbackTouchDevicePhys));
  return kFallTouchDeviceIdentifier;
}

// static
uint32_t TouchDeviceIdentifier::GenerateIdentifier(std::string name,
                                                   uint16_t vendor_id,
                                                   uint16_t product_id) {
  std::string hash_str = name + "-" + base::NumberToString(vendor_id) + "-" +
                         base::NumberToString(product_id);
  return base::PersistentHash(hash_str);
}

// static
TouchDeviceIdentifier TouchDeviceIdentifier::FromDevice(
    const ui::TouchscreenDevice& touch_device) {
  if (!touch_device.id)
    return GetFallbackTouchDeviceIdentifier();
  return TouchDeviceIdentifier(
      GenerateIdentifier(touch_device.name, touch_device.vendor_id,
                         touch_device.product_id),
      base::PersistentHash(touch_device.phys));
}

TouchDeviceIdentifier::TouchDeviceIdentifier(uint32_t identifier)
    : id_(identifier),
      secondary_id_(base::PersistentHash(kFallbackTouchDevicePhys)) {}

TouchDeviceIdentifier::TouchDeviceIdentifier(uint32_t identifier,
                                             uint32_t secondary_id)
    : id_(identifier), secondary_id_(secondary_id) {}

TouchDeviceIdentifier::TouchDeviceIdentifier(const TouchDeviceIdentifier& other)
    : id_(other.id_), secondary_id_(other.secondary_id_) {}

TouchDeviceIdentifier& TouchDeviceIdentifier::operator=(
    TouchDeviceIdentifier other) {
  id_ = other.id_;
  secondary_id_ = other.secondary_id_;
  return *this;
}

bool TouchDeviceIdentifier::operator<(const TouchDeviceIdentifier& rhs) const {
  return std::tie(id_, secondary_id_) < std::tie(rhs.id_, rhs.secondary_id_);
}

bool TouchDeviceIdentifier::operator==(const TouchDeviceIdentifier& rhs) const {
  return id_ == rhs.id_ && secondary_id_ == rhs.secondary_id_;
}

bool TouchDeviceIdentifier::operator!=(const TouchDeviceIdentifier& rhs) const {
  return !(*this == rhs);
}

std::string TouchDeviceIdentifier::ToString() const {
  return base::NumberToString(id_);
}

std::string TouchDeviceIdentifier::SecondaryIdToString() const {
  return base::NumberToString(secondary_id_);
}

////////////////////////////////////////////////////////////////////////////////
// TouchCalibrationData

// static
bool TouchCalibrationData::CalibrationPointPairCompare(
    const CalibrationPointPair& pair_1,
    const CalibrationPointPair& pair_2) {
  return pair_1.first < pair_2.first;
}

TouchCalibrationData::TouchCalibrationData() = default;

TouchCalibrationData::TouchCalibrationData(
    const TouchCalibrationData::CalibrationPointPairQuad& point_pairs,
    const gfx::Size& bounds)
    : point_pairs(point_pairs), bounds(bounds) {}

TouchCalibrationData::TouchCalibrationData(
    const TouchCalibrationData& calibration_data) = default;

TouchCalibrationData& TouchCalibrationData::operator=(
    const TouchCalibrationData& calibration_data) = default;

bool TouchCalibrationData::operator==(const TouchCalibrationData& other) const {
  if (bounds != other.bounds)
    return false;
  CalibrationPointPairQuad quad_1 = point_pairs;
  CalibrationPointPairQuad quad_2 = other.point_pairs;

  // Make sure the point pairs are in the correct order.
  std::sort(quad_1.begin(), quad_1.end(), CalibrationPointPairCompare);
  std::sort(quad_2.begin(), quad_2.end(), CalibrationPointPairCompare);

  return quad_1 == quad_2;
}

bool TouchCalibrationData::IsEmpty() const {
  return bounds.IsEmpty();
}

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceManager
TouchDeviceManager::TouchDeviceManager() {}

TouchDeviceManager::~TouchDeviceManager() {}

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceManager
// Touch screen association logic
void TouchDeviceManager::AssociateTouchscreens(
    std::vector<ManagedDisplayInfo>* all_displays,
    const std::vector<ui::TouchscreenDevice>& all_devices) {
  active_touch_associations_.clear();
  // |displays| and |devices| contain pointers directly to the values stored
  // inside of |all_displays| and |all_devices|. When a display or input device
  // has been associated, it is removed from the |displays| or |devices| list.

  // Construct our initial set of display/devices that we will process.
  ManagedDisplayInfoList displays;
  for (ManagedDisplayInfo& display : *all_displays) {
    // Reset touch support from the display.
    display.set_touch_support(Display::TouchSupport::UNAVAILABLE);
    displays.push_back(&display);
  }

  // Construct initial set of devices.
  DeviceList devices;
  for (const ui::TouchscreenDevice& device : all_devices)
    devices.push_back(device);

  if (VLOG_IS_ON(2)) {
    for (const ManagedDisplayInfo* display : displays) {
      VLOG(2) << "Received display " << display->name()
              << " (size: " << display->GetNativeModeSize().ToString() << ", "
              << "sys_path: " << display->sys_path().LossyDisplayName() << ")";
    }
    for (const ui::TouchscreenDevice& device : devices) {
      VLOG(2) << "Received device " << device.name
              << " (size: " << device.size.ToString()
              << ", sys_path: " << device.sys_path.LossyDisplayName() << ")";
    }
  }

  AssociateInternalDevices(&displays, &devices);
  AssociateDevicesWithCollision(&displays, &devices);
  AssociateFromHistoricalData(&displays, &devices);
  AssociateUsbDevices(&displays, &devices);
  AssociateSameSizeDevices(&displays, &devices);
  AssociateToSingleDisplay(&displays, &devices);
  AssociateAnyRemainingDevices(&displays, &devices);

  for (const ui::TouchscreenDevice& device : devices)
    LOG(WARNING) << "Unmatched device " << device.name;
}

void TouchDeviceManager::AssociateInternalDevices(
    ManagedDisplayInfoList* displays,
    DeviceList* devices) {
  VLOG(2) << "Trying to match internal devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  // Internal device association has a couple of gotchas:
  // - There can be internal devices but no internal display, or visa-versa.
  // - There can be multiple internal devices matching one internal display. We
  //   assume there is at most one internal display.

  // Capture the internal display reference as we remove it from |displays|.
  ManagedDisplayInfo* internal_display = GetInternalDisplay(displays);

  bool matched = false;

  // Remove all internal devices from |devices|. If we have an internal display,
  // then associate the device with the display before removing it.
  for (auto device_it = devices->begin(); device_it != devices->end();) {
    const ui::TouchscreenDevice& internal_device = *device_it;

    // Not an internal device, skip it.
    if (!IsInternalDevice(internal_device)) {
      ++device_it;
      continue;
    }

    if (internal_display) {
      VLOG(2) << "=> Matched device " << internal_device.name << " to display "
              << internal_display->name();
      Associate(internal_display, internal_device);
      matched = true;
    } else {
      // We do not want to associate an internal device to any other display.
      VLOG(2) << "=> Removing internal device " << internal_device.name;
    }
    device_it = devices->erase(device_it);
  }

  if (!matched && internal_display) {
    VLOG(2) << "=> No device found to match with internal display "
            << internal_display->name();
  }
}

void TouchDeviceManager::AssociateDevicesWithCollision(
    ManagedDisplayInfoList* displays,
    DeviceList* devices) {
  if (!devices->size() || !displays->size())
    return;

  // Get a list of touch devices that have the same primary ids but connected
  // via different interfaces.
  std::set<TouchDeviceIdentifier, TouchDeviceIdentifier::WeakComp>
      collision_set = GetCollisionSet(*devices);
  if (collision_set.empty())
    return;

  VLOG(2) << "Trying to match " << devices->size() << " devices "
          << "and " << displays->size() << " displays where there is/are "
          << collision_set.size() << " collisions with the touch device ids";

  for (auto device_it = devices->begin(); device_it != devices->end();) {
    const auto identifier = TouchDeviceIdentifier::FromDevice(*device_it);
    // If this device is not the one that has a collision or if this device is
    // the one that has collision but we have no past port mapping information
    // associated with it, then we skip.
    if (!base::Contains(collision_set, identifier) ||
        !base::Contains(port_associations_, identifier)) {
      device_it++;
      continue;
    }

    int64_t display_id = port_associations_.at(identifier);

    // Find the display associated with |display_id| from |displays|.
    ManagedDisplayInfoList::iterator display_it =
        base::ranges::find(*displays, display_id, &ManagedDisplayInfo::id);

    if (display_it != displays->end()) {
      VLOG(2) << "=> Matched device " << (*device_it).name << " to display "
              << (*display_it)->name();
      Associate(*display_it, *device_it);
      device_it = devices->erase(device_it);
    } else {
      device_it++;
    }
  }
}

void TouchDeviceManager::AssociateFromHistoricalData(
    ManagedDisplayInfoList* displays,
    DeviceList* devices) {
  if (!devices->size() || !displays->size())
    return;

  VLOG(2) << "Trying to match " << devices->size() << " devices "
          << "and " << displays->size() << " displays based on historical "
          << "preferences.";

  for (auto device_it = devices->begin(); device_it != devices->end();) {
    auto* matched_display_info = GetBestMatchForDevice(
        touch_associations_, TouchDeviceIdentifier::FromDevice(*device_it),
        displays);
    if (matched_display_info) {
      VLOG(2) << "=> Matched device " << (*device_it).name << " to display "
              << matched_display_info->name();
      Associate(matched_display_info, *device_it);
      device_it = devices->erase(device_it);
    } else {
      device_it++;
    }
  }
}

void TouchDeviceManager::AssociateUsbDevices(ManagedDisplayInfoList* displays,
                                             DeviceList* devices) {
  VLOG(2) << "Trying to match usb devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  for (auto display_it = displays->begin(); display_it != displays->end();
       display_it++) {
    ManagedDisplayInfo* display = *display_it;
    auto device_it = GuessBestUsbDevice(display, *devices);

    if (device_it != devices->end()) {
      const ui::TouchscreenDevice& device = *device_it;
      VLOG(2) << "=> Matched device " << device.name << " to display "
              << display->name()
              << " (score=" << GetUsbAssociationScore(display, device) << ")";
      Associate(display, device);
      devices->erase(device_it);
    }
  }
}

void TouchDeviceManager::AssociateSameSizeDevices(
    ManagedDisplayInfoList* displays,
    DeviceList* devices) {
  // Associate screens/displays with the same size.
  VLOG(2) << "Trying to match same-size devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  for (auto display_it = displays->begin(); display_it != displays->end();
       display_it++) {
    ManagedDisplayInfo* display = *display_it;
    // We do not want to associate any other devices to the internal display.
    if (IsInternalDisplay(display))
      continue;

    const gfx::Size native_size = display->GetNativeModeSize();

    // Try to find an input device with roughly the same size as the display.
    DeviceList::iterator device_it = base::ranges::find_if(
        *devices, [&native_size](const ui::TouchscreenDevice& device) {
          // Allow 1 pixel difference between screen and touchscreen
          // resolutions. Because in some cases for monitor resolution
          // 1024x768 touchscreen's resolution would be 1024x768, but for
          // some 1023x767. It really depends on touchscreen's firmware
          // configuration.
          return std::abs(native_size.width() - device.size.width()) <= 1 &&
                 std::abs(native_size.height() - device.size.height()) <= 1;
        });

    if (device_it != devices->end()) {
      const ui::TouchscreenDevice& device = *device_it;
      VLOG(2) << "=> Matched device " << device.name << " to display "
              << display->name() << " (display_size: " << native_size.ToString()
              << ", device_size: " << device.size.ToString() << ")";
      Associate(display, device);

      device_it = devices->erase(device_it);
    }
  }
}

void TouchDeviceManager::AssociateToSingleDisplay(
    ManagedDisplayInfoList* displays,
    DeviceList* devices) {
  // If there is only one display left, then we should associate all input
  // devices with it.
  VLOG(2) << "Trying to match to single display (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  std::size_t num_displays_excluding_internal = displays->size();
  ManagedDisplayInfo* internal_display = GetInternalDisplay(displays);
  if (internal_display)
    num_displays_excluding_internal--;

  // We only associate to one display.
  if (num_displays_excluding_internal != 1 || devices->empty())
    return;

  // Pick the non internal display.
  ManagedDisplayInfo* display = *displays->begin();
  if (display == internal_display)
    display = (*displays)[1];

  for (const ui::TouchscreenDevice& device : *devices) {
    VLOG(2) << "=> Matched device " << device.name << " to display "
            << display->name();
    Associate(display, device);
  }
  devices->clear();
}

void TouchDeviceManager::AssociateAnyRemainingDevices(
    ManagedDisplayInfoList* displays,
    DeviceList* devices) {
  if (!displays->size() || !devices->size())
    return;
  VLOG(2) << "Trying to match remaining " << devices->size()
          << " devices to a display.";

  // Try to match all devices to the internal display.
  ManagedDisplayInfo* display = GetInternalDisplay(displays);
  if (!display) {
    // If no internal displays were found, then associate the devices to any of
    // the other displays.
    display = *(displays->begin());

    VLOG(2) << "Could not find any internal display. Matching all devices to a "
            << "random non internal display.";
  }
  VLOG(2) << "Matching " << devices->size() << " touch devices to "
          << display->name() << "[" << display->id() << "]";

  // device_it is iterated within the loop.
  for (auto device_it = devices->begin(); device_it != devices->end();) {
    // We do not want to associate an internal touch device with anything other
    // than internal display.
    if ((*device_it).type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      device_it++;
      continue;
    }

    VLOG(2) << "=> Matched " << (*device_it).name << " to " << display->name();
    Associate(display, *device_it);
    device_it = devices->erase(device_it);
  }
}

void TouchDeviceManager::Associate(ManagedDisplayInfo* display,
                                   const ui::TouchscreenDevice& device) {
  display->set_touch_support(Display::TouchSupport::AVAILABLE);
  active_touch_associations_[TouchDeviceIdentifier::FromDevice(device)] =
      display->id();
}

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceManager
// Managing Touch device calibration data

void TouchDeviceManager::AddTouchCalibrationData(
    const ui::TouchscreenDevice& device,
    int64_t display_id,
    const TouchCalibrationData& data) {
  AddTouchCalibrationDataImpl(device, display_id, &data);
}

void TouchDeviceManager::AddTouchAssociation(
    const ui::TouchscreenDevice& device,
    int64_t display_id) {
  AddTouchCalibrationDataImpl(device, display_id, /*data=*/nullptr);
}

void TouchDeviceManager::AddTouchCalibrationDataImpl(
    const ui::TouchscreenDevice& device,
    int64_t display_id,
    const TouchCalibrationData* data) {
  const TouchDeviceIdentifier identifier =
      TouchDeviceIdentifier::FromDevice(device);

  // Update the current touch association and associate the display identified
  // by |display_id| to the touch device identified by |identifier|.
  active_touch_associations_[identifier] = display_id;

  auto& association_info_map = touch_associations_[identifier];
  auto it = association_info_map.find(display_id);
  if (it != association_info_map.end()) {
    if (data) {
      // Update the timestamp and calibration data if information about the
      // display identified by |display_id| already exists for the touch device
      // identified by |identifier|.
      it->second.calibration_data = *data;
      it->second.timestamp = base::Time::Now();
    }
  } else {
    // Add a new entry for the display identified by |display_id| in the map
    // of associations for the touch device identified by |identifier|.
    TouchAssociationInfo info;
    info.timestamp = base::Time::Now();
    if (data) {
      info.calibration_data = *data;
    }
    association_info_map.emplace(display_id, info);
  }

  // Store the port association information, i.e. the touch device identified by
  // |identifier| when connected to port |identifier.secondary_id()| was
  // associated with display identified by |display_id|.
  port_associations_[identifier] = display_id;
}

void TouchDeviceManager::ClearTouchCalibrationData(
    const ui::TouchscreenDevice& device,
    int64_t display_id) {
  const TouchDeviceIdentifier identifier =
      TouchDeviceIdentifier::FromDevice(device);
  if (base::Contains(touch_associations_, identifier)) {
    ClearCalibrationDataInMap(touch_associations_.at(identifier), display_id);
  }
}

void TouchDeviceManager::ClearAllTouchCalibrationData(int64_t display_id) {
  for (auto it : touch_associations_) {
    // Erase all calibration data from the persistent storage associated with
    // the display identified by |display_id|.
    ClearCalibrationDataInMap(it.second, display_id);
  }
}

TouchCalibrationData TouchDeviceManager::GetCalibrationData(
    const ui::TouchscreenDevice& touchscreen,
    int64_t display_id) const {
  TouchDeviceIdentifier identifier =
      TouchDeviceIdentifier::FromDevice(touchscreen);
  if (display_id == kInvalidDisplayId) {
    // If the touch device is currently not associated with any display and the
    // |display_id| was not provided, then this is an invalid query.
    if (!base::Contains(active_touch_associations_, identifier))
      return TouchCalibrationData();

    // If the display id is not provided, we return the calibration information
    // for the touch device |touchscreen| and the display it is actively
    // associated with.
    display_id = active_touch_associations_.at(identifier);
  }

  if (base::Contains(touch_associations_, identifier)) {
    const AssociationInfoMap& info_map = touch_associations_.at(identifier);
    if (info_map.find(display_id) != info_map.end())
      return info_map.at(display_id).calibration_data;
  }

  // Check for legacy calibration data.
  TouchDeviceIdentifier fallback_identifier(
      TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier());
  if (base::Contains(touch_associations_, fallback_identifier)) {
    const AssociationInfoMap& info_map =
        touch_associations_.at(fallback_identifier);
    if (info_map.find(display_id) != info_map.end())
      return info_map.at(display_id).calibration_data;
  }

  // Return an empty calibration data if none was found.
  return TouchCalibrationData();
}

bool TouchDeviceManager::DisplayHasTouchDevice(
    int64_t display_id,
    const ui::TouchscreenDevice& device) const {
  const TouchDeviceIdentifier identifier =
      TouchDeviceIdentifier::FromDevice(device);
  return base::Contains(active_touch_associations_, identifier) &&
         active_touch_associations_.at(identifier) == display_id;
}

int64_t TouchDeviceManager::GetAssociatedDisplay(
    const ui::TouchscreenDevice& device) const {
  const TouchDeviceIdentifier identifier =
      TouchDeviceIdentifier::FromDevice(device);
  if (base::Contains(active_touch_associations_, identifier))
    return active_touch_associations_.at(identifier);
  return kInvalidDisplayId;
}

std::vector<ui::TouchscreenDevice>
TouchDeviceManager::GetAssociatedTouchDevicesForDisplay(
    int64_t display_id) const {
  std::vector<ui::TouchscreenDevice> result;
  for (const auto& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    const TouchDeviceIdentifier identifier =
        TouchDeviceIdentifier::FromDevice(device);

    const auto it = active_touch_associations_.find(identifier);
    if (it != active_touch_associations_.end() && it->second == display_id)
      result.push_back(device);
  }
  return result;
}

void TouchDeviceManager::RegisterTouchAssociations(
    const TouchAssociationMap& touch_associations,
    const PortAssociationMap& port_associations) {
  touch_associations_ = touch_associations;
  port_associations_ = port_associations;
}

std::ostream& operator<<(std::ostream& os,
                         const TouchDeviceIdentifier& identifier) {
  return os << identifier.ToString() << " [" << identifier.SecondaryIdToString()
            << "]";
}

bool HasExternalTouchscreenDevice() {
  for (const auto& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.type == ui::InputDeviceType::INPUT_DEVICE_USB ||
        device.type == ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH) {
      return true;
    }
  }
  return false;
}

}  // namespace display
