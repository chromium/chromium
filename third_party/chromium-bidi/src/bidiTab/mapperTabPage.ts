/**
 * Copyright 2022 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
import {type LogPrefix, LogType} from '../utils/log.js';

/** HTML source code for the user-facing Mapper tab. */
const mapperPageSource =
  '<!DOCTYPE html><title>BiDi-CDP Mapper</title><style>body{font-family: Roboto, serif; font-size: 13px; color: #202124;}.log{padding: 12px; font-family: Menlo, Consolas, Monaco, Liberation Mono, Lucida Console, monospace; font-size: 11px; line-height: 180%; background: #f1f3f4; border-radius: 4px;}.pre{overflow-wrap: break-word; padding: 10px;}.card{margin: 60px auto; padding: 2px 0; max-width: 900px; box-shadow: 0 1px 4px rgba(0, 0, 0, 0.15), 0 1px 6px rgba(0, 0, 0, 0.2); border-radius: 8px;}.divider{height: 1px; background: #f0f0f0;}.item{padding: 16px 20px;}</style><div class="card"><div class="item"><h1>BiDi-CDP Mapper is controlling this tab</h1><p>Closing or reloading it will stop the BiDi process. <a target="_blank" title="BiDi-CDP Mapper GitHub Repository" href="https://github.com/GoogleChromeLabs/chromium-bidi">Details.</a></p></div><div class="divider"></div><details id="details"><summary class="item">Debug information</summary></details></div>';

/**
 * The following piece of HTML should be added to the `debug` element:
 *
 * <div class="divider"></div>
 * <div class="item">
 * <h3>${name}</h3>
 * <div id="${name}_log" class="log">
 */
function findOrCreateTypeLogContainer(logPrefix: LogPrefix): HTMLElement {
  const logType = logPrefix.split(':')[0];
  const containerId = `${logType}_log`;

  const existingContainer = document.getElementById(containerId);
  if (existingContainer) {
    return existingContainer;
  }

  const debugElement = document.getElementById('details')!;

  const divider = document.createElement('div');
  divider.className = 'divider';
  debugElement.appendChild(divider);

  const htmlItem = document.createElement('div');
  htmlItem.className = 'item';
  htmlItem.innerHTML = `<h3>${logType}</h3><div id="${containerId}" class="log"></div>`;
  debugElement.appendChild(htmlItem);

  return document.getElementById(containerId)!;
}

export function generatePage() {
  // If run not in browser (e.g. unit test), do nothing.
  if (!globalThis.document.documentElement) {
    return;
  }
  globalThis.document.documentElement.innerHTML = mapperPageSource;

  // Create main log containers in proper order.
  findOrCreateTypeLogContainer(LogType.debugInfo);
  findOrCreateTypeLogContainer(LogType.bidi);
  findOrCreateTypeLogContainer(LogType.cdp);
}

function stringify(message: unknown) {
  if (typeof message === 'object') {
    return JSON.stringify(message, null, 2);
  }
  return message;
}

export function log(logPrefix: LogPrefix, ...messages: unknown[]) {
  // If run not in browser (e.g. unit test), do nothing.
  if (!globalThis.document.documentElement) {
    return;
  }

  // Skip sending BiDi logs as they are logged once by `bidi:server:*`
  if (!logPrefix.startsWith(LogType.bidi)) {
    // If `sendDebugMessage` is defined, send the log message there.
    globalThis.window?.sendDebugMessage?.(
      JSON.stringify({logType: logPrefix, messages})
    );
  }

  const typeLogContainer = findOrCreateTypeLogContainer(logPrefix);

  // This piece of HTML should be added:
  // <div class="pre">...log message...</div>
  const lineElement = document.createElement('div');
  lineElement.className = 'pre';
  lineElement.textContent = [logPrefix, ...messages].map(stringify).join(' ');
  typeLogContainer.appendChild(lineElement);
  if (typeLogContainer.childNodes.length > 200) {
    typeLogContainer.removeChild(typeLogContainer.childNodes[0]!);
  }
}
