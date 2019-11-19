// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A subclass of the navigation description container
 * specialising on math objects.
 *
 */


goog.provide('cvox.NavMathDescription');

goog.require('cvox.NavDescription');


/**
 * Class specialising navigation descriptions for mathematics.
 * @param {{context: (undefined|string),
 *          text: (string),
 *          userValue: (undefined|string),
 *          annotation: (undefined|string),
 *          earcons: (undefined|Array<cvox.Earcon>),
 *          personality: (undefined|Object),
 *          hint: (undefined|string),
 *          category: (undefined|string),
 *          domain: (undefined|string),
 *          style: (undefined|string)}} kwargs The arguments for
 * the specialised math navigationdescription. See arguments of nav
 * description plus the following:
 * domain Domain for translation.
 * style Style for translation.
 * @constructor
 * @extends {cvox.NavDescription}
 */
cvox.NavMathDescription = function(kwargs) {
  goog.base(this, kwargs);

  var newPersonality = this.personality ? this.personality : {};
  var mathDescr = new Object();

  mathDescr['domain'] = kwargs.domain ? kwargs.domain : '';
  // TODO (sorge) Collate and document styles in an enum structure.
  mathDescr['style'] = kwargs.style ? kwargs.style : '';
  newPersonality['math'] = mathDescr;
  this.personality = newPersonality;
};
goog.inherits(cvox.NavMathDescription, cvox.NavDescription);
