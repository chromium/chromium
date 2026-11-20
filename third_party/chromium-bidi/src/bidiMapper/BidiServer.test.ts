/**
 * Copyright 2026 Google LLC.
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

import {describe, it} from 'node:test';

import {assert} from 'chai';
import sinon from 'sinon';

import type {CdpClient} from '../cdp/CdpClient.js';
import type {CdpConnection} from '../cdp/CdpConnection.js';
import type {ChromiumBidi} from '../protocol/protocol.js';

import {BidiServer} from './BidiServer.js';
import type {BidiTransport} from './BidiTransport.js';

describe('BidiServer', () => {
  async function createTestServer(restrictedMode?: boolean) {
    let onMessage: ((message: ChromiumBidi.Command) => void) | undefined;
    const sentMessages: ChromiumBidi.Message[] = [];
    const bidiTransport: BidiTransport = {
      setOnMessage(callback) {
        onMessage = callback;
      },
      sendMessage(message) {
        sentMessages.push(message);
        return Promise.resolve();
      },
      close: sinon.stub(),
    };

    const browserCdpClient = {
      sendCommand: sinon.stub().callsFake((method: string) => {
        switch (method) {
          case 'Target.getBrowserContexts':
            return Promise.resolve({
              defaultBrowserContextId: 'default',
              browserContextIds: ['default'],
            });
          case 'Target.getTargets':
            return Promise.resolve({targetInfos: []});
          case 'Browser.getVersion':
            return Promise.resolve({
              product: 'Chrome/test',
              revision: '1',
              userAgent: 'test-ua',
            });
          default:
            return Promise.resolve({});
        }
      }),
      on: sinon.stub(),
      off: sinon.stub(),
    } as unknown as CdpClient;

    const cdpConnection = {
      getCdpClient: sinon.stub().returns(browserCdpClient),
    } as unknown as CdpConnection;

    const server = await BidiServer.createAndStart(
      bidiTransport,
      cdpConnection,
      browserCdpClient,
      'self-target-id',
      undefined,
      undefined,
      restrictedMode,
    );

    const sendCommand = async (
      command: ChromiumBidi.Command,
    ): Promise<ChromiumBidi.Message> => {
      onMessage?.(command);
      // Wait for the async message processing, but fail instead of hanging if
      // the response never arrives.
      for (let i = 0; i < 1000; i++) {
        const response = sentMessages.find(
          (message) => 'id' in message && message.id === command.id,
        );
        if (response !== undefined) {
          return response;
        }
        await new Promise((resolve) => setTimeout(resolve, 0));
      }
      assert.fail(`No response to command ${command.id}.`);
    };

    return {server, sendCommand};
  }

  it('rejects commands other than session.new and session.status before session.new', async () => {
    const {server, sendCommand} = await createTestServer();
    try {
      const treeBeforeSession = await sendCommand({
        id: 1,
        method: 'browsingContext.getTree',
        params: {},
      });
      assert.deepEqual(treeBeforeSession, {
        type: 'error',
        id: 1,
        error: 'invalid session id',
        message:
          "Command 'browsingContext.getTree' requires a session. Call 'session.new' first.",
        stacktrace: undefined,
      });

      const setAclBeforeSession = await sendCommand({
        id: 2,
        method: 'goog:session.setAcl',
        params: {acl: {}},
      });
      assert.include(setAclBeforeSession, {
        type: 'error',
        id: 2,
        error: 'invalid session id',
      });

      const status = await sendCommand({
        id: 3,
        method: 'session.status',
        params: {},
      });
      assert.include(status, {type: 'success', id: 3});

      const standardSession = await sendCommand({
        id: 4,
        method: 'session.new',
        params: {capabilities: {}},
      });
      assert.include(standardSession, {type: 'success', id: 4});

      // The ACL of a standard session is not enforced.
      const treeAfterSession = await sendCommand({
        id: 5,
        method: 'browsingContext.getTree',
        params: {},
      });
      assert.deepEqual(treeAfterSession, {
        type: 'success',
        id: 5,
        result: {contexts: []},
      });
    } finally {
      server.close();
    }
  });

  it('requires a restricted session when restrictedMode is true', async () => {
    const {server, sendCommand} = await createTestServer(true);
    try {
      const rejectedSession = await sendCommand({
        id: 1,
        method: 'session.new',
        params: {capabilities: {}},
      });
      assert.include(rejectedSession, {
        type: 'error',
        id: 1,
        error: 'session not created',
        message:
          'Only restricted sessions are allowed. Request capability "goog:sessionType": "restricted".',
      });

      const restrictedSession = await sendCommand({
        id: 2,
        method: 'session.new',
        params: {
          capabilities: {
            alwaysMatch: {'goog:sessionType': 'restricted'},
          },
        },
      });
      assert.nestedInclude(restrictedSession, {
        type: 'success',
        id: 2,
        'result.capabilities.goog:sessionType': 'restricted',
      });

      // The ACL of the restricted session starts empty.
      const treeAfterSession = await sendCommand({
        id: 3,
        method: 'browsingContext.getTree',
        params: {},
      });
      assert.include(treeAfterSession, {
        type: 'error',
        id: 3,
        error: 'unsupported operation',
      });
    } finally {
      server.close();
    }
  });
});
