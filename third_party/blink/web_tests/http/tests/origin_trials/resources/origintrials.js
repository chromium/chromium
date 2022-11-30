// The sample API integrates origin trial checks at various entry points.
// References to "partial interface" mean that the [RuntimeEnabled]
// IDL attribute is applied to an entire partial interface, instead of
// applied to individual IDL members.

// Verify that the given member exists, and returns an actual value
// (i.e. not undefined).
expect_member = (member_name, get_value_func) => {
  var testObject = internals.originTrialsTest();
  assert_idl_attribute(testObject, member_name);
  assert_true(get_value_func(testObject),
    'Member should return boolean value');
}

// Verify that the given member exists on the returned dictionary, and returns
// an actual value (i.e. not undefined).
expect_dictionary_member = (dictionary_member_name) => {
  var testObject = internals.originTrialsTest();
  var dictionary = testObject.getDictionaryMethod();
  assert_own_property(dictionary, dictionary_member_name);
  assert_true(dictionary[dictionary_member_name],
    'Dictionary member ' + dictionary_member_name + ' should return boolean value');
}

// Verify that the given member is accessed as part of dictionary input to a
// method
expect_input_dictionary_member = (dictionary_member_name) => {
  var testObject = internals.originTrialsTest();

  // Test via a getter in the object to see if the member is accessed
  var memberAccessed = false;
  try {
    var dictionary = Object.defineProperty({}, dictionary_member_name, {
      get: function() {
        memberAccessed = true;
      }
    });
    testObject.checkDictionaryMethod(dictionary);
  } catch (e) {}

  assert_true(memberAccessed,
    'Dictionary member ' + dictionary_member_name + ' should be accessed by method');
}

// Verify that the given static member exists, and returns an actual value
// (i.e. not undefined).
expect_static_member = (member_name, get_value_func) => {
  var testObject = internals.originTrialsTest();
  var testInterface = testObject.constructor;
  assert_own_property(testInterface, member_name);
  assert_true(get_value_func(testInterface),
    'Static member should return boolean value');
}

// Verify that the given member exists in element styles.
expect_style_member = (member_name) => {
  var testObject = document.createElement('div');
  var testInterface = testObject.style;
  assert_own_property(testInterface, member_name);
}

// Verify that the CSS supports return true for given member and value, and
// style declarations in @supports are applied.
expect_css_supports = (member_name, member_css_name, member_value, element, computed_style) => {
  assert_true(CSS.supports(member_css_name, member_value));
  assert_equals(getComputedStyle(element)[member_name], computed_style);
}

make_css_media_feature_string = (feature_name, feature_value) => {
  return "(" + feature_name + ")";
}

expect_css_media = (feature_name) => {
  let media_feature_string = make_css_media_feature_string(feature_name);
  assert_true(window.matchMedia(media_feature_string).matches);

  assert_equals(getComputedStyle(document.documentElement).opacity, "0.8");

  let media_list = document.styleSheets[0].media;
  media_list.appendMedium(media_feature_string);
  assert_true(media_list.mediaText.indexOf("not all") === -1);
  media_list.mediaText = media_feature_string;
  assert_true(media_list.mediaText.indexOf("not all") === -1);
}

// Verify that given member does not exist, and does not provide a value
// (i.e. is undefined).
expect_member_fails = (member_name) => {
  var testObject = internals.originTrialsTest();
  assert_false(member_name in testObject);
  assert_equals(testObject[member_name], undefined);
}

// Verify that the given member does not exist on the returned dictionary, and
// does not provide a value (i.e. is undefined).
expect_dictionary_member_fails = (dictionary_member_name) => {
  var testObject = internals.originTrialsTest();
  var dictionary = testObject.getDictionaryMethod();
  assert_false(dictionary_member_name in dictionary);
  assert_equals(dictionary[dictionary_member_name], undefined,
    'Dictionary member ' + dictionary_member_name + ' should not have a value');
}

// Verify that the given member is not accessed as part of dictionary input to a
// method
expect_input_dictionary_member_fails = (dictionary_member_name) => {
  var testObject = internals.originTrialsTest();

  // Test via a getter in the object to see if the member is accessed
  var memberAccessed = false;
  try {
    var dictionary = Object.defineProperty({}, dictionary_member_name, {
      get: function() {
        memberAccessed = true;
      }
    });
    testObject.checkDictionaryMethod(dictionary);
  } catch (e) {}

  assert_false(memberAccessed,
    'Dictionary member ' + dictionary_member_name + ' should not be accessed by method');
}

