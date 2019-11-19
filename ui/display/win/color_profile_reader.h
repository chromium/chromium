// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_COLOR_PROFILE_READER_H_
#define UI_DISPLAY_WIN_COLOR_PROFILE_READER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/display/display_export.h"
#include "ui/gfx/icc_profile.h"

#include <map>
#include <string>

namespace display {
namespace win {

// Monitor ICC profiles are stored in the filesystem, and a blocking read from
// file is required to read them. This is expensive and shouldn't be done on the
// main thread, so this class manages asynchronously doing these readings and
// calling back into its client when the profiles are noticed to have changed.
class DISPLAY_EXPORT ColorProfileReader {
 public:
  class Client {
   public:
    virtual void OnColorProfilesChanged() = 0;
  };

  ColorProfileReader(Client* client);
  ~ColorProfileReader();

  // Check to see if the screen profile filenames have changed. If so, spawn a
  // task to update them. When the task has completed, this will call the
  // client's OnColorProfileReaderChanged on the main thread.
  void UpdateIfNeeded();

  // Look up the color space for a given device name. If the device's color
  // profile has not yet been read, this will return sRGB (which is what the
  // file on disk will say most of the time).
  gfx::ColorSpace GetDisplayColorSpace(int64_t id) const;

 private:
  typedef std::map<base::string16, base::string16> DeviceToPathMap;
  typedef std::map<base::string16, std::string> DeviceToDataMap;

  // Enumerate displays and return a map to their ICC profile path. This
  // needs to be run off of the main thread.
  static ColorProfileReader::DeviceToPathMap
  BuildDeviceToPathMapOnBackgroundThread();

  // Called on the main thread when the device paths have been retrieved
  void BuildDeviceToPathMapCompleted(DeviceToPathMap new_device_to_path_map);

  // Do the actual reading from the filesystem. This needs to be run off of the
  // main thread.
  static DeviceToDataMap ReadProfilesOnBackgroundThread(
      DeviceToPathMap new_device_to_path_map);

  // Called on the main thread when the read has completed.
  void ReadProfilesCompleted(DeviceToDataMap device_to_data_map);

  Client* const client_ = nullptr;
  // Set to true once profiles have been read at least once, to avoid
  // histogramming the default value.
  bool has_read_profiles_ = false;
  bool update_in_flight_ = false;
  DeviceToPathMap device_to_path_map_;
  std::map<int64_t, gfx::ICCProfile> display_id_to_profile_map_;
  base::WeakPtrFactory<ColorProfileReader> weak_factory_{this};
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_COLOR_PROFILE_READER_H_
