/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides facilities for creating and querying tweaks.
 * @see http://code.google.com/p/closure-library/wiki/UsingTweaks
 */

goog.provide('goog.tweak');
goog.provide('goog.tweak.ConfigParams');

goog.require('goog.asserts');
goog.require('goog.tweak.BaseSetting');
goog.require('goog.tweak.BooleanGroup');
goog.require('goog.tweak.BooleanInGroupSetting');
goog.require('goog.tweak.BooleanSetting');
goog.require('goog.tweak.ButtonAction');
goog.require('goog.tweak.NumericSetting');
goog.require('goog.tweak.Registry');
goog.require('goog.tweak.StringSetting');
goog.requireType('goog.tweak.BaseEntry');



/**
 * The global reference to the registry, if it exists.
 * @type {?goog.tweak.Registry}
 * @private
 */
goog.tweak.registry_ = null;


/**
 * The boolean group set by beginBooleanGroup and cleared by endBooleanGroup.
 * @type {?goog.tweak.BooleanGroup}
 * @private
 */
goog.tweak.activeBooleanGroup_ = null;


/**
 * Returns/creates the registry singleton.
 * @return {!goog.tweak.Registry} The tweak registry.
 * @deprecated
 */
goog.tweak.getRegistry = function() {
  'use strict';
  if (!goog.tweak.registry_) {
    var queryString = window.location.search;
    goog.tweak.registry_ = new goog.tweak.Registry(queryString);
  }
  return goog.tweak.registry_;
};


/**
 * Type for configParams.
 * TODO(agrieve): Remove |Object when optional fields in struct types are
 *     implemented.
 * @typedef {{
 *     label:(string|undefined),
 *     validValues:(!Array<string>|!Array<number>|undefined),
 *     paramName:(string|undefined),
 *     restartRequired:(boolean|undefined),
 *     callback:(Function|undefined),
 *     token:(string|undefined)
 *     }|!Object}
 */
goog.tweak.ConfigParams;


/**
 * Applies all extra configuration parameters in configParams.
 * @param {!goog.tweak.BaseEntry} entry The entry to apply them to.
 * @param {!goog.tweak.ConfigParams} configParams Extra configuration
 *     parameters.
 * @private
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.tweak.applyConfigParams_ = function(entry, configParams) {
  'use strict';
  if (configParams.label) {
    entry.label = configParams.label;
    delete configParams.label;
  }
  if (configParams.validValues) {
    goog.asserts.assert(
        entry instanceof goog.tweak.StringSetting ||
            entry instanceof goog.tweak.NumericSetting,
        'Cannot set validValues on tweak: %s', entry.getId());
    if (entry instanceof goog.tweak.StringSetting) {
      entry.setValidValues(configParams.validValues);
    } else if (entry instanceof goog.tweak.NumericSetting) {
      entry.setValidValues(configParams.validValues);
    }
    delete configParams.validValues;
  }
  if (configParams.paramName !== undefined) {
    goog.asserts.assertInstanceof(
        entry, goog.tweak.BaseSetting, 'Cannot set paramName on tweak: %s',
        entry.getId());
    entry.setParamName(configParams.paramName);
    delete configParams.paramName;
  }
  if (configParams.restartRequired !== undefined) {
    entry.setRestartRequired(configParams.restartRequired);
    delete configParams.restartRequired;
  }
  if (configParams.callback) {
    entry.addCallback(configParams.callback);
    delete configParams.callback;
    goog.asserts.assert(
        !entry.isRestartRequired() || (configParams.restartRequired == false),
        'Tweak %s should set restartRequired: false, when adding a callback.',
        entry.getId());
  }
  if (configParams.token) {
    goog.asserts.assertInstanceof(
        entry, goog.tweak.BooleanInGroupSetting,
        'Cannot set token on tweak: %s', entry.getId());
    entry.setToken(configParams.token);
    delete configParams.token;
  }
  for (var key in configParams) {
    goog.asserts.fail(
        'Unknown config options (' + key + '=' + configParams[key] +
        ') for tweak ' + entry.getId());
  }
};


/**
 * Registers a tweak using the given factoryFunc.
 * @param {!goog.tweak.BaseEntry} entry The entry to register.
 * @param {boolean|string|number=} opt_defaultValue Default value.
 * @param {goog.tweak.ConfigParams=} opt_configParams Extra
 *     configuration parameters.
 * @private
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.tweak.doRegister_ = function(entry, opt_defaultValue, opt_configParams) {
  'use strict';
  if (opt_configParams) {
    goog.tweak.applyConfigParams_(entry, opt_configParams);
  }
  if (opt_defaultValue != undefined) {
    entry.setDefaultValue(opt_defaultValue);
  }
  if (goog.tweak.activeBooleanGroup_) {
    goog.asserts.assertInstanceof(
        entry, goog.tweak.BooleanInGroupSetting,
        'Forgot to end Boolean Group: %s',
        goog.tweak.activeBooleanGroup_.getId());
    goog.tweak.activeBooleanGroup_.addChild(
        /** @type {!goog.tweak.BooleanInGroupSetting} */ (entry));
  }
  goog.tweak.getRegistry().register(entry);
};