// Verify that given static member does not exist, and does not provide a value
// (i.e. is undefined).
expect_static_member_fails = (member_name) => {
  var testObject = internals.originTrialsTest();
  var testInterface = testObject.constructor;
  assert_false(member_name in testInterface);
  assert_equals(testInterface[member_name], undefined);
}

// Verify that the given member does not exist in element style, and does not
// provide a value (i.e. is undefined).
expect_style_member_fails = (member_name) => {
  var testObject = document.createElement('div');
  var testInterface = testObject.style;
  assert_false(member_name in testInterface);
  assert_equals(testInterface[member_name], undefined);
}

// Verify that the CSS supports return false for given member and value, and
// style declarations in @supports are ignored
expect_css_supports_fails = (member_name, member_css_name, member_value, element) => {
  assert_false(CSS.supports(member_css_name, member_value));
  assert_equals(getComputedStyle(element)[member_name], undefined);
}

expect_css_media_fails = (feature_name) => {
  let media_feature_string = make_css_media_feature_string(feature_name);
  assert_false(window.matchMedia(media_feature_string).matches);
  assert_false(window.matchMedia(`not (${media_feature_string})`).matches);

  assert_equals(getComputedStyle(document.documentElement).opacity, "1");
}

// These tests verify that any gated parts of the API are not available.
expect_failure = (skip_worker) => {

  test(() => {
      var testObject = internals.originTrialsTest();
      assert_idl_attribute(testObject, 'throwingAttribute');
      assert_throws_dom("NotSupportedError", () => { testObject.throwingAttribute; },
          'Accessing attribute should throw error');
    }, 'Accessing attribute should throw error');

  test(() => {
      expect_member_fails('normalAttribute');
    }, 'Attribute should not exist, with trial disabled');

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/disabled-worker.js'));
  }
};

// These tests verify that gated css properties are not available.
expect_failure_css = (element) => {

  test(() => {
    expect_style_member_fails('originTrialTestProperty');
  }, 'CSS property should not exist in style, with trial disabled');

  test(() => {
      expect_css_supports_fails('originTrialTestProperty',
        'origin-trial-test-property', 'initial', element);
    }, 'CSS @supports should fail for property, with trial disabled');

  test(() => {
    expect_css_media_fails("origin-trial-test")
  }, "CSS media feature should fail with trial disabled");
};

// These tests verify that any gated parts of the API are not available for a
// deprecation trial.
expect_failure_deprecation = (skip_worker) => {

  test(() => {
    expect_member_fails('deprecationAttribute');
  }, 'Deprecation attribute should not exist, with trial disabled');

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/deprecation-disabled-worker.js'));
  }
};

// These tests verify that any gated parts of the API are not available for an
// implied trial.
expect_failure_implied = (skip_worker) => {

  test(() => {
      expect_member_fails('impliedAttribute');
    }, 'Implied attribute should not exist, with trial disabled');

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/implied-disabled-worker.js'));
  }
};

// These tests verify that any gated parts of the API are not available
// for a trial with invalid OS
expect_failure_invalid_os = (skip_worker) => {

  test(() => {
    expect_member_fails('invalidOSAttribute');
  }, 'Invalid OS attribute should not exist, even with the trial token present.');

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/invalid-os-worker.js'));
  }
};

// These tests verify that any gated parts of the API are not available for a
// third-party trial.
expect_failure_third_party = async (skip_worker) => {
  test(() => {
    expect_member_fails('thirdPartyAttribute');
  }, 'Third-party attribute should not exist, with trial disabled');

  if (!skip_worker) {
    await fetch_tests_from_worker(
        new Worker('resources/third-party-disabled-worker.js'));
  }
};

// These tests verify that the API functions correctly with an enabled trial.
expect_success = () => {

  test(() => {
      expect_member('throwingAttribute', (testObject) => {
          return testObject.throwingAttribute;
        });
    }, 'Accessing attribute should return value and not throw exception');

  test(() => {
      expect_member('normalAttribute', (testObject) => {
          return testObject.normalAttribute;
        });
    }, 'Attribute should exist on object and return value');

  test(() => {
    assert_true('testOriginTrialGlobalAttribute' in self,
      'Attribute exists on global scope (window)');
    assert_true(self.testOriginTrialGlobalAttribute,
      'Atttribute on global scope (window) should return boolean value');
  }, 'Attribute should exist on global scope (window) and return value');

  fetch_tests_from_worker(new Worker('resources/enabled-worker.js'));
};


