// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"

#include <fnmatch.h>
#include <gestures/gestures.h>
#include <libevdev/libevdev.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <unordered_map>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_feedback.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"

// GesturesProp implementation.
//
// Check the header file for its definition.
GesturesProp::GesturesProp(const std::string& name,
                           const PropertyType type,
                           const size_t count)
    : name_(name), type_(type), count_(count) {}

std::vector<int> GesturesProp::GetIntValue() const {
  NOTREACHED_IN_MIGRATION();
  return std::vector<int>();
}

bool GesturesProp::SetIntValue(const std::vector<int>& value) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::vector<int16_t> GesturesProp::GetShortValue() const {
  NOTREACHED_IN_MIGRATION();
  return std::vector<int16_t>();
}

bool GesturesProp::SetShortValue(const std::vector<int16_t>& value) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::vector<bool> GesturesProp::GetBoolValue() const {
  NOTREACHED_IN_MIGRATION();
  return std::vector<bool>();
}

bool GesturesProp::SetBoolValue(const std::vector<bool>& value) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::string GesturesProp::GetStringValue() const {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

bool GesturesProp::SetStringValue(const std::string& value) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::vector<double> GesturesProp::GetDoubleValue() const {
  NOTREACHED_IN_MIGRATION();
  return std::vector<double>();
}

bool GesturesProp::SetDoubleValue(const std::vector<double>& value) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void GesturesProp::SetHandlers(GesturesPropGetHandler get,
                               GesturesPropSetHandler set,
                               void* data) {
  get_ = get;
  set_ = set;
  handler_data_ = data;
}

void GesturesProp::OnGet() const {
  // We don't have the X server now so there is currently nothing to do when
  // the get handler returns true.
  // TODO(sheckylin): Re-visit this if we use handlers that modifies the
  // property.
  if (get_)
    get_(handler_data_);
}

void GesturesProp::OnSet() const {
  // Call the property set handler if available.
  if (set_)
    set_(handler_data_);
}

const char** GesturesProp::GetStringWritebackPtr() const {
  NOTREACHED_IN_MIGRATION();
  return NULL;
}

bool GesturesProp::IsAllocated() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

// Type-templated GesturesProp.
template <typename T>
class TypedGesturesProp : public GesturesProp {
 public:
  TypedGesturesProp(const std::string& name,
                    const PropertyType type,
                    const size_t count,
                    T* value)
      : GesturesProp(name, type, count),
        value_(value),
        is_read_only_(false),
        is_allocated_(false) {
    Init();
  }
  ~TypedGesturesProp() override {
    if (is_allocated_)
      delete[] value_;
  }

  // Accessors.
  bool IsReadOnly() const override { return is_read_only_; }

 protected:
  // Functions for setting/getting numerical properties.
  //
  // These two functions calls the set/get handler and should only be used in
  // Get*Value/Set*Value functions.
  template <typename U>
  std::vector<U> GetNumericalPropertyValue() const {
    // Nothing should be modified so it is OK to call the get handler first.
    OnGet();
    return this->template GetNumericalValue<U>();
  }

  template <typename U>
  bool SetNumericalPropertyValue(const std::vector<U>& value) {
    // Set the value only if not read-only and the vector size matches.
    //
    // As per the legacy guideline, all read-only properties (created with NULL)
    // can't be modified. If we want to change this in the future, re-think
    // about the different cases here (e.g., should we allow setting an array
    // value of different size?).
    if (is_read_only_ || value.size() != count())
      return false;
    bool ret = this->template SetNumericalValue(value);
    OnSet();
    return ret;
  }

  // Initialize a numerical property's value. Note that a (numerical) default
  // property's value is always stored in double.
  void InitializeNumericalProperty(const T* init,
                                   const GesturesProp* default_property) {
    if (IsDefaultPropertyUsable(default_property)) {
      DVLOG(2) << "Default property found. Using its value ...";
      this->template SetNumericalValue(default_property->GetDoubleValue());
    } else {
      // To work with the interface exposed by the gesture lib, we have no
      // choice but to trust that the init array has sufficient size.
      std::vector<T> temp(init, init + count());
      this->template SetNumericalValue(temp);
    }
  }

  // Data pointer.
  T* value_;

  // If the flag is on, it means the GesturesProp is created by passing a NULL
  // data pointer to the creator functions. We define the property as a
  // read-only one and that no value change will be allowed for it. Note that
  // the flag is different from is_allocated in that StringProperty will always
  // allocate no matter it is created with NULL or not.
  bool is_read_only_;

 private:
  // Initialize the object.
  void Init() {
    // If no external data pointer is passed, we have to create our own.
    if (!value_) {
      value_ = new T[GesturesProp::count()];
      is_read_only_ = true;
      is_allocated_ = true;
    }
  }

  // Low-level functions for setting/getting numerical properties.
  template <typename U>
  std::vector<U> GetNumericalValue() const {
    // We do type-casting because the numerical types may not totally match.
    // For example, we store bool as GesturesPropBool to be compatible with the
    // gesture library. Also, all parsed xorg-conf property values are stored
    // as double because we can't identify their original type lexically.
    // TODO(sheckylin): Handle value out-of-range (e.g., double to int).
    std::vector<U> result(count());
    for (size_t i = 0; i < count(); ++i)
      result[i] = static_cast<U>(value_[i]);
    return result;
  }

  template <typename U>
  bool SetNumericalValue(const std::vector<U>& value) {
    for (size_t i = 0; i < count(); ++i)
      value_[i] = static_cast<T>(value[i]);
    return true;
  }

  // Check if a default property usable for (numerical) initialization.
  bool IsDefaultPropertyUsable(const GesturesProp* default_property) const {
    // We currently assumed that we won't specify any array property in the
    // configuration files. The code needs to be updated if the assumption
    // becomes invalid in the future.
    return (count() == 1 && default_property &&
            default_property->type() != PropertyType::PT_STRING);
  }

  // Accessors.
  bool IsAllocated() const override { return is_allocated_; }

  // If the flag is on, it means the memory that the data pointer points to is
  // allocated here. We will need to free the memory by ourselves when the
  // GesturesProp is destroyed.
  bool is_allocated_;
};

class GesturesIntProp : public TypedGesturesProp<int> {
 public:
  GesturesIntProp(const std::string& name,
                  const size_t count,
                  int* value,
                  const int* init,
                  const GesturesProp* default_property)
      : TypedGesturesProp<int>(name, PropertyType::PT_INT, count, value) {
    InitializeNumericalProperty(init, default_property);
  }
  std::vector<int> GetIntValue() const override {
    return this->template GetNumericalPropertyValue<int>();
  }
  bool SetIntValue(const std::vector<int>& value) override {
    return this->template SetNumericalPropertyValue(value);
  }
};

class GesturesShortProp : public TypedGesturesProp<short> {
 public:
  GesturesShortProp(const std::string& name,
                    const size_t count,
                    short* value,
                    const short* init,
                    const GesturesProp* default_property)
      : TypedGesturesProp<short>(name, PropertyType::PT_SHORT, count, value) {
    InitializeNumericalProperty(init, default_property);
  }
  std::vector<int16_t> GetShortValue() const override {
    return this->template GetNumericalPropertyValue<int16_t>();
  }
  bool SetShortValue(const std::vector<int16_t>& value) override {
    return this->template SetNumericalPropertyValue(value);
  }
};

class GesturesBoolProp : public TypedGesturesProp<GesturesPropBool> {
 public:
  GesturesBoolProp(const std::string& name,
                   const size_t count,
                   GesturesPropBool* value,
                   const GesturesPropBool* init,
                   const GesturesProp* default_property)
      : TypedGesturesProp<GesturesPropBool>(name,
                                            PropertyType::PT_BOOL,
                                            count,
                                            value) {
    InitializeNumericalProperty(init, default_property);
  }
  std::vector<bool> GetBoolValue() const override {
    return this->template GetNumericalPropertyValue<bool>();
  }
  bool SetBoolValue(const std::vector<bool>& value) override {
    return this->template SetNumericalPropertyValue(value);
  }
};

class GesturesDoubleProp : public TypedGesturesProp<double> {
 public:
  GesturesDoubleProp(const std::string& name,
                     const size_t count,
                     double* value,
                     const double* init,
                     const GesturesProp* default_property)
      : TypedGesturesProp<double>(name, PropertyType::PT_REAL, count, value) {
    InitializeNumericalProperty(init, default_property);
  }
  std::vector<double> GetDoubleValue() const override {
    return this->template GetNumericalPropertyValue<double>();
  }
  bool SetDoubleValue(const std::vector<double>& value) override {
    return this->template SetNumericalPropertyValue(value);
  }
};

class GesturesStringProp : public TypedGesturesProp<std::string> {
 public:
  // StringProperty's memory is always allocated on this side instead of
  // externally in the gesture lib as the original one will be destroyed right
  // after the constructor call (check the design of StringProperty). To do
  // this, we call the TypedGesturesProp constructor with NULL pointer so that
  // it always allocates.
  GesturesStringProp(const std::string& name,
                     const char** value,
                     const char* init,
                     const GesturesProp* default_property)
      : TypedGesturesProp<std::string>(name, PropertyType::PT_STRING, 1, NULL),
        write_back_(NULL) {
    InitializeStringProperty(value, init, default_property);
  }
  std::string GetStringValue() const override {
    OnGet();
    return *value_;
  }
  bool SetStringValue(const std::string& value) override {
    if (is_read_only_)
      return false;
    *value_ = value;

    // Write back the pointer in case it may change (e.g., string
    // re-allocation).
    if (write_back_)
      *(write_back_) = value_->c_str();
    OnSet();
    return true;
  }

 private:
  // Initialize the object.
  void InitializeStringProperty(const char** value,
                                const char* init,
                                const GesturesProp* default_property) {
    // Initialize the property value similar to the numerical types.
    if (IsDefaultPropertyUsable(default_property)) {
      DVLOG(2) << "Default property found. Using its value ...";
      *value_ = default_property->GetStringValue();
    } else {
      *value_ = init;
    }

    // If the provided pointer is not NULL, replace its content
    // (val_ of StringProperty) with the address of our allocated string.
    // Note that we don't have to do this for the other data types as they will
    // use the original data pointer if possible and it is unnecessary to do so
    // if the pointer is NULL.
    if (value) {
      *value = value_->c_str();
      write_back_ = value;
      // Set the read-only flag back to false.
      is_read_only_ = false;
    }
  }

  // Re-write the function with different criteria as we want string properties
  // now.
  bool IsDefaultPropertyUsable(const GesturesProp* default_property) const {
    return (default_property &&
            default_property->type() == PropertyType::PT_STRING);
  }

  const char** GetStringWritebackPtr() const override { return write_back_; }

  // In some cases, we don't directly use the data pointer provided by the
  // creators due to its limitation and instead use our own types (e.g., in
  // the case of string). We thus need to store the write back pointer so that
  // we can update the value in the gesture lib if the property value gets
  // changed.
  const char** write_back_;
};

// Anonymous namespace for utility functions and internal constants.
namespace {

// The path that we will look for conf files.
const char kConfigurationFilePath[] = "/etc/gesture";

// Special keywords for boolean values.
const char* kTrue[] = {"on", "true", "yes"};
const char* kFalse[] = {"off", "false", "no"};

// Check if a device falls into one device type category.
bool IsDeviceOfType(const ui::GesturePropertyProvider::DevicePtr device,
                    const ui::EventDeviceType type,
                    const GesturesProp* device_mouse_property,
                    const GesturesProp* device_touchpad_property) {
  // Get the device type info from gesture properties if they are available.
  // Otherwise, fallback to the libevdev device info.
  bool is_mouse = false, is_touchpad = false;
  EvdevClass evdev_class = device->info.evdev_class;
  if (device_mouse_property) {
    is_mouse = device_mouse_property->GetBoolValue()[0];
  } else {
    is_mouse = (evdev_class == EvdevClassMouse ||
                evdev_class == EvdevClassMultitouchMouse);
  }
  if (device_touchpad_property) {
    is_touchpad = device_touchpad_property->GetBoolValue()[0];
  } else {
    is_touchpad = (evdev_class == EvdevClassTouchpad ||
                   evdev_class == EvdevClassTouchscreen ||
                   evdev_class == EvdevClassMultitouchMouse);
  }

  switch (type) {
    case ui::DT_KEYBOARD:
      return (evdev_class == EvdevClassKeyboard);
    case ui::DT_MOUSE:
      return is_mouse;
    case ui::DT_POINTING_STICK:
      return (evdev_class == EvdevClassPointingStick);
    case ui::DT_TOUCHPAD:
      return (!is_mouse) && is_touchpad;
    case ui::DT_TOUCHSCREEN:
      return (evdev_class == EvdevClassTouchscreen);
    case ui::DT_MULTITOUCH:
      return is_touchpad;
    case ui::DT_MULTITOUCH_MOUSE:
      return is_mouse && is_touchpad;
    case ui::DT_ALL:
      return true;
    default:
      break;
  }
  return false;
}

// Trick to get the device path from a file descriptor.
std::string GetDeviceNodePath(
    const ui::GesturePropertyProvider::DevicePtr device) {
  std::string proc_symlink =
      "/proc/self/fd/" + base::NumberToString(device->fd);
  base::FilePath path;
  if (!base::ReadSymbolicLink(base::FilePath(proc_symlink), &path))
    return std::string();
  return path.value();
}

bool IsMatchTypeSupported(const std::string& match_type) {
  // Check if a match criteria is currently implemented. We support only match
  // types that have already been used. One should change this if we start using
  // new types in the future. Note that most unsupported match types are either
  // useless in CrOS or inapplicable to the non-X environment.
  constexpr auto kSupportedMatchTypes =
      base::MakeFixedFlatSet<std::string_view>(
          {"MatchProduct", "MatchDevicePath", "MatchUSBID", "MatchDMIProduct",
           "MatchIsPointer", "MatchIsTouchpad", "MatchIsTouchscreen"});
  constexpr auto kUnsupportedMatchTypes =
      base::MakeFixedFlatSet<std::string_view>(
          {"MatchVendor", "MatchOS", "MatchPnPID", "MatchDriver", "MatchTag",
           "MatchLayout", "MatchIsKeyboard", "MatchIsJoystick",
           "MatchIsTablet"});

  if (kSupportedMatchTypes.contains(match_type)) {
    return true;
  }

  if (kUnsupportedMatchTypes.contains(match_type)) {
    LOG(ERROR) << "Unsupported gestures input class match type: " << match_type;
    return false;
  }

  return false;
}

// Check if a match criteria is a device type one.
bool IsMatchDeviceType(const std::string& match_type) {
  return base::StartsWith(match_type, "MatchIs", base::CompareCase::SENSITIVE);
}

// Parse a boolean value keyword (e.g., on/off, true/false).
int ParseBooleanKeyword(const std::string& value) {
  for (size_t i = 0; i < std::size(kTrue); ++i) {
    if (base::EqualsCaseInsensitiveASCII(value, kTrue[i]))
      return 1;
  }
  for (size_t i = 0; i < std::size(kFalse); ++i) {
    if (base::EqualsCaseInsensitiveASCII(value, kFalse[i]))
      return -1;
  }
  return 0;
}

// Log the value of an array property.
template <typename T>
void LogArrayProperty(std::ostream& os, const std::vector<T>& value) {
  os << "(";
  for (size_t i = 0; i < value.size(); ++i) {
    if (i > 0)
      os << ", ";
    os << value[i];
  }
  os << ")";
}

// Property type logging function.
std::ostream& operator<<(std::ostream& out,
                         const ui::GesturePropertyProvider::PropertyType type) {
  std::string s;
#define TYPE_CASE(TYPE)                     \
  case (ui::GesturePropertyProvider::TYPE): \
    s = #TYPE;                              \
    break;
  switch (type) {
    TYPE_CASE(PT_INT);
    TYPE_CASE(PT_SHORT);
    TYPE_CASE(PT_BOOL);
    TYPE_CASE(PT_STRING);
    TYPE_CASE(PT_REAL);
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
#undef TYPE_CASE
  return out << s;
}

// A relay function that dumps evdev log to a place that we have access to
// (the default directory is inaccessible without X11).
void DumpTouchEvdevDebugLog(void* data) {
  Event_Dump_Debug_Log_To(data, ui::kTouchpadEvdevLogPath);
}

}  // namespace

// GesturesProp logging function.
std::ostream& operator<<(std::ostream& os, const GesturesProp& prop) {
  const GesturesProp* property = &prop;

  // Output the property content.
  os << "\"" << property->name() << "\", " << property->type() << ", "
     << property->count() << ", (" << property->IsAllocated() << ", "
     << property->IsReadOnly() << "), ";

  // Only the string property has the write back pointer.
  if (property->type() == ui::GesturePropertyProvider::PT_STRING)
    os << property->GetStringWritebackPtr();
  else
    os << "NULL";

  // Output the property values.
  os << ", ";
  switch (property->type()) {
    case ui::GesturePropertyProvider::PT_INT:
      LogArrayProperty(os, property->GetIntValue());
      break;
    case ui::GesturePropertyProvider::PT_SHORT:
      LogArrayProperty(os, property->GetShortValue());
      break;
    case ui::GesturePropertyProvider::PT_BOOL:
      LogArrayProperty(os, property->GetBoolValue());
      break;
    case ui::GesturePropertyProvider::PT_STRING:
      os << "\"" << property->GetStringValue() << "\"";
      break;
    case ui::GesturePropertyProvider::PT_REAL:
      LogArrayProperty(os, property->GetDoubleValue());
      break;
    default:
      LOG(ERROR) << "Unknown gesture property type: " << property->type();
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return os;
}

namespace ui {
namespace internal {

// Struct holding properties of a device.
struct GestureDevicePropertyData {
  GestureDevicePropertyData() {}

  // Properties owned and being used by the device.
  std::unordered_map<std::string, std::unique_ptr<GesturesProp>> properties;

  // Unowned default properties (owned by the configuration file). Their values
  // will be applied when a property of the same name is created. These are
  // usually only a small portion of all properties in use.
  std::unordered_map<std::string, GesturesProp*> default_properties;
};

// Base class for device match criterias in conf files.
// Check the xorg-conf spec for more detailed information.
class MatchCriteria {
 public:
  typedef ui::GesturePropertyProvider::DevicePtr DevicePtr;
  explicit MatchCriteria(const std::string& arg);
  virtual ~MatchCriteria() {}
  virtual bool Match(const DevicePtr device) = 0;

 protected:
  std::vector<std::string> args_;
};

// Match a device based on its evdev name string.
class MatchProduct : public MatchCriteria {
 public:
  explicit MatchProduct(const std::string& arg);
  ~MatchProduct() override {}
  bool Match(const DevicePtr device) override;
};

// Math a device based on its device node path.
class MatchDevicePath : public MatchCriteria {
 public:
  explicit MatchDevicePath(const std::string& arg);
  ~MatchDevicePath() override {}
  bool Match(const DevicePtr device) override;
};

// Math a USB device based on its USB vid and pid.
// Mostly used for external mice and touchpads.
class MatchUSBID : public MatchCriteria {
 public:
  explicit MatchUSBID(const std::string& arg);
  ~MatchUSBID() override {}
  bool Match(const DevicePtr device) override;

 private:
  bool IsValidPattern(const std::string& pattern);
  std::vector<std::string> vid_patterns_;
  std::vector<std::string> pid_patterns_;
};

// Match a device based on the system's DMI Product Name. Useful for internal
// devices that don't report a very unique vendor and product ID.
class MatchDmiProduct : public MatchCriteria {
 public:
  // Setting load_error to true indicates that the product name couldn't be
  // loaded, producing a matcher that will never match.
  explicit MatchDmiProduct(const std::string& dmi_product_name,
                           const std::string& arg,
                           bool load_error = false);
  ~MatchDmiProduct() override {}
  bool Match(const DevicePtr device) override;

 private:
  std::string dmi_product_name_;
  bool load_error_;
};

// Generic base class for device type math criteria.
class MatchDeviceType : public MatchCriteria {
 public:
  explicit MatchDeviceType(const std::string& arg);
  ~MatchDeviceType() override {}

 protected:
  bool value_;
  bool is_valid_;
};

// Check if a device is a pointer device.
class MatchIsPointer : public MatchDeviceType {
 public:
  explicit MatchIsPointer(const std::string& arg);
  ~MatchIsPointer() override {}
  bool Match(const DevicePtr device) override;
};

// Check if a device is a touchpad.
class MatchIsTouchpad : public MatchDeviceType {
 public:
  explicit MatchIsTouchpad(const std::string& arg);
  ~MatchIsTouchpad() override {}
  bool Match(const DevicePtr device) override;
};

// Check if a device is a touchscreen.
class MatchIsTouchscreen : public MatchDeviceType {
 public:
  explicit MatchIsTouchscreen(const std::string& arg);
  ~MatchIsTouchscreen() override {}
  bool Match(const DevicePtr device) override;
};

// Struct for sections in xorg conf files.
struct ConfigurationSection {
  typedef ui::GesturePropertyProvider::DevicePtr DevicePtr;
  ConfigurationSection() {}
  bool Match(const DevicePtr device);
  std::string identifier;
  std::vector<std::unique_ptr<MatchCriteria>> criterias;
  std::vector<std::unique_ptr<GesturesProp>> properties;
};

MatchCriteria::MatchCriteria(const std::string& arg) {
  // TODO(sheckylin): Should we trim all tokens here?
  args_ = base::SplitString(
      arg, "|", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (args_.empty()) {
    LOG(ERROR) << "Empty match pattern found, will evaluate to the default "
                  "value (true): \"" << arg << "\"";
  }
}

MatchProduct::MatchProduct(const std::string& arg) : MatchCriteria(arg) {
}

bool MatchProduct::Match(const DevicePtr device) {
  if (args_.empty())
    return true;
  std::string name(device->info.name);
  for (size_t i = 0; i < args_.size(); ++i)
    if (name.find(args_[i]) != std::string::npos)
      return true;
  return false;
}

MatchDevicePath::MatchDevicePath(const std::string& arg) : MatchCriteria(arg) {
}

bool MatchDevicePath::Match(const DevicePtr device) {
  if (args_.empty())
    return true;

  // Check if the device path matches any pattern.
  std::string path = GetDeviceNodePath(device);
  if (path.empty())
    return false;
  for (size_t i = 0; i < args_.size(); ++i)
    if (fnmatch(args_[i].c_str(), path.c_str(), FNM_NOESCAPE) == 0)
      return true;
  return false;
}

MatchUSBID::MatchUSBID(const std::string& arg) : MatchCriteria(arg) {
  // Check each pattern and split valid ones into vids and pids.
  for (size_t i = 0; i < args_.size(); ++i) {
    if (!IsValidPattern(args_[i])) {
      LOG(ERROR) << "Invalid USB ID: " << args_[i];
      continue;
    }
    std::vector<std::string> tokens = base::SplitString(
        args_[i], ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    vid_patterns_.push_back(base::ToLowerASCII(tokens[0]));
    pid_patterns_.push_back(base::ToLowerASCII(tokens[1]));
  }
  if (vid_patterns_.empty()) {
    LOG(ERROR) << "No valid USB ID pattern found, will be ignored: \"" << arg
               << "\"";
  }
}

bool MatchUSBID::Match(const DevicePtr device) {
  if (vid_patterns_.empty())
    return true;
  std::string vid = base::StringPrintf("%04x", device->info.id.vendor);
  std::string pid = base::StringPrintf("%04x", device->info.id.product);
  for (size_t i = 0; i < vid_patterns_.size(); ++i) {
    if (fnmatch(vid_patterns_[i].c_str(), vid.c_str(), FNM_NOESCAPE) == 0 &&
        fnmatch(pid_patterns_[i].c_str(), pid.c_str(), FNM_NOESCAPE) == 0) {
      return true;
    }
  }
  return false;
}

bool MatchUSBID::IsValidPattern(const std::string& pattern) {
  // Each USB id should be in the lsusb format, i.e., xxxx:xxxx. We choose to do
  // a lazy check here: if the pattern contains wrong characters not in the hex
  // number range, it won't be matched anyway.
  int number_of_colons = 0;
  size_t pos_of_colon = 0;
  for (size_t i = 0; i < pattern.size(); ++i)
    if (pattern[i] == ':')
      ++number_of_colons, pos_of_colon = i;
  return (number_of_colons == 1) && (pos_of_colon != 0) &&
         (pos_of_colon != pattern.size() - 1);
}

MatchDmiProduct::MatchDmiProduct(const std::string& dmi_product_name,
                                 const std::string& arg,
                                 bool load_error)
    : MatchCriteria(arg),
      dmi_product_name_(dmi_product_name),
      load_error_(load_error) {}

bool MatchDmiProduct::Match(const DevicePtr device) {
  // Default value of a match criteria is true.
  if (args_.empty())
    return true;

  if (load_error_)
    return false;

  for (size_t i = 0; i < args_.size(); ++i) {
    if (dmi_product_name_ == args_[i])
      return true;
  }
  return false;
}

MatchDeviceType::MatchDeviceType(const std::string& arg)
    : MatchCriteria(arg), value_(true), is_valid_(false) {
  // Default value of a match criteria is true.
  if (args_.empty())
    args_.push_back("on");

  // We care only about the first argument.
  int value = ParseBooleanKeyword(args_[0]);
  if (value) {
    is_valid_ = true;
    value_ = value > 0;
  }
  if (!is_valid_) {
    LOG(ERROR)
        << "No valid device class boolean keyword found, will be ignored: \""
        << arg << "\"";
  }
}

MatchIsPointer::MatchIsPointer(const std::string& arg) : MatchDeviceType(arg) {
}

bool MatchIsPointer::Match(const DevicePtr device) {
  if (!is_valid_)
    return true;
  return (value_ == (device->info.evdev_class == EvdevClassMouse ||
                     device->info.evdev_class == EvdevClassPointingStick ||
                     device->info.evdev_class == EvdevClassMultitouchMouse));
}

MatchIsTouchpad::MatchIsTouchpad(const std::string& arg)
    : MatchDeviceType(arg) {
}

bool MatchIsTouchpad::Match(const DevicePtr device) {
  if (!is_valid_)
    return true;
  return (value_ == (device->info.evdev_class == EvdevClassTouchpad));
}

MatchIsTouchscreen::MatchIsTouchscreen(const std::string& arg)
    : MatchDeviceType(arg) {
}

bool MatchIsTouchscreen::Match(const DevicePtr device) {
  if (!is_valid_)
    return true;
  return (value_ == (device->info.evdev_class == EvdevClassTouchscreen));
}

bool ConfigurationSection::Match(DevicePtr device) {
  for (size_t i = 0; i < criterias.size(); ++i)
    if (!criterias[i]->Match(device))
      return false;
  return true;
}

}  // namespace internal

GesturePropertyProvider::GesturePropertyProvider() {
  LoadDeviceConfigurations();
}

GesturePropertyProvider::~GesturePropertyProvider() {
}

bool GesturePropertyProvider::GetDeviceIdsByType(
    const EventDeviceType type,
    std::vector<DeviceId>* device_ids) {
  bool exists = false;
  for (auto it = device_map_.begin(); it != device_map_.end(); ++it) {
    if (IsDeviceIdOfType(it->first, type)) {
      exists = true;
      if (device_ids)
        device_ids->push_back(it->first);
    }
  }
  return exists;
}

bool GesturePropertyProvider::IsDeviceIdOfType(const DeviceId device_id,
                                               const EventDeviceType type) {
  auto it = device_map_.find(device_id);
  if (it == device_map_.end())
    return false;
  return IsDeviceOfType(it->second, type,
                        GetProperty(device_id, "Device Mouse"),
                        GetProperty(device_id, "Device Touchpad"));
}

GesturesProp* GesturePropertyProvider::GetProperty(const DeviceId device_id,
                                                   const std::string& name) {
  return FindProperty(device_id, name);
}

std::vector<std::string> GesturePropertyProvider::GetPropertyNamesById(
    const DeviceId device_id) {
  auto it = device_data_map_.find(device_id);
  if (it == device_data_map_.end())
    return std::vector<std::string>();

  internal::GestureDevicePropertyData* device_data = it->second.get();

  // Dump all property names of the device.
  std::vector<std::string> names;
  for (const auto& pair : device_data->properties)
    names.push_back(pair.first);
  return names;
}

std::string GesturePropertyProvider::GetDeviceNameById(
    const DeviceId device_id) {
  auto it = device_map_.find(device_id);
  if (it == device_map_.end())
    return std::string();
  return std::string(it->second->info.name);
}

void GesturePropertyProvider::RegisterDevice(const DeviceId id,
                                             const DevicePtr device) {
  auto it = device_map_.find(id);
  if (it != device_map_.end())
    return;

  // Setup data-structures.
  device_map_[id] = device;
  device_data_map_[id] =
      std::make_unique<internal::GestureDevicePropertyData>();

  // Gather default property values for the device from the parsed conf files.
  SetupDefaultProperties(id, device);
  return;
}

void GesturePropertyProvider::UnregisterDevice(const DeviceId id) {
  auto it = device_map_.find(id);
  if (it == device_map_.end())
    return;
  device_data_map_.erase(id);
  device_map_.erase(it);
}

void GesturePropertyProvider::AddProperty(
    const DeviceId device_id,
    const std::string& name,
    std::unique_ptr<GesturesProp> property) {
  // The look-up should never fail because ideally a property can only be
  // created with GesturesPropCreate* functions from the gesture lib side.
  // Therefore, we simply return on failure.
  auto it = device_data_map_.find(device_id);
  if (it != device_data_map_.end())
    it->second->properties[name] = std::move(property);
}

void GesturePropertyProvider::DeleteProperty(const DeviceId device_id,
                                             const std::string& name) {
  auto it = device_data_map_.find(device_id);
  if (it != device_data_map_.end())
    it->second->properties.erase(name);
}

GesturesProp* GesturePropertyProvider::FindProperty(const DeviceId device_id,
                                                    const std::string& name) {
  auto it = device_data_map_.find(device_id);
  if (it == device_data_map_.end())
    return nullptr;

  auto it2 = it->second->properties.find(name);
  if (it2 == it->second->properties.end())
    return nullptr;

  return it2->second.get();
}

GesturesProp* GesturePropertyProvider::GetDefaultProperty(
    const DeviceId device_id,
    const std::string& name) {
  auto it = device_data_map_.find(device_id);
  if (it == device_data_map_.end())
    return nullptr;

  auto it2 = it->second->default_properties.find(name);
  if (it2 == it->second->default_properties.end())
    return nullptr;

  return it2->second;
}

void GesturePropertyProvider::LoadDeviceConfigurations() {
  // Enumerate conf files and sort them lexicographically.
  std::set<base::FilePath> files;
  base::FileEnumerator file_enum(base::FilePath(kConfigurationFilePath),
                                 false,
                                 base::FileEnumerator::FILES,
                                 "*.conf");
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    files.insert(path);
  }
  DVLOG(2) << files.size() << " conf files were found";

  // Parse conf files one-by-one.
  for (auto file_iter = files.begin(); file_iter != files.end(); ++file_iter) {
    DVLOG(2) << "Parsing conf file: " << (*file_iter).value();
    std::string content;
    if (!base::ReadFileToString(*file_iter, &content)) {
      LOG(ERROR) << "Can't loading gestures conf file: "
                 << (*file_iter).value();
      continue;
    }
    ParseXorgConfFile(content);
  }
}

void GesturePropertyProvider::ParseXorgConfFile(const std::string& content) {
  // To simplify the parsing work, we made some assumption about the conf file
  // format which doesn't exist in the original xorg-conf spec. Most important
  // ones are:
  // 1. All keywords and names are now case-sensitive. Also, underscores are not
  //    ignored.
  // 2. Each entry takes up one and exactly one line in the file.
  // 3. No negation of the option value even if the option name is prefixed with
  //    "No" as it may cause problems for option names that does start with "No"
  //    (e.g., "Non-linearity").

  // Break the content into sections, lines and then pieces.
  // Sections are delimited by the "EndSection" keyword.
  // Lines are delimited by "\n".
  // Pieces are delimited by all white-spaces.
  for (const std::string& section :
       base::SplitStringUsingSubstr(content, "EndSection",
                                    base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_ALL)) {
    // Create a new configuration section.
    configurations_.push_back(
        std::make_unique<internal::ConfigurationSection>());
    internal::ConfigurationSection* config = configurations_.back().get();

    // Break the section into lines.
    base::StringTokenizer lines(section, "\n");
    bool is_input_class_section = true;
    bool has_checked_section_type = false;
    while (is_input_class_section && lines.GetNext()) {
      // Parse the line w.r.t. the xorg-conf format.
      std::string line(lines.token());

      // Skip empty lines.
      if (line.empty())
        continue;

      // Treat all whitespaces as delimiters.
      base::StringTokenizer pieces(line, base::kWhitespaceASCII);
      pieces.set_quote_chars("\"");
      bool is_parsing = false;
      bool has_error = false;
      bool next_is_section_type = false;
      bool next_is_option_name = false;
      bool next_is_option_value = false;
      bool next_is_match_criteria = false;
      bool next_is_identifier = false;
      std::string match_type, option_name;
      while (pieces.GetNext()) {
        std::string piece(pieces.token());

        // Skip empty pieces.
        if (piece.empty())
          continue;

        // See if we are currently parsing an entry or are still looking for
        // one.
        if (is_parsing) {
          // Stop parsing the current line if the format is wrong.
          if (piece.size() <= 2 || piece[0] != '\"' || piece.back() != '\"') {
            LOG(ERROR) << "Error parsing line: " << lines.token();
            has_error = true;
            if (next_is_section_type)
              is_input_class_section = false;
            break;
          }

          // Parse the arguments. Note that we don't break even if a whitespace
          // string is passed. It will just be handled in various ways based on
          // the entry type.
          std::string arg;
          base::TrimWhitespaceASCII(
              piece.substr(1, piece.size() - 2), base::TRIM_ALL, &arg);
          if (next_is_section_type) {
            // We only care about InputClass sections.
            if (arg != "InputClass") {
              has_error = true;
              is_input_class_section = false;
            } else {
              DVLOG(2) << "New InputClass section found";
              has_checked_section_type = true;
            }
            break;
          } else if (next_is_identifier) {
            DVLOG(2) << "Identifier: " << arg;
            config->identifier = arg;
            next_is_identifier = false;
            break;
          } else if (next_is_option_name) {
            // TODO(sheckylin): Support option "Ignore".
            option_name = arg;
            next_is_option_value = true;
            next_is_option_name = false;
          } else if (next_is_option_value) {
            std::unique_ptr<GesturesProp> property =
                CreateDefaultProperty(option_name, arg);
            if (property)
              config->properties.push_back(std::move(property));
            next_is_option_value = false;
            break;
          } else if (next_is_match_criteria) {
            // Skip all match types that are not supported.
            if (IsMatchTypeSupported(match_type)) {
              std::unique_ptr<internal::MatchCriteria> criteria =
                  CreateMatchCriteria(match_type, arg);
              if (criteria)
                config->criterias.push_back(std::move(criteria));
            }
            next_is_match_criteria = false;
            break;
          }
        } else {
          // If the section type hasn't been decided yet, look for it.
          // Otherwise, look for valid entries according to the spec.
          if (has_checked_section_type) {
            if (piece == "Driver") {
              // "Driver" field is meaningless for non-X11 setup.
              break;
            } else if (piece == "Identifier") {
              is_parsing = true;
              next_is_identifier = true;
              continue;
            } else if (piece == "Option") {
              is_parsing = true;
              next_is_option_name = true;
              continue;
            } else if (piece.size() > 5 && piece.compare(0, 5, "Match") == 0) {
              match_type = piece;
              is_parsing = true;
              next_is_match_criteria = true;
              continue;
            }
          } else if (piece == "Section") {
            is_parsing = true;
            next_is_section_type = true;
            continue;
          }

          // If none of the above is found, check if the current piece starts a
          // comment.
          if (piece.empty() || piece[0] != '#') {
            LOG(ERROR) << "Error parsing line: " << lines.token();
            has_error = true;
          }
          break;
        }
      }

      // The value of a boolean option is skipped (default is true).
      if (!has_error && (next_is_option_value || next_is_match_criteria)) {
        if (next_is_option_value) {
          std::unique_ptr<GesturesProp> property =
              CreateDefaultProperty(option_name, "on");
          if (property)
            config->properties.push_back(std::move(property));
        } else if (IsMatchTypeSupported(match_type) &&
                   IsMatchDeviceType(match_type)) {
          std::unique_ptr<internal::MatchCriteria> criteria =
              CreateMatchCriteria(match_type, "on");
          if (criteria)
            config->criterias.push_back(std::move(criteria));
        }
      }
    }

    // Remove useless config sections.
    if (!is_input_class_section ||
        (config->criterias.empty() && config->properties.empty())) {
      configurations_.pop_back();
    }
  }
}

std::unique_ptr<internal::MatchCriteria>
GesturePropertyProvider::CreateMatchCriteria(const std::string& match_type,
                                             const std::string& arg) {
  DVLOG(2) << "Creating match criteria: (" << match_type << ", " << arg << ")";
  if (match_type == "MatchProduct")
    return std::make_unique<internal::MatchProduct>(arg);
  if (match_type == "MatchDevicePath")
    return std::make_unique<internal::MatchDevicePath>(arg);
  if (match_type == "MatchUSBID")
    return std::make_unique<internal::MatchUSBID>(arg);
  if (match_type == "MatchDMIProduct") {
    if (!dmi_product_name_loaded_ && !LoadDmiProductName()) {
      // Avoid matching all MatchDMIProduct configs on machines with bad DMI
      // info, by returning a matcher that will never match.
      return std::make_unique<internal::MatchDmiProduct>("", arg, true);
    }
    return std::make_unique<internal::MatchDmiProduct>(dmi_product_name_, arg);
  }
  if (match_type == "MatchIsPointer")
    return std::make_unique<internal::MatchIsPointer>(arg);
  if (match_type == "MatchIsTouchpad")
    return std::make_unique<internal::MatchIsTouchpad>(arg);
  if (match_type == "MatchIsTouchscreen")
    return std::make_unique<internal::MatchIsTouchscreen>(arg);
  NOTREACHED_IN_MIGRATION();
  return NULL;
}

bool GesturePropertyProvider::LoadDmiProductName() {
  const auto path = base::FilePath("/sys/class/dmi/id/product_name");

  if (!base::ReadFileToString(path, &dmi_product_name_)) {
    LOG(WARNING) << "Unable to read the DMI product_name.";
    return false;
  }

  base::TrimWhitespaceASCII(dmi_product_name_, base::TRIM_ALL,
                            &dmi_product_name_);
  dmi_product_name_loaded_ = true;
  return true;
}

std::unique_ptr<GesturesProp> GesturePropertyProvider::CreateDefaultProperty(
    const std::string& name,
    const std::string& value) {
  // Our parsing rule:
  // 1. No hex or oct number is accepted.
  // 2. All numbers will be stored as double.
  // 3. Array elements can be separated by both white-spaces or commas.
  // 4. A token is treated as numeric either if it is one of the special
  //    keywords for boolean values (on, true, yes, off, false, no) or if
  //    base::StringToDouble succeeds.
  // 5. The property is treated as numeric if and only if all of its elements
  //    (if any) are numerics. Otherwise, it will be treated as a string.
  // 6. A string property will be trimmed before storing its value.
  DVLOG(2) << "Creating default property: (" << name << ", " << value << ")";

  // Parse elements one-by-one.
  std::string delimiters(base::kWhitespaceASCII);
  delimiters.append(",");
  base::StringTokenizer tokens(value, delimiters);
  bool is_all_numeric = true;
  std::vector<double> numbers;
  while (tokens.GetNext()) {
    // Skip empty tokens.
    std::string token(tokens.token());
    if (token.empty())
      continue;

    // Check if it is a boolean keyword.
    int bool_result = ParseBooleanKeyword(token);
    if (bool_result) {
      numbers.push_back(bool_result > 0);
      continue;
    }

    // Check if it is a number.
    double real_result;
    bool success = base::StringToDouble(token, &real_result);
    if (!success) {
      is_all_numeric = false;
      break;
    }
    numbers.push_back(real_result);
  }

  // Create the GesturesProp. Array properties need to contain at least one
  // number and may contain numbers only.
  std::unique_ptr<GesturesProp> property;
  if (is_all_numeric && numbers.size()) {
    property.reset(new GesturesDoubleProp(name, numbers.size(), NULL,
                                          numbers.data(), NULL));
  } else {
    property.reset(new GesturesStringProp(name, NULL, value.c_str(), NULL));
  }

  DVLOG(2) << "Prop: " << *property;
  // The function will always succeed for now but it may change later if we
  // specify some name or args as invalid.
  return property;
}

void GesturePropertyProvider::SetupDefaultProperties(const DeviceId device_id,
                                                     const DevicePtr device) {
  DVLOG(2) << "Setting up default properties for (" << device << ", "
           << device_id << ", " << device->info.name << ")";

  // Go through all parsed sections.
  auto& property_map = device_data_map_[device_id]->default_properties;
  for (const auto& configuration : configurations_) {
    if (configuration->Match(device)) {
      DVLOG(2) << "Conf section \"" << configuration->identifier
               << "\" is matched";
      for (const auto& property : configuration->properties) {
        // We can't use insert here because a property may be set for several
        // times along the way.
        property_map[property->name()] = property.get();
      }
    }
  }
}

GesturesProp* GesturesPropFunctionsWrapper::CreateInt(void* device_data,
                                                      const char* name,
                                                      int* value,
                                                      size_t count,
                                                      const int* init) {
  return CreateProperty<int, GesturesIntProp>(
      device_data, name, value, count, init);
}

GesturesProp* GesturesPropFunctionsWrapper::CreateShort(void* device_data,
                                                        const char* name,
                                                        short* value,
                                                        size_t count,
                                                        const short* init) {
  return CreateProperty<short, GesturesShortProp>(
      device_data, name, value, count, init);
}

GesturesProp* GesturesPropFunctionsWrapper::CreateBool(
    void* device_data,
    const char* name,
    GesturesPropBool* value,
    size_t count,
    const GesturesPropBool* init) {
  return CreateProperty<GesturesPropBool, GesturesBoolProp>(
      device_data, name, value, count, init);
}

GesturesProp* GesturesPropFunctionsWrapper::CreateReal(void* device_data,
                                                       const char* name,
                                                       double* value,
                                                       size_t count,
                                                       const double* init) {
  return CreateProperty<double, GesturesDoubleProp>(
      device_data, name, value, count, init);
}

GesturesProp* GesturesPropFunctionsWrapper::CreateString(void* device_data,
                                                         const char* name,
                                                         const char** value,
                                                         const char* init) {
  GesturesProp* default_property = NULL;
  if (!PreCreateProperty(device_data, name, &default_property))
    return NULL;
  GesturesProp* property =
      new GesturesStringProp(name, value, init, default_property);
  PostCreateProperty(device_data, name, base::WrapUnique(property));
  return property;
}

void GesturesPropFunctionsWrapper::RegisterHandlers(
    void* device_data,
    GesturesProp* property,
    void* handler_data,
    GesturesPropGetHandler get,
    GesturesPropSetHandler set) {
  // Sanity checks
  if (!device_data || !property)
    return;

  property->SetHandlers(get, set, handler_data);
}

void GesturesPropFunctionsWrapper::Free(void* device_data,
                                        GesturesProp* property) {
  if (!property)
    return;
  GesturePropertyProvider* provider = GetPropertyProvider(device_data);

  // No need to manually delete the prop pointer as it is implicitly handled
  // with scoped ptr.
  DVLOG(3) << "Freeing Property: \"" << property->name() << "\"";
  provider->DeleteProperty(GetDeviceId(device_data), property->name());
}

bool GesturesPropFunctionsWrapper::InitializeDeviceProperties(
    void* device_data,
    GestureDeviceProperties* properties) {
  if (!device_data)
    return false;
  GesturePropertyProvider::DevicePtr device = GetDevicePointer(device_data);

  /* Create Device Properties */

  // Read Only properties.
  CreateString(
      device_data, "Device Node", NULL, GetDeviceNodePath(device).c_str());
  short vid = static_cast<short>(device->info.id.vendor);
  CreateShort(device_data, "Device Vendor ID", NULL, 1, &vid);
  short pid = static_cast<short>(device->info.id.product);
  CreateShort(device_data, "Device Product ID", NULL, 1, &pid);

  // Useable trackpad area. If not configured in .conf file,
  // use x/y valuator min/max as reported by kernel driver.
  CreateIntSingle(device_data,
                  "Active Area Left",
                  &properties->area_left,
                  Event_Get_Left(device));
  CreateIntSingle(device_data,
                  "Active Area Right",
                  &properties->area_right,
                  Event_Get_Right(device));
  CreateIntSingle(device_data,
                  "Active Area Top",
                  &properties->area_top,
                  Event_Get_Top(device));
  CreateIntSingle(device_data,
                  "Active Area Bottom",
                  &properties->area_bottom,
                  Event_Get_Bottom(device));

  // Trackpad resolution (pixels/mm). If not configured in .conf file,
  // use x/y resolution as reported by kernel driver.
  CreateIntSingle(device_data,
                  "Vertical Resolution",
                  &properties->res_y,
                  Event_Get_Res_Y(device));
  CreateIntSingle(device_data,
                  "Horizontal Resolution",
                  &properties->res_x,
                  Event_Get_Res_X(device));

  // Trackpad orientation minimum/maximum. If not configured in .conf file,
  // use min/max as reported by kernel driver.
  CreateIntSingle(device_data,
                  "Orientation Minimum",
                  &properties->orientation_minimum,
                  Event_Get_Orientation_Minimum(device));
  CreateIntSingle(device_data,
                  "Orientation Maximum",
                  &properties->orientation_maximum,
                  Event_Get_Orientation_Maximum(device));

  // Log dump property. Will call Event_Dump_Debug_Log when its value is being
  // set.
  GesturesProp* dump_debug_log_prop = CreateBoolSingle(
      device_data, "Dump Debug Log", &properties->dump_debug_log, false);
  RegisterHandlers(device_data, dump_debug_log_prop, device, NULL,
                   DumpTouchEvdevDebugLog);

  // Whether to do the gesture recognition or just passing the multi-touch data
  // to upper layers.
  CreateBoolSingle(device_data,
                   "Raw Touch Passthrough",
                   &properties->raw_passthrough,
                   false);
  return true;
}

void GesturesPropFunctionsWrapper::UnregisterDevice(void* device_data) {
  GesturePropertyProvider* provider = GetPropertyProvider(device_data);
  provider->UnregisterDevice(GetDeviceId(device_data));
}

template <typename T, class PROPTYPE>
GesturesProp* GesturesPropFunctionsWrapper::CreateProperty(void* device_data,
                                                           const char* name,
                                                           T* value,
                                                           size_t count,
                                                           const T* init) {
  // Create the property. Use the default property value if possible.
  GesturesProp* default_property = NULL;
  if (!PreCreateProperty(device_data, name, &default_property))
    return NULL;
  GesturesProp* property =
      new PROPTYPE(name, count, value, init, default_property);

  // Start tracking the property in the provider.
  PostCreateProperty(device_data, name, base::WrapUnique(property));
  return property;
}

bool GesturesPropFunctionsWrapper::PreCreateProperty(
    void* device_data,
    const char* name,
    GesturesProp** default_property) {
  GesturePropertyProvider* provider = GetPropertyProvider(device_data);
  GesturePropertyProvider::DeviceId device_id = GetDeviceId(device_data);

  // Register the device in the property provider if not yet.
  provider->RegisterDevice(device_id, GetDevicePointer(device_data));

  // First, see if the GesturesProp already exists.
  DVLOG(3) << "Creating Property: \"" << name << "\"";
  GesturesProp* property = provider->FindProperty(device_id, name);

  // If so, delete it as we can't reuse the data structure (newly-created
  // property may have different data type and count, which are fixed upon
  // creation via the template mechanism).
  if (property) {
    LOG(WARNING) << "Gesture property \"" << name
                 << "\" re-created. This shouldn't happen at the normal usage.";
    Free(device_data, property);
  }

  // Return the found default property from conf files (could be NULL).
  *default_property = provider->GetDefaultProperty(device_id, name);
  return true;
}

void GesturesPropFunctionsWrapper::PostCreateProperty(
    void* device_data,
    const char* name,
    std::unique_ptr<GesturesProp> property) {
  // Log the creation.
  DVLOG(3) << "Created active prop: " << *property;

  // Add the property to the gesture property provider. The gesture property
  // provider will own it from now on.
  GesturePropertyProvider* provider = GetPropertyProvider(device_data);
  provider->AddProperty(GetDeviceId(device_data), name, std::move(property));
}

GesturesProp* GesturesPropFunctionsWrapper::CreateIntSingle(void* device_data,
                                                            const char* name,
                                                            int* value,
                                                            int init) {
  return CreateInt(device_data, name, value, 1, &init);
}

GesturesProp* GesturesPropFunctionsWrapper::CreateBoolSingle(
    void* device_data,
    const char* name,
    GesturesPropBool* value,
    GesturesPropBool init) {
  return CreateBool(device_data, name, value, 1, &init);
}

GesturePropertyProvider* GesturesPropFunctionsWrapper::GetPropertyProvider(
    void* device_data) {
  return static_cast<GestureInterpreterLibevdevCros*>(device_data)
      ->property_provider();
}

GesturePropertyProvider::DevicePtr
GesturesPropFunctionsWrapper::GetDevicePointer(void* device_data) {
  return static_cast<GestureInterpreterLibevdevCros*>(device_data)->evdev();
}

GesturePropertyProvider::DeviceId GesturesPropFunctionsWrapper::GetDeviceId(
    void* device_data) {
  return static_cast<GestureInterpreterLibevdevCros*>(device_data)->id();
}

/* Global GesturesPropProvider
 *
 * Used by PropRegistry in GestureInterpreter to forward property value
 * creations from there.
 * */
const GesturesPropProvider kGesturePropProvider = {
    GesturesPropFunctionsWrapper::CreateInt,
    GesturesPropFunctionsWrapper::CreateShort,
    GesturesPropFunctionsWrapper::CreateBool,
    GesturesPropFunctionsWrapper::CreateString,
    GesturesPropFunctionsWrapper::CreateReal,
    GesturesPropFunctionsWrapper::RegisterHandlers,
    GesturesPropFunctionsWrapper::Free};

}  // namespace ui
