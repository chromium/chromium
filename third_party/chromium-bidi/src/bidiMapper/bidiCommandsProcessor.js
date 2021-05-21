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
import Context from './domains/browsingContext';

export async function runBidiCommandsProcessor(
  cdpServer,
  bidiServer,
  getCurrentTargetId
) {
  Context.getCurrentContextId = getCurrentTargetId;
  Context.cdpServer = cdpServer;

  const targets = {};

  const _isValidTarget = (target) => {
    if (target.targetId === getCurrentTargetId()) return false;
    if (!target.type || target.type !== 'page') return false;
    return true;
  };

  const _getErrorResponse = (commandData, errorCode, errorMessage) => {
    // TODO: this is bizarre per spec. We reparse the payload and
    // extract the ID, regardless of what kind of value it was.
    let commandId = undefined;
    try {
      commandId = commandData.id;
    } catch {}

    return {
      id: commandId,
      error: errorCode,
      message: errorMessage,
      // TODO: optional stacktrace field.
    };
  };

  const _respondWithError = (commandData, errorCode, errorMessage) => {
    const errorResponse = _getErrorResponse(
      commandData,
      errorCode,
      errorMessage
    );
    bidiServer.sendMessage(errorResponse);
  };

  const targetToContext = (t) => ({
    context: t.targetId,
    parent: t.openerId ? t.openerId : null,
    url: t.url,
  });

  const process_browsingContext_getTree = async function (params) {
    const cdpTargets = await cdpServer.sendMessage({
      method: 'Target.getTargets',
    });
    const contexts = cdpTargets.targetInfos
      // Don't expose any information about the tab with Mapper running.
      .filter(_isValidTarget)
      .map(targetToContext);
    return { contexts };
  };

  const process_PROTO_browsingContext_createContext = async function (params) {
    const { targetId } = await cdpServer.sendMessage({
      method: 'Target.createTarget',
      params: { url: params.url },
    });
    return targetToContext(targets[targetId]);
  };

  const process_DEBUG_Page_close = async function (params) {
    await cdpServer.sendMessage({
      method: 'Target.closeTarget',
      params: { targetId: params.context },
    });
    return {};
  };

  const process_session_status = async function (params) {
    return { ready: true, message: 'ready' };
  };

  Context.onContextCreated = (context) => {
    bidiServer.sendMessage({
      method: 'browsingContext.contextCreated',
      params: context.toBidi(),
    });
  };
  Context.onContextDestroyed = (context) => {
    bidiServer.sendMessage({
      method: 'browsingContext.contextDestroyed',
      params: context.toBidi(),
    });
  };

  const onCdpMessage = function (message) {
    switch (message.method) {
      case 'Target.attachedToTarget':
        Context.handleAttachedToTargetEvent(message);
        return;
      case 'Target.targetInfoChanged':
        Context.handleInfoChangedEvent(message);
        return;
      case 'Target.detachedFromTarget':
        Context.handleDetachedFromTargetEvent(message);
        return;
    }
  };

  const processCommand = async (commandData) => {
    const response = {};
    response.id = commandData.id;

    switch (commandData.method) {
      case 'session.status':
        return await process_session_status(commandData.params);
      case 'browsingContext.getTree':
        return await process_browsingContext_getTree(commandData.params);

      case 'PROTO.browsingContext.createContext':
        return await Context.process_createContext(commandData.params);

      case 'DEBUG.Page.close':
        return await process_DEBUG_Page_close(commandData.params);

      default:
        throw new Error('unknown command');
    }
  };

  const onBidiMessage = async (message) => {
    await processCommand(message)
      .then((result) => {
        const response = {
          id: message.id,
          result,
        };

        bidiServer.sendMessage(response);
      })
      .catch((e) => {
        console.error(e);
        _respondWithError(message, 'unknown error', e.message);
      });
  };

  cdpServer.setOnMessage(onCdpMessage);
  bidiServer.setOnMessage(onBidiMessage);

  await cdpServer.sendMessage({
    method: 'Target.setAutoAttach',
    params: {
      autoAttach: true,
      waitForDebuggerOnStart: false,
      flatten: true,
    },
  });
}
