// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestURLSelectionOperation {
  async run(urls, data) {
    if (data && data.hasOwnProperty('mockResult')) {
      return data['mockResult'];
    }

    return -1;
  }
}

class VerifyKeyValue {
  async run(urls, data) {
    if (data && data.hasOwnProperty('expectedKey') &&
        data.hasOwnProperty('expectedValue')) {
      const expectedValue = data['expectedValue'];
      const value = await sharedStorage.get(data['expectedKey']);
      if (value === expectedValue) {
        return 1;
      }
    }
    return -1;
  }
}

class VerifyKeyNotFound {
  async run(urls, data) {
    if (data && data.hasOwnProperty('expectedKey')) {
      const value = await sharedStorage.get(data['expectedKey']);
      if (typeof value === 'undefined') {
        return 1;
      }
    }
    return -1;
  }
}

class ContributeToHistogramOperation {
  async run(data) {
    if (data.enableDebugMode) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
    for (const contribution of data.contributions) {
      privateAggregation.contributeToHistogram(contribution);
    }
    if (data.enableDebugModeAfterOp) {
      privateAggregation.enableDebugMode(data.enableDebugModeArgs);
    }
  }
}

register('contribute-to-histogram', ContributeToHistogramOperation);
register('test-url-selection-operation', TestURLSelectionOperation);
register('verify-key-value', VerifyKeyValue);
register('verify-key-not-found', VerifyKeyNotFound);
