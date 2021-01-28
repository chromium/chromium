import {PaymentRequest, PaymentRequestReceiver} from '/gen/third_party/blink/public/mojom/payments/payment_request.mojom.m.js';

export class PaymentRequestMock {
  constructor() {
    this.pendingResponse_ = null;
    this.receiver_ = new PaymentRequestReceiver(this);

    this.interceptor_ =
        new MojoInterfaceInterceptor(PaymentRequest.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
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
  onPaymentDetailsNotUpdated() {}
  abort() {}
  complete(success) {}
  retry(errors) {}
  canMakePayment() {}
  hasEnrolledInstrument() {}

  onPaymentResponse(data) {
    if (!this.client_) {
      this.pendingResponse_ = data;
      return;
    }
    this.client_.onPaymentResponse(data);
  }

  onComplete() {
    this.client_.onComplete();
  }
}
