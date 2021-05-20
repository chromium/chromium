/**
 * Copyright 2021 Google Inc. All rights reserved.
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
import { log } from './log';
const logCdp = log('cdp');

export function createCdpClient(cdpBinding) {
  const commandCallbacks = [];
  const messageHandlers = [];

  const _onCdpMessage = function (messageStr) {
    logCdp('received < ' + messageStr);

    const message = JSON.parse(messageStr);

    if (commandCallbacks.hasOwnProperty(message.id)) {
      commandCallbacks[message.id](message.result);
    } else {
      for (let handler of messageHandlers) handler(message);
    }
  };

  cdpBinding.onmessage = _onCdpMessage;

  return {
    sendCdpCommand: async function (cdpCommand) {
      const id = commandCallbacks.length;
      cdpCommand.id = id;

      const cdpCommandStr = JSON.stringify(cdpCommand);
      logCdp('sent > ' + cdpCommandStr);
      cdpBinding.send(cdpCommandStr);
      return new Promise((resolve) => {
        commandCallbacks[id] = resolve;
      });
    },
    setCdpMessageHandler: function (handler) {
      messageHandlers.push(handler);
    },
  };
}
