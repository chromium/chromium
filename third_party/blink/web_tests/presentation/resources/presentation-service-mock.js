/*
 * Mock implementation of mojo PresentationService.
 */

"use strict";

class MockPresentationConnection {
};

class PresentationServiceMock {
  constructor() {
    this.pendingResponse_ = null;
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.PresentationService);
    this.controllerConnectionPtr_ = null;
    this.receiverConnectionRequest_ = null;

    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.PresentationService.name, "context", true);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();

    this.controller_ = null;
    this.onSetController = null;

    this.receiver_ = null;
    this.onSetReceiver = null;
  }

  reset() {
    this.bindingSet_.closeAllBindings();
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

  async startPresentation(urls) {
    const controller_ptr = new blink.mojom.PresentationConnectionPtr();
    const receiver_ptr = new blink.mojom.PresentationConnectionPtr();
    this.controllerConnectionPtr_ = controller_ptr;
    this.receiverConnectionRequest_ = mojo.makeRequest(receiver_ptr);
    return {
      result: {
        presentationInfo: {url: urls[0], id: 'fakePresentationId'},
        connectionRemote: receiver_ptr,
        connectionReceiver: mojo.makeRequest(controller_ptr),
      },
      error: null,
    };
  }

  async reconnectPresentation(urls) {
    const controller_ptr = new blink.mojom.PresentationConnectionPtr();
    const receiver_ptr = new blink.mojom.PresentationConnectionPtr();
    this.controllerConnectionPtr_ = controller_ptr;
    this.receiverConnectionRequest_ = mojo.makeRequest(receiver_ptr);
    return {
      result: {
        presentationInfo: {url: urls[0], id: 'fakePresentationId'},
        connectionRemote: receiver_ptr,
        connectionReceiver: mojo.makeRequest(controller_ptr),
      },
      error: null,
    };
  }

  terminate(presentationUrl, presentationId) {
    this.controller_.onConnectionStateChanged(
        { url: presentationUrl, id: presentationId },
        blink.mojom.PresentationConnectionState.TERMINATED);
  }

  onReceiverConnectionAvailable(
      strUrl, id, opt_controllerConnectionPtr, opt_receiverConnectionRequest) {
    const mojoUrl = new url.mojom.Url();
    mojoUrl.url = strUrl;
    var controllerConnectionPtr = opt_controllerConnectionPtr;
    if (!controllerConnectionPtr) {
      controllerConnectionPtr = new blink.mojom.PresentationConnectionPtr();
      const connectionBinding = new mojo.Binding(
          blink.mojom.PresentationConnection,
          new MockPresentationConnection(),
          mojo.makeRequest(controllerConnectionPtr));
    }

    var receiverConnectionRequest = opt_receiverConnectionRequest;
    if (!receiverConnectionRequest) {
      receiverConnectionRequest = mojo.makeRequest(
          new blink.mojom.PresentationConnectionPtr());
    }

    console.log('onReceiverConnectionAvailable: ' + mojoUrl + ',' + id);
    this.receiver_.onReceiverConnectionAvailable(
        { url: mojoUrl, id: id },
        controllerConnectionPtr, receiverConnectionRequest);
    console.log('after onReceiverConnectionAvailable');
  }

  getControllerConnectionPtr() {
    return this.controllerConnectionPtr_;
  }

  getReceiverConnectionRequest() {
    return this.receiverConnectionRequest_;
  }
}

function waitForClick(callback, button) {
  button.addEventListener('click', callback, { once: true });

  if (!('eventSender' in window))
    return;

  const boundingRect = button.getBoundingClientRect();
  const x = boundingRect.left + boundingRect.width / 2;
  const y = boundingRect.top + boundingRect.height / 2;

  eventSender.mouseMoveTo(x, y);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

let presentationServiceMock = new PresentationServiceMock();
