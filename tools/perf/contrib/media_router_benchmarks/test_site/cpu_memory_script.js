/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Script to interact with test extension to get CPU/memory usage.
 *
 */


// MR test extension ID.
var extensionId = 'ocihafebdemjoofhbdnamghkfobfgbal';

/**
 * The dictionary to store the performance results with the following format:
 * key: the metric, e.g. CPU, private memory
 * value: map of the performance results for different processes.
 *    key: process type, e.g. tab, browser, gpu, extension.
 *    value: list of the performance results per second. Task Manager notifies
 *           the event listener nearly every second for the latest status of
 *           each process.
 */
window.perfResults = {};

/**
 * Connects to the test extension and starts to collect CPU/memory usage data.
 */
function collectPerfData() {
  processCpuPort_ = openPort_('collectData');
  if (processCpuPort_) {
    processCpuPort_.onMessage.addListener(function(message) {
      for (metric in message) {
        if (!window.perfResults[metric]) {
          window.perfResults[metric] = {};
        }
        for (process_type in message[metric]) {
          if (!window.perfResults[metric][process_type]) {
            window.perfResults[metric][process_type] = []
          }
          window.perfResults[metric][process_type].push(
            message[metric][process_type]);
        }
      }
    });
  } else {
    console.log('Unable to connect to port');
  }
}

function openPort_(name) {
  var rt = window.chrome.runtime;
  if (rt && rt.connect) {
    console.info('Opening port named ' + name);
    return rt.connect(extensionId, {'name': name});
  }
  return null;
}
