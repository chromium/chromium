// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

export {isPath};


/**
 * Returns true if the given string is a structured data path (has dots).
 *
 * Example:
 *
 * ```
 * isPath('foo.bar.baz') // true
 * isPath('foo')         // false
 * ```
 *
 * @returns True if the string contained one or more dots
 */
declare function isPath(path: string): boolean;

export {root};


/**
 * Returns the root property name for the given path.
 *
 * Example:
 *
 * ```
 * root('foo.bar.baz') // 'foo'
 * root('foo')         // 'foo'
 * ```
 *
 * @returns Root property name
 */
declare function root(path: string): string;

export {isAncestor};


/**
 * Given `base` is `foo.bar`, `foo` is an ancestor, `foo.bar` is not
 * Returns true if the given path is an ancestor of the base path.
 *
 * Example:
 *
 * ```
 * isAncestor('foo.bar', 'foo')         // true
 * isAncestor('foo.bar', 'foo.bar')     // false
 * isAncestor('foo.bar', 'foo.bar.baz') // false
 * ```
 *
 * @returns True if `path` is an ancestor of `base`.
 */
declare function isAncestor(base: string, path: string): boolean;

export {isDescendant};


/**
 * Given `base` is `foo.bar`, `foo.bar.baz` is an descendant
 *
 * Example:
 *
 * ```
 * isDescendant('foo.bar', 'foo.bar.baz') // true
 * isDescendant('foo.bar', 'foo.bar')     // false
 * isDescendant('foo.bar', 'foo')         // false
 * ```
 *
 * @returns True if `path` is a descendant of `base`.
 */
declare function isDescendant(base: string, path: string): boolean;

export {translate};


/**
 * Replaces a previous base path with a new base path, preserving the
 * remainder of the path.
 *
 * User must ensure `path` has a prefix of `base`.
 *
 * Example:
 *
 * ```
 * translate('foo.bar', 'zot', 'foo.bar.baz') // 'zot.baz'
 * ```
 *
 * @returns Translated string
 */
declare function translate(base: string, newBase: string, path: string): string;

export {matches};


/**
 * @returns True if `path` is equal to `base`
 */
declare function matches(base: string, path: string): boolean;

export {normalize};


/**
 * Converts array-based paths to flattened path.  String-based paths
 * are returned as-is.
 *
 * Example:
 *
 * ```
 * normalize(['foo.bar', 0, 'baz'])  // 'foo.bar.0.baz'
 * normalize('foo.bar.0.baz')        // 'foo.bar.0.baz'
 * ```
 *
 * @returns Flattened path
 */
declare function normalize(path: string|Array<string|number>): string;

export {split};


/**
 * Splits a path into an array of property names. Accepts either arrays
 * of path parts or strings.
 *
 * Example:
 *
 * ```
 * split(['foo.bar', 0, 'baz'])  // ['foo', 'bar', '0', 'baz']
 * split('foo.bar.0.baz')        // ['foo', 'bar', '0', 'baz']
 * ```
 *
 * @returns Array of path parts
 */
declare function split(path: string|Array<string|number>): string[];

export {get};


/**
 * Reads a value from a path.  If any sub-property in the path is `undefined`,
 * this method returns `undefined` (will never throw.
 *
 * @returns Value at path, or `undefined` if the path could not be
 *  fully dereferenced.
 */
declare function get(root: object|null, path: string|Array<string|number>, info?: object|null): any;

export {set};


/**
 * Sets a value to a path.  If any sub-property in the path is `undefined`,
 * this method will no-op.
 *
 * @returns The normalized version of the input path
 */
declare function set(root: object|null, path: string|Array<string|number>, value: any): string|undefined;
