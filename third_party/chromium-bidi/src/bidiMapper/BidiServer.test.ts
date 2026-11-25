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
import type {ChromiumBidi, GoogSession} from '../protocol/protocol.js';
import {Deferred} from '../utils/Deferred.js';

import {BidiServer} from './BidiServer.js';
import type {BidiTransport} from './BidiTransport.js';
import type {
  AclHandler,
  UpdateAclHandler,
} from './modules/session/AclManager.js';

describe('BidiServer', () => {
  async function createTestServer(
    restrictedMode?: boolean,
    aclHandler?: AclHandler,
  ) {
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
      isCloseError: sinon.stub().returns(false),
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
      aclHandler,
    );

    const dispatchCommand = (command: ChromiumBidi.Command): void => {
      onMessage?.(command);
    };

    const waitForResponse = async (
      id: number,
    ): Promise<ChromiumBidi.Message> => {
      // Wait for the async message processing, but fail instead of hanging if
      // the response never arrives.
      for (let i = 0; i < 1000; i++) {
        const response = sentMessages.find(
          (message) => 'id' in message && message.id === id,
        );
        if (response !== undefined) {
          return response;
        }
        await new Promise((resolve) => setTimeout(resolve, 0));
      }
      assert.fail(`No response to command ${id}.`);
    };

    const sendCommand = async (
      command: ChromiumBidi.Command,
    ): Promise<ChromiumBidi.Message> => {
      dispatchCommand(command);
      return await waitForResponse(command.id);
    };

    return {
      server,
      sentMessages,
      dispatchCommand,
      waitForResponse,
      sendCommand,
    };
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

  describe('aclHandler', () => {
    const RESTRICTED_SESSION_PARAMS = {
      capabilities: {
        alwaysMatch: {'goog:sessionType': 'restricted'},
      },
    };

    it('routes goog:session.setAcl to aclHandler in restrictedMode and enforces its decision', async () => {
      const receivedCalls: {acl: GoogSession.Acl; reason?: string}[] = [];
      const denied = {
        acl: {
          default: {
            commands: ['browsingContext.navigate'],
            events: ['log.entryAdded'],
          },
        },
        reason: 'User denied navigation and logs.',
      };
      const aclHandler: AclHandler & {defaultAcl: GoogSession.Acl} = {
        defaultAcl: {
          default: {
            // Embedder-supplied default grant not requested by the client.
            commands: ['browsingContext.getTree'],
            events: ['browsingContext.navigationCommitted'],
          },
        },
        setAcl(acl, reason) {
          receivedCalls.push({acl, reason});
          return Promise.resolve({
            acl: this.defaultAcl,
            denied,
          });
        },
      };

      const {server, sendCommand} = await createTestServer(true, aclHandler);
      try {
        // Pre-session setAcl and standard session.new must not invoke the handler.
        const preSessionSetAcl = await sendCommand({
          id: 1,
          method: 'goog:session.setAcl',
          params: {acl: {}},
        });
        assert.include(preSessionSetAcl, {
          type: 'error',
          id: 1,
          error: 'invalid session id',
        });

        const standardSession = await sendCommand({
          id: 2,
          method: 'session.new',
          params: {capabilities: {}},
        });
        assert.include(standardSession, {
          type: 'error',
          id: 2,
          error: 'session not created',
        });
        assert.isEmpty(receivedCalls);

        const restrictedSession = await sendCommand({
          id: 3,
          method: 'session.new',
          params: RESTRICTED_SESSION_PARAMS,
        });
        assert.include(restrictedSession, {type: 'success', id: 3});

        const setAclResponse = await sendCommand({
          id: 4,
          method: 'goog:session.setAcl',
          params: {
            acl: {
              default: {
                commands: [
                  'browsingContext.navigate',
                  // Unsupported commands are dropped before calling the handler.
                  'network.addIntercept',
                ],
                events: [
                  'browsingContext.navigationCommitted',
                  'log.entryAdded',
                ],
              },
            },
            reason: 'Check the page.',
          },
        });

        assert.deepEqual(receivedCalls, [
          {
            acl: {
              default: {
                commands: ['browsingContext.navigate'],
                events: [
                  'browsingContext.navigationCommitted',
                  'log.entryAdded',
                ],
              },
              perNavigable: {},
            },
            reason: 'Check the page.',
          },
        ]);
        assert.deepEqual(setAclResponse, {
          type: 'success',
          id: 4,
          result: {
            acl: {
              default: {
                commands: ['browsingContext.getTree'],
                events: ['browsingContext.navigationCommitted'],
              },
              perNavigable: {},
            },
            denied,
          },
        });

        // Embedder-granted command succeeds, denied command is rejected.
        const treeResponse = await sendCommand({
          id: 5,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.deepEqual(treeResponse, {
          type: 'success',
          id: 5,
          result: {contexts: []},
        });

        const navigateResponse = await sendCommand({
          id: 6,
          method: 'browsingContext.navigate',
          params: {context: 'unknown-context', url: 'about:blank'},
        });
        assert.include(navigateResponse, {
          type: 'error',
          id: 6,
          error: 'unsupported operation',
        });
      } finally {
        server.close();
      }
    });

    it('uses aclHandler only for restricted sessions when restrictedMode is false', async () => {
      const standardSetAcl = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .callsFake((acl) => Promise.resolve({acl}));
      const standard = await createTestServer(false, {setAcl: standardSetAcl});
      try {
        const session = await standard.sendCommand({
          id: 1,
          method: 'session.new',
          params: {capabilities: {}},
        });
        assert.include(session, {type: 'success', id: 1});

        const tree = await standard.sendCommand({
          id: 2,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.include(tree, {type: 'success', id: 2});

        const setAclResponse = await standard.sendCommand({
          id: 3,
          method: 'goog:session.setAcl',
          params: {acl: {}},
        });
        assert.include(setAclResponse, {
          type: 'error',
          id: 3,
          error: 'unsupported operation',
        });
        sinon.assert.notCalled(standardSetAcl);
      } finally {
        standard.server.close();
      }

      const restrictedSetAcl = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .resolves({
          acl: {default: {commands: ['browsingContext.getTree']}},
        });
      const restricted = await createTestServer(false, {
        setAcl: restrictedSetAcl,
      });
      try {
        const session = await restricted.sendCommand({
          id: 1,
          method: 'session.new',
          params: RESTRICTED_SESSION_PARAMS,
        });
        assert.include(session, {type: 'success', id: 1});

        const setAclResponse = await restricted.sendCommand({
          id: 2,
          method: 'goog:session.setAcl',
          params: {
            acl: {
              default: {
                commands: ['browsingContext.getTree', 'browsingContext.create'],
              },
            },
          },
        });
        assert.deepEqual(setAclResponse, {
          type: 'success',
          id: 2,
          result: {
            acl: {
              default: {commands: ['browsingContext.getTree'], events: []},
              perNavigable: {},
            },
          },
        });
        sinon.assert.calledOnce(restrictedSetAcl);

        const tree = await restricted.sendCommand({
          id: 3,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.include(tree, {type: 'success', id: 3});

        const create = await restricted.sendCommand({
          id: 4,
          method: 'browsingContext.create',
          params: {type: 'tab'},
        });
        assert.include(create, {
          type: 'error',
          id: 4,
          error: 'unsupported operation',
        });
      } finally {
        restricted.server.close();
      }
    });

    it('isolates aclHandler state across concurrent BidiServer instances', async () => {
      const getTreeAcl: GoogSession.Acl = {
        default: {commands: ['browsingContext.getTree']},
      };
      const grantSetAcl = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .resolves({acl: getTreeAcl});
      const denySetAcl = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .resolves({
          acl: {},
          denied: {acl: getTreeAcl, reason: 'Denied'},
        });

      const first = await createTestServer(true, {setAcl: grantSetAcl});
      try {
        const second = await createTestServer(true, {setAcl: denySetAcl});
        try {
          await first.sendCommand({
            id: 1,
            method: 'session.new',
            params: RESTRICTED_SESSION_PARAMS,
          });
          await second.sendCommand({
            id: 1,
            method: 'session.new',
            params: RESTRICTED_SESSION_PARAMS,
          });

          assert.deepEqual(
            await first.sendCommand({
              id: 2,
              method: 'goog:session.setAcl',
              params: {acl: getTreeAcl},
            }),
            {
              type: 'success',
              id: 2,
              result: {
                acl: {
                  default: {commands: ['browsingContext.getTree'], events: []},
                  perNavigable: {},
                },
              },
            },
          );
          assert.deepEqual(
            await second.sendCommand({
              id: 2,
              method: 'goog:session.setAcl',
              params: {acl: getTreeAcl},
            }),
            {
              type: 'success',
              id: 2,
              result: {
                acl: {
                  default: {commands: [], events: []},
                  perNavigable: {},
                },
                denied: {acl: getTreeAcl, reason: 'Denied'},
              },
            },
          );
          sinon.assert.calledOnce(grantSetAcl);
          sinon.assert.calledOnce(denySetAcl);

          assert.include(
            await first.sendCommand({
              id: 3,
              method: 'browsingContext.getTree',
              params: {},
            }),
            {type: 'success', id: 3},
          );
          assert.include(
            await second.sendCommand({
              id: 3,
              method: 'browsingContext.getTree',
              params: {},
            }),
            {
              type: 'error',
              id: 3,
              error: 'unsupported operation',
            },
          );
        } finally {
          second.server.close();
        }
      } finally {
        first.server.close();
      }
    });

    it('returns unknown error and preserves the previous ACL when aclHandler.setAcl rejects', async () => {
      const setAcl = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .callsFake((acl) => Promise.resolve({acl}));
      const {server, sendCommand} = await createTestServer(true, {setAcl});
      try {
        await sendCommand({
          id: 1,
          method: 'session.new',
          params: RESTRICTED_SESSION_PARAMS,
        });
        await sendCommand({
          id: 2,
          method: 'goog:session.setAcl',
          params: {acl: {default: {commands: ['browsingContext.getTree']}}},
        });

        setAcl.onSecondCall().rejects(new Error('embedder prompt failed'));
        const failedSetAcl = await sendCommand({
          id: 3,
          method: 'goog:session.setAcl',
          params: {acl: {}},
        });
        assert.include(failedSetAcl, {
          type: 'error',
          id: 3,
          error: 'unknown error',
          message: 'embedder prompt failed',
        });

        // Previous ACL remains active.
        const tree = await sendCommand({
          id: 4,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.include(tree, {type: 'success', id: 4});

        // Subsequent setAcl still succeeds.
        const nextSetAcl = await sendCommand({
          id: 5,
          method: 'goog:session.setAcl',
          params: {acl: {}},
        });
        assert.include(nextSetAcl, {type: 'success', id: 5});

        const treeAfterRevoke = await sendCommand({
          id: 6,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.include(treeAfterRevoke, {
          type: 'error',
          id: 6,
          error: 'unsupported operation',
        });

        // Non-Error rejections still return a valid `unknown error` response.
        for (const [id, rejection] of [
          [7, 'cancelled'],
          [8, undefined],
        ] as const) {
          setAcl.callsFake(() => Promise.reject(rejection));
          const nonErrorRejection = await sendCommand({
            id,
            method: 'goog:session.setAcl',
            params: {acl: {}},
          });
          assert.include(nonErrorRejection, {
            type: 'error',
            id,
            error: 'unknown error',
            message: String(rejection),
          });
        }
      } finally {
        server.close();
      }
    });

    it('rejects aclHandler without a callable setAcl before sending CDP commands', async () => {
      for (const invalidHandler of [null, {}, {setAcl: 'not-a-function'}]) {
        const setOnMessage = sinon.stub();
        const bidiTransport: BidiTransport = {
          setOnMessage,
          sendMessage: sinon.stub().resolves(),
          close: sinon.stub(),
        };
        const sendCommand = sinon.stub().resolves({});
        const browserCdpClient = {
          sendCommand,
          on: sinon.stub(),
          off: sinon.stub(),
          isCloseError: sinon.stub().returns(false),
        } as unknown as CdpClient;
        const cdpConnection = {
          getCdpClient: sinon.stub().returns(browserCdpClient),
        } as unknown as CdpConnection;

        let error: unknown;
        try {
          await BidiServer.createAndStart(
            bidiTransport,
            cdpConnection,
            browserCdpClient,
            'self-target-id',
            undefined,
            undefined,
            true,
            invalidHandler as unknown as AclHandler,
          );
        } catch (err) {
          error = err;
        }
        assert.instanceOf(error, TypeError);
        assert.strictEqual(
          (error as TypeError).message,
          'aclHandler.setAcl must be a function',
        );
        sinon.assert.notCalled(setOnMessage);
        sinon.assert.notCalled(sendCommand);
      }
    });

    it('does not block other commands while setAcl is pending and queues concurrent setAcl calls', async () => {
      const firstPending = new Deferred<GoogSession.SetAclResult>();
      const secondPending = new Deferred<GoogSession.SetAclResult>();
      const setAcl = sinon
        .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
        .callsFake((acl) => Promise.resolve({acl}));
      const {server, dispatchCommand, waitForResponse, sendCommand} =
        await createTestServer(true, {setAcl});
      try {
        await sendCommand({
          id: 1,
          method: 'session.new',
          params: RESTRICTED_SESSION_PARAMS,
        });
        await sendCommand({
          id: 2,
          method: 'goog:session.setAcl',
          params: {acl: {default: {commands: ['browsingContext.getTree']}}},
        });
        setAcl.resetHistory();
        setAcl.onFirstCall().returns(firstPending);
        setAcl.onSecondCall().returns(secondPending);

        dispatchCommand({
          id: 3,
          method: 'goog:session.setAcl',
          params: {acl: {}},
        });
        dispatchCommand({
          id: 4,
          method: 'goog:session.setAcl',
          params: {
            acl: {default: {commands: ['browsingContext.navigate']}},
          },
        });
        await new Promise((resolve) => setTimeout(resolve, 0));

        // Second setAcl waits for the first one.
        sinon.assert.calledOnce(setAcl);

        // Other commands do not wait and use the previous ACL.
        const treeDuringPending = await sendCommand({
          id: 5,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.include(treeDuringPending, {type: 'success', id: 5});

        firstPending.resolve({acl: {}});
        assert.include(await waitForResponse(3), {type: 'success', id: 3});

        await new Promise((resolve) => setTimeout(resolve, 0));
        sinon.assert.calledTwice(setAcl);

        secondPending.resolve({
          acl: {default: {commands: ['browsingContext.navigate']}},
        });
        assert.include(await waitForResponse(4), {type: 'success', id: 4});

        const treeAfter = await sendCommand({
          id: 6,
          method: 'browsingContext.getTree',
          params: {},
        });
        assert.include(treeAfter, {
          type: 'error',
          id: 6,
          error: 'unsupported operation',
        });

        // `browsingContext.navigate` now passes the ACL check and fails on the
        // unknown browsing context instead of `unsupported operation`.
        const navigateAfter = await sendCommand({
          id: 7,
          method: 'browsingContext.navigate',
          params: {context: 'unknown-context', url: 'about:blank'},
        });
        assert.include(navigateAfter, {
          type: 'error',
          id: 7,
          error: 'no such frame',
        });
      } finally {
        server.close();
      }
    });

    it('drops pending setAcl settlements without sending messages or unhandled rejections after close', async () => {
      const unhandledRejections: unknown[] = [];
      const onUnhandledRejection = (reason: unknown) => {
        unhandledRejections.push(reason);
      };
      process.on('unhandledRejection', onUnhandledRejection);
      try {
        for (const settle of [
          (d: Deferred<GoogSession.SetAclResult>) => d.resolve({acl: {}}),
          (d: Deferred<GoogSession.SetAclResult>) =>
            d.reject(new Error('embedder closed')),
        ]) {
          const pending = new Deferred<GoogSession.SetAclResult>();
          const setAcl = sinon
            .stub<Parameters<UpdateAclHandler>, ReturnType<UpdateAclHandler>>()
            .returns(pending);
          const {server, sentMessages, dispatchCommand, sendCommand} =
            await createTestServer(true, {setAcl});

          await sendCommand({
            id: 1,
            method: 'session.new',
            params: RESTRICTED_SESSION_PARAMS,
          });
          dispatchCommand({
            id: 2,
            method: 'goog:session.setAcl',
            params: {acl: {}},
          });
          await new Promise((resolve) => setTimeout(resolve, 0));
          sinon.assert.calledOnce(setAcl);

          const messagesBeforeClose = sentMessages.length;
          server.close();
          settle(pending);
          await new Promise((resolve) => setTimeout(resolve, 0));

          assert.lengthOf(sentMessages, messagesBeforeClose);
        }
        assert.isEmpty(unhandledRejections);
      } finally {
        process.off('unhandledRejection', onUnhandledRejection);
      }
    });
  });
});
