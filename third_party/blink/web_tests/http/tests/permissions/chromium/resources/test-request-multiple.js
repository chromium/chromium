if (self.importScripts) {
    importScripts('../../resources/helpers.js');
    importScripts('testrunner-helpers.js');

    if (get_current_scope() == 'ServiceWorker')
        importScripts('../../../serviceworker/resources/worker-testharness.js');
    else
        importScripts('../../../resources/testharness.js');
}

var tests = [
{
    test: async_test('Test empty array in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([]).then(function(result) {
            assert_array_equals(result, []);
            callback();
        }, function(error) {
            assert_unreached(error);
            callback();
        });
    }
}, {
    test: async_test('Test single permission with update in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([{name:'geolocation'}]).then(function(result) {
            assert_equals(result.length, 1);
            assert_true(result[0] instanceof PermissionStatus);
            assert_equals(result[0].state, 'denied');
            return setPermission('geolocation', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}]);
        }).then(function(result) {
            assert_equals(result.length, 1);
            assert_true(result[0] instanceof PermissionStatus);
            assert_equals(result[0].state, 'granted');
            // Set back to denied to cleanup.
            return setPermission('geolocation', 'denied', location.origin, location.origin);
        })
        .then(callback)
        .catch(function(error) {
            assert_unreached(error);
            callback();
        });
    }
}, {
    test: async_test('Test two permissions with update in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([{name:'geolocation'}, {name:'notifications'}]).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'denied');
            }
            return setPermission('geolocation', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}, {name:'notifications'}]);
        }).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++)
                assert_true(result[i] instanceof PermissionStatus);
            assert_equals(result[0].state, 'granted');
            assert_equals(result[1].state, 'denied');
            return setPermission('notifications', 'prompt', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}, {name:'notifications'}]);
        }).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++)
                assert_true(result[i] instanceof PermissionStatus);
            assert_equals(result[0].state, 'granted');
            assert_equals(result[1].state, 'prompt');
            // Set back to denied to cleanup.
            return setPermission('geolocation', 'denied', location.origin, location.origin);
        }).then(function() {
            return setPermission('notifications', 'denied', location.origin, location.origin);
        })
        .then(callback)
        .catch(function(error) {
            assert_unreached(error);
            callback();
        });
    }
}, {
    test: async_test('Test two permissions (inverted) with update in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([{name:'notifications'}, {name:'geolocation'}]).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'denied');
            }
            return setPermission('notifications', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'notifications'}, {name:'geolocation'}]);
        }).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++)
                assert_true(result[i] instanceof PermissionStatus);
            assert_equals(result[0].state, 'granted');
            assert_equals(result[1].state, 'denied');
            return setPermission('geolocation', 'prompt', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'notifications'}, {name:'geolocation'}]);
        }).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++)
                assert_true(result[i] instanceof PermissionStatus);
            assert_equals(result[0].state, 'granted');
            assert_equals(result[1].state, 'prompt');
            // Set back to denied to cleanup.
            return setPermission('geolocation', 'denied', location.origin, location.origin);
        }).then(function() {
            return setPermission('notifications', 'denied', location.origin, location.origin);
        })
        .then(callback)
        .catch(function(error) {
            assert_unreached(error);
            callback();
        });
    }
}, {
    test: async_test('Test duplicate permissions with update in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([{name:'geolocation'}, {name:'geolocation'}]).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'denied');
            }
            return setPermission('geolocation', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}, {name:'geolocation'}]);
        }).then(function(result) {
            assert_equals(result.length, 2);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'granted');
            }
            // Set back to denied to cleanup.
            return setPermission('geolocation', 'denied', location.origin, location.origin);
        })
        .then(callback)
        .catch(function(error) {
            assert_unreached(error);
            callback();
        });
    }
}, {
    test: async_test('Test duplicate permissions (2) with update in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([{name:'geolocation'}, {name:'geolocation'}, {name:'notifications'}, {name:'notifications'}]).then(function(result) {
            assert_equals(result.length, 4);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'denied');
            }
            return setPermission('geolocation', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}, {name:'geolocation'}, {name:'notifications'}, {name:'notifications'}]);
        }).then(function(result) {
            assert_equals(result.length, 4);
            for (var i = 0; i < 2; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'granted');
            }
            for (var i = 2; i < 4; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'denied');
            }
            // Set back to denied to cleanup.
            return setPermission('geolocation', 'denied', location.origin, location.origin);
        })
        .then(callback)
        .catch(function(error) {
            assert_unreached(error);
            callback();
        });
    }
}, {
    test: async_test('Test duplicate permissions (3) with update in ' + get_current_scope() + ' scope.'),
    fn: function(callback) {
        navigator.permissions.request([{name:'geolocation'}, {name:'notifications'}, {name:'geolocation'}, {name:'notifications'}]).then(function(result) {
            assert_equals(result.length, 4);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'denied');
            }
            return setPermission('geolocation', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}, {name:'notifications'}, {name:'geolocation'}, {name:'notifications'}]);
        }).then(function(result) {
            assert_equals(result.length, 4);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
            }
            assert_equals(result[0].state, 'granted');
            assert_equals(result[1].state, 'denied');
            assert_equals(result[2].state, 'granted');
            assert_equals(result[3].state, 'denied');
            return setPermission('notifications', 'granted', location.origin, location.origin);
        }).then(function() {
            return navigator.permissions.request([{name:'geolocation'}, {name:'notifications'}, {name:'geolocation'}, {name:'notifications'}]);
        }).then(function(result) {
            assert_equals(result.length, 4);
            for (var i = 0; i < result.length; i++) {
                assert_true(result[i] instanceof PermissionStatus);
                assert_equals(result[i].state, 'granted');
            }
            // Set back to denied to cleanup.
            return setPermission('geolocation', 'denied', location.origin, location.origin);
        }).then(function() {
            return setPermission('notifications', 'denied', location.origin, location.origin);
        })
        .then(callback)
        .catch(function(error) {
            assert_unreached(error);
            callback();
        });
    }
}];

function runTest(i) {
  tests[i].test.step(function() {
      tests[i].fn(function() {
          tests[i].test.done();
          if (i + 1 < tests.length) {
              runTest(i + 1);
          } else {
              done();
          }
      });
  });
}
runTest(0);
