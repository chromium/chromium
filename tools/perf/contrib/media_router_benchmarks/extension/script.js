// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extension ID: ocihafebdemjoofhbdnamghkfobfgbal
// Collects CPU/memory usage information and posts to the page.
function collectData(port) {
  function processCpuListener(processes) {
    _postData(processes, port, 'cpu');
  }

  function processMemoryListener(processes) {
    _postData(processes, port, 'privateMemory');
  }

  chrome.processes.onUpdated.addListener(processCpuListener);
  chrome.processes.onUpdatedWithMemory.addListener(processMemoryListener);
  port.onDisconnect.addListener(function() {
    chrome.processes.onUpdated.removeListener(processCpuListener);
    chrome.processes.onUpdated.removeListener(processMemoryListener);
  });
}

/**
 * Posts the metric data to the page.
 *
 * @param processes list of current processes.
 * @param port the port used for the communication between the page and
 *             extension.
 * @param metric_name the metric name, e.g cpu.
 */
function _postData(processes, port, metric_name) {
  let tabPid = port.sender.tab.id;
  if (!tabPid) {
    return;
  }
  let tabProcess = "";
  for (let p in processes) {
    for (let task in processes[p].tasks) {
      if (processes[p].tasks[task].tabId == tabPid)
        tabProcess = processes[p].osProcessId;
    }
    if (tabProcess)
      break;
  }
  if (!tabProcess) {
    return;
  }
  let message = {};
  message[metric_name] = {'current_tab': tabProcess[metric_name]};
  for (let pid in processes) {
    let process = processes[pid];
    data = process[metric_name];
    if (['browser', 'gpu', 'extension'].indexOf(process.type) > -1) {
       if (process.type == 'extension'){
         for (let index in process.tasks) {
          let task = process.tasks[index];
          if (task.title && task.title.indexOf('Chrome Media Router') > -1) {
            message[metric_name]['mr_' + process.type] = data;
          }
        }
       } else {
         message[metric_name][process.type] = data;
       }
    }
  }
  port.postMessage(message);
}

chrome.runtime.onConnectExternal.addListener(function(port) {
  if (port.name == 'collectData') {
    collectData(port);
  } else {
    console.warn('Unknown port, disconnect the port.');
    port.disconnect();
  }
});
