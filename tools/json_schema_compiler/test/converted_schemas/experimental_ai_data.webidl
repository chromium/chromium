// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Experimental API to handle data collection in the browser process for
// AI features.
[implemented_in="chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"]
interface ExperimentalAiData {
  // |PromiseValue|: data
  [requiredCallback] static Promise<ArrayBuffer> getAiData(long domNodeId,
                                                           DOMString frameId,
                                                           DOMString userInput,
                                                           long tabId);
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<ArrayBuffer> getAiDataWithSpecifier(
      long tabId,
      ArrayBuffer aiDataSpecifier);
};

partial interface Browser {
  static attribute ExperimentalAiData experimentalAiData;
};
