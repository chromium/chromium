// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum StorageUnitType {
  // The storage has fixed media, e.g. hard disk or SSD.
  "fixed",
  // The storage is removable, e.g. USB flash drive.
  "removable",
  // The storage type is unknown.
  "unknown"
};

dictionary StorageUnitInfo {
  // The transient ID that uniquely identifies the storage device.
  // This ID will be persistent within the same run of a single application.
  // It will not be a persistent identifier between different runs of an
  // application, or between different applications.
  required DOMString id;
  // The name of the storage unit.
  required DOMString name;
  // The media type of the storage unit.
  required StorageUnitType type;
  // The total amount of the storage space, in bytes.
  required double capacity;
};

dictionary StorageAvailableCapacityInfo {
  // A copied |id| of getAvailableCapacity function parameter |id|.
  required DOMString id;
  // The available capacity of the storage device, in bytes.
  required double availableCapacity;
};

enum EjectDeviceResultCode {
  // The ejection command is successful -- the application can prompt the user
  // to remove the device.
  "success",
  // The device is in use by another application. The ejection did not
  // succeed; the user should not remove the device until the other
  // application is done with the device.
  "in_use",
  // There is no such device known.
  "no_such_device",
  // The ejection command failed.
  "failure"
};

callback OnAttachedListener = undefined(StorageUnitInfo info);

interface OnAttachedEvent : ExtensionEvent {
  static undefined addListener(OnAttachedListener listener);
  static undefined removeListener(OnAttachedListener listener);
  static boolean hasListener(OnAttachedListener listener);
};

callback OnDetachedListener = undefined(DOMString id);

interface OnDetachedEvent : ExtensionEvent {
  static undefined addListener(OnDetachedListener listener);
  static undefined removeListener(OnDetachedListener listener);
  static boolean hasListener(OnDetachedListener listener);
};

// Use the <code>chrome.system.storage</code> API to query storage device
// information and be notified when a removable storage device is attached and
// detached.
interface Storage {
  // Get the storage information from the system. The argument passed to the
  // callback is an array of StorageUnitInfo objects.
  // |PromiseValue|: info
  [requiredCallback] static Promise<sequence<StorageUnitInfo>> getInfo();

  // Ejects a removable storage device.
  // |PromiseValue|: result
  [requiredCallback] static Promise<EjectDeviceResultCode> ejectDevice(
      DOMString id);

  // Get the available capacity of a specified |id| storage device.
  // The |id| is the transient device ID from StorageUnitInfo.
  // |PromiseValue|: info
  [requiredCallback] static Promise<StorageAvailableCapacityInfo>
      getAvailableCapacity(DOMString id);

  // Fired when a new removable storage is attached to the system.
  static attribute OnAttachedEvent onAttached;

  // Fired when a removable storage is detached from the system.
  static attribute OnDetachedEvent onDetached;
};

partial interface System {
  static attribute Storage storage;
};

partial interface Browser {
  static attribute System system;
};
