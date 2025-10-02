/**
 * Copyright 2025 Google LLC.
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

import {Speculation} from '../../../protocol/protocol.js';
import type {LoggerFn} from '../../../utils/log.js';
import {LogType} from '../../../utils/log.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {EventManager} from '../session/EventManager.js';

export class SpeculationProcessor {
  #eventManager: EventManager;
  readonly #logger: LoggerFn | undefined;

  constructor(eventManager: EventManager, logger: LoggerFn | undefined) {
    this.#eventManager = eventManager;
    this.#logger = logger;
  }

  onCdpTargetCreated(cdpTarget: CdpTarget) {
    cdpTarget.cdpClient.on('Preload.prefetchStatusUpdated', (event) => {
      let prefetchStatus: Speculation.PreloadingStatus;
      switch (event.status) {
        case 'Running':
          prefetchStatus = Speculation.PreloadingStatus.Pending;
          break;
        case 'Ready':
          prefetchStatus = Speculation.PreloadingStatus.Ready;
          break;
        case 'Success':
          prefetchStatus = Speculation.PreloadingStatus.Success;
          break;
        case 'Failure':
          prefetchStatus = Speculation.PreloadingStatus.Failure;
          break;
        default:
          // If status is not recognized, skip the event
          this.#logger?.(
            LogType.debugWarn,
            `Unknown prefetch status: ${event.status}`,
          );
          return;
      }
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: 'speculation.prefetchStatusUpdated',
          params: {
            context: event.initiatingFrameId,
            url: event.prefetchUrl,
            status: prefetchStatus,
          },
        },
        cdpTarget.id,
      );
    });
  }
}
