cr.define('cr.testFoo', () => {
  let instance_ = null;

  let bar_ = 1;

  /* #export */ const someExport = true;

  /* #export */ function getInstance() {
    return assert(instance_);
  }

  function getBarInternal_() {
    return bar_;
  }

  /* #export */ function getBar(isTest) {
    return isTest ? 0 : getBarInternal_();
  }

  /* #export */ let CrTestFooElement = Polymer({
    is: 'cr-test-foo',
    behaviors: [Polymer.PaperRippleBehavior],
  });

  // #cr_define_end
  return {
    CrTestFooElement: CrTestFooElement,
    someExport: someExport,
    getInstance: getInstance,
    getBar: getBar,
  };
});
