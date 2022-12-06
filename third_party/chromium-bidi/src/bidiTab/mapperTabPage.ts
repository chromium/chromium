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
import {LogType} from '../utils/log';

declare global {
  interface Window {
    MapperTabPage: MapperTabPage;
  }
}

export class MapperTabPage {
  // HTML source code for the user-facing Mapper tab.
  static #mapperPageSource =
    '<!DOCTYPE html><title>BiDi-CDP Mapper</title><style>body{font-family: Roboto, serif; font-size: 13px; color: #202124;}.log{padding: 12px; font-family: Menlo, Consolas, Monaco, Liberation Mono, Lucida Console, monospace; font-size: 11px; line-height: 180%; background: #f1f3f4; border-radius: 4px;}.pre{overflow-wrap: break-word; padding: 10px;}.card{margin: 60px auto; padding: 2px 0; max-width: 900px; box-shadow: 0 1px 4px rgba(0, 0, 0, 0.15), 0 1px 6px rgba(0, 0, 0, 0.2); border-radius: 8px;}.divider{height: 1px; background: #f0f0f0;}.item{padding: 16px 20px;}</style><div class="card"><div class="item"><h1>BiDi-CDP Mapper is controlling this tab</h1><p>Closing or reloading it will stop the BiDi process. <a target="_blank" title="BiDi-CDP Mapper GitHub Repository" href="https://github.com/GoogleChromeLabs/chromium-bidi">Details.</a></p></div><div class="divider"></div><details id="details"><summary class="item">Debug information</summary></details></div>';

  static generatePage() {
    // If run not in browser (e.g. unit test), do nothing.
    if (!globalThis.document?.documentElement) {
      return;
    }
    window.MapperTabPage = MapperTabPage;
    window.document.documentElement.innerHTML = this.#mapperPageSource;
    // Create main log containers in proper order.
    this.#findOrCreateTypeLogContainer('System');
    this.#findOrCreateTypeLogContainer('BiDi Messages');
    this.#findOrCreateTypeLogContainer('Browsing Contexts');
    this.#findOrCreateTypeLogContainer('CDP');
  }

  static log(logType: LogType, ...messages: unknown[]) {
    // If run not in browser (e.g. unit test), do nothing.
    if (!globalThis.document?.documentElement) {
      return;
    }
    const typeLogContainer = this.#findOrCreateTypeLogContainer(logType);

    // This piece of HTML should be added:
    /*
      <div class="pre">...log message...</div>
    */
    const lineElement = document.createElement('div');
    lineElement.className = 'pre';
    lineElement.textContent = messages.join(', ');
    typeLogContainer.appendChild(lineElement);
  }

  // This piece of HTML should be added to the `debug` element:
  /*
      <div class="divider"></div>
      <div class="item">
        <h3>${name}</h3>
        <div id="${name}_log" class="log">
  */
  static #findOrCreateTypeLogContainer(logType: string) {
    const containerId = logType + '_log';

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
}
