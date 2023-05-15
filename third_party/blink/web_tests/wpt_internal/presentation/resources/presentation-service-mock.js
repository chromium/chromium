/*
 * Mock implementation of mojo PresentationService.
 */

import {PresentationConnectionRemote, PresentationConnectionState, PresentationService, PresentationServiceReceiver} from '/gen/third_party/blink/public/mojom/presentation/presentation.mojom.m.js';

export class PresentationServiceMock {
  constructor() {
    this.pendingResponse_ = null;
    this.serviceReceiver_ = new PresentationServiceReceiver(this);
    this.controllerConnectionPtr_ = null;
    this.receiverConnectionRequest_ = null;

    this.interceptor_ =
        new MojoInterfaceInterceptor(PresentationService.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.serviceReceiver_.$.bindHandle(e.handle);
    this.interceptor_.start();

    this.controller_ = null;
    this.onSetController = null;

    this.receiver_ = null;
    this.onSetReceiver = null;
  }

  reset() {
    this.serviceReceiver_.closeBindings();
    this.interceptor_.stop();
  }

  setController(controller) {
    this.controller_ = controller;

    if (this.onSetController)
      this.onSetController();
  }

  setReceiver(receiver) {
    console.log('setReceiver');
    this.receiver_ = receiver;

    if (this.onSetReceiver)
      this.onSetReceiver();
  }

  setDefaultPresentationUrls(urls) {}

  listenForScreenAvailability(url) {}

  stopListeningForScreenAvailability(url) {}

  startPresentation(urls) {
    const controller_ptr = new PresentationConnectionRemote();
    const receiver_ptr = new PresentationConnectionRemote();
    this.controllerConnectionPtr_ = controller_ptr;
    this.receiverConnectionRequest_ =
        receiver_ptr.$.bindNewPipeAndPassReceiver();
    return {
      result: {
        presentationInfo: {url: urls[0], id: 'fakePresentationId'},
        connectionRemote: receiver_ptr,
        connectionReceiver: controller_ptr.$.bindNewPipeAndPassReceiver(),
      },
      error: null,
    };
  }

  reconnectPresentation(urls) {
    const controller_ptr = new PresentationConnectionRemote();
    const receiver_ptr = new PresentationConnectionRemote();
    this.controllerConnectionPtr_ = controller_ptr;
    this.receiverConnectionRequest_ =
        receiver_ptr.$.bindNewPipeAndPassReceiver();
    return {
      result: {
        presentationInfo: {url: urls[0], id: 'fakePresentationId'},
        connectionRemote: receiver_ptr,
        connectionReceiver: controller_ptr.$.bindNewPipeAndPassReceiver(),
      },
      error: null,
    };
  }

  closeConnection(url, id) {}

  terminate(presentationUrl, presentationId) {
    this.controller_.onConnectionStateChanged(
        { url: presentationUrl, id: presentationId },
        PresentationConnectionState.TERMINATED);
  }

  onReceiverConnectionAvailable(
      strUrl, id, opt_controllerConnectionPtr, opt_receiverConnectionRequest) {
    const mojoUrl = {url: strUrl}
    var controllerConnectionPtr = opt_controllerConnectionPtr;
    if (!controllerConnectionPtr) {
      controllerConnectionPtr = new PresentationConnectionRemote();
      controllerConnectionPtr.$.bindNewPipeAndPassReceiver();
    }

    var receiverConnectionRequest = opt_receiverConnectionRequest;
    if (!receiverConnectionRequest) {
      const remote = new PresentationConnectionRemote();
      receiverConnectionRequest = remote.$.bindNewPipeAndPassReceiver();
    }

    console.log('onReceiverConnectionAvailable: ' + mojoUrl + ',' + id);
    this.receiver_.onReceiverConnectionAvailable({
      presentationInfo: {url: mojoUrl, id: id},
      connectionRemote: controllerConnectionPtr,
      connectionReceiver: receiverConnectionRequest,
    });
    console.log('after onReceiverConnectionAvailable');
  }

  getControllerConnectionPtr() {
    return this.controllerConnectionPtr_;
  }

  getReceiverConnectionRequest() {
    return this.receiverConnectionRequest_;
  }
}

export function waitForClick(button) {
  return new Promise(resolve => {
    button.addEventListener('click', resolve, {once: true});

    if (!('eventSender' in window))
      return;

    const boundingRect = button.getBoundingClientRect();
    const x = boundingRect.left + boundingRect.width / 2;
    const y = boundingRect.top + boundingRect.height / 2;

    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
  });
}
