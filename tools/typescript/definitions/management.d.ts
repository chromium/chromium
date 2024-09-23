// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.management API. */
// TODO(crbug.com/40179454): Auto-generate this file.

declare namespace chrome {
  export namespace management {
    export interface ExtensionInfo {
      id: string;
      name: string;
      shortName: string;
      description: string;
      version: string;
      versionName?: string;
      mayDisable: boolean;
      mayEnable?: boolean;
      enabled: boolean;
      // TODO(crbug.com/40173780): Define commented out fields as needed.
      //disabledReason?: ExtensionDisabledReason;
      isApp: boolean;
      //type: ExtensionType;
      appLaunchUrl?: string;
      homepageUrl?: string;
      updateUrl?: string;
      offlineEnabled: boolean;
      optionsUrl: string;
      //icons?: Array<IconInfo>;
      permissions: string[];
      hostPermissions: string[];
      //installType?: ExtensionInstallType;
      //launchType?: LaunchType;
      //availableLaunchTypes?: Array<LaunchType>;
    }

    export interface UninstallOptions {
      showConfirmDialog?: boolean;
    }

    export function get(id: string): Promise<ExtensionInfo>;
    export function uninstall(id: string, options?: UninstallOptions):
        Promise<void>;
    export function setEnabled(id: string, enabled: boolean): Promise<void>;
  }
}
