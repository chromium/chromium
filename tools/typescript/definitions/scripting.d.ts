// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.scripting API. */
// TODO(crbug.com/40179454): Auto-generate this file from
// chrome/common/extensions/api/scripting.idl.

declare namespace chrome {
  export namespace scripting {
    interface InjectionTarget {
      allFrames?: boolean;
      tabId: number;
    }

    interface ScriptInjection {
      files?: string[];
      target: InjectionTarget;
    }

    interface InjectionResult {
      documentId: string;
      frameId: number;
      result: any;
    }

    export function executeScript(injection?: ScriptInjection):
        Promise<InjectionResult[]>;
  }
}
