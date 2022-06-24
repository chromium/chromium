/**
 * Copyright 2022 Google LLC.
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

import { IContext } from './iContext';
import { BrowsingContext, Script } from '../protocol/bidiProtocolTypes';
import { NoSuchFrameException } from '../protocol/error';

export abstract class Context implements IContext {
  static #contexts: Map<string, IContext> = new Map();

  public static getTopLevelContexts(): IContext[] {
    return Array.from(Context.#contexts.values()).filter(
      (c) => c.getParentId() === null
    );
  }

  public static removeContext(contextId: string) {
    Context.#contexts.delete(contextId);
  }

  public static addContext(context: IContext) {
    Context.#contexts.set(context.getContextId(), context);
  }

  public static hasKnownContext(contextId: string): boolean {
    return Context.#contexts.has(contextId);
  }

  public static getKnownContext(contextId: string): IContext {
    if (!Context.hasKnownContext(contextId)) {
      throw new NoSuchFrameException(`Context ${contextId} not found`);
    }
    return Context.#contexts.get(contextId)!;
  }

  readonly #contextId: string;
  readonly #sessionId: string;
  readonly #parentId: string | null;
  readonly #childrenIds: Set<string> = new Set();
  #url: string = 'about:blank';

  public getContextId = (): string => this.#contextId;
  public getChildren = (): IContext[] =>
    Array.from(this.#childrenIds).map((contextId) =>
      Context.getKnownContext(contextId)
    );
  public getSessionId = (): string => this.#sessionId;
  public getParentId = (): string | null => this.#parentId;
  public getUrl = (): string | null => this.#url;

  protected setUrl(url: string) {
    this.#url = url;
  }

  public addChild(child: IContext) {
    this.#childrenIds.add(child.getContextId());
  }

  protected constructor(
    contextId: string,
    parent: string | null,
    sessionId: string
  ) {
    this.#contextId = contextId;
    this.#parentId = parent;
    this.#sessionId = sessionId;
  }

  abstract callFunction(
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean
  ): Promise<Script.CallFunctionResult>;

  abstract navigate(
    url: string,
    wait: BrowsingContext.ReadinessState
  ): Promise<BrowsingContext.NavigateResult>;

  abstract scriptEvaluate(
    expression: string,
    awaitPromise: boolean
  ): Promise<Script.EvaluateResult>;

  public serializeToBidiValue(maxDepth: number): BrowsingContext.Info {
    return {
      context: this.#contextId,
      parent: this.#parentId,
      url: this.#url,
      children:
        maxDepth > 0
          ? this.getChildren().map((c) => c.serializeToBidiValue(maxDepth - 1))
          : null,
    };
  }

  public async findElement(
    selector: string
  ): Promise<BrowsingContext.PROTO.FindElementResult> {
    const functionDeclaration = String((resultsSelector: string) =>
      document.querySelector(resultsSelector)
    );
    const _arguments: Script.ArgumentValue[] = [
      { type: 'string', value: selector },
    ];

    // TODO(sadym): handle not found exception.
    const result = await this.callFunction(
      functionDeclaration,
      {
        type: 'undefined',
      },
      _arguments,
      true
    );

    // TODO(sadym): handle type properly.
    return result as any as BrowsingContext.PROTO.FindElementResult;
  }
}
