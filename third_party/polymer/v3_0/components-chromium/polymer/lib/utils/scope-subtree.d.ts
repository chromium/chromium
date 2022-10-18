// tslint:disable:variable-name Describing an API that's defined elsewhere.

export {scopeSubtree};


/**
 * Ensure that elements in a ShadowDOM container are scoped correctly.
 * This function is only needed when ShadyDOM is used and unpatched DOM APIs are used in third party code.
 * This can happen in noPatch mode or when specialized APIs like ranges or tables are used to mutate DOM.
 *
 * @returns Returns a new MutationObserver on `container` if `shouldObserve` is true.
 */
declare function scopeSubtree(container: Element, shouldObserve?: boolean): MutationObserver|null;
