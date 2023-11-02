// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_PROPERTY_PROVIDER_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_PROPERTY_PROVIDER_H_

#include <gestures/gestures.h>
#include <libevdev/libevdev.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

class GesturesPropFunctionsWrapper;
class GestureInterpreterLibevdevCros;

// Not for public consumption, so we wrap it in namespace internal.
namespace internal {
struct GestureDevicePropertyData;
class MatchCriteria;
struct ConfigurationSection;
}

// A struct holding device properties that are useful when interacting with
// the gestures lib.
struct GestureDeviceProperties {
  int area_left = 0;
  int area_right = 0;
  int area_top = 0;
  int area_bottom = 0;
  int res_y = 0;
  int res_x = 0;
  int orientation_minimum = 0;
  int orientation_maximum = 0;
  GesturesPropBool raw_passthrough;
  GesturesPropBool dump_debug_log;
};

// Provide the interface to access the CrOS gesture library properties.
// It maintains the property data for all input devices that runs with
// the gesture library.
//
// The class also parses the configuration files on the system to
// initialize the specified property values. The configuration files are
// currently in the xorg-conf format so that they can be shared with non-Ozone
// builds.
class COMPONENT_EXPORT(EVDEV) GesturePropertyProvider {
 public:
  // Property types.
  enum PropertyType {
    PT_INT,
    PT_SHORT,
    PT_BOOL,
    PT_STRING,
    PT_REAL,
  };

  // Pointer that leads to the device info.
  typedef Evdev* DevicePtr;

  // The device ids are only maintained by EventFactoryEvdev to identify the
  // input devices and are not to be confused with the Evdev input node id, for
  // example.
  typedef int DeviceId;

  GesturePropertyProvider();

  GesturePropertyProvider(const GesturePropertyProvider&) = delete;
  GesturePropertyProvider& operator=(const GesturePropertyProvider&) = delete;

  ~GesturePropertyProvider();

  // Get a list of device ids that matches a device type. Return true if the
  // list is not empty. |device_ids| can be NULL. Existing data in |device_ids|
  // won't be deleted.
  bool GetDeviceIdsByType(const EventDeviceType type,
                          std::vector<DeviceId>* device_ids);

  // Check if a device id matches a device type. Return true if it matches.
  // Return false if it doesn't match or if it doesn't use
  // the GesturePropertyProvider.
  bool IsDeviceIdOfType(const DeviceId device_id, const EventDeviceType type);

  // Get the GesturesProp object. Returns NULL if not found.
  //
  // The user may use the object returned to set/get the property value in the
  // gesture library's memory. Note that the values in preferences are not
  // synced with the ones here in realtime - they are only applied from the
  // preference side in a single way once appropriate (e.g., when the user
  // clicked "OK").
  GesturesProp* GetProperty(const DeviceId device_id, const std::string& name);

  // Get the names of all properties of one device. Mostly used for the logging
  // purpose.
  std::vector<std::string> GetPropertyNamesById(const DeviceId device_id);

  // Get the (Evdev) device name. Mostly used for the logging purpose.
  std::string GetDeviceNameById(const DeviceId device_id);

 private:
  friend class GesturesPropFunctionsWrapper;

  // Mapping table from a device id to its device pointer.
  typedef std::map<DeviceId, DevicePtr> DeviceMap;

  // Register a device. Setup data-structures and the device's default
  // properties.
  void RegisterDevice(const DeviceId id, const DevicePtr device);

  // Unregister a device. Remove all of its properties being tracked.
  void UnregisterDevice(const DeviceId id);

  // Called by functions in GesturesPropFunctionsWrapper to manipulate
  // properties. Note these functions do not new/delete the GesturesProp
  // pointers. It is caller's responsibility to manage them.
  void AddProperty(const DeviceId device_id,
                   const std::string& name,
                   std::unique_ptr<GesturesProp> property);
  void DeleteProperty(const DeviceId device_id, const std::string& name);

  // Check if a property exists for a device. Return if it is found.
  GesturesProp* FindProperty(const DeviceId device_id, const std::string& name);

  // Get the default value of a property based on the configuration files.
  GesturesProp* GetDefaultProperty(const DeviceId device_id,
                                   const std::string& name);

  // The device configuration files are parsed and stored in the memory upon
  // Chrome starts. The default property values are then applied to each device
  // when it is attached/detected.
  void LoadDeviceConfigurations();

  // Parse a xorg-conf file. We ignore all sections other than InputClass.
  // Check the xorg-conf spec for more infomation about its format.
  void ParseXorgConfFile(const std::string& content);

  // Create a match criteria.
  std::unique_ptr<internal::MatchCriteria> CreateMatchCriteria(
      const std::string& match_type,
      const std::string& arg);

  // Load the DMI product name from sysfs. Returns true if successful.
  bool LoadDmiProductName();

  // Create a property that comes from the conf files.
  std::unique_ptr<GesturesProp> CreateDefaultProperty(const std::string& name,
                                                      const std::string& value);

  // Setup default property values for a newly found device.
  void SetupDefaultProperties(const DeviceId device_id, const DevicePtr device);

  // Map from device ids to device pointers.
  DeviceMap device_map_;

  // Mapping table from a device id to its property data.
  // GestureDevicePropertyData contains both properties in use and default
  // properties whose values will be applied upon the device attachment.
  std::unordered_map<DeviceId,
                     std::unique_ptr<internal::GestureDevicePropertyData>>
      device_data_map_;

  // A vector of parsed sections in configuration files. Owns MatchCriterias,
  // GesturesProps and ConfigurationSections in it.
  std::vector<std::unique_ptr<internal::ConfigurationSection>> configurations_;

