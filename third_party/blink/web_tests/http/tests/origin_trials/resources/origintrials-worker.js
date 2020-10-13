importScripts('/resources/testharness.js');

get_worker_type = () => {
  var type = Object.prototype.toString.call(self);
  if (type.indexOf('ServiceWorkerGlobalScope') !== -1) {
    return 'service';
  }
  if (type.indexOf('SharedWorkerGlobalScope') !== -1) {
    if (self.name) {
      return 'shared (' + self.name + ')';
    }
    return 'shared';
  }
  if (type.indexOf('DedicatedWorkerGlobalScope') !== -1) {
    return 'dedicated';
  }
  return 'unknown';
}

// Test whether the origin-trial-enabled attributes are *NOT* attached in a
// worker where the trial is not enabled.
// This is deliberately just a minimal set of tests to ensure that trials are
// available in a worker. The full suite of tests are in origintrials.js.
expect_failure_worker = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_idl_attribute(testObject, 'throwingAttribute');
    assert_throws_dom('NotSupportedError', () => {
      testObject.throwingAttribute;
    }, 'Accessing attribute should throw error');
  }, 'Accessing attribute should throw error in ' + worker_type + ' worker');
  test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_false('normalAttribute' in testObject);
    assert_equals(testObject.normalAttribute, undefined);
  }, 'Attribute should not exist in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are *NOT* attached in a
// worker where the deprecation trial is not enabled.
expect_failure_worker_deprecation = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_false('deprecationAttribute' in testObject);
    assert_equals(testObject.deprecationAttribute, undefined);
  }, 'Deprecation attribute should not exist in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are *NOT* attached in a
// worker where the implied trial is not enabled.
expect_failure_worker_implied = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_false('impliedAttribute' in testObject);
    assert_equals(testObject.impliedAttribute, undefined);
  }, 'Implied attribute should not exist in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are *NOT* attached in a
// worker where the trial is not enabled for the OS.
expect_failure_worker_invalid_os = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_false('invalidOSAttribute' in testObject);
    assert_equals(testObject.invalidOSAttribute, undefined);
  }, 'Invalid OS attribute should not exist in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are *NOT* attached in a
// worker where the third-party trial is not enabled.
expect_failure_worker_third_party = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
    var testObject = self.internals.originTrialsTest();
    assert_false('thirdPartyAttribute' in testObject);
    assert_equals(testObject.impliedAttribute, undefined);
  }, 'Third-party attribute should not exist in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are attached in a worker
// where the trial is enabled.
// This is deliberately just a minimal set of tests to ensure that trials are
// available in a worker. The full suite of tests are in origintrials.js.
expect_success_worker = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
      var testObject = self.internals.originTrialsTest();
      assert_idl_attribute(testObject, 'throwingAttribute');
      assert_true(testObject.throwingAttribute, 'Attribute should return boolean value');
    }, 'Accessing attribute should return value and not throw exception in ' + worker_type + ' worker');
  test(() => {
      var testObject = self.internals.originTrialsTest();
      assert_idl_attribute(testObject, 'normalAttribute');
      assert_true(testObject.normalAttribute, 'Attribute should return boolean value');
    }, 'Attribute should exist and return value in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are attached in a worker
// where the deprecation trial is enabled.
expect_success_worker_deprecation = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
      var testObject = self.internals.originTrialsTest();
      assert_idl_attribute(testObject, 'deprecationAttribute');
      assert_true(testObject.deprecationAttribute, 'Attribute should return boolean value');
    }, 'Deprecation attribute should exist and return value in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are attached in a worker
// where the implied trial is enabled, either directly or by the related trial.
expect_success_worker_implied = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
      var testObject = self.internals.originTrialsTest();
      assert_idl_attribute(testObject, 'impliedAttribute');
      assert_true(testObject.impliedAttribute, 'Attribute should return boolean value');
    }, 'Implied attribute should exist and return value in ' + worker_type + ' worker');
  done();
}

// Test whether the origin-trial-enabled attributes are attached in a worker
// where the third-party trial is enabled.
expect_success_worker_third_party = () => {
  // Use |worker_type| to make the test descriptions unique when multiple
  // workers are created in a single test file.
  var worker_type = get_worker_type();
  test(() => {
      var testObject = self.internals.originTrialsTest();
      assert_idl_attribute(testObject, 'thirdPartyAttribute');
      assert_true(testObject.thirdPartyAttribute, 'Attribute should return boolean value');
    }, 'Implied attribute should exist and return value in ' + worker_type + ' worker');
  done();
}
