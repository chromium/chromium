// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {mojo} from '//resources/mojo/mojo/public/js/bindings.js';

type ReceiverHelper<Remote, PendingReceiver> =
    mojo.internal.interfaceSupport
        .InterfaceReceiverHelper<Remote, PendingReceiver>;

type RemoteBaseWrapper<Receiver> =
    mojo.internal.interfaceSupport.InterfaceRemoteBaseWrapper<Receiver>;

/**
 * Generic container holding a WebUI page handler and callback router.
 */
export interface MojoBrowserProxy<HandlerInterface, CallbackRouter> {
  handler: HandlerInterface;
  callbackRouter: CallbackRouter;
}

/**
 * Reusable factory that encapsulates singleton management, production Mojo pipe
 * binding, and test mock creation for WebUI browser proxies with precise Mojo
 * pipe types.
 */
// Disable clang-format to avoid mangling of long type list.
// clang-format off
export class MojoBrowserProxyFactory<
    HandlerInterface, Remote, Receiver, PendingReceiver,
    CallbackRouter extends {$: ReceiverHelper<Remote, PendingReceiver>},
    HandlerRemote extends HandlerInterface&{$: RemoteBaseWrapper<Receiver>}> {
//clang-format on
  private instance_: MojoBrowserProxy<HandlerInterface, CallbackRouter>|null =
      null;

  /**
   * @param handlerRemoteCtor Constructor for the production Mojo remote (e.g.,
   *     PageHandlerRemote).
   * @param callbackRouterCtor Constructor for the Mojo callback router (e.g.,
   *     PageCallbackRouter).
   * @param bindFn Callback invoked in production to bind the pipes via the
   *     factory.
   */
  constructor(
      private handlerRemoteCtor_: new() => HandlerRemote,
      private callbackRouterCtor_: new() => CallbackRouter,
      private bindFn_: (remote: Remote, receiver: Receiver) => void) {}

  /**
   * Retrieves or creates the production singleton instance.
   */
  getInstance(): MojoBrowserProxy<HandlerInterface, CallbackRouter> {
    if (!this.instance_) {
      const handler = new this.handlerRemoteCtor_();
      const callbackRouter = new this.callbackRouterCtor_();
      this.bindFn_(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      this.instance_ = {handler, callbackRouter};
    }
    return this.instance_;
  }

  /**
   * Sets the singleton instance directly (used during test setup).
   */
  setInstance(proxy: MojoBrowserProxy<HandlerInterface, CallbackRouter>) {
    this.instance_ = proxy;
  }

  /**
   * Creates a test proxy instance wrapped with a mock handler and a real
   * callback router.
   */
  createForTest(handler: HandlerInterface): {
    instance: MojoBrowserProxy<HandlerInterface, CallbackRouter>,
    remote: Remote,
  } {
    const callbackRouter = new this.callbackRouterCtor_();
    return {
      instance: {handler, callbackRouter},
      remote: callbackRouter.$.bindNewPipeAndPassRemote(),
    };
  }
}