  // The system's DMI product name.
  std::string dmi_product_name_;
  // Whether dmi_product_name_ has been loaded successfully yet.
  bool dmi_product_name_loaded_ = false;
};

// Wrapper of GesturesProp related functions. We group them together so that we
// can friend them all at once.
class GesturesPropFunctionsWrapper {
 public:
  // Property provider interface implementation.
  //
  // These functions will create a GesturesProp object that can link back to the
  // memory that holds the real value, which is often declared in the gesture
  // lib.
  static GesturesProp* CreateInt(void* device_data,
                                 const char* name,
                                 int* value,
                                 size_t count,
                                 const int* init);
  static GesturesProp* CreateShort(void* device_data,
                                   const char* name,
                                   short* value,
                                   size_t count,
                                   const short* init);
  static GesturesProp* CreateBool(void* device_data,
                                  const char* name,
                                  GesturesPropBool* value,
                                  size_t count,
                                  const GesturesPropBool* init);

  // String GestureProps needs special care due to the use of const char* in the
  // gesture lib. Its argument list is also different from numeric properties'.
  static GesturesProp* CreateString(void* device_data,
                                    const char* name,
                                    const char** value,
                                    const char* init);

  static GesturesProp* CreateReal(void* device_data,
                                  const char* name,
                                  double* value,
                                  size_t count,
                                  const double* init);

  // Set the handlers to call when a property is accessed.
  static void RegisterHandlers(void* device_data,
                               GesturesProp* property,
                               void* handler_data,
                               GesturesPropGetHandler get,
                               GesturesPropSetHandler set);

  // Free a property.
  static void Free(void* device_data, GesturesProp* property);

  // Initialize hardware-related device properties which will be used in the
  // gesture lib.
  static bool InitializeDeviceProperties(void* device_data,
                                         GestureDeviceProperties* properties);

  // Unregister device from the gesture property provider.
  static void UnregisterDevice(void* device_data);

 private:
  // Property helper functions.
  // Core template function for creating GestureProps. Used by numerical types.
  template <typename T, class PROPTYPE>
  static GesturesProp* CreateProperty(void* device_data,
                                      const char* name,
                                      T* value,
                                      size_t count,
                                      const T* init);

  // Do things that should happen BEFORE we create the property.
  static bool PreCreateProperty(void* device_data,
                                const char* name,
                                GesturesProp** default_property);

  // Do things that should happen AFTER we create the property.
  static void PostCreateProperty(void* device_data,
                                 const char* name,
                                 std::unique_ptr<GesturesProp> property);

  // Some other utility functions used in InitializeDeviceProperties.
  static GesturesProp* CreateIntSingle(void* device_data,
                                       const char* name,
                                       int* value,
                                       int init);
  static GesturesProp* CreateBoolSingle(void* device_data,
                                        const char* name,
                                        GesturesPropBool* value,
                                        GesturesPropBool init);

  // Routines to extract information from the GestureInterpreterLibevdevCros
  // pointer.
  static GesturePropertyProvider* GetPropertyProvider(void* device_data);
  static GesturePropertyProvider::DevicePtr GetDevicePointer(void* device_data);
  static GesturePropertyProvider::DeviceId GetDeviceId(void* device_data);
};

extern const GesturesPropProvider kGesturePropProvider;

}  // namespace ui

// GesturesProp logging function.
std::ostream& operator<<(std::ostream& os, const GesturesProp& prop);

// Implementation of GesturesProp declared in gestures.h
//
// libgestures requires that this be in the top level namespace. We have also
// to put it in the header file so that other classes will be able to use the
// gesture property objects.
struct GesturesProp {
 public:
  typedef ui::GesturePropertyProvider::PropertyType PropertyType;

  GesturesProp(const std::string& name,
               const PropertyType type,
               const size_t count);

  GesturesProp(const GesturesProp&) = delete;
  GesturesProp& operator=(const GesturesProp&) = delete;

  virtual ~GesturesProp() {}

  // Variant-ish interfaces for accessing the property value. Each type of
  // property should override the corresponding interfaces for it.
  virtual std::vector<int> GetIntValue() const;
  virtual bool SetIntValue(const std::vector<int>& value);
  virtual std::vector<int16_t> GetShortValue() const;
  virtual bool SetShortValue(const std::vector<int16_t>& value);
  virtual std::vector<bool> GetBoolValue() const;
  virtual bool SetBoolValue(const std::vector<bool>& value);
  virtual std::string GetStringValue() const;
  virtual bool SetStringValue(const std::string& value);
  virtual std::vector<double> GetDoubleValue() const;
  virtual bool SetDoubleValue(const std::vector<double>& value);

  // Set property access handlers.
  void SetHandlers(GesturesPropGetHandler get,
                   GesturesPropSetHandler set,
                   void* data);

  // Accessors.
  const std::string& name() const { return name_; }
  PropertyType type() const { return type_; }
  size_t count() const { return count_; }
  virtual bool IsReadOnly() const = 0;

 protected:
  // Functions to be called when the property value was accessed.
  void OnGet() const;
  void OnSet() const;

 private:
  // For logging purpose.
  friend std::ostream& operator<<(std::ostream& os,
                                  const GesturesProp& property);

  // Interfaces for getting internal pointers and stuff.
  virtual const char** GetStringWritebackPtr() const;
  virtual bool IsAllocated() const;

  // Property name, type and number of elements.
  std::string name_;
  PropertyType type_;
  size_t count_;

  // Handler function pointers and the data to be passed to them when the
  // property is accessed.
  GesturesPropGetHandler get_ = nullptr;
  GesturesPropSetHandler set_ = nullptr;
  void* handler_data_ = nullptr;
};

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_PROPERTY_PROVIDER_H_
