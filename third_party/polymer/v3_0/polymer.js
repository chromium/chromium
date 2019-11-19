/**
@license
Copyright (c) 2019 The Polymer Project Authors. All rights reserved.
This code may only be used under the BSD style license found at http://polymer.github.io/LICENSE.txt
The complete set of authors may be found at http://polymer.github.io/AUTHORS.txt
The complete set of contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt
Code distributed by Google as part of the polymer project is also
subject to an additional IP rights grant found at http://polymer.github.io/PATENTS.txt
*/

export {afterNextRender, beforeNextRender} from './lib/utils/render-status.js';
export {animationFrame, idlePeriod, microTask} from './lib/utils/async.js';
import * as gestures from './lib/utils/gestures.js';
export {gestures};
export {Base} from './polymer-legacy.js';
export {dashToCamelCase} from './lib/utils/case-map.js';
export {Debouncer, enqueueDebouncer} from './lib/utils/debounce.js';
export {dom, flush} from './lib/legacy/polymer.dom.js';
export {html} from './lib/utils/html-tag.js';
export {matches, translate} from './lib/utils/path.js';
export {OptionalMutableDataBehavior} from './lib/legacy/mutable-data-behavior.js';
export {Polymer} from './lib/legacy/polymer-fn.js';
export {PolymerElement} from './polymer-element.js';
export {TemplateInstanceBase, templatize} from './lib/utils/templatize.js';
export {Templatizer} from './lib/legacy/templatizer-behavior.js';
export {calculateSplices} from './lib/utils/array-splice.js';
export {useShadow} from './lib/utils/settings.js';
