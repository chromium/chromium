import {
  BillingResponseCode,
  CreateDigitalGoodsResponseCode,
  ItemType,
} from '/gen/components/digital_goods/mojom/digital_goods.mojom.m.js';

import {
  DigitalGoodsFactory,
  DigitalGoodsFactoryReceiver,
  DigitalGoodsReceiver,
  DigitalGoodsRemote,
} from '/gen/third_party/blink/public/mojom/digital_goods/digital_goods.mojom.m.js';

class MockDigitalGoods {
  constructor() {
    this.resetRecordedAction_();
  }

  bind(request) {
    this.receiver = new DigitalGoodsReceiver(this);
    this.receiver.$.bindHandle(request.handle);
  }

  getRecordedAction_() {
    return this.action;
  }

  resetRecordedAction_() {
    this.action = new Promise((resolve, reject) => {
      this.actionResolve_ = resolve;
    });
  }

  makeItemDetails_(id) {
    // itemDetails is a payments.mojom.ItemDetails.
    let itemDetails = {};
    itemDetails.itemId = id;
    itemDetails.title = 'title:' + id;
    itemDetails.description = 'description:' + id;
    // price is a payments.mojom.PaymentCurrencyAmount.
    itemDetails.price = {};
    itemDetails.price.currency = 'AUD';
    // Set price.value as a number in |id| dollars.
    const matchNum = id.match(/\d+/);
    const num = matchNum ? matchNum[0] : 0;
    itemDetails.price.value = num + '.00';
    if (num % 2) {
      // Add optional fields.
      itemDetails.subscriptionPeriod = 'P' + num + 'Y';
      itemDetails.freeTrialPeriod = 'P' + num + 'M';
      itemDetails.introductoryPrice = {};
      itemDetails.introductoryPrice.currency = 'JPY';
      itemDetails.introductoryPrice.value = 2*num + '';
      itemDetails.introductoryPricePeriod = 'P' + num + 'D';
      itemDetails.introductoryPriceCycles = 3*num;
      itemDetails.type = Math.floor(num / 2) % 2 ? ItemType.kProduct : ItemType.kSubscription;
      itemDetails.iconUrls = [{url: 'https://example.com/icon.png'}];
    }
    return itemDetails;
  }

  async getDetails(itemIds) {
    this.actionResolve_('getDetails:' + itemIds);

    // Simulate some backend failure response.
    if (itemIds.includes('fail')) {
      return {code: BillingResponseCode.kError, itemDetailsList: []};
    }

    let itemDetailsList = [];
    // Simulate some specified IDs are not found.
    const found = itemIds.filter(id => !id.includes('gone'));
    for (const id of found) {
      itemDetailsList.push(this.makeItemDetails_(id));
    }

    return {
      code: BillingResponseCode.kOk,
      itemDetailsList,
    };
  }

  makePurchaseReference_(id) {
    // purchaseReference is a payments.mojom.PurchaseReference.
    let purchaseReference = {};
    purchaseReference.itemId = 'id:' + id;
    purchaseReference.purchaseToken = 'purchaseToken:' + id;
    return purchaseReference;
  }

  async listPurchases() {
    this.actionResolve_('listPurchases');

    let result = [];
    for (let i = 0; i < 10; i++) {
      result.push(this.makePurchaseReference_(i));
    }

    return {
      code: BillingResponseCode.kOk,
      purchaseReferenceList: result
    };
  }

  async listPurchaseHistory() {
    this.actionResolve_('listPurchaseHistory');

    let result = [];
    for (let i = 0; i < 20; i++) {
      result.push(this.makePurchaseReference_(i));
    }

    return {
      code: BillingResponseCode.kOk,
      purchaseReferenceList: result
    };
  }

  async consume(purchaseToken) {
    this.actionResolve_('consume:' + purchaseToken);

    if (purchaseToken === 'fail') {
      return { code: BillingResponseCode.kError };
    }
    return { code: BillingResponseCode.kOk };
  }
}

let mockDigitalGoods = new MockDigitalGoods();

class MockDigitalGoodsFactory {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(DigitalGoodsFactory.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new DigitalGoodsFactoryReceiver(this);

    this.interceptor_.start();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  async createDigitalGoods(paymentMethod) {
    if (paymentMethod !== 'https://example.com/billing') {
      return {
        code: CreateDigitalGoodsResponseCode.kUnsupportedPaymentMethod,
        digitalGoods: null
      };
    }

    const digitalGoods = new DigitalGoodsRemote();
    mockDigitalGoods.bind(digitalGoods.$.bindNewPipeAndPassReceiver());

    return {
      code: CreateDigitalGoodsResponseCode.kOk,
      digitalGoods,
    };
  }
}

let mockDigitalGoodsFactory = new MockDigitalGoodsFactory();

export function digital_goods_test(func, {
  title,
  expectedAction,
  paymentMethod = 'https://example.com/billing',
} = {}) {
  promise_test(async () => {
    mockDigitalGoods.resetRecordedAction_();
    const service = await window.getDigitalGoodsService(paymentMethod);

    await func(service);

    if (expectedAction) {
      const action = await mockDigitalGoods.getRecordedAction_();
      assert_equals(action, expectedAction);
    }
  }, title);
}