// These tests verify that css properties functions correctly with an enabled trial.
expect_success_css = (element, computed_style) => {

  test(() => {
    expect_style_member('originTrialTestProperty');
  }, 'CSS property should exist in style');

  test(() => {
      expect_css_supports('originTrialTestProperty',
        'origin-trial-test-property', 'initial', element, computed_style);
    }, 'CSS @supports should pass for property and rules are correctly applied');
  test(() => {
    expect_css_media('origin-trial-test');
  }, "CSS media feature must parse via style sheets and OM if origin trial enabled");
};

// These tests verify that the API functions correctly with a deprecation trial
// that is enabled.
expect_success_deprecation = (opt_description_suffix, skip_worker) => {
  var description_suffix = opt_description_suffix || '';

  test(() => {
    expect_member('deprecationAttribute', (testObject) => {
      return testObject.deprecationAttribute;
    });
  }, 'Deprecation attribute should exist on object and return value' + description_suffix);

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/deprecation-enabled-worker.js'));
  }
};

// These tests verify that the API functions correctly with an implied trial
// that is enabled.
expect_success_implied = (opt_description_suffix, skip_worker) => {
  var description_suffix = opt_description_suffix || '';

  test(() => {
      expect_member('impliedAttribute', (testObject) => {
          return testObject.impliedAttribute;
        });
    }, 'Implied attribute should exist on object and return value' + description_suffix);

  if (!skip_worker) {
    fetch_tests_from_worker(new Worker('resources/implied-enabled-worker.js'));
  }
};

// These tests verify that the API functions correctly with a third-party trial
// that is enabled.
expect_success_third_party = () => {
  test(() => {
    expect_member('thirdPartyAttribute', (testObject) => {
      return testObject.thirdPartyAttribute;
    });
  }, 'Third-party attribute should exist on object and return value');

  // TODO(crbug.com/1083407): Implement when dedicated workers are supported for
  // third-party trials.
  // fetch_tests_from_worker(new Worker('resources/third-party-enabled-worker.js'));
};

// These tests should pass, regardless of the state of the trial. These are
// control tests for IDL members without the [RuntimeEnabled] extended
// attribute. The control tests will vary for secure vs insecure context.
expect_always_bindings = (insecure_context, opt_description_suffix) => {
  var description_suffix = opt_description_suffix || '';

  test(() => {
      assert_idl_attribute(window.internals, 'originTrialsTest');
    }, 'Test object should exist on window.internals, regardless of trial' + description_suffix);

  test(() => {
      expect_member('unconditionalAttribute', (testObject) => {
          return testObject.unconditionalAttribute;
        });
    }, 'Attribute should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_static_member('staticUnconditionalAttribute', (testObject) => {
          return testObject.staticUnconditionalAttribute;
        });
    }, 'Static attribute should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_member('unconditionalMethod', (testObject) => {
          return testObject.unconditionalMethod();
        });
    }, 'Method should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_static_member('staticUnconditionalMethod', (testObject) => {
          return testObject.staticUnconditionalMethod();
        });
    }, 'Static method should exist and return value, regardless of trial' + description_suffix);

  test(() => {
      expect_dictionary_member('unconditionalBool');
    }, 'Dictionary output from method should return member value, regardless of trial' + description_suffix);

  test(() => {
      expect_input_dictionary_member('unconditionalBool');
    }, 'Method with input dictionary should access member value, regardless of trial' + description_suffix);

  if (insecure_context) {
    test(() => {
        expect_member_fails('secureUnconditionalAttribute');
      }, 'Secure attribute should not exist, regardless of trial' + description_suffix);

    test(() => {
        expect_member_fails('secureStaticUnconditionalAttribute');
      }, 'Secure static attribute should not exist, regardless of trial' + description_suffix);

    test(() => {
        expect_member_fails('secureUnconditionalMethod');
      }, 'Secure method should not exist, regardless of trial' + description_suffix);

    test(() => {
        expect_member_fails('secureStaticUnconditionalMethod');
      }, 'Secure static method should not exist, regardless of trial' + description_suffix);
  } else {
    test(() => {
        expect_member('secureUnconditionalAttribute', (testObject) => {
            return testObject.secureUnconditionalAttribute;
          });
      }, 'Secure attribute should exist and return value, regardless of trial' + description_suffix);

    test(() => {
        expect_static_member('secureStaticUnconditionalAttribute', (testObject) => {
            return testObject.secureStaticUnconditionalAttribute;
          });
      }, 'Secure static attribute should exist and return value, regardless of trial' + description_suffix);

    test(() => {
        expect_member('secureUnconditionalMethod', (testObject) => {
            return testObject.secureUnconditionalMethod();
          });
      }, 'Secure method should exist and return value, regardless of trial' + description_suffix);

    test(() => {
        expect_static_member('secureStaticUnconditionalMethod', (testObject) => {
            return testObject.secureStaticUnconditionalMethod();
          });
      }, 'Secure static method should exist and return value, regardless of trial' + description_suffix);
  }
};

