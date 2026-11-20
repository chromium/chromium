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
      const countBefore = sentMessages.length;
      onMessage?.(command);
      // Wait for async message processing queue to flush.
      while (sentMessages.length === countBefore) {
        await new Promise((resolve) => setTimeout(resolve, 0));
      }
      return sentMessages.at(-1)!;
    };

    return {server, sendCommand};
  }

  it('enables ACL from the start and requires restricted session when restrictedMode is true', async () => {
    const {server, sendCommand} = await createTestServer(true);
    try {
      const preSessionResponse = await sendCommand({
        id: 1,
        method: 'browsingContext.getTree',
        params: {},
      });
      assert.deepEqual(preSessionResponse, {
        type: 'error',
        id: 1,
        error: 'unsupported operation',
        message:
          "Command 'browsingContext.getTree' is not allowed by the session ACL.",
        stacktrace: undefined,
      });

      const rejectedSession = await sendCommand({
        id: 2,
        method: 'session.new',
        params: {capabilities: {}},
      });
      assert.include(rejectedSession, {
        type: 'error',
        id: 2,
        error: 'session not created',
        message:
          'Only restricted sessions are allowed. Request capability "goog:sessionType": "restricted".',
      });

      const restrictedSession = await sendCommand({
        id: 3,
        method: 'session.new',
        params: {
          capabilities: {
            alwaysMatch: {'goog:sessionType': 'restricted'},
          },
        },
      });
      assert.deepEqual(restrictedSession, {
        type: 'success',
        id: 3,
        result: {
          sessionId: 'unknown',
          capabilities: {
            unhandledPromptBehavior: undefined,
            'goog:sessionType': 'restricted',
            acceptInsecureCerts: false,
            browserName: 'Chrome/test',
            browserVersion: '1',
            platformName: '',
            setWindowRect: false,
            webSocketUrl: '',
            userAgent: 'test-ua',
          },
        },
      });
    } finally {
      server.close();
    }
  });

  it('does not enable ACL or require restricted session when restrictedMode is omitted', async () => {
    const {server, sendCommand} = await createTestServer();
    try {
      const treeBeforeSession = await sendCommand({
        id: 1,
        method: 'browsingContext.getTree',
        params: {},
      });
      assert.deepEqual(treeBeforeSession, {
        type: 'success',
        id: 1,
        result: {contexts: []},
      });

      const standardSession = await sendCommand({
        id: 2,
        method: 'session.new',
        params: {capabilities: {}},
      });
      assert.include(standardSession, {
        type: 'success',
        id: 2,
      });
    } finally {
      server.close();
    }
  });
});
