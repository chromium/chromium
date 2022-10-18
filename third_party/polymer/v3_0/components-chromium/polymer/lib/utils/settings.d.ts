// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {pathFromUrl} from './resolve-url.js';

export {setRootPath};


/**
 * Sets the global rootPath property used by `ElementMixin` and
 * available via `rootPath`.
 */
declare function setRootPath(path: string): void;


export type SanitizeDOMValueFunction =
    (value: unknown, name: string, type: 'property'|'attribute',
     node: Node|null|undefined) => unknown;

export {setSanitizeDOMValue};


/**
 * Sets the global sanitizeDOMValue available via this module's exported
 * `sanitizeDOMValue` variable.
 */
declare function setSanitizeDOMValue(newSanitizeDOMValue: SanitizeDOMValueFunction|undefined): void;

export {getSanitizeDOMValue};


/**
 * Gets sanitizeDOMValue, for environments that don't well support `export let`.
 *
 * @returns sanitizeDOMValue
 */
declare function getSanitizeDOMValue(): SanitizeDOMValueFunction|undefined;

export {setPassiveTouchGestures};


/**
 * Sets `passiveTouchGestures` globally for all elements using Polymer Gestures.
 */
declare function setPassiveTouchGestures(usePassive: boolean): void;

export {setStrictTemplatePolicy};


/**
 * Sets `strictTemplatePolicy` globally for all elements
 */
declare function setStrictTemplatePolicy(useStrictPolicy: boolean): void;

export {setAllowTemplateFromDomModule};


/**
 * Sets `lookupTemplateFromDomModule` globally for all elements
 */
declare function setAllowTemplateFromDomModule(allowDomModule: boolean): void;

export {setLegacyOptimizations};


/**
 * Sets `legacyOptimizations` globally for all elements to enable optimizations
 * when only legacy based elements are used.
 */
declare function setLegacyOptimizations(useLegacyOptimizations: boolean): void;

export {setLegacyWarnings};


/**
 * Sets `legacyWarnings` globally for all elements to migration warnings.
 */
declare function setLegacyWarnings(useLegacyWarnings: boolean): void;

export {setSyncInitialRender};


/**
 * Sets `syncInitialRender` globally for all elements to enable synchronous
 * initial rendering.
 */
declare function setSyncInitialRender(useSyncInitialRender: boolean): void;

export {setLegacyUndefined};


/**
 * Sets `legacyUndefined` globally for all elements to enable legacy
 * multi-property behavior for undefined values.
 */
declare function setLegacyUndefined(useLegacyUndefined: boolean): void;

export {setOrderedComputed};


/**
 * Sets `orderedComputed` globally for all elements to enable ordered computed
 * property computation.
 */
declare function setOrderedComputed(useOrderedComputed: boolean): void;

export {setCancelSyntheticClickEvents};


/**
 * Sets `setCancelSyntheticEvents` globally for all elements to cancel synthetic click events.
 */
declare function setCancelSyntheticClickEvents(useCancelSyntheticClickEvents: boolean): void;

export {setRemoveNestedTemplates};


/**
 * Sets `removeNestedTemplates` globally, to eliminate nested templates
 * inside `dom-if` and `dom-repeat` as part of template parsing.
 */
declare function setRemoveNestedTemplates(useRemoveNestedTemplates: boolean): void;

export {setFastDomIf};


/**
 * Sets `fastDomIf` globally, to put `dom-if` in a performance-optimized mode.
 */
declare function setFastDomIf(useFastDomIf: boolean): void;

export {setSuppressTemplateNotifications};


/**
 * Sets `suppressTemplateNotifications` globally, to disable `dom-change` and
 * `rendered-item-count` events from `dom-if` and `dom-repeat`.
 */
declare function setSuppressTemplateNotifications(suppress: boolean): void;

export {setLegacyNoObservedAttributes};


/**
 * Sets `legacyNoObservedAttributes` globally, to disable `observedAttributes`.
 */
declare function setLegacyNoObservedAttributes(noObservedAttributes: boolean): void;

export {setUseAdoptedStyleSheetsWithBuiltCSS};


/**
 * Sets `useAdoptedStyleSheetsWithBuiltCSS` globally.
 */
declare function setUseAdoptedStyleSheetsWithBuiltCSS(value: boolean): void;

export const useShadow: boolean;
export const useNativeCSSProperties: boolean;
export const useNativeCustomElements: boolean;
export const supportsAdoptingStyleSheets: boolean;
export let legacyOptimizations: boolean;
