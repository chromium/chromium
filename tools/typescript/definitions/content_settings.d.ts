// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare namespace chrome {
  export namespace contentSettings {

    interface GetContenSettingParams {
      primaryUrl: string;
      secondaryUrl?: string;
    }

    interface ContentSetting {
      get(details: GetContenSettingParams,
          callback: (result: {setting: any}) => void): void;
    }

    export const javascript: ContentSetting;
  }
}
