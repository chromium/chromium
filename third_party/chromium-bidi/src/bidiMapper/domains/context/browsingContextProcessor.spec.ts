/**
 * Copyright 2021 Google LLC.
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

import { StubTransport } from '../../../tests/stubTransport.spec';

import * as sinon from 'sinon';

import { BrowsingContextProcessor } from './browsingContextProcessor';
import { CdpClient, CdpConnection } from '../../../cdp';
import { Context } from './context';
import { BrowsingContext } from '../../bidiProtocolTypes';
import { BidiServer, IBidiServer } from '../../utils/bidiServer';
import { EventManager, IEventManager } from '../events/EventManager';

describe('BrowsingContextProcessor', function () {
  let mockCdpServer: StubTransport;
  let browsingContextProcessor: BrowsingContextProcessor;
  let cdpConnection: CdpConnection;
  let bidiServer: IBidiServer;
  let eventManager: IEventManager;

  const EVALUATOR_SCRIPT = 'EVALUATOR_SCRIPT';
  const NEW_CONTEXT_ID = 'NEW_CONTEXT_ID';
  const TARGET_ATTACHED_TO_TARGET_EVENT = {
    method: 'Target.attachedToTarget',
    params: {
      sessionId: '_ANY_VALUE_',
      targetInfo: {
        targetId: NEW_CONTEXT_ID,
        type: 'page',
        title: '',
        url: '',
        attached: true,
        canAccessOpener: false,
        browserContextId: '_ANY_ANOTHER_VALUE_',
      },
      waitingForDebugger: false,
    },
  };

  beforeEach(async function () {
    mockCdpServer = new StubTransport();
    cdpConnection = new CdpConnection(mockCdpServer);
    bidiServer = sinon.createStubInstance(BidiServer);
    eventManager = sinon.createStubInstance(EventManager);

    browsingContextProcessor = new BrowsingContextProcessor(
      cdpConnection,
      'SELF_TARGET_ID',
      bidiServer,
      eventManager,
      EVALUATOR_SCRIPT
    );

    // Actual `Context.create` logic involves several CDP calls, so mock it to avoid all the simulations.
    Context.create = sinon.fake(
      async (_1: string, _2: CdpClient, _3: string) => {
        return sinon.createStubInstance(Context) as unknown as Context;
      }
    );
  });

  describe('handle events', async function () {
    it('`Target.attachedToTarget` creates Context', async function () {
      sinon.assert.notCalled(Context.create as sinon.SinonSpy);
      await mockCdpServer.emulateIncomingMessage(
        TARGET_ATTACHED_TO_TARGET_EVENT
      );
      sinon.assert.calledOnceWithExactly(
        Context.create as sinon.SinonSpy,
        NEW_CONTEXT_ID,
        sinon.match.any, // cdpClient.
        sinon.match.any, // bidiServer.
        sinon.match.any, // eventManager.
        EVALUATOR_SCRIPT
      );
    });
  });

  describe('handle `process_PROTO_browsingContext_create`', async function () {
    const BROWSING_CONTEXT_CREATE_COMMAND: BrowsingContext.CreateCommand = {
      method: 'browsingContext.create',
      params: {},
    };

    const EXPECTED_TARGET_CREATE_TARGET_CALL = {
      id: 0,
      method: 'Target.createTarget',
      params: {
        url: 'about:blank',
        newWindow: false,
      },
    };

    const TARGET_CREATE_TARGET_RESPONSE = {
      id: 0,
      result: {
        targetId: NEW_CONTEXT_ID,
      },
    };

    it('Target.attachedToTarget before command finished', async function () {
      const createResultPromise =
        browsingContextProcessor.process_browsingContext_create(
          BROWSING_CONTEXT_CREATE_COMMAND
        );

      sinon.assert.calledOnceWithExactly(
        mockCdpServer.sendMessage,
        JSON.stringify(EXPECTED_TARGET_CREATE_TARGET_CALL)
      );

      await mockCdpServer.emulateIncomingMessage(
        TARGET_ATTACHED_TO_TARGET_EVENT
      );

      await mockCdpServer.emulateIncomingMessage(TARGET_CREATE_TARGET_RESPONSE);

      await createResultPromise;
    });

    it('Target.attachedToTarget after command finished', async function () {
      const createResultPromise =
        browsingContextProcessor.process_browsingContext_create(
          BROWSING_CONTEXT_CREATE_COMMAND
        );

      sinon.assert.calledOnceWithExactly(
        mockCdpServer.sendMessage,
        JSON.stringify(EXPECTED_TARGET_CREATE_TARGET_CALL)
      );

      await mockCdpServer.emulateIncomingMessage(TARGET_CREATE_TARGET_RESPONSE);

      await mockCdpServer.emulateIncomingMessage(
        TARGET_ATTACHED_TO_TARGET_EVENT
      );

      await createResultPromise;
    });
  });
});
