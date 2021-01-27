import {BadgeService, BadgeServiceReceiver} from '/gen/third_party/blink/public/mojom/badging/badging.mojom.m.js';

class MockBadgeService {
  constructor() {
    this.receiver_ = new BadgeServiceReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(BadgeService.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
  }

  init_(expectedAction) {
    this.expectedAction = expectedAction;
    return new Promise((resolve, reject) => {
      this.reject_ = reject;
      this.resolve_ = resolve;
    });
  }

  setBadge(value) {
    // Accessing number when the union is a flag will throw, so read the
    // value in a try catch.
    let number;
    try {
      number = value.number;
    } catch (error) {
      number = undefined;
    }

    try {
      const action = number === undefined ? 'flag' : 'number:' + number;
      assert_equals(action, this.expectedAction);
      this.resolve_();
    } catch (error) {
      this.reject_(error);
    }
  }

  clearBadge() {
    try {
      assert_equals('clear', this.expectedAction);
      this.resolve_();
    } catch (error) {
      this.reject_(error);
    }
  }
}

let mockBadgeService = new MockBadgeService();

export function badge_test(func, expectedAction, expectedError) {
  promise_test(async () => {
    let mockPromise = mockBadgeService.init_(expectedAction);

    try {
      await func();
    } catch (error) {
      return assert_equals(error.name, expectedError);
    }

    await mockPromise;
  });
}