// Verify that all IDL members are correctly exposed with an enabled trial.
expect_success_bindings = (insecure_context) => {
  expect_always_bindings(insecure_context);

  if (insecure_context) {
    // Origin trials only work in secure contexts, so tests cannot distinguish
    // between [RuntimeEnabled] or [SecureContext] preventing exposure of
    // IDL members. These tests at least ensure IDL members are not exposed in
    // insecure contexts, regardless of reason.
    test(() => {
      expect_member_fails('normalAttribute');
    }, 'Attribute should not exist in insecure context');

    test(() => {
        expect_member_fails('secureAttribute');
      }, 'Secure attribute should not exist');
    test(() => {
        expect_static_member_fails('secureStaticAttribute');
      }, 'Secure static attribute should not exist');
    test(() => {
        expect_member_fails('secureMethod');
      }, 'Secure method should not exist');
    test(() => {
        expect_static_member_fails('secureStaticMethod');
      }, 'Secure static method should not exist');
    test(() => {
        expect_member_fails('secureAttributePartial');
      }, 'Secure attribute should not exist on partial interface');
    test(() => {
        expect_static_member_fails('secureStaticAttributePartial');
      }, 'Secure static attribute should not exist on partial interface');
    test(() => {
        expect_member_fails('secureMethodPartial');
      }, 'Secure method should not exist on partial interface');
    test(() => {
        expect_static_member_fails('secureStaticMethodPartial');
      }, 'Secure static method should not exist on partial interface');

    return;
  }

  test(() => {
      expect_member('normalAttribute', (testObject) => {
          return testObject.normalAttribute;
        });
    }, 'Attribute should exist and return value');

  test(() => {
      expect_static_member('staticAttribute', (testObject) => {
          return testObject.staticAttribute;
        });
    }, 'Static attribute should exist and return value');

  test(() => {
      expect_member('normalMethod', (testObject) => {
          return testObject.normalMethod();
        });
    }, 'Method should exist and return value');

  test(() => {
      expect_static_member('staticMethod', (testObject) => {
          return testObject.staticMethod();
        });
    }, 'Static method should exist and return value');

  test(() => {
      expect_dictionary_member('normalBool');
    }, 'Dictionary output from method should return member value');

  test(() => {
      expect_input_dictionary_member('normalBool');
    }, 'Method with input dictionary should access member value');

  // Tests for [RuntimeEnabled] on partial interfaces
  test(() => {
      expect_member('normalAttributePartial', (testObject) => {
          return testObject.normalAttributePartial;
        });
    }, 'Attribute should exist on partial interface and return value');

  test(() => {
      expect_static_member('staticAttributePartial', (testObject) => {
          return testObject.staticAttributePartial;
        });
    }, 'Static attribute should exist on partial interface and return value');

  test(() => {
      expect_member('normalMethodPartial', (testObject) => {
          return testObject.normalMethodPartial();
        });
    }, 'Method should exist on partial interface and return value');

  test(() => {
      expect_static_member('staticMethodPartial', (testObject) => {
          return testObject.staticMethodPartial();
        });
    }, 'Static method should exist on partial interface and return value');

  // Tests for combination of [RuntimeEnabled] and [SecureContext]
  test(() => {
      expect_member('secureAttribute', (testObject) => {
          return testObject.secureAttribute;
        });
    }, 'Secure attribute should exist and return value');

  test(() => {
      expect_static_member('secureStaticAttribute', (testObject) => {
          return testObject.secureStaticAttribute;
        });
    }, 'Secure static attribute should exist and return value');

  test(() => {
      expect_member('secureMethod', (testObject) => {
          return testObject.secureMethod();
        });
    }, 'Secure method should exist and return value');

  test(() => {
      expect_static_member('secureStaticMethod', (testObject) => {
          return testObject.secureStaticMethod();
        });
    }, 'Secure static method should exist and return value');

  test(() => {
      expect_member('secureAttributePartial', (testObject) => {
          return testObject.secureAttributePartial;
        });
    }, 'Secure attribute should exist on partial interface and return value');

  test(() => {
      expect_static_member('secureStaticAttributePartial', (testObject) => {
          return testObject.secureStaticAttributePartial;
        });
    }, 'Secure static attribute should exist on partial interface and return value');

  test(() => {
      expect_member('secureMethodPartial', (testObject) => {
          return testObject.secureMethodPartial();
        });
    }, 'Secure method should exist on partial interface and return value');

  test(() => {
      expect_static_member('secureStaticMethodPartial', (testObject) => {
          return testObject.secureStaticMethodPartial();
        });
    }, 'Secure static method should exist on partial interface and return value');

};

