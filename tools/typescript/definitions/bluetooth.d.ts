// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Type definitions for chrome.bluetooth API. */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace bluetooth {
      export enum VendorIdSource {
        BLUETOOTH = 'bluetooth',
        USB = 'usb',
      }

      export enum DeviceType {
        COMPUTER = 'computer',
        PHONE = 'phone',
        MODEM = 'modem',
        AUDIO = 'audio',
        CAR_AUDIO = 'carAudio',
        VIDEO = 'video',
        PERIPHERAL = 'peripheral',
        JOYSTICK = 'joystick',
        GAMEPAD = 'gamepad',
        KEYBOARD = 'keyboard',
        MOUSE = 'mouse',
        TABLET = 'tablet',
        KEYBOARD_MOUSE_COMBO = 'keyboardMouseCombo',
      }

      export enum Transport {
        INVALID = 'invalid',
        CLASSIC = 'classic',
        LE = 'le',
        DUAL = 'dual',
      }

      export interface Device {
        address: string;
        name?: string;
        deviceClass?: number;
        vendorIdSource?: VendorIdSource;
        vendorId?: number;
        productId?: number;
        deviceId?: number;
        type?: DeviceType;
        paired?: boolean;
        connected?: boolean;
        connecting?: boolean;
        connectable?: boolean;
        uuids?: string[];
        inquiryRssi?: number;
        inquiryTxPower?: number;
        transport?: Transport;
        batteryPercentage?: number;
      }

      export function getDevice(deviceAddress: string): Promise<Device>;
      export function getDevices(): Promise<Device[]>;
      export function startDiscovery(): Promise<void>;
      export function stopDiscovery(): Promise<void>;

      export const onDeviceAdded: ChromeEvent<(device: Device) => void>;
      export const onDeviceChanged: ChromeEvent<(device: Device) => void>;
      export const onDeviceRemoved: ChromeEvent<(device: Device) => void>;
    }
  }
}
