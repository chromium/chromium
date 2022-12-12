importScripts('../../serviceworker/resources/worker-testharness.js');

test(() => {
  assert_true('PaymentRequestEvent' in self);
  assert_inherits(PaymentRequestEvent.prototype, 'waitUntil');
  assert_own_property(PaymentRequestEvent.prototype, 'topOrigin');
  assert_own_property(PaymentRequestEvent.prototype, 'paymentRequestOrigin');
  assert_own_property(PaymentRequestEvent.prototype, 'paymentRequestId');
  assert_own_property(PaymentRequestEvent.prototype, 'methodData');
  assert_own_property(PaymentRequestEvent.prototype, 'total');
  assert_own_property(PaymentRequestEvent.prototype, 'modifiers');
  assert_own_property(PaymentRequestEvent.prototype, 'instrumentKey');
  assert_own_property(PaymentRequestEvent.prototype, 'respondWith');
});

promise_test(() => {
  return new Promise(resolve => {
    var eventWithInit = new PaymentRequestEvent('paymentrequest', {
      topOrigin: 'https://example.test',
      paymentRequestOrigin: 'https://example.test',
      paymentRequestId: 'payment-request-id',
      methodData: [{
        supportedMethods: 'basic-card'
      }],
      total: {
        currency: 'USD',
        value: '55.00'
      },
      modifiers: [{
        supportedMethods: 'basic-card'
      }],
      instrumentKey: 'payment-instrument-key'
    });

    self.addEventListener('paymentrequest', e => {
      assert_equals(e.topOrigin, 'https://example.test');
      assert_equals(e.paymentRequestOrigin, 'https://example.test');
      assert_equals(e.paymentRequestId, 'payment-request-id');
      assert_equals(e.methodData.length, 1);
      assert_equals(e.methodData[0].supportedMethods, 'basic-card');
      assert_equals(e.total.currency, 'USD');
      assert_equals(e.total.value, '55.00');
      assert_equals(e.modifiers.length, 1);
      assert_equals(e.modifiers[0].supportedMethods, 'basic-card');
      assert_equals(e.instrumentKey, 'payment-instrument-key');
      resolve();
    });

    self.dispatchEvent(eventWithInit);
  });
});
