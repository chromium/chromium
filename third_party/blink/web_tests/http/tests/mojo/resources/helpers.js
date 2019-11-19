const kTestReply = "hehe got ya";

// An impl of the test interface which replies to reverse() with a fixed
// message rather than the normally expected value.
class TestHelperImpl {
  constructor() {
    this.binding_ =
        new mojo.Binding(content.mojom.MojoWebTestHelper, this);
  }
  bindRequest(request) { this.binding_.bind(request); }
  getLastString() { return this.lastString_; }
  reverse(message) {
    this.lastString_ = message;
    return Promise.resolve({ reversed: kTestReply });
  }
}
