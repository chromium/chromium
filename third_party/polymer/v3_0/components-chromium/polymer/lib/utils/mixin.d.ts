// tslint:disable:variable-name Describing an API that's defined elsewhere.

export {dedupingMixin};


/**
 * Wraps an ES6 class expression mixin such that the mixin is only applied
 * if it has not already been applied its base argument. Also memoizes mixin
 * applications.
 */
declare function dedupingMixin<T>(mixin: T): T;
