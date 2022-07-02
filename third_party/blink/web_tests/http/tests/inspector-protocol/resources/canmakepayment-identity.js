self.addEventListener('install', (evt) => {
  console.log(evt); // Should pause here.
  const canMakePaymentEvent = new CanMakePaymentEvent('canmakepayment', {
      topOrigin: 'https://test1.example',
      paymentRequestOrigin: 'https://test2.example',
      methodData: [],
      modifiers: [],
    });
  // Access the deprecated identity fields:
  console.log(canMakePaymentEvent.topOrigin);
  console.log(canMakePaymentEvent.paymentRequestOrigin);
  console.log(canMakePaymentEvent.methodData);
  console.log(canMakePaymentEvent.modifiers);
});
