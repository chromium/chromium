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

import {describe, it, beforeEach} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import {
  type Autofill,
  InvalidArgumentException,
  UnsupportedOperationException,
} from '../../../protocol/protocol.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {getSharedId} from '../script/SharedId.js';

import {AutofillProcessor} from './AutofillProcessor.js';

const CONTEXT_ID = 'context-id';
const DOCUMENT_ID = 'document-id';
const BACKEND_NODE_ID = 42;
const ELEMENT = {
  sharedId: getSharedId(CONTEXT_ID, DOCUMENT_ID, BACKEND_NODE_ID),
};
const CARD: Autofill.Card = {
  number: '4444444444444444',
  name: 'John Smith',
  expiryMonth: '01',
  expiryYear: '2030',
  cvc: '123',
};

async function assertRejectsWith(
  promise: Promise<unknown>,
  errorType: new (message: string) => Error,
  message: string,
): Promise<void> {
  try {
    await promise;
  } catch (error) {
    assert.instanceOf(error, errorType);
    assert.strictEqual((error as Error).message, message);
    return;
  }
  assert.fail(`Expected ${errorType.name} to be thrown`);
}

describe('AutofillProcessor', () => {
  let sendCommand: sinon.SinonStub;
  let callFunction: sinon.SinonStub;
  let processor: AutofillProcessor;

  beforeEach(() => {
    sendCommand = sinon.stub().resolves({});
    callFunction = sinon.stub().resolves({
      type: 'success',
      result: {type: 'boolean', value: true},
      realm: 'realm',
    });
    const context = {
      id: CONTEXT_ID,
      cdpTarget: {cdpClient: {sendCommand}} as unknown as CdpTarget,
      getOrCreateHiddenSandbox: sinon.stub().resolves({callFunction}),
    } as unknown as BrowsingContextImpl;
    const browsingContextStorage = sinon.createStubInstance(
      BrowsingContextStorage,
    );
    browsingContextStorage.getContext.withArgs(CONTEXT_ID).returns(context);
    processor = new AutofillProcessor(browsingContextStorage);
  });

  it('triggers autofill with card data', async () => {
    const result = await processor.trigger({
      context: CONTEXT_ID,
      element: ELEMENT,
      card: CARD,
    });

    assert.deepEqual(result, {});
    sinon.assert.calledOnceWithExactly(sendCommand, 'Autofill.trigger', {
      fieldId: BACKEND_NODE_ID,
      frameId: CONTEXT_ID,
      card: CARD,
      address: undefined,
    });
  });

  it('maps address field names to Chromium field types', async () => {
    await processor.trigger({
      context: CONTEXT_ID,
      element: ELEMENT,
      address: {
        fields: [
          {name: 'given-name', value: 'John'},
          {name: 'family-name', value: 'Smith'},
          {name: 'street-address', value: '123 Main St'},
          {name: 'address-level2', value: 'San Francisco'},
          {name: 'address-level1', value: 'CA'},
          {name: 'postal-code', value: '94102'},
          {name: 'country', value: 'US'},
          {name: 'tel', value: '+14155550100'},
          {name: 'email', value: 'john@example.com'},
        ],
      },
    });

    sinon.assert.calledOnceWithExactly(sendCommand, 'Autofill.trigger', {
      fieldId: BACKEND_NODE_ID,
      frameId: CONTEXT_ID,
      card: undefined,
      address: {
        fields: [
          {name: 'NAME_FIRST', value: 'John'},
          {name: 'NAME_LAST', value: 'Smith'},
          {name: 'ADDRESS_HOME_STREET_ADDRESS', value: '123 Main St'},
          {name: 'ADDRESS_HOME_CITY', value: 'San Francisco'},
          {name: 'ADDRESS_HOME_STATE', value: 'CA'},
          {name: 'ADDRESS_HOME_ZIP', value: '94102'},
          {name: 'ADDRESS_HOME_COUNTRY', value: 'US'},
          {name: 'PHONE_HOME_WHOLE_NUMBER', value: '+14155550100'},
          {name: 'EMAIL_ADDRESS', value: 'john@example.com'},
        ],
      },
    });
  });

  it('calls the element check with the element as `this`', async () => {
    await processor.trigger({
      context: CONTEXT_ID,
      element: ELEMENT,
      card: CARD,
    });

    sinon.assert.calledOnce(callFunction);
    assert.strictEqual(callFunction.firstCall.args[2], ELEMENT);
  });

  it('throws if the element is not a form control with a form owner', async () => {
    callFunction.resolves({
      type: 'success',
      result: {type: 'boolean', value: false},
      realm: 'realm',
    });

    await assertRejectsWith(
      processor.trigger({context: CONTEXT_ID, element: ELEMENT, card: CARD}),
      InvalidArgumentException,
      `Element "${ELEMENT.sharedId}" is not a form control with a form owner`,
    );
    sinon.assert.notCalled(sendCommand);
  });

  it('throws if both card and address are provided', async () => {
    await assertRejectsWith(
      processor.trigger({
        context: CONTEXT_ID,
        element: ELEMENT,
        card: CARD,
        address: {fields: [{name: 'given-name', value: 'John'}]},
      }),
      InvalidArgumentException,
      'Card and address cannot both be provided',
    );
    sinon.assert.notCalled(sendCommand);
  });

  it('throws if neither card nor address are provided', async () => {
    await assertRejectsWith(
      processor.trigger({context: CONTEXT_ID, element: ELEMENT}),
      InvalidArgumentException,
      'Either card or address must be provided',
    );
    sinon.assert.notCalled(sendCommand);
  });

  for (const name of [
    'honorific-suffix',
    'nickname',
    'address-level4',
    'country-name',
  ] as const) {
    it(`throws for unsupported field "${name}"`, async () => {
      await assertRejectsWith(
        processor.trigger({
          context: CONTEXT_ID,
          element: ELEMENT,
          address: {
            fields: [
              {name: 'given-name', value: 'John'},
              {name, value: 'value'},
            ],
          },
        }),
        UnsupportedOperationException,
        `Autofill field "${name}" is not supported`,
      );
      sinon.assert.notCalled(sendCommand);
    });
  }

  it('throws UnsupportedOperationException if CDP Autofill is unavailable', async () => {
    sendCommand.rejects({
      code: -32601,
      message: "'Autofill.trigger' wasn't found",
    });

    await assertRejectsWith(
      processor.trigger({context: CONTEXT_ID, element: ELEMENT, card: CARD}),
      UnsupportedOperationException,
      'Autofill is not supported by this browser',
    );
  });

  it('throws InvalidArgumentException if the field cannot be autofilled', async () => {
    sendCommand.rejects({code: -32600, message: 'Field not found'});

    await assertRejectsWith(
      processor.trigger({context: CONTEXT_ID, element: ELEMENT, card: CARD}),
      InvalidArgumentException,
      `Element "${ELEMENT.sharedId}" cannot be autofilled`,
    );
  });

  it('rethrows other CDP errors', async () => {
    const error = {code: -32000, message: 'Frame not found'};
    sendCommand.rejects(error);

    let thrown: unknown;
    try {
      await processor.trigger({
        context: CONTEXT_ID,
        element: ELEMENT,
        card: CARD,
      });
    } catch (e) {
      thrown = e;
    }
    assert.strictEqual(thrown, error);
  });
});
