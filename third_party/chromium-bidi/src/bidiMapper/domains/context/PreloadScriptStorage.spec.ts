/**
 * Copyright 2023 Google LLC.
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
import {expect} from 'chai';
import * as sinon from 'sinon';

import {CdpTarget} from './cdpTarget.js';
import {PreloadScriptStorage} from './PreloadScriptStorage';
import {BidiPreloadScript} from './bidiPreloadScript.js';

const MOCKED_UUID_1 = '00000000-0000-0000-0000-00000000000a';
const MOCKED_UUID_2 = '00000000-0000-0000-0000-00000000000b';
const CDP_TARGET_ID = 'TARGET_ID';

describe('PreloadScriptStorage', () => {
  let preloadScriptStorage: PreloadScriptStorage;

  let cdpTarget: sinon.SinonStubbedInstance<CdpTarget>;

  beforeEach(() => {
    preloadScriptStorage = new PreloadScriptStorage();
    cdpTarget = sinon.createStubInstance(CdpTarget);
    sinon.stub(cdpTarget, 'targetId').get(() => CDP_TARGET_ID);
  });

  it('initial state', () => {
    expect(preloadScriptStorage.findPreloadScripts()).to.be.empty;
    expect(preloadScriptStorage.findPreloadScripts({})).to.be.empty;
    expect(
      preloadScriptStorage.findPreloadScripts({
        id: '',
      })
    ).to.be.empty;
    expect(
      preloadScriptStorage.findPreloadScripts({
        contextId: null,
      })
    ).to.be.empty;
    expect(
      preloadScriptStorage.findPreloadScripts({
        contextId: '',
      })
    ).to.be.empty;
  });

  [
    {contextDescription: 'global context', context: null},
    {contextDescription: 'non-global context', context: 'CONTEXT_1'},
  ].forEach(({contextDescription, context}) => {
    it(`add preload scripts in ${contextDescription}`, () => {
      const preloadScript1 = createPreloadScript(MOCKED_UUID_1, context);
      const preloadScript2 = createPreloadScript(MOCKED_UUID_2, context);

      preloadScriptStorage.addPreloadScript(preloadScript1);
      preloadScriptStorage.addPreloadScript(preloadScript2);

      expect(
        preloadScriptStorage.findPreloadScripts({contextId: context})
      ).to.deep.equal([preloadScript1, preloadScript2]);
    });

    it(`remove non-existing BiDi id in ${contextDescription}`, () => {
      const preloadScript1 = createPreloadScript(MOCKED_UUID_1, context);
      const preloadScript2 = createPreloadScript(MOCKED_UUID_2, context);
      preloadScriptStorage.addPreloadScript(preloadScript1);
      preloadScriptStorage.addPreloadScript(preloadScript2);

      const preloadScripts = preloadScriptStorage.findPreloadScripts({
        contextId: context,
      });

      preloadScriptStorage.removeBiDiPreloadScripts({
        contextId: context,
        id: `${MOCKED_UUID_1}_NON_EXISTING`,
      });

      expect(
        preloadScriptStorage.findPreloadScripts({contextId: context})
      ).to.be.deep.equal(preloadScripts);
    });

    [
      {filterDescription: 'context id', filter: {contextId: null}},
      {filterDescription: 'context ids', filter: {contextIds: [null]}},
      {
        filterDescription: 'bidi id',
        filter: {id: MOCKED_UUID_1},
      },
      {
        filterDescription: 'target id',
        filter: {targetId: CDP_TARGET_ID},
      },
    ].forEach(({filterDescription, filter}) => {
      it(`find preload scripts in ${contextDescription} by ${filterDescription}`, () => {
        const preloadScript1 = createPreloadScript(MOCKED_UUID_1, null);
        preloadScriptStorage.addPreloadScript(preloadScript1);

        expect(preloadScriptStorage.findPreloadScripts(filter)).to.deep.equal([
          preloadScript1,
        ]);
      });

      it(`remove preload scripts by in ${contextDescription} by ${filterDescription}`, () => {
        const preloadScript1 = createPreloadScript(MOCKED_UUID_1, context);
        preloadScriptStorage.addPreloadScript(preloadScript1);
        preloadScriptStorage.removeBiDiPreloadScripts(filter);

        expect(preloadScriptStorage.findPreloadScripts(filter)).to.be.empty;
      });
    });
  });

  afterEach(() => {
    sinon.restore();
  });

  function createPreloadScript(
    id: string,
    context: null | string
  ): BidiPreloadScript {
    const preloadScript = sinon.createStubInstance(BidiPreloadScript);
    sinon.stub(preloadScript, 'id').get(() => id);
    sinon.stub(preloadScript, 'contextId').get(() => context);
    sinon.stub(preloadScript, 'targetIds').get(() => new Set([CDP_TARGET_ID]));
    return preloadScript;
  }
});
