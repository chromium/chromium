import {MojoWebTestHelperReceiver} from '/gen/content/test/data/mojo_web_test_helper_test.mojom.m.js';

export const kTestReply = "hehe got ya";

// An impl of the test interface which replies to reverse() with a fixed
// message rather than the normally expected value.
export class TestHelperImpl {
  constructor() {
    this.receiver_ = new MojoWebTestHelperReceiver(this);
  }
  bindRequest(request) { this.receiver_.$.bindHandle(request); }
  getLastString() { return this.lastString_; }
  reverse(message) {
    this.lastString_ = message;
    return {reversed: kTestReply};
  }
}
