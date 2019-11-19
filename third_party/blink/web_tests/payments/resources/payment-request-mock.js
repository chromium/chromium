/*
 * payment-request-mock contains a mock implementation of PaymentRequest.
 */

"use strict";

class PaymentRequestMock {
  constructor() {
    this.pendingResponse_ = null;
    this.bindings_ = new mojo.BindingSet(payments.mojom.PaymentRequest);

    this.interceptor_ = new MojoInterfaceInterceptor(
        payments.mojom.PaymentRequest.name, "context", true);
    this.interceptor_.oninterfacerequest =
        e => this.bindings_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  init(client, supportedMethods, details, options) {
    this.client_ = client;
    if (this.pendingResponse_) {
      let response = this.pendingResponse_;
      this.pendingResponse_ = null;
      this.onPaymentResponse(response);
    }
  }

  show() {}

  updateWith(details) {}

  complete(success) {}

  onPaymentResponse(data) {
    if (!this.client_) {
      this.pendingResponse_ = data;
      return;
    }
    this.client_.onPaymentResponse(new payments.mojom.PaymentResponse(data));
  }

  onComplete() {
    this.client_.onComplete();
  }
}

let paymentRequestMock = new PaymentRequestMock(mojo.frameInterfaces);
