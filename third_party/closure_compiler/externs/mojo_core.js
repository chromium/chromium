/**
 * @fileoverview Closure definitions of Mojo core IDL objects only.
 */

const Mojo = {};

/**
 * @param {string} name
 * @param {MojoHandle} handle
 * @param {string=} scope
 * @param {boolean=} useBrowserInterfaceBroker
 */
Mojo.bindInterface = function(
    name, handle, scope, useBrowserInterfaceBroker) {};

/** @typedef {number} */
let MojoResult;

/** @type {!MojoResult} */
Mojo.RESULT_OK;

/** @type {!MojoResult} */
Mojo.RESULT_CANCELLED;

/** @type {!MojoResult} */
Mojo.RESULT_FAILED_PRECONDITION;

/** @type {!MojoResult} */
Mojo.RESULT_SHOULD_WAIT;

/**
 * @typedef {{
 *   result: MojoResult,
 *   buffer: !ArrayBuffer,
 *   handles: !Array<MojoHandle>
 * }}
 */
let MojoReadMessageResult;

class MojoWatcher {
  /** @return {MojoResult} */
  cancel() {}
}

/**
 * @typedef {{
 *   readable: (?boolean|undefined),
 *   writable: (?boolean|undefined),
 *   peerClosed: (?boolean|undefined)
 * }}
 */
let MojoHandleSignals;

class MojoHandle {
  close() {}

  /**
   * @return {!MojoReadMessageResult}
   */
  readMessage() {}

  /**
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   * @return {MojoResult}
   */
  writeMessage(buffer, handles) {}

  /**
   * @param {!MojoHandleSignals} signals
   * @param {function(MojoResult)} handler
   * @return {!MojoWatcher}
   */
  watch(signals, handler) {}
};

/**
 * @typedef {{
 *   result: !MojoResult,
 *   handle0: !MojoHandle,
 *   handle1: !MojoHandle,
 * }}
 */
let MojoCreateMessagePipeResult;

/**
 * @return {!MojoCreateMessagePipeResult}
 */
Mojo.createMessagePipe = function() {}
