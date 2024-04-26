// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.chromeosInfoPrivate API
 * Generated from: chrome/common/extensions/api/chromeos_info_private.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/chromeos_info_private.json -g ts_definitions` to
 * regenerate.
 */



declare namespace chrome {
  export namespace chromeosInfoPrivate {

    export enum PropertyName {
      TIMEZONE = 'timezone',
      A11Y_LARGE_CURSOR_ENABLED = 'a11yLargeCursorEnabled',
      A11Y_STICKY_KEYS_ENABLED = 'a11yStickyKeysEnabled',
      A11Y_SPOKEN_FEEDBACK_ENABLED = 'a11ySpokenFeedbackEnabled',
      A11Y_HIGH_CONTRAST_ENABLED = 'a11yHighContrastEnabled',
      A11Y_SCREEN_MAGNIFIER_ENABLED = 'a11yScreenMagnifierEnabled',
      A11Y_AUTO_CLICK_ENABLED = 'a11yAutoClickEnabled',
      A11Y_VIRTUAL_KEYBOARD_ENABLED = 'a11yVirtualKeyboardEnabled',
      A11Y_CARET_HIGHLIGHT_ENABLED = 'a11yCaretHighlightEnabled',
      A11Y_CURSOR_HIGHLIGHT_ENABLED = 'a11yCursorHighlightEnabled',
      A11Y_FOCUS_HIGHLIGHT_ENABLED = 'a11yFocusHighlightEnabled',
      A11Y_SELECT_TO_SPEAK_ENABLED = 'a11ySelectToSpeakEnabled',
      A11Y_SWITCH_ACCESS_ENABLED = 'a11ySwitchAccessEnabled',
      A11Y_CURSOR_COLOR_ENABLED = 'a11yCursorColorEnabled',
      A11Y_DOCKED_MAGNIFIER_ENABLED = 'a11yDockedMagnifierEnabled',
      SEND_FUNCTION_KEYS = 'sendFunctionKeys',
    }

    export enum SessionType {
      NORMAL = 'normal',
      KIOSK = 'kiosk',
      PUBLIC_SESSION = 'public session',
    }

    export enum PlayStoreStatus {
      NOT_AVAILABLE = 'not available',
      AVAILABLE = 'available',
      ENABLED = 'enabled',
    }

    export enum ManagedDeviceStatus {
      MANAGED = 'managed',
      NOT_MANAGED = 'not managed',
    }

    export enum DeviceType {
      CHROMEBASE = 'chromebase',
      CHROMEBIT = 'chromebit',
      CHROMEBOOK = 'chromebook',
      CHROMEBOX = 'chromebox',
      CHROMEDEVICE = 'chromedevice',
    }

    export enum StylusStatus {
      UNSUPPORTED = 'unsupported',
      SUPPORTED = 'supported',
      SEEN = 'seen',
    }

    export enum AssistantStatus {
      UNSUPPORTED = 'unsupported',
      SUPPORTED = 'supported',
    }

    export function get(propertyNames: string[]): Promise<{
      board?: string,
      customizationId?: string,
      homeProvider?: string,
      hwid?: string,
      deviceRequisition?: string,
      isMeetDevice?: boolean,
      initialLocale?: string,
      isOwner?: boolean,
      sessionType?: SessionType,
      playStoreStatus?: PlayStoreStatus,
      managedDeviceStatus?: ManagedDeviceStatus,
      deviceType?: DeviceType,
      stylusStatus?: StylusStatus,
      assistantStatus?: AssistantStatus,
      clientId?: string,
      timezone?: string,
      a11yLargeCursorEnabled?: boolean,
      a11yStickyKeysEnabled?: boolean,
      a11ySpokenFeedbackEnabled?: boolean,
      a11yHighContrastEnabled?: boolean,
      a11yScreenMagnifierEnabled?: boolean,
      a11yAutoClickEnabled?: boolean,
      a11yVirtualKeyboardEnabled?: boolean,
      a11yCaretHighlightEnabled?: boolean,
      a11yCursorHighlightEnabled?: boolean,
      a11yFocusHighlightEnabled?: boolean,
      a11ySelectToSpeakEnabled?: boolean,
      a11ySwitchAccessEnabled?: boolean,
      a11yCursorColorEnabled?: boolean,
      a11yDockedMagnifierEnabled?: boolean,
      sendFunctionKeys?: boolean,
      supportedTimezones?: string[][],
    }>;

    export function set(propertyName: PropertyName, propertyValue: any): void;

    export function isTabletModeEnabled(): Promise<boolean>;

    export function isRunningOnLacros(): Promise<boolean>;

  }
}
