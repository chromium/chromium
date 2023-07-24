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

import {
  type EmptyResult,
  Script,
  NoSuchScriptException,
} from '../../../protocol/protocol';
import type {BrowsingContextStorage} from '../context/browsingContextStorage';
import type {CdpTarget} from '../context/cdpTarget';

import type {PreloadScriptStorage} from './PreloadScriptStorage';
import {BidiPreloadScript} from './bidiPreloadScript';
import type {Realm} from './realm';
import type {RealmStorage} from './realmStorage';

export class ScriptProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;
  readonly #realmStorage: RealmStorage;
  readonly #preloadScriptStorage;

  constructor(
    browsingContextStorage: BrowsingContextStorage,
    realmStorage: RealmStorage,
    preloadScriptStorage: PreloadScriptStorage
  ) {
    this.#browsingContextStorage = browsingContextStorage;
    this.#realmStorage = realmStorage;
    this.#preloadScriptStorage = preloadScriptStorage;
  }

  async addPreloadScript(
    params: Script.AddPreloadScriptParameters
  ): Promise<Script.AddPreloadScriptResult> {
    const preloadScript = new BidiPreloadScript(params);
    this.#preloadScriptStorage.addPreloadScript(preloadScript);

    const cdpTargets = new Set<CdpTarget>(
      this.#browsingContextStorage
        .getTopLevelContexts()
        .map((context) => context.cdpTarget)
    );

    await preloadScript.initInTargets(cdpTargets, false);

    return {
      script: preloadScript.id,
    };
  }

  async removePreloadScript(
    params: Script.RemovePreloadScriptParameters
  ): Promise<EmptyResult> {
    const bidiId = params.script;

    const scripts = this.#preloadScriptStorage.findPreloadScripts({
      id: bidiId,
    });

    if (scripts.length === 0) {
      throw new NoSuchScriptException(
        `No preload script with BiDi ID '${bidiId}'`
      );
    }

    await Promise.all(scripts.map((script) => script.remove()));

    this.#preloadScriptStorage.removeBiDiPreloadScripts({
      id: bidiId,
    });

    return {};
  }

  async callFunction(
    params: Script.CallFunctionParameters
  ): Promise<Script.EvaluateResult> {
    const realm = await this.#getRealm(params.target);
    return realm.callFunction(
      params.functionDeclaration,
      params.this ?? {
        type: 'undefined',
      }, // `this` is `undefined` by default.
      params.arguments ?? [], // `arguments` is `[]` by default.
      params.awaitPromise,
      params.resultOwnership ?? Script.ResultOwnership.None,
      params.serializationOptions ?? {}
    );
  }

  async evaluate(
    params: Script.EvaluateParameters
  ): Promise<Script.EvaluateResult> {
    const realm = await this.#getRealm(params.target);
    return realm.evaluate(
      params.expression,
      params.awaitPromise,
      params.resultOwnership ?? Script.ResultOwnership.None,
      params.serializationOptions ?? {}
    );
  }

  async disown(params: Script.DisownParameters): Promise<EmptyResult> {
    const realm = await this.#getRealm(params.target);
    await Promise.all(
      params.handles.map(async (handle) => realm.disown(handle))
    );
    return {};
  }

  getRealms(params: Script.GetRealmsParameters): Script.GetRealmsResult {
    if (params.context !== undefined) {
      // Make sure the context is known.
      this.#browsingContextStorage.getContext(params.context);
    }
    const realms = this.#realmStorage
      .findRealms({
        browsingContextId: params.context,
        type: params.type,
      })
      .map((realm: Realm) => realm.realmInfo);
    return {realms};
  }

  async #getRealm(target: Script.Target): Promise<Realm> {
    if ('realm' in target) {
      return this.#realmStorage.getRealm({
        realmId: target.realm,
      });
    }
    const context = this.#browsingContextStorage.getContext(target.context);
    return context.getOrCreateSandbox(target.sandbox);
  }
}
