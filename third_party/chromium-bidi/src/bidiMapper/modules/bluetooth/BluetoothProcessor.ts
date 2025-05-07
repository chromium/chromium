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

import {
  type Bluetooth,
  type EmptyResult,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {EventManager} from '../session/EventManager.js';

/** Represents a Bluetooth service. */
class BluetoothService {
  readonly id: string;

  constructor(id: string) {
    this.id = id;
  }
}

/** Represents a Bluetooth device. */
class BluetoothDevice {
  readonly address: string;
  readonly services = new Map<string, BluetoothService>();

  constructor(address: string) {
    this.address = address;
  }
}

export class BluetoothProcessor {
  #eventManager: EventManager;
  #browsingContextStorage: BrowsingContextStorage;
  #bluetoothDevices: Map<string, BluetoothDevice>;

  constructor(
    eventManager: EventManager,
    browsingContextStorage: BrowsingContextStorage,
  ) {
    this.#eventManager = eventManager;
    this.#browsingContextStorage = browsingContextStorage;
    this.#bluetoothDevices = new Map();
  }

  async simulateAdapter(
    params: Bluetooth.SimulateAdapterParameters,
  ): Promise<EmptyResult> {
    if (params.state === undefined) {
      // The bluetooth.simulateAdapter Command
      // Step 4.2. If params["state"] does not exist, return error with error code invalid argument.
      // https://webbluetoothcg.github.io/web-bluetooth/#bluetooth-simulateAdapter-command
      throw new InvalidArgumentException(
        `Parameter "state" is required for creating a Bluetooth adapter`,
      );
    }
    const context = this.#browsingContextStorage.getContext(params.context);
    // Bluetooth spec requires overriding the existing adapter (step 6). From the CDP
    // perspective, we need to disable the emulation first.
    // https://webbluetoothcg.github.io/web-bluetooth/#bluetooth-simulateAdapter-command
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.disable',
    );
    this.#bluetoothDevices.clear();
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.enable',
      {
        state: params.state,
        leSupported: params.leSupported ?? true,
      },
    );
    return {};
  }

  async disableSimulation(
    params: Bluetooth.DisableSimulationParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.disable',
    );
    this.#bluetoothDevices.clear();
    return {};
  }

  async simulatePreconnectedPeripheral(
    params: Bluetooth.SimulatePreconnectedPeripheralParameters,
  ): Promise<EmptyResult> {
    if (this.#bluetoothDevices.has(params.address)) {
      throw new InvalidArgumentException(
        `Bluetooth device with address ${params.address} already exists`,
      );
    }
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
    this.#bluetoothDevices.set(
      params.address,
      new BluetoothDevice(params.address),
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

  async simulateGattConnectionResponse(
    params: Bluetooth.SimulateGattConnectionResponseParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.simulateGATTOperationResponse',
      {
        address: params.address,
        type: 'connection',
        code: params.code,
      },
    );
    return {};
  }

  async simulateGattDisconnection(
    params: Bluetooth.SimulateGattDisconnectionParameters,
  ): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await context.cdpTarget.browserCdpClient.sendCommand(
      'BluetoothEmulation.simulateGATTDisconnection',
      {
        address: params.address,
      },
    );
    return {};
  }

  async simulateService(
    params: Bluetooth.SimulateServiceParameters,
  ): Promise<EmptyResult> {
    if (!this.#bluetoothDevices.has(params.address)) {
      throw new InvalidArgumentException(
        `Bluetooth device with address ${params.address} does not exist`,
      );
    }
    const device = this.#bluetoothDevices.get(params.address);
    const context = this.#browsingContextStorage.getContext(params.context);
    switch (params.type) {
      case 'add': {
        if (device!.services.has(params.uuid)) {
          throw new InvalidArgumentException(
            `Service with UUID ${params.uuid} already exists`,
          );
        }
        const response = await context.cdpTarget.browserCdpClient.sendCommand(
          'BluetoothEmulation.addService',
          {
            address: params.address,
            serviceUuid: params.uuid,
          },
        );
        device!.services.set(
          params.uuid,
          new BluetoothService(response.serviceId),
        );
        return {};
      }
      case 'remove': {
        if (!device!.services.has(params.uuid)) {
          throw new InvalidArgumentException(
            `Service with UUID ${params.uuid} does not exist`,
          );
        }
        const service = device!.services.get(params.uuid);
        await context.cdpTarget.browserCdpClient.sendCommand(
          'BluetoothEmulation.removeService',
          {
            serviceId: service!.id,
          },
        );
        device!.services.delete(params.uuid);
        return {};
      }
      default:
        throw new InvalidArgumentException(
          `Parameter "type" of ${params.type} is not supported`,
        );
    }
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
    cdpTarget.browserCdpClient.on(
      'BluetoothEmulation.gattOperationReceived',
      async (event) => {
        switch (event.type) {
          case 'connection':
            this.#eventManager.registerEvent(
              {
                type: 'event',
                method: 'bluetooth.gattConnectionAttempted',
                params: {
                  context: cdpTarget.id,
                  address: event.address,
                },
              },
              cdpTarget.id,
            );
            return;
          case 'discovery':
            // Chromium Web Bluetooth simulation generates this GATT discovery event when
            // a page attempts to get services for a given Bluetooth device for the first time.
            // This 'get services' operation is put on hold until a GATT discovery response
            // is sent to the simulation.
            // Note: Web Bluetooth automation (see https://webbluetoothcg.github.io/web-bluetooth/#automated-testing)
            // does not support simulating a GATT discovery response. This is because simulated services, characteristics,
            // or descriptors are immediately visible to the simulation, meaning it doesn't have a distinct
            // DISCOVERY state. Therefore, this code simulates a successful GATT discovery
            // response upon receiving this event.
            await cdpTarget.browserCdpClient.sendCommand(
              'BluetoothEmulation.simulateGATTOperationResponse',
              {
                address: event.address,
                type: 'discovery',
                code: 0x0,
              },
            );
        }
      },
    );
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