/**
 * Creates and registers a group of BooleanSettings that are all set by a
 * single query parameter. A call to goog.tweak.endBooleanGroup() must be used
 * to close this group. Only goog.tweak.registerBoolean() calls are allowed with
 * the beginBooleanGroup()/endBooleanGroup().
 * @param {string} id The unique ID for the setting.
 * @param {string} description A description of what the setting does.
 * @param {goog.tweak.ConfigParams=} opt_configParams Extra configuration
 *     parameters.
 */
goog.tweak.beginBooleanGroup = function(id, description, opt_configParams) {
  'use strict';
  var entry = new goog.tweak.BooleanGroup(id, description);
  goog.tweak.doRegister_(entry, undefined, opt_configParams);
  goog.tweak.activeBooleanGroup_ = entry;
};


/**
 * Stops adding boolean entries to the active boolean group.
 */
goog.tweak.endBooleanGroup = function() {
  'use strict';
  goog.tweak.activeBooleanGroup_ = null;
};


/**
 * Creates and registers a BooleanSetting.
 * @param {string} id The unique ID for the setting.
 * @param {string} description A description of what the setting does.
 * @param {boolean=} opt_defaultValue The default value for the setting.
 * @param {goog.tweak.ConfigParams=} opt_configParams Extra configuration
 *     parameters.
 * @deprecated
 */
goog.tweak.registerBoolean = function(
    id, description, opt_defaultValue, opt_configParams) {
  'use strict';
  // TODO(agrieve): There is a bug in the compiler that causes these calls not
  //     to be stripped without this outer if. Might be Issue #90.
  if (goog.tweak.activeBooleanGroup_) {
    var entry = new goog.tweak.BooleanInGroupSetting(
        id, description, goog.tweak.activeBooleanGroup_);
  } else {
    entry = new goog.tweak.BooleanSetting(id, description);
  }
  goog.tweak.doRegister_(entry, opt_defaultValue, opt_configParams);
};


/**
 * Creates and registers a StringSetting.
 * @param {string} id The unique ID for the setting.
 * @param {string} description A description of what the setting does.
 * @param {string=} opt_defaultValue The default value for the setting.
 * @param {goog.tweak.ConfigParams=} opt_configParams Extra configuration
 *     parameters.
 * @deprecated
 */
goog.tweak.registerString = function(
    id, description, opt_defaultValue, opt_configParams) {
  'use strict';
  goog.tweak.doRegister_(
      new goog.tweak.StringSetting(id, description), opt_defaultValue,
      opt_configParams);
};


/**
 * Creates and registers a NumericSetting.
 * @param {string} id The unique ID for the setting.
 * @param {string} description A description of what the setting does.
 * @param {number=} opt_defaultValue The default value for the setting.
 * @param {goog.tweak.ConfigParams=} opt_configParams Extra configuration
 *     parameters.
 * @deprecated
 */
goog.tweak.registerNumber = function(
    id, description, opt_defaultValue, opt_configParams) {
  'use strict';
  goog.tweak.doRegister_(
      new goog.tweak.NumericSetting(id, description), opt_defaultValue,
      opt_configParams);
};


/**
 * Creates and registers a ButtonAction.
 * @param {string} id The unique ID for the setting.
 * @param {string} description A description of what the action does.
 * @param {!Function} callback Function to call when the button is clicked.
 * @param {string=} opt_label The button text (instead of the ID).
 */
goog.tweak.registerButton = function(id, description, callback, opt_label) {
  'use strict';
  var tweak = new goog.tweak.ButtonAction(id, description, callback);
  tweak.label = opt_label || tweak.label;
  goog.tweak.doRegister_(tweak);
};


/**
 * Returns the value of the boolean setting with the given ID.
 * @param {string} id The unique string that identifies this entry.
 * @return {boolean} The value of the tweak.
 */
goog.tweak.getBoolean = function(id) {
  'use strict';
  return goog.tweak.getRegistry().getBooleanSetting(id).getValue();
};


/**
 * Returns the value of the string setting with the given ID,
 * @param {string} id The unique string that identifies this entry.
 * @return {string} The value of the tweak.
 */
goog.tweak.getString = function(id) {
  'use strict';
  return goog.tweak.getRegistry().getStringSetting(id).getValue();
};


/**
 * Returns the value of the numeric setting with the given ID.
 * @param {string} id The unique string that identifies this entry.
 * @return {number} The value of the tweak.
 */
goog.tweak.getNumber = function(id) {
  'use strict';
  return goog.tweak.getRegistry().getNumericSetting(id).getValue();
};