// Verify that all IDL members are correctly exposed with an enabled trial, with
// an insecure context.
expect_success_bindings_insecure_context = () => {
  expect_success_bindings(true);
};

// Verify that all IDL members are not exposed with a disabled trial.
expect_failure_bindings_impl = (insecure_context, description_suffix) => {
  expect_always_bindings(insecure_context, description_suffix);

  test(() => {
      expect_member_fails('normalMethod');
    }, 'Method should not exist, with trial disabled');

  test(() => {
      expect_static_member_fails('staticMethod');
    }, 'Static method should not exist, with trial disabled');

  test(() => {
      expect_dictionary_member_fails('normalBool');
    }, 'Dictionary output from method should not return member value, with trial disabled');

  test(() => {
      expect_input_dictionary_member_fails('normalBool');
    }, 'Method with input dictionary should not access member value, with trial disabled');


  // Tests for combination of [RuntimeEnabled] and [SecureContext]
  if (insecure_context) {
    // Origin trials only work in secure contexts, so tests cannot distinguish
    // between [RuntimeEnabled] or [SecureContext] preventing exposure of
    // IDL members. There are tests to ensure IDL members are not exposed in
    // insecure contexts in expect_success_bindings().
    return;
  }

  // Tests for [RuntimeEnabled] on partial interfaces
  test(() => {
      expect_member_fails('normalAttributePartial');
    }, 'Attribute should not exist on partial interface, with trial disabled');
  test(() => {
      expect_static_member_fails('staticAttributePartial');
    }, 'Static attribute should not exist on partial interface, with trial disabled');
  test(() => {
      expect_member_fails('methodPartial');
    }, 'Method should not exist on partial interface, with trial disabled');
  test(() => {
      expect_static_member_fails('staticMethodPartial');
    }, 'Static method should not exist on partial interface, with trial disabled');

  // Tests for combination of [RuntimeEnabled] and [SecureContext]
  test(() => {
      expect_member_fails('secureAttribute');
    }, 'Secure attribute should not exist, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticAttribute');
    }, 'Secure static attribute should not exist, with trial disabled');
  test(() => {
      expect_member_fails('secureMethod');
    }, 'Secure method should not exist, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticMethod');
    }, 'Secure static method should not exist, with trial disabled');
  test(() => {
      expect_member_fails('secureAttributePartial');
    }, 'Secure attribute should not exist on partial interface, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticAttributePartial');
    }, 'Secure static attribute should not exist on partial interface, with trial disabled');
  test(() => {
      expect_member_fails('secureMethodPartial');
    }, 'Secure method should not exist on partial interface, with trial disabled');
  test(() => {
      expect_static_member_fails('secureStaticMethodPartial');
    }, 'Secure static method should not exist on partial interface, with trial disabled');
};

// Verify that all IDL members are not exposed with a disabled trial.
// Assumes a secure context.
expect_failure_bindings = (description_suffix) => {
  expect_failure_bindings_impl(false, description_suffix);
};

// Verify that all IDL members are not exposed with a disabled trial, with an
// insecure context
expect_failure_bindings_insecure_context = (description_suffix) => {
  expect_failure_bindings_impl(true, description_suffix);
};
