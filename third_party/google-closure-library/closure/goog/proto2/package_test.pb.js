/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Generated Protocol Buffer code for file
 * closure/goog/proto2/package_test.proto.
 */

goog.provide('someprotopackage.TestPackageTypes');
goog.setTestOnly('someprotopackage.TestPackageTypes');

goog.require('goog.proto2.Message');
goog.require('proto2.TestAllTypes');
goog.requireType('goog.proto2.Descriptor');



/**
 * Message TestPackageTypes.
 * @constructor
 * @extends {goog.proto2.Message}
 * @final
 */
someprotopackage.TestPackageTypes = function() {
  'use strict';
  goog.proto2.Message.call(this);
};
goog.inherits(someprotopackage.TestPackageTypes, goog.proto2.Message);


/**
 * Descriptor for this message, deserialized lazily in getDescriptor().
 * @private {?goog.proto2.Descriptor}
 */
someprotopackage.TestPackageTypes.descriptor_ = null;


/**
 * Overrides {@link goog.proto2.Message#clone} to specify its exact return type.
 * @return {!someprotopackage.TestPackageTypes} The cloned message.
 * @override
 */
someprotopackage.TestPackageTypes.prototype.clone;


/**
 * Gets the value of the optional_int32 field.
 * @return {?number} The value.
 */
someprotopackage.TestPackageTypes.prototype.getOptionalInt32 = function() {
  'use strict';
  return /** @type {?number} */ (this.get$Value(1));
};


/**
 * Gets the value of the optional_int32 field or the default value if not set.
 * @return {number} The value.
 */
someprotopackage.TestPackageTypes.prototype.getOptionalInt32OrDefault =
    function() {
  'use strict';
  return /** @type {number} */ (this.get$ValueOrDefault(1));
};


/**
 * Sets the value of the optional_int32 field.
 * @param {number} value The value.
 */
someprotopackage.TestPackageTypes.prototype.setOptionalInt32 = function(value) {
  'use strict';
  this.set$Value(1, value);
};


/**
 * @return {boolean} Whether the optional_int32 field has a value.
 */
someprotopackage.TestPackageTypes.prototype.hasOptionalInt32 = function() {
  'use strict';
  return this.has$Value(1);
};


/**
 * @return {number} The number of values in the optional_int32 field.
 */
someprotopackage.TestPackageTypes.prototype.optionalInt32Count = function() {
  'use strict';
  return this.count$Values(1);
};


/**
 * Clears the values in the optional_int32 field.
 */
someprotopackage.TestPackageTypes.prototype.clearOptionalInt32 = function() {
  'use strict';
  this.clear$Field(1);
};


/**
 * Gets the value of the other_all field.
 * @return {?proto2.TestAllTypes} The value.
 */
someprotopackage.TestPackageTypes.prototype.getOtherAll = function() {
  'use strict';
  return /** @type {?proto2.TestAllTypes} */ (this.get$Value(2));
};


/**
 * Gets the value of the other_all field or the default value if not set.
 * @return {!proto2.TestAllTypes} The value.
 */
someprotopackage.TestPackageTypes.prototype.getOtherAllOrDefault = function() {
  'use strict';
  return /** @type {!proto2.TestAllTypes} */ (this.get$ValueOrDefault(2));
};


/**
 * Sets the value of the other_all field.
 * @param {!proto2.TestAllTypes} value The value.
 */
someprotopackage.TestPackageTypes.prototype.setOtherAll = function(value) {
  'use strict';
  this.set$Value(2, value);
};


/**
 * @return {boolean} Whether the other_all field has a value.
 */
someprotopackage.TestPackageTypes.prototype.hasOtherAll = function() {
  'use strict';
  return this.has$Value(2);
};


/**
 * @return {number} The number of values in the other_all field.
 */
someprotopackage.TestPackageTypes.prototype.otherAllCount = function() {
  'use strict';
  return this.count$Values(2);
};


/**
 * Clears the values in the other_all field.
 */
someprotopackage.TestPackageTypes.prototype.clearOtherAll = function() {
  'use strict';
  this.clear$Field(2);
};


/** @override */
someprotopackage.TestPackageTypes.prototype.getDescriptor = function() {
  'use strict';
  let descriptor = someprotopackage.TestPackageTypes.descriptor_;
  if (!descriptor) {
    // The descriptor is created lazily when we instantiate a new instance.
    const descriptorObj = {
      0: {
        name: 'TestPackageTypes',
        fullName: 'someprotopackage.TestPackageTypes'
      },
      1: {
        name: 'optional_int32',
        fieldType: goog.proto2.Message.FieldType.INT32,
        type: Number
      },
      2: {
        name: 'other_all',
        fieldType: goog.proto2.Message.FieldType.MESSAGE,
        type: proto2.TestAllTypes
      }
    };
    someprotopackage.TestPackageTypes.descriptor_ = descriptor =
        goog.proto2.Message.createDescriptor(
             someprotopackage.TestPackageTypes, descriptorObj);
  }
  return descriptor;
};


/** @nocollapse */
someprotopackage.TestPackageTypes.getDescriptor =
    someprotopackage.TestPackageTypes.prototype.getDescriptor;
