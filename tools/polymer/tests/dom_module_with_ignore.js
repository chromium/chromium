Polymer({
  is: 'cr-test-foo',
  behaviors: [Polymer.PaperRippleBehavior],
  /** @override */
  ready() {
    /* #ignore */ this.importHref('./foo.html');
  },
});
