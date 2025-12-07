// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Type of stream an audio device provides.
enum StreamType {
  "INPUT",
  "OUTPUT"
};

// Available audio device types.
enum DeviceType {
  "HEADPHONE",
  "MIC",
  "USB",
  "BLUETOOTH",
  "HDMI",
  "INTERNAL_SPEAKER",
  "INTERNAL_MIC",
  "FRONT_MIC",
  "REAR_MIC",
  "KEYBOARD_MIC",
  "HOTWORD",
  "LINEOUT",
  "POST_MIX_LOOPBACK",
  "POST_DSP_LOOPBACK",
  "ALSA_LOOPBACK",
  "OTHER"
};

dictionary AudioDeviceInfo {
  // The unique identifier of the audio device.
  required DOMString id;
  // Stream type associated with this device.
  required StreamType streamType;
  // Type of the device.
  required DeviceType deviceType;
  // The user-friendly name (e.g. "USB Microphone").
  required DOMString displayName;
  // Device name.
  required DOMString deviceName;
  // True if this is the current active device.
  required boolean isActive;
  // The sound level of the device, volume for output, gain for input.
  required long level;
  // The stable/persisted device id string when available.
  DOMString stableDeviceId;
};

dictionary DeviceFilter {
  // If set, only audio devices whose stream type is included in this list
  // will satisfy the filter.
  sequence<StreamType> streamTypes;

  // If set, only audio devices whose active state matches this value will
  // satisfy the filter.
  boolean isActive;
};

dictionary DeviceProperties {
  // <p>
  //   The audio device's desired sound level. Defaults to the device's
  //   current sound level.
  // </p>
  // <p>If used with audio input device, represents audio device gain.</p>
  // <p>If used with audio output device, represents audio device volume.</p>
  long level;
};

dictionary DeviceIdLists {
  // <p>List of input devices specified by their ID.</p>
  // <p>To indicate input devices should be unaffected, leave this property
  //   unset.</p>
  sequence<DOMString> input;

  // <p>List of output devices specified by their ID.</p>
  // <p>To indicate output devices should be unaffected, leave this property
  //   unset.</p>
  sequence<DOMString> output;
};

dictionary MuteChangedEvent {
  // The type of the stream for which the mute value changed. The updated mute
  // value applies to all devices with this stream type.
  required StreamType streamType;

  // Whether or not the stream is now muted.
  required boolean isMuted;
};

dictionary LevelChangedEvent {
  // ID of device whose sound level has changed.
  required DOMString deviceId;

  // The device's new sound level.
  required long level;
};

callback OnLevelChangedListener = undefined(LevelChangedEvent event);

interface OnLevelChangedEvent : ExtensionEvent {
  static undefined addListener(OnLevelChangedListener listener);
  static undefined removeListener(OnLevelChangedListener listener);
  static boolean hasListener(OnLevelChangedListener listener);
};

callback OnMuteChangedListener = undefined(MuteChangedEvent event);

interface OnMuteChangedEvent : ExtensionEvent {
  static undefined addListener(OnMuteChangedListener listener);
  static undefined removeListener(OnMuteChangedListener listener);
  static boolean hasListener(OnMuteChangedListener listener);
};

// |devices|: List of all present audio devices after the change.
callback OnDeviceListChangedListener = undefined(sequence<AudioDeviceInfo> devices);

interface OnDeviceListChangedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceListChangedListener listener);
  static undefined removeListener(OnDeviceListChangedListener listener);
  static boolean hasListener(OnDeviceListChangedListener listener);
};

// The <code>chrome.audio</code> API is provided to allow users to
// get information about and control the audio devices attached to the
// system.
// This API is currently only available in kiosk mode for ChromeOS.
interface Audio {
  // Gets a list of audio devices filtered based on |filter|.
  // |filter|: Device properties by which to filter the list of returned
  //     audio devices. If the filter is not set or set to <code>{}</code>,
  //     returned device list will contain all available audio devices.
  // |Returns|: Reports the requested list of audio devices.
  // |PromiseValue|: devices
  [requiredCallback] static Promise<sequence<AudioDeviceInfo>> getDevices(
      optional DeviceFilter filter);

  // Sets lists of active input and/or output devices.
  // |ids|: <p>Specifies IDs of devices that should be active. If either the
  //     input or output list is not set, devices in that category are
  //     unaffected.
  //     </p>
  //     <p>It is an error to pass in a non-existent device ID.</p>
  [requiredCallback] static Promise<undefined> setActiveDevices(
      DeviceIdLists ids);

  // Sets the properties for the input or output device.
  [requiredCallback] static Promise<undefined> setProperties(
      DOMString id,
      DeviceProperties properties);

  // Gets the system-wide mute state for the specified stream type.
  // |streamType|: Stream type for which mute state should be fetched.
  // |Returns|: Callback reporting whether mute is set or not for specified
  // stream type.
  // |PromiseValue|: value
  [requiredCallback] static Promise<boolean> getMute(StreamType streamType);

  // Sets mute state for a stream type. The mute state will apply to all audio
  // devices with the specified audio stream type.
  // |streamType|: Stream type for which mute state should be set.
  // |isMuted|: New mute value.
  static Promise<undefined> setMute(
      StreamType streamType,
      boolean isMuted);

  // Fired when sound level changes for an active audio device.
  static attribute OnLevelChangedEvent onLevelChanged;

  // Fired when the mute state of the audio input or output changes.
  // Note that mute state is system-wide and the new value applies to every
  // audio device with specified stream type.
  static attribute OnMuteChangedEvent onMuteChanged;

  // Fired when audio devices change, either new devices being added, or
  // existing devices being removed.
  static attribute OnDeviceListChangedEvent onDeviceListChanged;
};

partial interface Browser {
  static attribute Audio audio;
};
