// This file provides an OriginTrialsHelper object which can be used by
// WebTests that are checking members exposed to script by origin trials.
//
// The current available methods are:
// check_properties_exist:
//   Tests that the given property names exist on the given interface
//   names on the global object. It can also test for properties of the
//   global object itself by giving 'global' as the interface name.
// Example:
//   OriginTrialsHelper.check_properties_exist(
//     this,
//     {'InstallEvent':['registerForeignFetch'], 'global':['onforeignfetch']});
//
// check_properties_missing:
//   Tests that the given property names do NOT exist on the given interface
//   names on the global object.  It can also test for properties of the
//   global object itself by giving 'global' as the interface name.
//   In other words, tests for the opposite of check_properties_exist().
// Example:
//   OriginTrialsHelper.check_properties_missing(
//     this,
//     {'InstallEvent':['registerForeignFetch'], 'global':['onforeignfetch']});
//
// check_properties_missing_unless_runtime_flag
//   Tests that the given property names exist on the given interface
//   names on the global object if and only if the specified runtime flag is
//   enabled. It can also test for properties of the global object itself by
//   giving 'global' as the interface name.
//   Equivalent to calling check_properties_exist() if the specified runtime
//   flag is enabled and calling check_properties_missing() otherwise.
// Example:
//   OriginTrialsHelper.check_properties_missing_unless_runtime_flag(
//     this,
//     {'InstallEvent':['registerForeignFetch'], 'global':['onforeignfetch']},
//     'foreignFetchEnabled');
//
// check_interfaces_exist:
//   Tests that the given interface names exist on the global object.
// Example:
//   OriginTrialsHelper.check_interfaces_exist(
//     this,
//     ['USBAlternateInterface', 'USBConfiguration']);
//
// check_interfaces_missing:
//   Tests that the given interface names do NOT exist on the global object.
//   In other words, tests for the opposite of check_interfaces_exist().
// Example:
//   OriginTrialsHelper.check_interfaces_missing(
//     this,
//     ['USBAlternateInterface', 'USBConfiguration']);
//
// check_interfaces_missing_unless_runtime_flag
//   Tests that the given interface names exist on the global object if and
//   only if the specified runtime flag is enabled.
//   Equivalent to calling check_interfaces_exist() if the specified runtime
//   flag is enabled and calling check_interfaces_missing() otherwise.
// Example:
//   OriginTrialsHelper.check_interfaces_missing_unless_runtime_flag(
//     this,
//     ['USBAlternateInterface', 'USBConfiguration'],
//     'webUSBEnabled');
//
// add_token:
//   Adds a trial token to the document, to enable a trial via script
// Example:
//   OriginTrialsHelper.add_token('token produced by generate_token.py');
//
// is_runtime_flag_enabled:
//   Returns whether the specified runtime flag is enabled.
//   Throws if the specified flag does not exist, making it more robust against
//   typos and changes than checking self.internals.runtimeFlags directly.
//   Prefer using other methods except in rare cases not covered by them.
// Example:
//   if (!OriginTrialsHelper.is_runtime_flag_enabled('webXREnabled')...
'use strict';

var OriginTrialsHelper = (function() {
  function check_properties_impl(global_object, property_filters, should_exist) {
    let interface_names = Object.getOwnPropertyNames(property_filters).sort();
    interface_names.forEach(function(interface_name) {
      let interface_prototype;
      if (interface_name === 'global') {
        interface_prototype = global_object;
      } else {
        let interface_object = global_object[interface_name];
        if (interface_object) {
          interface_prototype = interface_object.prototype;
        }
      }
      assert_true(interface_prototype !== undefined, 'Interface ' + interface_name + ' exists');
      property_filters[interface_name].forEach(function(property_name) {
        assert_equals(interface_prototype.hasOwnProperty(property_name),
            should_exist,
            'Property ' + property_name + ' exists on ' + interface_name);
      });
    });
  }

  function check_interfaces_impl(global_object, interface_names, should_exist) {
    interface_names.sort();
    interface_names.forEach(function(interface_name) {
      assert_equals(global_object.hasOwnProperty(interface_name), should_exist,
        'Interface ' + interface_name + ' exists on provided object');
    });
  }

  return {
    check_properties_exist: (global_object, property_filters) => {
      check_properties_impl(global_object, property_filters, true);
    },

    check_properties_missing: (global_object, property_filters) => {
      check_properties_impl(global_object, property_filters, false);
    },

    check_properties_missing_unless_runtime_flag: (global_object, property_filters, flag_name) => {
      check_properties_impl(global_object, property_filters,
                            OriginTrialsHelper.is_runtime_flag_enabled(flag_name));
    },

    check_interfaces_exist: (global_object, interface_names) => {
      check_interfaces_impl(global_object, interface_names, true);
    },

    check_interfaces_missing: (global_object, interface_names) => {
      check_interfaces_impl(global_object, interface_names, false);
    },

    check_interfaces_missing_unless_runtime_flag: (global_object, interface_names, flag_name) => {
      check_interfaces_impl(global_object, interface_names,
                            OriginTrialsHelper.is_runtime_flag_enabled(flag_name));
    },

    add_token: (token_string) => {
      var tokenElement = document.createElement('meta');
      tokenElement.httpEquiv = 'origin-trial';
      tokenElement.content = token_string;
      document.head.appendChild(tokenElement);
    },

    is_runtime_flag_enabled: (flag_name) => {
      if (!(flag_name in self.internals.runtimeFlags))
        throw 'Runtime flag "' + flag_name + '" does not exist on self.internals.runtimeFlags';
      let flagValue = self.internals.runtimeFlags[flag_name];
      return flagValue;
    }
  }
})();
