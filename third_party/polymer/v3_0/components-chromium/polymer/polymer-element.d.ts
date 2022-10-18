// tslint:disable:variable-name Describing an API that's defined elsewhere.

import {ElementMixin} from './lib/mixins/element-mixin.js';

export {html} from './lib/utils/html-tag.js';

export {PolymerElement};

/**
 * Base class that provides the core API for Polymer's meta-programming
 * features including template stamping, data-binding, attribute deserialization,
 * and property change observation.
 */
declare class PolymerElement extends
  ElementMixin(
  HTMLElement) {
}
