/**
 * Copyright 2024 Google LLC.
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

import type {Bluetooth, EmptyResult} from '../../../protocol/protocol.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {EventManager} from '../session/EventManager.js';

export class BluetoothProcessor {
  #eventManager: EventManager;
  #browsingContextStorage: BrowsingContextStorage;

  constructor(
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
  ) {
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
  }

  async simulateAdapter(
    params: Bluetooth.SimulateAdapterParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    // Bluetooth spec requires overriding the existing adapter (step 6). From the CDP
    // perspective, we need to disable the emulation first.
    // https://webbluetoothcg.github.io/web-bluetooth/#bluetooth-simulateAdapter-command
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.disable',
    );
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.enable',
      {
        state: params.state,
      },
    );
    return {};
  }

  async simulatePreconnectedPeripheral(
    params: Bluetooth.SimulatePreconnectedPeripheralParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.simulatePreconnectedPeripheral',
      {
        address: params.address,
        name: params.name,
        knownServiceUuids: params.knownServiceUuids,
        manufacturerData: params.manufacturerData,
      },
    );
    return {};
  }

  async simulateAdvertisement(
    params: Bluetooth.SimulateAdvertisementParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.simulateAdvertisement',
      {
        entry: params.scanEntry,
      },
    );
    return {};
  }

  onCdpTargetCreated(cdpTarget: CdpTarget) {
    cdpTarget.cdpClient.on('DeviceAccess.deviceRequestPrompted', (event) => {
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: 'bluetooth.requestDevicePromptUpdated',
          params: {
            context: cdpTarget.id,
            prompt: event.id,
            devices: event.devices,
          },
        },
        cdpTarget.id,
      );
    });
  }

  async handleRequestDevicePrompt(
    params: Bluetooth.HandleRequestDevicePromptParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);

    if (params.accept) {
      await context.cdpTarget.cdpClient.sendCommand(
        'DeviceAccess.selectPrompt',
        {
          id: params.prompt,
          deviceId: params.device,
        },
      );
    } else {
      await context.cdpTarget.cdpClient.sendCommand(
        'DeviceAccess.cancelPrompt',
        {
          id: params.prompt,
        },
      );
    }
    return {};
  }
}
