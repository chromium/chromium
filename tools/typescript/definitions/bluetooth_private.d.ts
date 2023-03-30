// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Type definitions for chrome.bluetoothPrivate API. */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace bluetoothPrivate {
      export enum PairingEventType {
        REQUEST_PINCODE = 'requestPincode',
        DISPLAY_PINCODE = 'displayPincode',
        REQUEST_PASSKEY = 'requestPasskey',
        DISPLAY_PASSKEY = 'displayPasskey',
        KEYS_ENTERED = 'keysEntered',
        CONFIRM_PASSKEY = 'confirmPasskey',
        REQUEST_AUTHORIZATION = 'requestAuthorization',
        COMPLETE = 'complete',
      }

      export enum ConnectResultType {
        ALREADY_CONNECTED = 'alreadyConnected',
        AUTH_CANCELED = 'authCanceled',
        AUTH_FAILED = 'authFailed',
        AUTH_REJECTED = 'authRejected',
        AUTH_TIMEOUT = 'authTimeout',
        FAILED = 'failed',
        IN_PROGRESS = 'inProgress',
        SUCCESS = 'success',
        UNKNOWN_ERROR = 'unknownError',
        UNSUPPORTED_DEVICE = 'unsupportedDevice',
        NOT_READY = 'notReady',
        ALREADY_EXISTS = 'alreadyExists',
        NOT_CONNECTED = 'notConnected',
        DOES_NOT_EXIST = 'doesNotExist',
        INVALID_ARGS = 'invalidArgs',
      }

      export enum PairingResponse {
        CONFIRM = 'confirm',
        REJECT = 'reject',
        CANCEL = 'cancel',
      }

      export interface PairingEvent {
        pairing: PairingEventType;
        device: chrome.bluetooth.Device;
        pincode?: string;
        passkey?: number;
        enteredKey?: number;
      }

      export interface SetPairingResponseOptions {
        device: chrome.bluetooth.Device;
        response: PairingResponse;
        pincode?: string;
        passkey?: number;
      }

      export function setPairingResponse(options: SetPairingResponseOptions):
          Promise<void>;

      export function disconnectAll(deviceAddress: string): Promise<void>;

      export function forgetDevice(deviceAddress: string): Promise<void>;

      export function connect(deviceAddress: string):
          Promise<ConnectResultType>;

      export function pair(deviceAddress: string): Promise<void>;

      export const onPairing: ChromeEvent<(pairingEvent: PairingEvent) => void>;
    }
  }
}
