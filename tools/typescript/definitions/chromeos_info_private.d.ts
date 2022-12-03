// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* @fileoverview Definitions for chrome.chromeosInfoPrivate API */

declare namespace chrome {
  export namespace chromeosInfoPrivate {

    enum SessionType {
      NORMAL = 'normal',
      KIOSK = 'kiosk',
      PUBLIC_SESSION = 'public session',
    }

    enum PlayStoreStatus {
      NOT_AVAILABLE = 'not available',
      AVAILABLE = 'available',
      ENABLED = 'enabled',
    }

    enum ManagedDeviceStatus {
      MANAGED = 'managed',
      NOT_MANAGED = 'not managed',
    }

    enum DeviceType {
      CHROMEBASE = 'chromebase',
      CHROMEBIT = 'chromebit',
      CHROMEBOOK = 'chromebook',
      CHROMEBOX = 'chromebox',
      CHROMEDEVICE = 'chromedevice',
    }

    enum StylusStatus {
      UNSUPPORTED = 'unsupported',
      SUPPORTED = 'supported',
      SEEN = 'seen',
    }

    enum AssistantStatus {
      UNSUPPORTED = 'unsupported',
      SUPPORTED = 'supported',
    }

    export interface GetCustomizationValuesResult {
      board?: string;
      customizationId?: string;
      homeProvider?: string;
      hwid?: string;
      isMeetDevice?: boolean;
      initialLocale?: string;
      isOwner?: boolean;
      sessionType?: SessionType;
      playStoreStatus?: PlayStoreStatus;
      managedDeviceStatus?: ManagedDeviceStatus;
      deviceType?: DeviceType;
      stylusStatus?: StylusStatus;
      assistantStatus?: AssistantStatus;
      clientId?: string;
      timezone?: string;
      a11yLargeCursorEnabled?: boolean;
      a11yStickyKeysEnabled?: boolean;
      a11ySpokenFeedbackEnabled?: boolean;
      a11yHighContrastEnabled?: boolean;
      a11yScreenMagnifierEnabled?: boolean;
      a11yAutoClickEnabled?: boolean;
      a11yVirtualKeyboardEnabled?: boolean;
      a11yCaretHighlightEnabled?: boolean;
      a11yCursorHighlightEnabled?: boolean;
      a11yFocusHighlightEnabled?: boolean;
      a11ySelectToSpeakEnabled?: boolean;
      a11ySwitchAccessEnabled?: boolean;
      a11yCursorColorEnabled?: boolean;
      a11yDockedMagnifierEnabled?: boolean;
      sendFunctionKeys?: boolean;
      supportedTimezones?: string[][];
    }

    export function get(propertyNames: string[]):
        Promise<GetCustomizationValuesResult>;
  }
}
