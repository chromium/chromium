/**
 * @fileoverview Definitions for the D3.js library, based on the D3 API
 * reference at https://github.com/d3/d3/blob/master/API.md
 *
 * Some definitions were dumbed down, because JSCompiler has limited support for
 * function properties, overloads and tuples. A complete list of TypeScript
 * definitions is available at
 * https://github.com/DefinitelyTyped/DefinitelyTyped/tree/master/types/d3
 *
 * Usage examples can be found at http://blockbuilder.org/search#d3version=v4
 *
 * @externs
 */

/**
 * @const
 * @suppress {checkTypes} Prevents a mysterious compiler error triggered by the
 *     `const` annotation:
 *     "ERROR - initializing variable"
 *     found: {Arc: None, Arc_: ..., ...}
 *     required: {active: ..., arc: ..., ...}
 *     var d3 = {};
 */
var d3 = {};

/**
 * @type {string}
 */
d3.version;

////////////////////////////////////////////////////////////////////////////////
// Arrays
// https://github.com/d3/d3-array
////////////////////////////////////////////////////////////////////////////////

// API Reference

// Statistics

/**
 * TODO(pallosp): Narrow down accessor's type when
 *     https://github.com/google/closure-compiler/issues/2052 is fixed.
 * @param {!Array} array
 * @param {?Function=} accessor
 */
d3.min = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 */
d3.max = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 */
d3.extent = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 * @return {number}
 */
d3.sum = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 * @return {number | undefined}
 */
d3.mean = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 * @return {number | undefined}
 */
d3.median = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {number} p
 * @param {?Function=} accessor
 * @return {number | undefined}
 */
d3.quantile = function(array, p, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 * @return {number | undefined}
 */
d3.variance = function(array, accessor) {};

/**
 * @param {!Array} array
 * @param {?Function=} accessor
 * @return {number | undefined}
 */
d3.deviation = function(array, accessor) {};

// Search

/**
 * @param {!Array<T>} array
 * @param {function(T, T): number=} comparator
 * @return {number}
 * @template T
 */
d3.scan = function(array, comparator) {};

/**
 * @param {!Array<T>} array
 * @param {T} x
 * @param {number=} lo
 * @param {number=} hi
 * @return {number}
 * @template T
 */
d3.bisectLeft = function(array, x, lo, hi) {};

/**
 * @param {!Array<T>} array
 * @param {T} x
 * @param {number=} lo
 * @param {number=} hi
 * @return {number}
 * @template T
 */
d3.bisect = function(array, x, lo, hi) {};

/**
 * @param {!Array<T>} array
 * @param {T} x
 * @param {number=} lo
 * @param {number=} hi
 * @return {number}
 * @template T
 */
d3.bisectRight = function(array, x, lo, hi) {};

/**
 * @param {!Function} accessorOrComparator
 * @return {{
 *   left: function(!Array, ?, number=, number=): number,
 *   right: function(!Array, ?, number=, number=): number
 * }}
 */
d3.bisector = function(accessorOrComparator) {};

/**
 * @param {?} a
 * @param {?} b
 * @return {number}
 */
d3.ascending = function(a, b) {};

/**
 * @param {?} a
 * @param {?} b
 * @return {number}
 */
d3.descending = function(a, b) {};

// Transformations

/**
 * @param {!Array<T>} a
 * @param {!Array<U>} b
 * @param {function(T, U)=} reducer
 * @return {!Array}
 * @template T, U
 */
d3.cross = function(a, b, reducer) {};

/**
 * @param {!Array<!Array<T>>} arrays
 * @return {!Array<T>}
 * @template T
 */
d3.merge = function(arrays) {};

/**
 * @param {!Array<T>} array
 * @param {function(T, T)=} reducer
 * @return {!Array<!Array>}
 * @template T
 */
d3.pairs = function(array, reducer) {};

/**
 * @param {!Object<K, V>} arrayOrMap
 * @param {!Array<K>} indexes
 * @return {!Array<V>}
 * @template K, V
 */
d3.permute = function(arrayOrMap, indexes) {};

/**
 * @param {!Array<T>} array
 * @param {number=} lo
 * @param {number=} hi
 * @return {!Array<T>}
 * @template T
 */
d3.shuffle = function(array, lo, hi) {};

/**
 * @param {number} start
 * @param {number} stop
 * @param {number} count
 * @return {!Array<number>}
 */
d3.ticks = function(start, stop, count) {};

/**
 * @param {number} start
 * @param {number} stop
 * @param {number} count
 * @return {number}
 */
d3.tickIncrement = function(start, stop, count) {};

/**
 * @param {number} start
 * @param {number} stop
 * @param {number} count
 * @return {number}
 */
d3.tickStep = function(start, stop, count) {};

/**
 * @param {number} startOrStop
 * @param {number=} stop
 * @param {number=} step
 * @return {!Array<number>}
 */
d3.range = function(startOrStop, stop, step) {};

/**
 * @param {!Array<!Array<T>>} matrix
 * @return {!Array<!Array<T>>}
 * @template T
 */
d3.transpose = function(matrix) {};

/**
 * @param {...!Array<T>} var_args
 * @return {!Array<!Array<T>>}
 * @template T
 */
d3.zip = function(var_args) {};

// Histograms

/**
 * @return {!d3.Histogram}
 */
d3.histogram = function() {};

/**
 * @typedef {function(!Array<number>): !Array<!Array<number>>}
 */
d3.Histogram;

/**
 * @private {!d3.Histogram}
 */
d3.Histogram_;

/**
 * @param {function(T, number, !Array<T>): (number | !Date)=} value
 * @template T
 */
d3.Histogram_.value = function(value) {};

/**
 * @param {!Array<number | !Date> |
 *     function(!Array): !Array<number | !Date>=} domain
 */
d3.Histogram_.domain = function(domain) {};

/**
 * @param {number | !Array<number | !Date> | function(!Array, ?, ?)=}
 *     countOrThresholds
 */
d3.Histogram_.thresholds = function(countOrThresholds) {};

// Histogram Thresholds

/**
 * @param {!Array<number>} values
 * @param {number} min
 * @param {number} max
 * @return {number}
 */
d3.thresholdFreedmanDiaconis = function(values, min, max) {};

/**
 * @param {!Array<number>} values
 * @param {number} min
 * @param {number} max
 * @return {number}
 */
d3.thresholdScott = function(values, min, max) {};

/**
 * @param {!Array<number>} values
 * @return {number}
 */
d3.thresholdSturges = function(values) {};

////////////////////////////////////////////////////////////////////////////////
// Axes
// https://github.com/d3/d3-axis
////////////////////////////////////////////////////////////////////////////////

/**
 * @param {function(?): ?} scale
 * @return {!d3.Axis}
 */
d3.axisTop = function(scale) {};

/**
 * @param {function(?): ?} scale
 * @return {!d3.Axis}
 */
d3.axisRight = function(scale) {};

/**
 * @param {function(?): ?} scale
 * @return {!d3.Axis}
 */
d3.axisBottom = function(scale) {};

/**
 * @param {function(?): ?} scale
 * @return {!d3.Axis}
 */
d3.axisLeft = function(scale) {};

/**
 * @typedef {function(!(d3.selection | d3.transition))}
 */
d3.Axis;

/**
 * @private {!d3.Axis}
 */
d3.Axis_;

/**
 * @param {function(?): ?=} scale
 */
d3.Axis_.scale = function(scale) {};

/**
 * @param {?} countOrIntervalOrAny
 * @param {...?} var_args
 * @return {!d3.Axis}
 */
d3.Axis_.ticks = function(countOrIntervalOrAny, var_args) {};

/**
 * @param {!Array=} args
 */
d3.Axis_.tickArguments = function(args) {};

/**
 * @param {?Array=} values
 */
d3.Axis_.tickValues = function(values) {};

/**
 * @param {?function(?): string=} format
 */
d3.Axis_.tickFormat = function(format) {};

/**
 * @param {number=} size
 */
d3.Axis_.tickSize = function(size) {};

/**
 * @param {number=} size
 */
d3.Axis_.tickSizeInner = function(size) {};

/**
 * @param {number=} size
 */
d3.Axis_.tickSizeOuter = function(size) {};

/**
 * @param {number=} padding
 */
d3.Axis_.tickPadding = function(padding) {};

////////////////////////////////////////////////////////////////////////////////
// Brushes
// https://github.com/d3/d3-brush
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @return {!d3.Brush}
 */
d3.brush = function() {};

/**
 * @return {!d3.Brush}
 */
d3.brushX = function() {};

/**
 * @return {!d3.Brush}
 */
d3.brushY = function() {};

/**
 * @typedef {function(!d3.selection)}
 */
d3.Brush;

/**
 * @private {!d3.Brush}
 */
d3.Brush_;

/**
 * @param {!(d3.selection | d3.transition)} group
 * @param {!d3.BrushSelection |
 *     function(this:Element, T, number, !Array<T>): !d3.BrushSelection |
 *     null}
 *     selection
 * @return {void}
 * @template T
 */
d3.Brush_.move = function(group, selection) {};

/**
 * @param {!Array<!Array<number>> |
 *     function(this:Element, T, number, !Array<T>): !Array<!Array<number>>=}
 *     extent
 * @template T
 */
d3.Brush_.extent = function(extent) {};

/**
 * @param {function(this:Element, T, number, !Array<T>): boolean=} filter
 * @template T
 */
d3.Brush_.filter = function(filter) {};

/**
 * @param {number=} size
 */
d3.Brush_.handleSize = function(size) {};

/**
 * @param {string} typenames
 * @param {?function(this:Element, T, number, !Array<T>): void=} listener
 * @template T
 */
d3.Brush_.on = function(typenames, listener) {};

/**
 * @param {!Element} node
 * @return {?d3.BrushSelection}
 */
d3.brushSelection = function(node) {};

/**
 * @typedef {!Array<number> | !Array<!Array<number>>}
 */
d3.BrushSelection;

// Brush Events

/**
 * @record
 */
d3.BrushEvent = function() {};

/**
 * @type {!d3.Brush}
 */
d3.BrushEvent.prototype.target;

/**
 * @type {string}
 */
d3.BrushEvent.prototype.type;

/**
 * @type {!d3.BrushSelection}
 */
d3.BrushEvent.prototype.selection;

/**
 * @type {!Event}
 */
d3.BrushEvent.prototype.sourceEvent;

////////////////////////////////////////////////////////////////////////////////
// Chords
// https://github.com/d3/d3-chord
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @record
 */
d3.ChordGroup = function() {};

/**
 * @type {number}
 */
d3.ChordGroup.prototype.startAngle;

/**
 * @type {number}
 */
d3.ChordGroup.prototype.endAngle;

/**
 * @type {number}
 */
d3.ChordGroup.prototype.value;

/**
 * @type {number}
 */
d3.ChordGroup.prototype.index;

/**
 * @record
 * @extends {d3.ChordGroup}
 */
d3.ChordSubgroup = function() {};

/**
 * @type {number}
 */
d3.ChordSubgroup.prototype.subindex;

/**
 * @record
 * @extends {IArrayLike<{source: !d3.ChordSubgroup, target: !d3.ChordSubgroup}>}
 */
d3.ChordList = function() {};

/**
 * @type {!Array<!d3.ChordGroup>}
 */
d3.ChordList.prototype.groups;

/**
 * @return {!d3.Chord}
 */
d3.chord = function() {};

/**
 * @typedef {function(!Array<!Array<number>>): !d3.ChordList}
 */
d3.Chord;

/**
 * @private {!d3.Chord}
 */
d3.Chord_;

/**
 * @param {number=} angle
 */
d3.Chord_.padAngle = function(angle) {};

/**
 * @param {?function(number, number): number=} compare
 */
d3.Chord_.sortGroups = function(compare) {};

/**
 * @param {?function(number, number): number=} compare
 */
d3.Chord_.sortSubgroups = function(compare) {};

/**
 * @param {?function(number, number): number=} compare
 */
d3.Chord_.sortChords = function(compare) {};

/**
 * @return {!d3.Ribbon}
 */
d3.ribbon = function() {};

/**
 * @typedef {function(...?)}
 */
d3.Ribbon;

/**
 * @private {!d3.Ribbon}
 */
d3.Ribbon_;

/**
 * @param {!Function=} source
 */
d3.Ribbon_.source = function(source) {};

/**
 * @param {!Function=} target
 */
d3.Ribbon_.target = function(target) {};

/**
 * @param {number | function(...?): number=} radius
 */
d3.Ribbon_.radius = function(radius) {};

/**
 * @param {number | function(...?): number=} angle
 */
d3.Ribbon_.startAngle = function(angle) {};

/**
 * @param {number | function(...?): number=} angle
 */
d3.Ribbon_.endAngle = function(angle) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.Ribbon_.context = function(context) {};

////////////////////////////////////////////////////////////////////////////////
// Collections
// https://github.com/d3/d3-collection
////////////////////////////////////////////////////////////////////////////////

// API Reference

// Objects

/**
 * @param {!Object<?, ?>} object
 * @return {!Array<string>}
 */
d3.keys = function(object) {};

/**
 * @param {!Object<?, V>} object
 * @return {!Array<V>}
 * @template V
 */
d3.values = function(object) {};

/**
 * @param {!Object<K, V>} object
 * @return {!Array<!Object<K, V>>}
 * @template K, V
 */
d3.entries = function(object) {};

// Maps

/**
 * @param {!(d3.map<V> | Object<?, V> | Array<V>)=} object
 * @param {function(V, number): (string | number | boolean)=} keyFunction
 * @return {!d3.map<V>}
 * @constructor
 * @template V
 */
d3.map = function(object, keyFunction) {};

/**
 * @param {string | number | boolean} key
 * @return {boolean}
 */
d3.map.prototype.has = function(key) {};

/**
 * @param {string | number | boolean} key
 * @return {V | undefined}
 */
d3.map.prototype.get = function(key) {};

/**
 * @param {string | number | boolean} key
 * @param {V} value
 * @return {!d3.map<V>}
 */
d3.map.prototype.set = function(key, value) {};

/**
 * @param {string | number | boolean} key
 * @return {boolean}
 */
d3.map.prototype.remove = function(key) {};

/**
 * @return {void}
 */
d3.map.prototype.clear = function() {};

/**
 * @return {!Array<string>}
 */
d3.map.prototype.keys = function() {};

/**
 * @return {!Array<V>}
 */
d3.map.prototype.values = function() {};

/**
 * @return {!Array<{key: string, value: V}>}
 */
d3.map.prototype.entries = function() {};

/**
 * @param {function(string, V)} callback
 * @return {void}
 */
d3.map.prototype.each = function(callback) {};

/**
 * @return {boolean}
 */
d3.map.prototype.empty = function() {};

/**
 * @return {number}
 */
d3.map.prototype.size = function() {};

// Sets

/**
 * @param {!(Array | d3.set)=} arrayOrSet
 * @param {function(?, number): (string | number | boolean)=} mapper
 * @return {!d3.set}
 * @constructor
 */
d3.set = function(arrayOrSet, mapper) {};

/**
 * @param {string | number | boolean} value
 * @return {boolean}
 */
d3.set.prototype.has = function(value) {};

/**
 * @param {string | number | boolean} value
 * @return {!d3.set}
 */
d3.set.prototype.add = function(value) {};

/**
 * @param {string | number | boolean} value
 * @return {boolean}
 */
d3.set.prototype.remove = function(value) {};

/**
 * @return {void}
 */
d3.set.prototype.clear = function() {};

/**
 * @return {!Array<string>}
 */
d3.set.prototype.values = function() {};

/**
 * @param {function(string)} callback
 * @return {void}
 */
d3.set.prototype.each = function(callback) {};

/**
 * @return {boolean}
 */
d3.set.prototype.empty = function() {};

/**
 * @return {number}
 */
d3.set.prototype.size = function() {};

// Nests

/**
 * @return {!d3.Nest}
 */
d3.nest = function() {};

/**
 * @interface
 * @template T, R
 */
d3.Nest = function() {};

/**
 * @param {function(T): ?} keyFunction
 * @return {!d3.Nest}
 */
d3.Nest.prototype.key = function(keyFunction) {};

/**
 * @param {function(string, string): number} comparator
 * @return {!d3.Nest}
 */
d3.Nest.prototype.sortKeys = function(comparator) {};

/**
 * @param {function(T, T): number} comparator
 * @return {!d3.Nest}
 */
d3.Nest.prototype.sortValues = function(comparator) {};

/**
 * @param {function(!Array<T>): R} rollupFunction
 * @return {!d3.Nest}
 */
d3.Nest.prototype.rollup = function(rollupFunction) {};

/**
 * @param {!Array<T>} array
 * @return {!d3.map<!Array<T>> | !d3.map<R>}
 */
d3.Nest.prototype.map = function(array) {};

/**
 * @param {!Array<T>} array
 * @return {!Object<string, !Array<T>> | !Object<string, R>}
 */
d3.Nest.prototype.object = function(array) {};

/**
 * @param {!Array<T>} array
 * @return {!Array<{key: string, values: !Array<T>}> |
 *          !Array<{key: string, value: R}>}
 */
d3.Nest.prototype.entries = function(array) {};

////////////////////////////////////////////////////////////////////////////////
// Colors
// https://github.com/d3/d3-color
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {string} specifier
 * @return {?d3.color}
 * @constructor
 */
d3.color = function(specifier) {};

/**
 * @type {number}
 */
d3.color.prototype.opacity;

/**
 * @return {!d3.rgb}
 */
d3.color.prototype.rgb = function() {};

/**
 * @param {number=} k
 * @return {!d3.color}
 */
d3.color.prototype.brighter = function(k) {};

/**
 * @param {number=} k
 * @return {!d3.color}
 */
d3.color.prototype.darker = function(k) {};

/**
 * @return {boolean}
 */
d3.color.prototype.displayable = function() {};

/**
 * @override
 * @return {string}
 */
d3.color.prototype.toString = function() {};

/**
 * @param {number | string | !d3.color} redOrSpecifierOrColor
 * @param {number=} green
 * @param {number=} blue
 * @param {number=} opacity
 * @return {!d3.rgb}
 * @constructor
 * @extends {d3.color}
 */
d3.rgb = function(redOrSpecifierOrColor, green, blue, opacity) {};

/**
 * @type {number}
 */
d3.rgb.prototype.r;

/**
 * @type {number}
 */
d3.rgb.prototype.g;

/**
 * @type {number}
 */
d3.rgb.prototype.b;

/**
 * @param {number | string | !d3.color} hueOrSpecifierOrColor
 * @param {number=} saturation
 * @param {number=} lightness
 * @param {number=} opacity
 * @return {!d3.hsl}
 * @constructor
 * @extends {d3.color}
 */
d3.hsl = function(hueOrSpecifierOrColor, saturation, lightness,
    opacity) {};

/**
 * @type {number}
 */
d3.hsl.prototype.h;

/**
 * @type {number}
 */
d3.hsl.prototype.s;

/**
 * @type {number}
 */
d3.hsl.prototype.l;

/**
 * @param {number | string | !d3.color} lightnessOrSpecifierOrColor
 * @param {number=} a
 * @param {number=} b
 * @param {number=} opacity
 * @return {!d3.lab}
 * @constructor
 * @extends {d3.color}
 */
d3.lab = function(lightnessOrSpecifierOrColor, a, b, opacity) {};

/**
 * @type {number}
 */
d3.lab.prototype.l;

/**
 * @type {number}
 */
d3.lab.prototype.a;

/**
 * @type {number}
 */
d3.lab.prototype.b;

/**
 * @param {number} l
 * @param {number=} opacity
 * @return {!d3.lab}
 */
d3.gray = function(l, opacity) {};

/**
 * @param {number | string | !d3.color} hueOrSpecifierOrColor
 * @param {number=} chroma
 * @param {number=} luminance
 * @param {number=} opacity
 * @return {!d3.hcl}
 * @constructor
 * @extends {d3.color}
 */
d3.hcl = function(hueOrSpecifierOrColor, chroma, luminance, opacity) {};

/**
 * @type {number}
 */
d3.hcl.prototype.h;

/**
 * @type {number}
 */
d3.hcl.prototype.c;

/**
 * @type {number}
 */
d3.hcl.prototype.l;

/**
 * @param {number | string | !d3.color} luminanceOrSpecifierOrColor
 * @param {number=} chroma
 * @param {number=} hue
 * @param {number=} opacity
 * @return {!d3.hcl}
 */
d3.lch = function(luminanceOrSpecifierOrColor, chroma, hue, opacity) {};

/**
 * @param {number | string | !d3.color} hueOrSpecifierOrColor
 * @param {number=} saturation
 * @param {number=} lightness
 * @param {number=} opacity
 * @return {!d3.cubehelix}
 * @constructor
 * @extends {d3.color}
 */
d3.cubehelix = function(hueOrSpecifierOrColor, saturation, lightness,
    opacity) {};

/**
 * @type {number}
 */
d3.cubehelix.prototype.h;

/**
 * @type {number}
 */
d3.cubehelix.prototype.s;

/**
 * @type {number}
 */
d3.cubehelix.prototype.l;

////////////////////////////////////////////////////////////////////////////////
// Contours
// https://github.com/d3/d3-contour
////////////////////////////////////////////////////////////////////////////////

/**
 * @record
 * @extends {GeoJSON.MultiPolygon}
 */
d3.ContourMultiPolygon = function() {};

/**
 * @type {number}
 */
d3.ContourMultiPolygon.prototype.value;

// API reference

/**
 * @return {!d3.Contours}
 */
d3.contours = function() {};

/**
 * @typedef {function(!Array<number>): !Array<!d3.ContourMultiPolygon>}
 */
d3.Contours;

/**
 * @private {!d3.Contours}
 */
d3.Contours_;

/**
 * @param {!Array<number>} values
 * @param {number} threshold
 * @return {!d3.ContourMultiPolygon}
 */
d3.Contours_.contour = function(values, threshold) {};

/**
 * @param {!Array<number>=} size
 * @return {?}
 */
d3.Contours_.size = function(size) {};

/**
 * @param {boolean=} smooth
 * @return {?}
 */
d3.Contours_.smooth = function(smooth) {};

/**
 * @param {!(number | Array<number> | Function)=} thresholds
 * @return {?}
 */
d3.Contours_.thresholds = function(thresholds) {};

/**
 * @return {!d3.ContourDensity}
 */
d3.contourDensity = function() {};

/**
 * @typedef {function(!Array): !Array<!d3.ContourMultiPolygon>}
 */
d3.ContourDensity;

/**
 * @private {!d3.ContourDensity}
 */
d3.ContourDensity_;

/**
 * @param {number | function(T, number, !Array<T>): number=} x
 * @return {?}
 * @template T
 */
d3.ContourDensity_.x = function(x) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} y
 * @return {?}
 * @template T
 */
d3.ContourDensity_.y = function(y) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} weight
 * @return {?}
 * @template T
 */
d3.ContourDensity_.weight = function(weight) {};

/**
 * @param {!Array<number>=} size
 * @return {?}
 */
d3.ContourDensity_.size = function(size) {};

/**
 * @param {number=} cellSize
 * @return {?}
 */
d3.ContourDensity_.cellSize = function(cellSize) {};

/**
 * @param {!(number | Array<number> | Function)=} thresholds
 * @return {?}
 */
d3.ContourDensity_.thresholds = function(thresholds) {};

/**
 * @param {number=} bandwidth
 * @return {?}
 */
d3.ContourDensity_.bandwidth = function(bandwidth) {};

////////////////////////////////////////////////////////////////////////////////
// Dispatches
// https://github.com/d3/d3-dispatch
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {...string} var_args
 * @return {!d3.dispatch}
 * @constructor
 */
d3.dispatch = function(var_args) {};

/**
 * @param {string} typenames
 * @param {?Function=} listener
 */
d3.dispatch.prototype.on = function(typenames, listener) {};

/**
 * @return {!d3.dispatch}
 */
d3.dispatch.prototype.copy = function() {};

/**
 * @param {string} type
 * @param {?Object=} that
 * @param {...?} var_args
 * @return {void}
 */
d3.dispatch.prototype.call = function(type, that, var_args) {};

/**
 * @param {string} type
 * @param {?Object=} that
 * @param {!(Arguments | Array<?>)=} args
 * @return {void}
 */
d3.dispatch.prototype.apply = function(type, that, args) {};

////////////////////////////////////////////////////////////////////////////////
// Dragging
// https://github.com/d3/d3-drag
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @return {!d3.Drag}
 */
d3.drag = function() {};

/**
 * @typedef {function(!d3.selection)}
 */
d3.Drag;

/**
 * @private {!d3.Drag}
 */
d3.Drag_;

/**
 * @param {!Element | function(this:Element, T, !Array<T>): !Element=}
 *     container
 * @template T
 */
d3.Drag_.container = function(container) {};

/**
 * @param {function(this:Element, T, !Array<T>): boolean=} filter
 * @template T
 */
d3.Drag_.filter = function(filter) {};

/**
 * @param {function(this:Element): boolean=} touchable
 * @return {!Function}
 */
d3.Drag_.touchable = function(touchable) {};

/**
 * @param {function(this:Element, T, !Array<T>)=} subject
 * @template T
 */
d3.Drag_.subject = function(subject) {};

/**
 * @param {number=} distance
 * @return {?} Distance (0 arguments) or this (1 argument).
 */
d3.Drag_.clickDistance = function(distance) {};

/**
 * @param {?function(this:Element, T, number, !Array<T>): void=}
 *     listener
 * @template T
 */
d3.Drag_.on = function(typenames, listener) {};

/**
 * @param {!Window} window
 * @return {void}
 */
d3.dragDisable = function(window) {};

/**
 * @param {!Window} window
 * @param {boolean=} noclick
 * @return {void}
 */
d3.dragEnable = function(window, noclick) {};

// Drag Events

/**
 * @interface
 */
d3.DragEvent = function() {};

/**
 * @type {!d3.Drag}
 */
d3.DragEvent.prototype.target;

/**
 * @type {string}
 */
d3.DragEvent.prototype.type;

/**
 * @type {?}
 */
d3.DragEvent.prototype.subject;

/**
 * @type {number}
 */
d3.DragEvent.prototype.x;

/**
 * @type {number}
 */
d3.DragEvent.prototype.y;

/**
 * @type {number}
 */
d3.DragEvent.prototype.dx;

/**
 * @type {number}
 */
d3.DragEvent.prototype.dy;

/**
 * @type {number | string}
 */
d3.DragEvent.prototype.identifier;

/**
 * @type {number}
 */
d3.DragEvent.prototype.active;

/**
 * @type {!Event}
 */
d3.DragEvent.prototype.sourceEvent;

/**
 * @param {string} typenames
 * @param {?function(this:Element, ?, number, !IArrayLike<!Element>)=}
 *     listener
 */
d3.DragEvent.prototype.on = function(typenames, listener) {};

////////////////////////////////////////////////////////////////////////////////
// Delimiter-Separated Values
// https://github.com/d3/d3-dsv
////////////////////////////////////////////////////////////////////////////////

/**
 * @constructor
 * @extends {Array}
 */
d3.DsvParseResult = function() {};

/**
 * @type {!Array<string>}
 */
d3.DsvParseResult.prototype.columns;

/**
 * @typedef {function(
 *             !Object<string, (string | undefined)>,
 *             number,
 *             !Array<string>
 *           ): ?}
 */
d3.DsvRowConverter;

// API Reference

/**
 * @param {string} string
 * @param {!d3.DsvRowConverter=} rowConverter
 * @return {!d3.DsvParseResult}
 */
d3.csvParse = function(string, rowConverter) {};

/**
 * @param {string} string
 * @param {function(!Array<string>, number)=} rowMapper
 * @return {!Array}
 */
d3.csvParseRows = function(string, rowMapper) {};

/**
 * @param {!Array<!Object>} rows
 * @param {!Array<string>=} columnsToInclude
 * @return {string}
 */
d3.csvFormat = function(rows, columnsToInclude) {};

/**
 * @param {!Array<!Array>} rows
 * @return {string}
 */
d3.csvFormatRows = function(rows) {};

/**
 * @param {string} string
 * @param {!d3.DsvRowConverter=} rowConverter
 * @return {!d3.DsvParseResult}
 */
d3.tsvParse = function(string, rowConverter) {};

/**
 * @param {string} string
 * @param {function(!Array<string>, number)=} rowMapper
 * @return {!Array}
 */
d3.tsvParseRows = function(string, rowMapper) {};

/**
 * @param {!Array<!Object>} rows
 * @param {!Array<string>=} columnsToInclude
 * @return {string}
 */
d3.tsvFormat = function(rows, columnsToInclude) {};

/**
 * @param {!Array<!Array>} rows
 * @return {string}
 */
d3.tsvFormatRows = function(rows) {};

/**
 * @param {string} delimiter
 * @return {!d3.Dsv}
 */
d3.dsvFormat = function(delimiter) {};

/**
 * @interface
 */
d3.Dsv = function() {};

/**
 * @param {string} string
 * @param {!d3.DsvRowConverter=} rowConverter
 * @return {!d3.DsvParseResult}
 */
d3.Dsv.prototype.parse = function(string, rowConverter) {};

/**
 * @param {string} string
 * @param {function(!Array<string>, number)=} rowMapper
 * @return {!Array}
 */
d3.Dsv.prototype.parseRows = function(string, rowMapper) {};

/**
 * @param {!Array<!Object>} rows
 * @param {!Array<string>=} columns
 * @return {string}
 */
d3.Dsv.prototype.format = function(rows, columns) {};

/**
 * @param {!Array<!Array>} rows
 * @return {string}
 */
d3.Dsv.prototype.formatRows = function(rows) {};

////////////////////////////////////////////////////////////////////////////////
// Easings
// https://github.com/d3/d3-ease
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {number} t
 * @return {number}
 */
d3.easeLinear = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easePolyIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easePolyOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easePoly = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easePolyInOut = function(t) {};

/**
 * This declaration is not completely correct. It disallows the call pattern
 * d3.easePolyIn.exponent(e1).exponent(e2) which is technially valid, but not
 * very useful in practice. The alternative would be a typedef like
 * d3.ElasticEasing, but it would degrade type checking, because JSCompiler
 * doesn't understand function properties.
 *
 * @param {number} e
 * @return {function(number): number}
 */
d3.easePolyIn.exponent = function(e) {};

/**
 * @param {number} e
 * @return {function(number): number}
 */
d3.easePolyOut.exponent = function(e) {};

/**
 * @param {number} e
 * @return {function(number): number}
 */
d3.easePoly.exponent = function(e) {};

/**
 * @param {number} e
 * @return {function(number): number}
 */
d3.easePolyInOut.exponent = function(e) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeQuadIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeQuadOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeQuad = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeQuadInOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCubicIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCubicOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCubic = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCubicInOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeSinIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeSinOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeSin = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeSinInOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeExpIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeExpOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeExp = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeExpInOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCircleIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCircleOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCircle = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeCircleInOut = function(t) {};

/**
 * @type {!d3.ElasticEasing}
 */
d3.easeElasticIn;

/**
 * @type {!d3.ElasticEasing}
 */
d3.easeElastic;

/**
 * @type {!d3.ElasticEasing}
 */
d3.easeElasticOut;

/**
 * @type {!d3.ElasticEasing}
 */
d3.easeElasticInOut;

/**
 * @typedef {function(number): number}
 */
d3.ElasticEasing;

/**
 * @private {!d3.ElasticEasing}
 */
d3.ElasticEasing_;

/**
 * @param {number} a
 * @return {!d3.ElasticEasing}
 */
d3.ElasticEasing_.amplitude = function(a) {};

/**
 * @param {number} p
 * @return {!d3.ElasticEasing}
 */
d3.ElasticEasing_.period = function(p) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBackIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBackOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBack = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBackInOut = function(t) {};

/**
 * @param {number} s
 * @return {function(number): number}
 */
d3.easeBackIn.overshoot = function(s) {};

/**
 * @param {number} s
 * @return {function(number): number}
 */
d3.easeBackOut.overshoot = function(s) {};

/**
 * @param {number} s
 * @return {function(number): number}
 */
d3.easeBack.overshoot = function(s) {};

/**
 * @param {number} s
 * @return {function(number): number}
 */
d3.easeBackInOut.overshoot = function(s) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBounceIn = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBounceOut = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBounce = function(t) {};

/**
 * @param {number} t
 * @return {number}
 */
d3.easeBounceInOut = function(t) {};

////////////////////////////////////////////////////////////////////////////////
// Fetch
// https://github.com/d3/d3-fetch
////////////////////////////////////////////////////////////////////////////////

/**
 * @param {string} url
 * @param {!RequestInit=} init
 * @return {!Promise<!Blob>}
 */
d3.blob = function(url, init) {};

/**
 * @param {string} url
 * @param {!RequestInit=} init
 * @return {!Promise<!ArrayBuffer>}
 */
d3.buffer = function(url, init) {};

/**
 * @param {string} url
 * @param {?(RequestInit | d3.DsvRowConverter)=} initOrRowConverter
 * @param {?d3.DsvRowConverter=} rowConverter
 * @return {!Promise<!d3.DsvParseResult>}
 */
d3.csv = function(url, initOrRowConverter, rowConverter) {};

/**
 * @param {string} delimiter
 * @param {string} url
 * @param {?(RequestInit | d3.DsvRowConverter)=} initOrRowConverter
 * @param {?d3.DsvRowConverter=} rowConverter
 * @return {!Promise<!d3.DsvParseResult>}
 */
d3.dsv = function(delimiter, url, initOrRowConverter, rowConverter) {};

/**
 * @param {string} url
 * @param {?RequestInit=} init
 * @return {!Promise<!Document>}
 */
d3.html = function(url, init) {};

/**
 * @param {string} url
 * @param {?Object=} init
 * @return {!Promise<!HTMLImageElement>}
 */
d3.image = function(url, init) {};

/**
 * @param {string} url
 * @param {?RequestInit=} init
 * @return {!Promise<R>}
 * @template R
 */
d3.json = function(url, init) {};

/**
 * @param {string} url
 * @param {?RequestInit=} init
 * @return {!Promise<!Document>}
 */
d3.svg = function(url, init) {};

/**
 * @param {string} url
 * @param {?RequestInit=} init
 * @return {!Promise<string>}
 */
d3.text = function(url, init) {};

/**
 * @param {string} url
 * @param {?(RequestInit | d3.DsvRowConverter)=} initOrRowConverter
 * @param {?d3.DsvRowConverter=} rowConverter
 * @return {!Promise<!d3.DsvParseResult>}
 */
d3.tsv = function(url, initOrRowConverter, rowConverter) {};

/**
 * @param {string} url
 * @param {?RequestInit=} init
 * @return {!Promise<!Document>}
 */
d3.xml = function(url, init) {};

////////////////////////////////////////////////////////////////////////////////
// Forces
// https://github.com/d3/d3-force
////////////////////////////////////////////////////////////////////////////////

// Simulation

/**
 * @param {!Array<!d3.ForceNode>=} nodes
 * @return {!d3.ForceSimulation}
 */
d3.forceSimulation = function(nodes) {};

/**
 * @interface
 */
d3.ForceSimulation = function() {};

/**
 * @return {!d3.ForceSimulation}
 */
d3.ForceSimulation.prototype.restart = function() {};

/**
 * @return {!d3.ForceSimulation}
 */
d3.ForceSimulation.prototype.stop = function() {};

/**
 * @return {void}
 */
d3.ForceSimulation.prototype.tick = function() {};

/**
 * @param {!Array<!d3.ForceNode>=} nodes
 */
d3.ForceSimulation.prototype.nodes = function(nodes) {};

/**
 * @param {number=} alpha
 */
d3.ForceSimulation.prototype.alpha = function(alpha) {};

/**
 * @param {number=} min
 */
d3.ForceSimulation.prototype.alphaMin = function(min) {};

/**
 * @param {number=} decay
 */
d3.ForceSimulation.prototype.alphaDecay = function(decay) {};

/**
 * @param {number=} target
 */
d3.ForceSimulation.prototype.alphaTarget = function(target) {};

/**
 * @param {number=} decay
 */
d3.ForceSimulation.prototype.velocityDecay = function(decay) {};

/**
 * @param {string} name
 * @param {!d3.Force=} force
 */
d3.ForceSimulation.prototype.force = function(name, force) {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number=} radius
 * @return {!d3.ForceNode | undefined}
 */
d3.ForceSimulation.prototype.find = function(x, y, radius) {};

/**
 * @param {string} typenames
 * @param {?function(this:d3.ForceSimulation): void=} listener
 */
d3.ForceSimulation.prototype.on = function(typenames, listener) {};

/**
 * @record
 */
d3.ForceNode = function() {};

/**
 * @type {number | undefined}
 */
d3.ForceNode.prototype.index;

/**
 * @type {number | undefined}
 */
d3.ForceNode.prototype.x;

/**
 * @type {number | undefined}
 */
d3.ForceNode.prototype.y;

/**
 * @type {number | undefined}
 */
d3.ForceNode.prototype.vx;

/**
 * @type {number | undefined}
 */
d3.ForceNode.prototype.vy;

/**
 * @type {?number | undefined}
 */
d3.ForceNode.prototype.fx;

/**
 * @type {?number | undefined}
 */
d3.ForceNode.prototype.fy;

// Forces

/**
 * @typedef {function(number): void}
 */
d3.Force;

/**
 * @type {!d3.Force}
 */
d3.Force_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.Force_.initialize = function(nodes) {};

// Centering

/**
 * @param {number=} x
 * @param {number=} y
 * @return {!d3.CenterForce}
 */
d3.forceCenter = function(x, y) {};

/**
 * @typedef {function(number): void}
 */
d3.CenterForce;

/**
 * @private {!d3.CenterForce}
 */
d3.CenterForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.CenterForce_.initialize = function(nodes) {};

/**
 * @param {number=} x
 */
d3.CenterForce_.x = function(x) {};

/**
 * @param {number=} y
 */
d3.CenterForce_.y = function(y) {};

// Collision

/**
 * @param {number=} radius
 * @return {!d3.CollideForce}
 */
d3.forceCollide = function(radius) {};

/**
 * @typedef {function(number): void}
 */
d3.CollideForce;

/**
 * @private {!d3.CollideForce}
 */
d3.CollideForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.CollideForce_.initialize = function(nodes) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=}
 *     radius
 * @return {!Function} Radius accessor or this.
 */
d3.CollideForce_.radius = function(radius) {};

/**
 * @param {number=} strength
 */
d3.CollideForce_.strength = function(strength) {};

/**
 * @param {number=} iterations
 */
d3.CollideForce_.iterations = function(iterations) {};

// Links

/**
 * @record
 */
d3.ForceLink = function() {};

/**
 * @type {!d3.ForceNode | string | number}
 */
d3.ForceLink.prototype.source;

/**
 * @type {!d3.ForceNode | string | number}
 */
d3.ForceLink.prototype.target;

/**
 * @type {number | undefined}
 */
d3.ForceLink.prototype.index;

/**
 * @param {!Array<!d3.ForceLink>=} links
 * @return {!d3.LinkForce}
 */
d3.forceLink = function(links) {};

/**
 * @typedef {function(number): void}
 */
d3.LinkForce;

/**
 * @private {!d3.LinkForce}
 */
d3.LinkForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.LinkForce_.initialize = function(nodes) {};

/**
 * @param {!Array<!d3.ForceLink>=} links
 */
d3.LinkForce_.links = function(links) {};

/**
 * @param {function(!d3.ForceNode, number, !Array<!d3.ForceNode>): string=}
 *     id
 * @return {!Function} ID accessor or this.
 */
d3.LinkForce_.id = function(id) {};

/**
 * @param {number |
 *     function(!d3.ForceLink, number, !Array<!d3.ForceLink>): number=}
 *     distance
 * @return {!Function} Distance accessor or this.
 */
d3.LinkForce_.distance = function(distance) {};

/**
 * @param {number |
 *     function(!d3.ForceLink, number, !Array<!d3.ForceLink>): number=}
 *     strength
 * @return {!Function} Strength accessor or this.
 */
d3.LinkForce_.strength = function(strength) {};

/**
 * @param {number=} iterations
 */
d3.LinkForce_.iterations = function(iterations) {};

// Many-Body

/**
 * @return {!d3.ManyBodyForce}
 */
d3.forceManyBody = function() {};

/**
 * @typedef {function(number): void}
 */
d3.ManyBodyForce;

/**
 * @private {!d3.ManyBodyForce}
 */
d3.ManyBodyForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.ManyBodyForce_.initialize = function(nodes) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=}
 *     strength
 * @return {!Function} Strength accessor or this.
 */
d3.ManyBodyForce_.strength = function(strength) {};

/**
 * @param {number=} theta
 */
d3.ManyBodyForce_.theta = function(theta) {};

/**
 * @param {number=} distance
 */
d3.ManyBodyForce_.distanceMin = function(distance) {};

/**
 * @param {number=} distance
 */
d3.ManyBodyForce_.distanceMax = function(distance) {};

// Positioning

/**
 * @return {!d3.XForce}
 */
d3.forceX = function() {};

/**
 * @typedef {function(number): void}
 */
d3.XForce;

/**
 * @private {!d3.XForce}
 */
d3.XForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.XForce_.initialize = function(nodes) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=}
 *     strength
 * @return {!Function} Strength accessor or this.
 */
d3.XForce_.strength = function(strength) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=} x
 * @return {!Function} x accessor or this.
 */
d3.XForce_.x = function(x) {};

/**
 * @return {!d3.YForce}
 */
d3.forceY = function() {};

/**
 * @typedef {function(number): void}
 */
d3.YForce;

/**
 * @private {!d3.YForce}
 */
d3.YForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.YForce_.initialize = function(nodes) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=}
 *     strength
 * @return {!Function} Strength accessor or this.
 */
d3.YForce_.strength = function(strength) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=} y
 * @return {!Function} y accessor or this.
 */
d3.YForce_.y = function(y) {};

/**
 * @param {number} radius
 * @param {number=} x
 * @param {number=} y
 * @return {!d3.RadialForce}
 */
d3.forceRadial = function(radius, x, y) {};

/**
 * @typedef {function(number): void}
 */
d3.RadialForce;

/**
 * @private {!d3.RadialForce}
 */
d3.RadialForce_;

/**
 * @param {!Array<!d3.ForceNode>} nodes
 * @return {void}
 */
d3.RadialForce_.initialize = function(nodes) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=}
 *     strength
 * @return {?} Strength accessor function or this.
 */
d3.RadialForce_.strength = function(strength) {};

/**
 * @param {number |
 *     function(!d3.ForceNode, number, !Array<!d3.ForceNode>): number=}
 *     radius
 * @return {?} Radius accessor function or this.
 */
d3.RadialForce_.radius = function(radius) {};

/**
 * @param {number=} x
 */
d3.RadialForce_.x = function(x) {};

/**
 * @param {number=} y
 */
d3.RadialForce_.y = function(y) {};

////////////////////////////////////////////////////////////////////////////////
// Number Formats
// https://github.com/d3/d3-format
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {string} specifier
 * @return {function(number): string}
 */
d3.format = function(specifier) {};

/**
 * @param {string} specifier
 * @param {number} value
 * @return {function(number): string}
 */
d3.formatPrefix = function(specifier, value) {};

/**
 * @param {string} specifier
 * @return {!d3.formatSpecifier}
 * @constructor
 */
d3.formatSpecifier = function(specifier) {};

/**
 * @type {string}
 */
d3.formatSpecifier.prototype.fill;

/**
 * @type {string}
 */
d3.formatSpecifier.prototype.align;

/**
 * @type {string}
 */
d3.formatSpecifier.prototype.sign;

/**
 * @type {string}
 */
d3.formatSpecifier.prototype.symbol;

/**
 * @type {boolean}
 */
d3.formatSpecifier.prototype.zero;

/**
 * @type {number | undefined}
 */
d3.formatSpecifier.prototype.width;

/**
 * @type {boolean}
 */
d3.formatSpecifier.prototype.comma;

/**
 * @type {number | undefined}
 */
d3.formatSpecifier.prototype.precision;

/**
 * @type {string}
 */
d3.formatSpecifier.prototype.type;

/**
 * @param {number} step
 * @return {number}
 */
d3.precisionFixed = function(step) {};

/**
 * @param {number} step
 * @param {number} value
 * @return {number}
 */
d3.precisionPrefix = function(step, value) {};

/**
 * @param {number} step
 * @param {number} max
 * @return {number}
 */
d3.precisionRound = function(step, max) {};

// Locales

/**
 * @record
 */
d3.FormatLocaleDefinition = function() {};

/**
 * @type {string}
 */
d3.FormatLocaleDefinition.prototype.decimal;

/**
 * @type {string}
 */
d3.FormatLocaleDefinition.prototype.thousands;

/**
 * @type {!Array<number>}
 */
d3.FormatLocaleDefinition.prototype.grouping;

/**
 * @type {!Array<string>}
 */
d3.FormatLocaleDefinition.prototype.currency;

/**
 * @type {!Array<string> | undefined}
 */
d3.FormatLocaleDefinition.prototype.numerals;

/**
 * @type {string | undefined}
 */
d3.FormatLocaleDefinition.prototype.percent;

/**
 * @interface
 */
d3.FormatLocale = function() {};

/**
 * @param {string} specifier
 * @return {function(number): string}
 */
d3.FormatLocale.prototype.format = function(specifier) {};

/**
 * @param {string} specifier
 * @param {number} value
 * @return {function(number): string}
 */
d3.FormatLocale.prototype.formatPrefix = function(specifier, value) {};

/**
 * @param {!d3.FormatLocaleDefinition} definition
 * @return {!d3.FormatLocale}
 */
d3.formatLocale = function(definition) {};

/**
 * @param {!d3.FormatLocaleDefinition} definition
 * @return {!d3.FormatLocale}
 */
d3.formatDefaultLocale = function(definition) {};

////////////////////////////////////////////////////////////////////////////////
// Geographies
// https://github.com/d3/d3-geo
////////////////////////////////////////////////////////////////////////////////

// GeoJSON (http://geojson.org/geojson-spec.html)

/** @record */
var GeoJSON = function() {};

/** @type {string} */
GeoJSON.prototype.type;

/** @type {?GeoJSON.NamedCRS | GeoJSON.LinkedCRS | undefined} */
GeoJSON.prototype.crs;

/** @type {?Array<number> | undefined} */
GeoJSON.prototype.bbox;


/** @record */
GeoJSON.NamedCRS = function() {};

/** @type {string} */
GeoJSON.NamedCRS.prototype.type;

/** @type {{name: string}} */
GeoJSON.NamedCRS.prototype.properties;


/** @record */
GeoJSON.LinkedCRS = function() {};

/** @type {string} */
GeoJSON.LinkedCRS.prototype.type;

/** @type {{href: string, type: (string | undefined)}} */
GeoJSON.LinkedCRS.prototype.properties;


/** @record @extends {GeoJSON} */
GeoJSON.Geometry = function() {};


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.Point = function() {};

/** @type {!Array<number>} */
GeoJSON.Point.prototype.coordinates;


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.LineString = function() {};

/** @type {!Array<!Array<number>>} */
GeoJSON.LineString.prototype.coordinates;


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.Polygon = function() {};

/** @type {!Array<!Array<!Array<number>>>} */
GeoJSON.Polygon.prototype.coordinates;


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.MultiPoint = function() {};

/** @type {!Array<!Array<number>>} */
GeoJSON.MultiPoint.prototype.coordinates;


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.MultiLineString = function() {};

/** @type {!Array<!Array<!Array<number>>>} */
GeoJSON.MultiLineString.prototype.coordinates;


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.MultiPolygon = function() {};

/** @type {!Array<!Array<!Array<!Array<number>>>>} */
GeoJSON.MultiPolygon.prototype.coordinates;


/** @record @extends {GeoJSON.Geometry} */
GeoJSON.GeometryCollection = function() {};

/** @type {!Array<!GeoJSON.Geometry>} */
GeoJSON.GeometryCollection.prototype.geometries;


/** @record @extends {GeoJSON} */
GeoJSON.Feature = function() {};

/** @type {?GeoJSON.Geometry} */
GeoJSON.Feature.prototype.geometry;

/** @type {?Object} */
GeoJSON.Feature.prototype.properties;


/** @record @extends {GeoJSON} */
GeoJSON.FeatureCollection = function() {};

/** @type {!Array<!GeoJSON.Feature>} */
GeoJSON.FeatureCollection.prototype.features;

// Spherical Math

/**
 * [longitude in degrees, latitude in degrees]
 * @typedef {!Array<number>}
 */
d3.LngLat;

/**
 * @param {!GeoJSON.Feature} feature
 * @return {number}
 */
d3.geoArea = function(feature) {};

/**
 * @param {!GeoJSON.Feature} feature
 * @return {!Array<!d3.LngLat>}
 */
d3.geoBounds = function(feature) {};

/**
 * @param {!GeoJSON.Feature} feature
 * @return {!d3.LngLat}
 */
d3.geoCentroid = function(feature) {};

/**
 * @param {!d3.LngLat} a
 * @param {!d3.LngLat} b
 * @return {number}
 */
d3.geoDistance = function(a, b) {};

/**
 * @param {!GeoJSON.Feature} feature
 * @return {number}
 */
d3.geoLength = function(feature) {};

/**
 * @param {!d3.LngLat} a
 * @param {!d3.LngLat} b
 * @return {function(number): !d3.LngLat}
 */
d3.geoInterpolate = function(a, b) {};

/**
 * @param {!GeoJSON} object
 * @param {!d3.LngLat} point
 * @return {boolean}
 */
d3.geoContains = function(object, point) {};

/**
 * @param {!Array<number>} angles [yaw, pitch] or [yaw, pitch, roll]
 * @return {!d3.GeoRotation}
 */
d3.geoRotation = function(angles) {};

/**
 * @typedef {function(!d3.LngLat): !d3.LngLat}
 */
d3.GeoRotation;

/**
 * @private {!d3.GeoRotation}
 */
d3.GeoRotation_;

/**
 * @param {!d3.LngLat} point
 * @return {!d3.LngLat}
 */
d3.GeoRotation_.invert = function(point) {};

// Spherical Shapes

/**
 * @return {!d3.GeoCircle}
 */
d3.geoCircle = function() {};

/**
 * @typedef {function(...?): !GeoJSON.Polygon}
 */
d3.GeoCircle;

/**
 * @private {!d3.GeoCircle}
 */
d3.GeoCircle_;

/**
 * @param {!d3.LngLat | function(): !d3.LngLat=} center
 */
d3.GeoCircle_.center = function(center) {};

/**
 * @param {number | function(): number=} radius
 */
d3.GeoCircle_.radius = function(radius) {};

/**
 * @param {number | function(): number=} angle
 */
d3.GeoCircle_.precision = function(angle) {};

/**
 * @return {!d3.GeoGraticule}
 */
d3.geoGraticule = function() {};

/**
 * @typedef {function(...?): !GeoJSON.MultiLineString}
 */
d3.GeoGraticule;

/**
 * @private {!d3.GeoGraticule}
 */
d3.GeoGraticule_;

/**
 * @return {!Array<!GeoJSON.LineString>}
 */
d3.GeoGraticule_.lines = function() {};

/**
 * @return {!GeoJSON.Polygon}
 */
d3.GeoGraticule_.outline = function() {};

/**
 * @param {!Array<!d3.LngLat>=} extent
 */
d3.GeoGraticule_.extent = function(extent) {};

/**
 * @param {!Array<!d3.LngLat>=} extent
 */
d3.GeoGraticule_.extentMajor = function(extent) {};

/**
 * @param {!Array<!d3.LngLat>=} extent
 */
d3.GeoGraticule_.extentMinor = function(extent) {};

/**
 * @param {!Array<number>=} step
 */
d3.GeoGraticule_.step = function(step) {};

/**
 * @param {!Array<number>=} step
 */
d3.GeoGraticule_.stepMajor = function(step) {};

/**
 * @param {!Array<number>=} step
 */
d3.GeoGraticule_.stepMinor = function(step) {};

/**
 * @param {number=} angle
 */
d3.GeoGraticule_.precision = function(angle) {};

/**
 * @return {!GeoJSON.MultiLineString}
 */
d3.geoGraticule10 = function() {};

// Paths

/**
 * @param {?{stream: function(!d3.GeoStream): !d3.GeoStream}=} projection
 * @param {?d3.GeoPathContext=} context
 * @return {!d3.GeoPath}
 */
d3.geoPath = function(projection, context) {};

/**
 * @typedef {function(!GeoJSON, ...?): string}
 */
d3.GeoPath;

/**
 * @private {!d3.GeoPath}
 */
d3.GeoPath_;

/**
 * @param {!GeoJSON} object
 * @return {number}
 */
d3.GeoPath_.area = function(object) {};

/**
 * @param {!GeoJSON} object
 * @return {!Array<!Array<number>>}
 */
d3.GeoPath_.bounds = function(object) {};

/**
 * @param {!GeoJSON} object
 * @return {!Array<number>}
 */
d3.GeoPath_.centroid = function(object) {};

/**
 * @param {!GeoJSON} object
 * @return {number}
 */
d3.GeoPath_.measure = function(object) {};

/**
 * @param {?{stream: function(!d3.GeoStream): !d3.GeoStream}=} projection
 */
d3.GeoPath_.projection = function(projection) {};

/**
 * @param {?d3.GeoPathContext=} context
 */
d3.GeoPath_.context = function(context) {};

/**
 * @param {number | !Function=} radius
 */
d3.GeoPath_.pointRadius = function(radius) {};

/**
 * Subset of the CanvasRenderingContext2D interface.
 * @interface
 */
d3.GeoPathContext = function() {};

/**
 * @return {void}
 */
d3.GeoPathContext.prototype.beginPath = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.GeoPathContext.prototype.moveTo = function(x, y) {};

/**
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.GeoPathContext.prototype.lineTo = function(x, y) {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number} radius
 * @param {number} startAngle
 * @param {number} endAngle
 * @return {void}
 */
d3.GeoPathContext.prototype.arc =
    function(x, y, radius, startAngle, endAngle) {};

/**
 * @return {void}
 */
d3.GeoPathContext.prototype.closePath = function() {};

// Projections

/**
 * @param {!d3.RawProjection} project
 * @return {!d3.GeoProjection}
 */
d3.geoProjection = function(project) {};

/**
 * @param {!Function} factory
 * @return {!Function}
 */
d3.geoProjectionMutator = function(factory) {};

/**
 * @typedef {function(!d3.LngLat): ?Array<number>}
 */
d3.GeoProjection;

/**
 * @private {!d3.GeoProjection}
 */
d3.GeoProjection_;

/**
 * @param {!Array<number>} point
 * @return {?d3.LngLat}
 */
d3.GeoProjection_.invert = function(point) {};

/**
 * @param {!d3.GeoStream} stream
 * @return {!d3.GeoStream}
 */
d3.GeoProjection_.stream = function(stream) {};

/**
 * @param {function(!d3.GeoStream): !d3.GeoStream=} preclip
 * @return {!Function}
 */
d3.GeoProjection_.preclip = function(preclip) {};

/**
 * @param {function(!d3.GeoStream): !d3.GeoStream=} postclip
 * @return {!Function}
 */
d3.GeoProjection_.postclip = function(postclip) {};

/**
 * @param {?number=} angle
 */
d3.GeoProjection_.clipAngle = function(angle) {};

/**
 * @param {?Array<!Array<number>>=} extent
 */
d3.GeoProjection_.clipExtent = function(extent) {};

/**
 * @param {number=} scale
 */
d3.GeoProjection_.scale = function(scale) {};

/**
 * @param {!Array<number>=} translate
 */
d3.GeoProjection_.translate = function(translate) {};

/**
 * @param {!d3.LngLat=} center
 */
d3.GeoProjection_.center = function(center) {};

/**
 * @param {!Array<number>=} angles
 */
d3.GeoProjection_.rotate = function(angles) {};

/**
 * @param {number=} precision
 */
d3.GeoProjection_.precision = function(precision) {};

/**
 * @param {!Array<!Array<number>>} extent
 * @param {!GeoJSON} object
 * @return {!d3.GeoProjection}
 */
d3.GeoProjection_.fitExtent = function(extent, object) {};

/**
 * @param {!Array<number>} size
 * @param {!GeoJSON} object
 * @return {!d3.GeoProjection}
 */
d3.GeoProjection_.fitSize = function(size, object) {};

/**
 * @param {number} width
 * @param {!GeoJSON} object
 * @return {!d3.GeoProjection}
 */
d3.GeoProjection_.fitWidth = function(width, object) {};

/**
 * @param {number} height
 * @param {!GeoJSON} object
 * @return {!d3.GeoProjection}
 */
d3.GeoProjection_.fitHeight = function(height, object) {};

/**
 * Only exists for conic projections.
 * @type {function(!Array<number>=) | undefined}
 */
d3.GeoProjection_.parallels;

/**
 * @return {!d3.GeoProjection}
 */
d3.geoAlbers = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoAlbersUsa = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoAzimuthalEqualArea = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoAzimuthalEquidistant = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoConicConformal = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoConicEqualArea = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoConicEquidistant = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoEquirectangular = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoGnomonic = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoMercator = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoOrthographic = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoStereographic = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoEqualEarth = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoTransverseMercator = function() {};

/**
 * @return {!d3.GeoProjection}
 */
d3.geoNaturalEarth1 = function() {};

/**
 * @deprecated Use d3.geoIdentity's clipExtent instead.
 */
d3.geoClipExtent = null;

// Raw Projections

/**
 * (longitude in radians, latitude in radians) => [x, y]
 * @typedef {function(number, number): !Array<number>}
 */
d3.RawProjection;

/**
 * @private {!d3.RawProjection}
 */
d3.RawProjection_;

/**
 * @param {number} x
 * @param {number} y
 * @return {!Array<number>}
 */
d3.RawProjection_.invert = function(x, y) {};

/**
 * @type {!d3.RawProjection}
 */
d3.geoAzimuthalEqualAreaRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoAzimuthalEquidistantRaw;

/**
 * @param {number} phi0
 * @param {number} phi1
 * @return {!d3.RawProjection}
 */
d3.geoConicConformalRaw = function(phi0, phi1) {};

/**
 * @param {number} phi0
 * @param {number} phi1
 * @return {!d3.RawProjection}
 */
d3.geoConicEqualAreaRaw = function(phi0, phi1) {};

/**
 * @param {number} phi0
 * @param {number} phi1
 * @return {!d3.RawProjection}
 */
d3.geoConicEquidistantRaw = function(phi0, phi1) {};

/**
 * @type {!d3.RawProjection}
 */
d3.geoEquirectangularRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoGnomonicRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoMercatorRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoOrthographicRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoStereographicRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoEqualEarthRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoTransverseMercatorRaw;

/**
 * @type {!d3.RawProjection}
 */
d3.geoNaturalEarth1Raw;

// Projection Streams

/**
 * @param {!GeoJSON} object
 * @param {!d3.GeoStream} stream
 * @return {void}
 */
d3.geoStream = function(object, stream) {};

/**
 * @record
 */
d3.GeoStream = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number=} z
 * @return {void}
 */
d3.GeoStream.prototype.point = function(x, y, z) {};

/**
 * @return {void}
 */
d3.GeoStream.prototype.lineStart = function() {};

/**
 * @return {void}
 */
d3.GeoStream.prototype.lineEnd = function() {};

/**
 * @return {void}
 */
d3.GeoStream.prototype.polygonStart = function() {};

/**
 * @return {void}
 */
d3.GeoStream.prototype.polygonEnd = function() {};

/**
 * @return {void}
 */
d3.GeoStream.prototype.sphere = function() {};

// Transforms

/**
 * @record
 */
d3.GeoStreamOverrides = function() {};

/**
 * @type {undefined |
 *     function(this:d3.GeoStreamOverrides, number, number, number=): void}
 */
d3.GeoStreamOverrides.prototype.point;

/**
 * @type {undefined | function(this:d3.GeoStreamOverrides): void}
 */
d3.GeoStreamOverrides.prototype.lineStart;

/**
 * @type {undefined | function(this:d3.GeoStreamOverrides): void}
 */
d3.GeoStreamOverrides.prototype.lineEnd;

/**
 * @type {undefined | function(this:d3.GeoStreamOverrides): void}
 */
d3.GeoStreamOverrides.prototype.polygonStart;

/**
 * @type {undefined | function(this:d3.GeoStreamOverrides): void}
 */
d3.GeoStreamOverrides.prototype.polygonEnd;

/**
 * @type {undefined | function(this:d3.GeoStreamOverrides): void}
 */
d3.GeoStreamOverrides.prototype.sphere;

/**
 * @type {undefined | !d3.GeoStream}
 */
d3.GeoStreamOverrides.prototype.stream;

/**
 * @param {!d3.GeoStreamOverrides} methods
 * @return {{stream: function(!d3.GeoStream): !d3.GeoStream}}
 */
d3.geoTransform = function(methods) {};

/**
 * @return {!d3.GeoIdentity}
 */
d3.geoIdentity = function() {};

/**
 * @interface
 */
d3.GeoIdentity = function() {};

/**
 * @param {number=} scale
 */
d3.GeoIdentity.prototype.scale = function(scale) {};

/**
 * @param {!Array<number>=} translate
 */
d3.GeoIdentity.prototype.translate = function(translate) {};

/**
 * @param {boolean=} reflect
 */
d3.GeoIdentity.prototype.reflectX = function(reflect) {};

/**
 * @param {boolean=} reflect
 */
d3.GeoIdentity.prototype.reflectY = function(reflect) {};

/**
 * @param {?Array<!Array<number>>=} extent
 */
d3.GeoIdentity.prototype.clipExtent = function(extent) {};

/**
 * @param {!Array<number>} size
 * @param {!GeoJSON} object
 * @return {!d3.GeoIdentity}
 */
d3.GeoIdentity.prototype.fitSize = function(size, object) {};

/**
 * @param {!Array<!Array<number>>} extent
 * @param {!GeoJSON} object
 * @return {!d3.GeoIdentity}
 */
d3.GeoIdentity.prototype.fitExtent = function(extent, object) {};

/**
 * @param {!d3.GeoStream} stream
 * @return {!d3.GeoStream}
 */
d3.GeoIdentity.prototype.stream = function(stream) {};

// Clipping

/**
 * @param {!d3.GeoStream} stream
 * @return {!d3.GeoStream}
 */
d3.geoClipAntimeridian = function(stream) {};

/**
 * @param {number} angle
 * @return {function(!d3.GeoStream): !d3.GeoStream}
 */
d3.geoClipCircle = function(angle) {};

/**
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {function(!d3.GeoStream): !d3.GeoStream}
 */
d3.geoClipRectangle = function(x0, y0, x1, y1) {};

////////////////////////////////////////////////////////////////////////////////
// Hierarchy
// https://github.com/d3/d3-hierarchy
////////////////////////////////////////////////////////////////////////////////

// Hierarchy

/**
 * @param {T} data
 * @param {function(T): ?Array<T>=} children
 * @return {!d3.hierarchy<T>}
 * @constructor
 * @template T
 */
d3.hierarchy = function(data, children) {};

/**
 * @type {T}
 */
d3.hierarchy.prototype.data;

/**
 * @type {number}
 */
d3.hierarchy.prototype.depth;

/**
 * @type {number}
 */
d3.hierarchy.prototype.height;

/**
 * @type {?d3.hierarchy}
 */
d3.hierarchy.prototype.parent;

/**
 * @type {!Array<!d3.hierarchy<T>> | undefined}
 */
d3.hierarchy.prototype.children;

/**
 * @type {number | undefined}
 */
d3.hierarchy.prototype.value;

/**
 * @return {!Array<!d3.hierarchy<T>>}
 */
d3.hierarchy.prototype.ancestors = function() {};

/**
 * @return {!Array<!d3.hierarchy<T>>}
 */
d3.hierarchy.prototype.descendants = function() {};

/**
 * @return {!Array<!d3.hierarchy<T>>}
 */
d3.hierarchy.prototype.leaves = function() {};

/**
 * @param {!d3.hierarchy} target
 * @return {!Array<!d3.hierarchy<T>>}
 */
d3.hierarchy.prototype.path = function(target) {};

/**
 * @return {!Array<{source: !d3.hierarchy<T>, target: !d3.hierarchy<T>}>}
 */
d3.hierarchy.prototype.links = function() {};

/**
 * @return {!d3.hierarchy}
 */
d3.hierarchy.prototype.count = function() {};

/**
 * @param {function(T): number} value
 * @return {!d3.hierarchy}
 */
d3.hierarchy.prototype.sum = function(value) {};

/**
 * @param {function(!d3.hierarchy<T>, !d3.hierarchy<T>): number} compare
 * @return {!d3.hierarchy}
 */
d3.hierarchy.prototype.sort = function(compare) {};

/**
 * @param {function(!d3.hierarchy<T>): void} callback
 */
d3.hierarchy.prototype.each = function(callback) {};

/**
 * @param {function(!d3.hierarchy<T>): void} callback
 */
d3.hierarchy.prototype.eachAfter = function(callback) {};

/**
 * @param {function(!d3.hierarchy<T>): void} callback
 */
d3.hierarchy.prototype.eachBefore = function(callback) {};

/**
 * @return {!d3.hierarchy<T>}
 */
d3.hierarchy.prototype.copy = function() {};

// Stratify

/**
 * @return {!d3.Stratify}
 */
d3.stratify = function() {};

/**
 * @typedef {function(!Array): !d3.hierarchy}
 */
d3.Stratify;

/**
 * @private {!d3.Stratify}
 */
d3.Stratify_;

/**
 * @param {function(T, number, !Array<T>): ?(string | undefined)=} id
 * @template T
 */
d3.Stratify_.id = function(id) {};

/**
 * @param {function(T, number, !Array<T>): ?(string | undefined)=} parentId
 * @template T
 */
d3.Stratify_.parentId = function(parentId) {};

// Cluster

/**
 * @return {!d3.Cluster}
 */
d3.cluster = function() {};

/**
 * @typedef {function(!d3.hierarchy)}
 */
d3.Cluster;

/**
 * @private {!d3.Cluster}
 */
d3.Cluster_;

/**
 * @param {!Array<number>=} size
 */
d3.Cluster_.size = function(size) {};

/**
 * @param {!Array<number>=} size
 */
d3.Cluster_.nodeSize = function(size) {};

/**
 * @param {function(?, ?): number=} separation
 */
d3.Cluster_.separation = function(separation) {};

// Tree

/**
 * @return {!d3.Tree}
 */
d3.tree = function() {};

/**
 * @typedef {function(!d3.hierarchy)}
 */
d3.Tree;

/**
 * @private {!d3.Tree}
 */
d3.Tree_;

/**
 * @param {!Array<number>=} size
 */
d3.Tree_.size = function(size) {};

/**
 * @param {!Array<number>=} size
 */
d3.Tree_.nodeSize = function(size) {};

/**
 * @param {function(?, ?): number=} separation
 */
d3.Tree_.separation = function(separation) {};

// Treemap

/**
 * @return {!d3.Treemap}
 */
d3.treemap = function() {};

/**
 * @typedef {function(!d3.hierarchy)}
 */
d3.Treemap;

/**
 * @private {!d3.Treemap}
 */
d3.Treemap_;

/**
 * @param {{node: !d3.hierarchy,
 *          x0: number,
 *          y0: number,
 *          x1: number,
 *          y1: number}=} tile
 */
d3.Treemap_.tile = function(tile) {};

/**
 * @param {!Array<number>=} size
 */
d3.Treemap_.size = function(size) {};

/**
 * @param {boolean=} round
 */
d3.Treemap_.round = function(round) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.padding = function(padding) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.paddingInner = function(padding) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.paddingOuter = function(padding) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.paddingTop = function(padding) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.paddingRight = function(padding) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.paddingBottom = function(padding) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Treemap_.paddingLeft = function(padding) {};

// Treemap Tiling

/**
 * @param {!d3.hierarchy} node
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {void}
 */
d3.treemapBinary = function(node, x0, y0, x1, y1) {};

/**
 * @param {!d3.hierarchy} node
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {void}
 */
d3.treemapDice = function(node, x0, y0, x1, y1) {};

/**
 * @param {!d3.hierarchy} node
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {void}
 */
d3.treemapSlice = function(node, x0, y0, x1, y1) {};

/**
 * @param {!d3.hierarchy} node
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {void}
 */
d3.treemapSliceDice = function(node, x0, y0, x1, y1) {};

/**
 * @param {!d3.hierarchy} node
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {void}
 */
d3.treemapSquarify = function(node, x0, y0, x1, y1) {};

/**
 * @param {number} ratio
 * @return {function(!d3.hierarchy, number, number, number, number): void}
 */
d3.treemapSquarify.ratio = function(ratio) {};

/**
 * @param {!d3.hierarchy} node
 * @param {number} x0
 * @param {number} y0
 * @param {number} x1
 * @param {number} y1
 * @return {void}
 */
d3.treemapResquarify = function(node, x0, y0, x1, y1) {};

/**
 * @param {number} ratio
 * @return {function(!d3.hierarchy, number, number, number, number)}
 */
d3.treemapResquarify.ratio = function(ratio) {};

// Partition

/**
 * @return {!d3.Partition}
 */
d3.partition = function() {};

/**
 * @typedef {function(!d3.hierarchy)}
 */
d3.Partition;

/**
 * @private {!d3.Partition}
 */
d3.Partition_;

/**
 * @param {!Array<number>=} size
 */
d3.Partition_.size = function(size) {};

/**
 * @param {boolean=} round
 */
d3.Partition_.round = function(round) {};

/**
 * @param {number=} padding
 */
d3.Partition_.padding = function(padding) {};

// Pack

/**
 * @return {!d3.Pack}
 */
d3.pack = function() {};

/**
 * @typedef {function(!d3.hierarchy)}
 */
d3.Pack;

/**
 * @private {!d3.Pack}
 */
d3.Pack_;

/**
 * @param {function(?): number=} radius
 */
d3.Pack_.radius = function(radius) {};

/**
 * @param {!Array<number>=} size
 */
d3.Pack_.size = function(size) {};

/**
 * @param {number | function(?): number=} padding
 */
d3.Pack_.padding = function(padding) {};

/**
 * @param {!Array<{r: number}>} circles
 * @return {!Array<{x: number, y: number, r: number}>}
 */
d3.packSiblings = function(circles) {};

/**
 * @param {!Array<{x: number, y: number, r: number}>} circles
 * @return {{x: number, y: number, r: number}}
 */
d3.packEnclose = function(circles) {};

////////////////////////////////////////////////////////////////////////////////
// Interpolators
// https://github.com/d3/d3-interpolate
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {T} a
 * @param {T} b
 * @return {function(number): T}
 * @template T
 */
d3.interpolate = function(a, b) {};

/**
 * @param {number} a
 * @param {number} b
 * @return {function(number): number}
 */
d3.interpolateNumber = function(a, b) {};

/**
 * @param {number} a
 * @param {number} b
 * @return {function(number): number}
 */
d3.interpolateRound = function(a, b) {};

/**
 * @param {string} a
 * @param {string} b
 * @return {function(number): string}
 */
d3.interpolateString = function(a, b) {};

/**
 * @param {number | !Date} a
 * @param {number | !Date} b
 * @return {function(number): !Date}
 */
d3.interpolateDate = function(a, b) {};

/**
 * @param {!Array<T>} a
 * @param {!Array<T>} b
 * @return {function(number): !Array<T>}
 * @template T
 */
d3.interpolateArray = function(a, b) {};

/**
 * @param {!Object} a
 * @param {!Object} b
 * @return {function(number): !Object}
 */
d3.interpolateObject = function(a, b) {};

/**
 * @param {string} a
 * @param {string} b
 * @return {function(number): string}
 */
d3.interpolateTransformCss = function(a, b) {};

/**
 * @param {string} a
 * @param {string} b
 * @return {function(number): string}
 */
d3.interpolateTransformSvg = function(a, b) {};

/**
 * @param {!Array<number>} a
 * @param {!Array<number>} b
 * @return {function(number): !Array<number>}
 */
d3.interpolateZoom = function(a, b) {};

/**
 * @param {!Array<T>} values
 * @return {function(number): T}
 * @template T
 */
d3.interpolateDiscrete = function(values) {};

// Sampling

/**
 * @param {function(number): T} interpolator
 * @param {number} n
 * @return {!Array<T>}
 * @template T
 */
d3.quantize = function(interpolator, n) {};

// Color Spaces

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateRgb = function(a, b) {};

/**
 * @param {number} gamma
 * @return {function((string | !d3.color), (string | !d3.color)):
 *     function(number): string}
 */
d3.interpolateRgb.gamma = function(gamma) {};

/**
 * @param {!Array<string | !d3.color>} colors
 * @return {function(number): string}
 */
d3.interpolateRgbBasis = function(colors) {};

/**
 * @param {!Array<string | !d3.color>} colors
 * @return {function(number): string}
 */
d3.interpolateRgbBasisClosed = function(colors) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateHsl = function(a, b) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateHslLong = function(a, b) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateLab = function(a, b) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateHcl = function(a, b) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateHclLong = function(a, b) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateCubehelix = function(a, b) {};

/**
 * @param {number} gamma
 * @return {function((string | !d3.color), (string | !d3.color)):
 *     function(number): string}
 */
d3.interpolateCubehelix.gamma = function(gamma) {};

/**
 * @param {string | !d3.color} a
 * @param {string | !d3.color} b
 * @return {function(number): string}
 */
d3.interpolateCubehelixLong = function(a, b) {};

/**
 * @param {number} gamma
 * @return {function((string | !d3.color), (string | !d3.color)):
 *     function(number): string}
 */
d3.interpolateCubehelixLong.gamma = function(gamma) {};

/**
 * @param {number} a
 * @param {number} b
 * @return {function(number): number}
 */
d3.interpolateHue = function(a, b) {};

// Splines

/**
 * @param {!Array<number>} values
 * @return {function(number): number}
 */
d3.interpolateBasis = function(values) {};

/**
 * @param {!Array<number>} values
 * @return {function(number): number}
 */
d3.interpolateBasisClosed = function(values) {};

// Piecewise

/**
 * @param {function(T, T): function(number): T} interpolate
 * @param {!Array<T>} values
 * @return {function(number): T}
 * @template T
 */
d3.piecewise = function(interpolate, values) {};

////////////////////////////////////////////////////////////////////////////////
// Paths
// https://github.com/d3/d3-path
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @constructor
 * @return {!d3.path}
 */
d3.path = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.path.prototype.moveTo = function(x, y) {};

/**
 * @return {void}
 */
d3.path.prototype.closePath = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.path.prototype.lineTo = function(x, y) {};

/**
 * @param {number} cpx
 * @param {number} cpy
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.path.prototype.quadraticCurveTo = function(cpx, cpy, x, y) {};

/**
 * @param {number} cpx1
 * @param {number} cpy1
 * @param {number} cpx2
 * @param {number} cpy2
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.path.prototype.bezierCurveTo = function(cpx1, cpy1, cpx2, cpy2, x, y) {};

/**
 * @param {number} x1
 * @param {number} y1
 * @param {number} x2
 * @param {number} y2
 * @param {number} radius
 * @return {void}
 */
d3.path.prototype.arcTo = function(x1, y1, x2, y2, radius) {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number} radius
 * @param {number} startAngle
 * @param {number} endAngle
 * @param {boolean=} anticlockwise
 * @return {void}
 */
d3.path.prototype.arc = function(x, y, radius, startAngle, endAngle,
    anticlockwise) {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number} w
 * @param {number} h
 * @return {void}
 */
d3.path.prototype.rect = function(x, y, w, h) {};

/**
 * @override
 * @return {string}
 */
d3.path.prototype.toString = function() {};

////////////////////////////////////////////////////////////////////////////////
// Polygons
// https://github.com/d3/d3-polygon
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {!Array<!Array<number>>} polygon
 * @return {number}
 */
d3.polygonArea = function(polygon) {};

/**
 * @param {!Array<!Array<number>>} polygon
 * @return {!Array<number>}
 */
d3.polygonCentroid = function(polygon) {};

/**
 * @param {!Array<!Array<number>>} points
 * @return {?Array<!Array<number>>}
 */
d3.polygonHull = function(points) {};

/**
 * @param {!Array<!Array<number>>} polygon
 * @param {!Array<number>} point
 * @return {boolean}
 */
d3.polygonContains = function(polygon, point) {};

/**
 * @param {!Array<!Array<number>>} polygon
 * @return {number}
 */
d3.polygonLength = function(polygon) {};

////////////////////////////////////////////////////////////////////////////////
// Quadtrees
// https://github.com/d3/d3-quadtree
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {!Array<T>=} data
 * @param {function(T): number=} x
 * @param {function(T): number=} y
 * @constructor
 * @return {!d3.quadtree<T>}
 * @template T
 */
d3.quadtree = function(data, x, y) {};

/**
 * @param {function(T): number=} x
 */
d3.quadtree.prototype.x = function(x) {};

/**
 * @param {function(T): number=} y
 */
d3.quadtree.prototype.y = function(y) {};

/**
 * @param {!Array<!Array<number>>=} extent
 */
d3.quadtree.prototype.extent = function(extent) {};

/**
 * @param {number} x
 * @param {number} y
 * @return {!d3.quadtree<T>}
 */
d3.quadtree.prototype.cover = function(x, y) {};

/**
 * @param {T} datum
 * @return {!d3.quadtree<T>}
 */
d3.quadtree.prototype.add = function(datum) {};

/**
 * @param {!Array<T>} data
 * @return {!d3.quadtree<T>}
 */
d3.quadtree.prototype.addAll = function(data) {};

/**
 * @param {T} datum
 * @return {!d3.quadtree<T>}
 */
d3.quadtree.prototype.remove = function(datum) {};

/**
 * @param {!Array<T>} data
 * @return {!d3.quadtree<T>}
 */
d3.quadtree.prototype.removeAll = function(data) {};

/**
 * @return {!d3.quadtree<T>}
 */
d3.quadtree.prototype.copy = function() {};

/**
 * @return {!d3.QuadtreeNode | undefined}
 */
d3.quadtree.prototype.root = function() {};

/**
 * @return {!Array<T>}
 */
d3.quadtree.prototype.data = function() {};

/**
 * @return {number}
 */
d3.quadtree.prototype.size = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number=} radius
 * @return {T | undefined}
 */
d3.quadtree.prototype.find = function(x, y, radius) {};

/**
 * @param {function(!d3.QuadtreeNode, number, number, number, number):
 *     (boolean | undefined)} callback
 */
d3.quadtree.prototype.visit = function(callback) {};

/**
 * @param {function(!d3.QuadtreeNode, number, number, number, number): void}
 *     callback
 */
d3.quadtree.prototype.visitAfter = function(callback) {};

// Nodes

/**
 * @record
 */
d3.QuadtreeLeaf = function() {};

/**
 * @type {?}
 */
d3.QuadtreeLeaf.prototype.data;

/**
 * @type {!d3.QuadtreeLeaf | undefined}
 */
d3.QuadtreeLeaf.prototype.next;

/**
 * JSCompiler doesn't support recursive typedefs, therefore we use Object
 * instead of (!d3.QuadtreeNode | !d3.QuadtreeLeaf) for child nodes.
 * @typedef {!d3.QuadtreeLeaf | !Array<!Object | undefined>}
 */
d3.QuadtreeNode;

////////////////////////////////////////////////////////////////////////////////
// Random Numbers
// https://github.com/d3/d3-random
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {number=} minOrMax
 * @param {number=} max
 * @return {function(): number}
 */
d3.randomUniform = function(minOrMax, max) {};

/**
 * @param {function(): number} source
 * @return {function(number=, number=): function(): number}
 */
d3.randomUniform.source = function(source) {};

/**
 * @param {number=} mu
 * @param {number=} sigma
 * @return {function(): number}
 */
d3.randomNormal = function(mu, sigma) {};

/**
 * @param {function(): number} source
 * @return {function(number=, number=): function(): number}
 */
d3.randomNormal.source = function(source) {};

/**
 * @param {number=} mu
 * @param {number=} sigma
 * @return {function(): number}
 */
d3.randomLogNormal = function(mu, sigma) {};

/**
 * @param {function(): number} source
 * @return {function(number=, number=): function(): number}
 */
d3.randomLogNormal.source = function(source) {};

/**
 * @param {number} n
 * @return {function(): number}
 */
d3.randomBates = function(n) {};

/**
 * @param {function(): number} source
 * @return {function(number): function(): number}
 */
d3.randomBates.source = function(source) {};

/**
 * @param {number} n
 * @return {function(): number}
 */
d3.randomIrwinHall = function(n) {};

/**
 * @param {function(): number} source
 * @return {function(number): function(): number}
 */
d3.randomIrwinHall.source = function(source) {};

/**
 * @param {number} lambda
 * @return {function(): number}
 */
d3.randomExponential = function(lambda) {};

/**
 * @param {function(): number} source
 * @return {function(number): function(): number}
 */
d3.randomExponential.source = function(source) {};

////////////////////////////////////////////////////////////////////////////////
// Scales
// https://github.com/d3/d3-scale
////////////////////////////////////////////////////////////////////////////////

// Linear Scales

/**
 * @return {!d3.LinearScale}
 */
d3.scaleLinear = function() {};

/**
 * Besides numbers, continuous scales also support RGB string ranges.
 * @typedef {function(number): ?}
 */
d3.LinearScale;

/**
 * @private {!d3.LinearScale}
 */
d3.LinearScale_;

/**
 * @param {number} value
 * @return {number}
 */
d3.LinearScale_.invert = function(value) {};

/**
 * @param {!Array<number>=} domain
 */
d3.LinearScale_.domain = function(domain) {};

/**
 * @param {!(Array<number> | Array<string>)=} range
 */
d3.LinearScale_.range = function(range) {};

/**
 * @param {!Array<number>=} range
 */
d3.LinearScale_.rangeRound = function(range) {};

/**
 * @param {boolean=} clamp
 */
d3.LinearScale_.clamp = function(clamp) {};

/**
 * @param {function(?, ?): function(number)=} interpolate
 */
d3.LinearScale_.interpolate = function(interpolate) {};

/**
 * @param {number=} count
 * @return {!Array<number>}
 */
d3.LinearScale_.ticks = function(count) {};

/**
 * @param {number=} count
 * @param {string=} specifier
 * @return {function(number): string}
 */
d3.LinearScale_.tickFormat = function(count, specifier) {};

/**
 * @param {number=} count
 * @return {!d3.LinearScale}
 */
d3.LinearScale_.nice = function(count) {};

/**
 * @return {!d3.LinearScale}
 */
d3.LinearScale_.copy = function() {};

// Power Scales

/**
 * @return {!d3.PowScale}
 */
d3.scalePow = function() {};

/**
 * @typedef {function(number): ?}
 */
d3.PowScale;

/**
 * @private {!d3.PowScale}
 */
d3.PowScale_;

/**
 * @param {number} value
 * @return {number}
 */
d3.PowScale_.invert = function(value) {};

/**
 * @param {number=} exponent
 */
d3.PowScale_.exponent = function(exponent) {};

/**
 * @param {!Array<number>=} domain
 */
d3.PowScale_.domain = function(domain) {};

/**
 * @param {!(Array<number> | Array<string>)=} range
 */
d3.PowScale_.range = function(range) {};

/**
 * @param {!Array<number>=} range
 */
d3.PowScale_.rangeRound = function(range) {};

/**
 * @param {boolean=} clamp
 */
d3.PowScale_.clamp = function(clamp) {};

/**
 * @param {function(?, ?): function(number)=} interpolate
 */
d3.PowScale_.interpolate = function(interpolate) {};

/**
 * @param {number=} count
 * @return {!Array<number>}
 */
d3.PowScale_.ticks = function(count) {};

/**
 * @param {number=} count
 * @param {string=} specifier
 * @return {function(number): string}
 */
d3.PowScale_.tickFormat = function(count, specifier) {};

/**
 * @param {number=} count
 * @return {!d3.PowScale}
 */
d3.PowScale_.nice = function(count) {};

/**
 * @return {!d3.PowScale}
 */
d3.PowScale_.copy = function() {};

/**
 * @return {!d3.PowScale}
 */
d3.scaleSqrt = function() {};

// Log Scales

/**
 * @return {!d3.LogScale}
 */
d3.scaleLog = function() {};

/**
 * @typedef {function(number): ?}
 */
d3.LogScale;

/**
 * @private {!d3.LogScale}
 */
d3.LogScale_;

/**
 * @param {number} value
 * @return {number}
 */
d3.LogScale_.invert = function(value) {};

/**
 * @param {number=} base
 */
d3.LogScale_.base = function(base) {};

/**
 * @param {!Array<number>=} domain
 */
d3.LogScale_.domain = function(domain) {};

/**
 * @param {!(Array<number> | Array<string>)=} range
 */
d3.LogScale_.range = function(range) {};

/**
 * @param {!Array<number>=} range
 */
d3.LogScale_.rangeRound = function(range) {};

/**
 * @param {boolean=} clamp
 */
d3.LogScale_.clamp = function(clamp) {};

/**
 * @param {function(?, ?): function(number)=} interpolate
 */
d3.LogScale_.interpolate = function(interpolate) {};

/**
 * @param {number=} count
 * @return {!Array<number>}
 */
d3.LogScale_.ticks = function(count) {};

/**
 * @param {number=} count
 * @param {string=} specifier
 * @return {function(number): string}
 */
d3.LogScale_.tickFormat = function(count, specifier) {};

/**
 * @param {number=} count
 * @return {!d3.LogScale}
 */
d3.LogScale_.nice = function(count) {};

/**
 * @return {!d3.LogScale}
 */
d3.LogScale_.copy = function() {};

// Identity Scales

/**
 * @return {!d3.IdentityScale}
 */
d3.scaleIdentity = function() {};

/**
 * @typedef {function(number): number}
 */
d3.IdentityScale;

/**
 * @private {!d3.IdentityScale}
 */
d3.IdentityScale_;

/**
 * @param {number} value
 * @return {number}
 */
d3.IdentityScale_.invert = function(value) {};

/**
 * @param {!Array<number>=} domain
 */
d3.IdentityScale_.domain = function(domain) {};

/**
 * @param {!Array<number>=} range
 */
d3.IdentityScale_.range = function(range) {};

/**
 * @param {number=} count
 * @return {!Array<number>}
 */
d3.IdentityScale_.ticks = function(count) {};

/**
 * @param {number=} count
 * @param {string=} specifier
 * @return {function(number): string}
 */
d3.IdentityScale_.tickFormat = function(count, specifier) {};

/**
 * @param {number=} count
 * @return {!d3.IdentityScale}
 */
d3.IdentityScale_.nice = function(count) {};

/**
 * @return {!d3.IdentityScale}
 */
d3.IdentityScale_.copy = function() {};

// Time Scales

/**
 * @return {!d3.TimeScale}
 */
d3.scaleTime = function() {};

/**
 * @typedef {function((number | !Date)): ?}
 */
d3.TimeScale;

/**
 * @private {!d3.TimeScale}
 */
d3.TimeScale_;

/**
 * @param {number} value
 * @return {!Date}
 */
d3.TimeScale_.invert = function(value) {};

/**
 * @param {!Array<!Date>=} domain
 */
d3.TimeScale_.domain = function(domain) {};

/**
 * @param {!(Array<number> | Array<string>)=} range
 */
d3.TimeScale_.range = function(range) {};

/**
 * @param {!Array<number>=} range
 */
d3.TimeScale_.rangeRound = function(range) {};

/**
 * @param {boolean=} clamp
 */
d3.TimeScale_.clamp = function(clamp) {};

/**
 * @param {function(?, ?): function(number)=} interpolate
 */
d3.TimeScale_.interpolate = function(interpolate) {};

/**
 * @param {number | ?d3.Interval=} countOrInterval
 * @return {!Array<!Date>}
 */
d3.TimeScale_.ticks = function(countOrInterval) {};

/**
 * @param {?number | d3.Interval=} countOrInterval
 * @param {string=} specifier
 * @return {function(!Date): string}
 */
d3.TimeScale_.tickFormat = function(countOrInterval, specifier) {};

/**
 * @param {?number | d3.Interval=} countOrInterval
 * @param {number=} step
 */
d3.TimeScale_.nice = function(countOrInterval, step) {};

/**
 * @return {!d3.TimeScale}
 */
d3.TimeScale_.copy = function() {};

/**
 * @return {!d3.TimeScale}
 */
d3.scaleUtc = function() {};

// Sequential Scales

/**
 * @param {function(number): ?} interpolator
 * @return {!d3.SequentialScale}
 */
d3.scaleSequential = function(interpolator) {};

/**
 * @typedef {function(number): ?}
 */
d3.SequentialScale;

/**
 * @private {!d3.SequentialScale}
 */
d3.SequentialScale_;

/**
 * @param {!Array<number>=} domain
 */
d3.SequentialScale_.domain = function(domain) {};

/**
 * @param {boolean=} clamp
 */
d3.SequentialScale_.clamp = function(clamp) {};

/**
 * @param {function(number)=} interpolator
 */
d3.SequentialScale_.interpolator = function(interpolator) {};

/**
 * @return {!d3.SequentialScale}
 */
d3.SequentialScale_.copy = function() {};

// Diverging scales

/**
 * @param {?} interpolator
 * @return {!d3.DivergingScale}
 */
d3.scaleDiverging = function(interpolator) {};

/**
 * @typedef {function(number): ?}
 */
d3.DivergingScale;

/**
 * @private {!d3.DivergingScale}
 */
d3.DivergingScale_;

/**
 * @param {!Array<number>=} domain Exactly 3 values
 * @return {?}
 */
d3.DivergingScale_.domain = function(domain) {};

/**
 * @param {boolean=} clamp
 * @return {?}
 */
d3.DivergingScale_.clamp = function(clamp) {};

/**
 * @param {!Function=} interpolator
 * @return {?}
 */
d3.DivergingScale_.interpolator = function(interpolator) {};

/**
 * @return {!d3.DivergingScale}
 */
d3.DivergingScale_.copy = function() {};

// Quantize Scales

/**
 * @return {!d3.QuantizeScale}
 */
d3.scaleQuantize = function() {};

/**
 * @typedef {function(number): ?}
 */
d3.QuantizeScale;

/**
 * @private {!d3.QuantizeScale}
 */
d3.QuantizeScale_;

/**
 * @param {?} value
 * @return {!Array<number>}
 */
d3.QuantizeScale_.invertExtent = function(value) {};

/**
 * @param {!Array<number>=} domain
 */
d3.QuantizeScale_.domain = function(domain) {};

/**
 * @param {!Array=} range
 */
d3.QuantizeScale_.range = function(range) {};

/**
 * @param {number=} count
 * @return {!Array<number>}
 */
d3.QuantizeScale_.ticks = function(count) {};

/**
 * @param {number=} count
 * @param {string=} specifier
 * @return {function(number): string}
 */
d3.QuantizeScale_.tickFormat = function(count, specifier) {};

/**
 * @param {number=} count
 * @return {!d3.QuantizeScale}
 */
d3.QuantizeScale_.nice = function(count) {};

/**
 * @return {!d3.QuantizeScale}
 */
d3.QuantizeScale_.copy = function() {};

// Quantile Scales

/**
 * @return {!d3.QuantileScale}
 */
d3.scaleQuantile = function() {};

/**
 * @typedef {function(number): ?}
 */
d3.QuantileScale;

/**
 * @private {!d3.QuantileScale}
 */
d3.QuantileScale_;

/**
 * @param {?} value
 * @return {!Array<number>}
 */
d3.QuantileScale_.invertExtent = function(value) {};

/**
 * @param {!Array<number>=} domain
 */
d3.QuantileScale_.domain = function(domain) {};

/**
 * @param {!Array=} range
 */
d3.QuantileScale_.range = function(range) {};

/**
 * @return {!Array<number>}
 */
d3.QuantileScale_.quantiles = function() {};

/**
 * @return {!d3.QuantileScale}
 */
d3.QuantileScale_.copy = function() {};

// Threshold Scales

/**
 * @return {!d3.ThresholdScale}
 */
d3.scaleThreshold = function() {};

/**
 * @typedef {function((number | string)): ?}
 */
d3.ThresholdScale;

/**
 * @private {!d3.ThresholdScale}
 */
d3.ThresholdScale_;

/**
 * @param {?} value
 * @return {!Array}
 */
d3.ThresholdScale_.invertExtent = function(value) {};

/**
 * @param {!Array=} domain
 */
d3.ThresholdScale_.domain = function(domain) {};

/**
 * @param {!Array=} range
 */
d3.ThresholdScale_.range = function(range) {};

/**
 * @return {!d3.ThresholdScale}
 */
d3.ThresholdScale_.copy = function() {};

// Ordinal Scales

/**
 * @param {!Array<?>=} range
 * @return {!d3.OrdinalScale}
 */
d3.scaleOrdinal = function(range) {};

/**
 * @typedef {function((string | number)): ?}
 */
d3.OrdinalScale;

/**
 * @private {!d3.OrdinalScale}
 */
d3.OrdinalScale_;

/**
 * @param {!(Array<string> | Array<number>)=} domain
 */
d3.OrdinalScale_.domain = function(domain) {};

/**
 * @param {!Array=} range
 */
d3.OrdinalScale_.range = function(range) {};

/**
 * @param {?=} value
 */
d3.OrdinalScale_.unknown = function(value) {};

/**
 * @return {!d3.OrdinalScale}
 */
d3.OrdinalScale_.copy = function() {};

/**
 * @type {{name: string}}
 */
d3.scaleImplicit;

// Band Scales

/**
 * @return {!d3.BandScale}
 */
d3.scaleBand = function() {};

/**
 * @typedef {function(string): number}
 */
d3.BandScale;

/**
 * @private {!d3.BandScale}
 */
d3.BandScale_;

/**
 * @param {!(Array<string> | Array<number>)=} domain
 */
d3.BandScale_.domain = function(domain) {};

/**
 * @param {!Array<number>=} range
 */
d3.BandScale_.range = function(range) {};

/**
 * @param {!Array<number>=} range
 */
d3.BandScale_.rangeRound = function(range) {};

/**
 * @param {boolean=} round
 */
d3.BandScale_.round = function(round) {};

/**
 * @param {number=} padding
 */
d3.BandScale_.paddingInner = function(padding) {};

/**
 * @param {number=} padding
 */
d3.BandScale_.paddingOuter = function(padding) {};

/**
 * @param {number=} padding
 */
d3.BandScale_.padding = function(padding) {};

/**
 * @param {number=} align
 */
d3.BandScale_.align = function(align) {};

/**
 * @return {number}
 */
d3.BandScale_.bandwidth = function() {};

/**
 * @return {number}
 */
d3.BandScale_.step = function() {};

/**
 * @return {!d3.BandScale}
 */
d3.BandScale_.copy = function() {};

// Point Scales

/**
 * @return {!d3.PointScale}
 */
d3.scalePoint = function() {};

/**
 * @typedef {function(string): number}
 */
d3.PointScale;

/**
 * @private {!d3.PointScale}
 */
d3.PointScale_;

/**
 * @param {!(Array<string> | Array<number>)=} domain
 */
d3.PointScale_.domain = function(domain) {};

/**
 * @param {!Array<number>=} range
 */
d3.PointScale_.range = function(range) {};

/**
 * @param {!Array<number>=} range
 */
d3.PointScale_.rangeRound = function(range) {};

/**
 * @param {boolean=} round
 */
d3.PointScale_.round = function(round) {};

/**
 * @param {number=} padding
 */
d3.PointScale_.padding = function(padding) {};

/**
 * @param {number=} align
 */
d3.PointScale_.align = function(align) {};

/**
 * @return {number}
 */
d3.PointScale_.bandwidth = function() {};

/**
 * @return {number}
 */
d3.PointScale_.step = function() {};

/**
 * @return {!d3.PointScale}
 */
d3.PointScale_.copy = function() {};

////////////////////////////////////////////////////////////////////////////////
// Sequential, diverging and categorical color scales.
// https://github.com/d3/d3-scale-chromatic
////////////////////////////////////////////////////////////////////////////////

// Categorical

/**
 * @const {!Array<string>}
 */
d3.schemeCategory10;

/**
 * @const {!Array<string>}
 */
d3.schemeCategory20;

/**
 * @const {!Array<string>}
 */
d3.schemeCategory20b;

/**
 * @const {!Array<string>}
 */
d3.schemeCategory20c;

/**
 * @const {!Array<string>}
 */
d3.schemeAccent;

/**
 * @const {!Array<string>}
 */
d3.schemeDark2;

/**
 * @const {!Array<string>}
 */
d3.schemePaired;

/**
 * @const {!Array<string>}
 */
d3.schemePastel1;

/**
 * @const {!Array<string>}
 */
d3.schemePastel2;

/**
 * @const {!Array<string>}
 */
d3.schemeSet1;

/**
 * @const {!Array<string>}
 */
d3.schemeSet2;

/**
 * @const {!Array<string>}
 */
d3.schemeSet3;

// Diverging

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateBrBG = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeBrBG;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePRGn = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePRGn;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePiYG = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePiYG;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePuOr = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePuOr;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateRdBu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeRdBu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateRdGy = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeRdGy;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateRdYlBu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeRdYlBu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateRdYlGn = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeRdYlGn;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateSpectral = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeSpectral;

// Sequential

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateBlues = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeBlues;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateGreens = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeGreens;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateGreys = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeGreys;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateOranges = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeOranges;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePurples = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePurples;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateReds = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeReds;

// Sequential (Multi-Hue)

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateViridis = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateInferno = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateMagma = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePlasma = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateWarm = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateCool = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateCubehelixDefault = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateBuGn = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeBuGn;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateBuPu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeBuPu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateGnBu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeGnBu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateOrRd = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeOrRd;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePuBuGn = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePuBuGn;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePuBu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePuBu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolatePuRd = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemePuRd;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateRdPu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeRdPu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateYlGnBu = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeYlGnBu;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateYlGn = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeYlGn;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateYlOrBr = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeYlOrBr;

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateYlOrRd = function(t) {};

/**
 * @const {!Array<!Array<string>>}
 */
d3.schemeYlOrRd;

// Cyclical

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateRainbow = function(t) {};

/**
 * @param {number} t 0..1
 * @return {string}
 */
d3.interpolateSinebow = function(t) {};
////////////////////////////////////////////////////////////////////////////////
// Selections
// https://github.com/d3/d3-selection
////////////////////////////////////////////////////////////////////////////////

// API Reference

// Selecting Elements

// According to https://github.com/d3/d3-selection/issues/103 d3.selection has
// limited support for non-element item types. Although the following examples
// work
//
//   var s = d3.select(window).on('resize', function() { ... });
//   var s = d3.select('p').append(() => document.createTextNode('foo'));
//
// most method calls such as s.attr('foo') or s.select('p') throw an Error.
//
// The declarations below try to stay on the safe side, therefore they only
// accept elements. To select a text node or a window, cast it first to {?}.

/**
 * @return {!d3.selection}
 * @constructor
 */
d3.selection = function() {};

/**
 * @param {?string | Element | Window} selector
 *     WARNING: d3.select(window) doesn't return a fully functional selection.
 *     Registering event listeners with on() and setting properties with
 *     property() work, but most methods throw an Error or silently fail.
 * @return {!d3.selection}
 */
d3.select = function(selector) {};

/**
 * @param {?string | Array<!Element> | NodeList<!Element>} selector
 * @return {!d3.selection}
 */
d3.selectAll = function(selector) {};

/**
 * @param {?string | function(this:Element): ?Element} selector
 * @return {!d3.selection}
 */
d3.selection.prototype.select = function(selector) {};

/**
 * @param {?string | function(this:Element): !IArrayLike<!Element>} selector
 * @return {!d3.selection}
 */
d3.selection.prototype.selectAll = function(selector) {};

/**
 * @param {string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): boolean} filter
 * @return {!d3.selection}
 */
d3.selection.prototype.filter = function(filter) {};

/**
 * @param {!d3.selection} other
 * @return {!d3.selection}
 */
d3.selection.prototype.merge = function(other) {};

/**
 * @param {string} selector
 * @return {function(this:Element): boolean}
 */
d3.matcher = function(selector) {};

/**
 * @param {string} selector
 * @return {function(this:Element): ?Element}
 */
d3.selector = function(selector) {};

/**
 * @param {string} selector
 * @return {function(this:Element): !IArrayLike<!Element>}
 */
d3.selectorAll = function(selector) {};

/**
 * @param {!(Node | Document | Window)} node
 * @return {!Window}
 */
d3.window = function(node) {};

/**
 * @param {!Element} node
 * @param {string} name
 * @return {string}
 */
d3.style = function(node, name) {};

// Modifying Elements

/**
 * @param {string} name
 * @param {?string | number | boolean | d3.local |
 *     function(this:Element, ?, number, !IArrayLike<!Element>):
 *         ?(string | number | boolean)=} value
 */
d3.selection.prototype.attr = function(name, value) {};

/**
 * @param {string} names Space separated CSS class names.
 * @param {boolean |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): boolean=}
 *     value
 */
d3.selection.prototype.classed = function(names, value) {};

/**
 * @param {string} name
 * @param {?string | number |
 *    function(this:Element, ?, number, !IArrayLike<!Element>):
 *        ?(string | number)=} value
 * @param {?string=} priority
 * @return {?} Style value (1 argument) or this (2+ arguments)
 */
d3.selection.prototype.style = function(name, value, priority) {};

/**
 * @param {string | !d3.local} name
 * @param {* | function(this:Element, ?, number, !IArrayLike<!Element>)=}
 *     value
 */
d3.selection.prototype.property = function(name, value) {};

/**
 * @param {?string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): ?string=}
 *     value
 */
d3.selection.prototype.text = function(value) {};

/**
 * @param {?string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): ?string=}
 *     value
 */
d3.selection.prototype.html = function(value) {};

/**
 * @param {string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): !Element} type
 * @return {!d3.selection}
 */
d3.selection.prototype.append = function(type) {};

/**
 * @param {string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): !Element} type
 * @param {?string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): ?Element=}
 *     before
 * @return {!d3.selection}
 */
d3.selection.prototype.insert = function(type, before) {};

/**
 * @return {!d3.selection}
 */
d3.selection.prototype.remove = function() {};

/**
 * @param {boolean=} deep
 * @return {!d3.selection}
 */
d3.selection.prototype.clone = function(deep) {};

/**
 * @param {function(?, ?): number} compare
 * @return {!d3.selection}
 */
d3.selection.prototype.sort = function(compare) {};

/**
 * @return {!d3.selection}
 */
d3.selection.prototype.order = function() {};

/**
 * @return {!d3.selection}
 */
d3.selection.prototype.raise = function() {};

/**
 * @return {!d3.selection}
 */
d3.selection.prototype.lower = function() {};

/**
 * @param {string} name
 * @return {!d3.selection}
 */
d3.create = function(name) {};

/**
 * @param {string} name
 * @return {function(this:Element): !Element}
 */
d3.creator = function(name) {};

// Joining Data

/**
 * @param {!Array |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): !Array=}
 *     data
 * @param {function(this:Element, ?, number, !IArrayLike)=} key
 */
d3.selection.prototype.data = function(data, key) {};

/**
 * @return {!d3.selection}
 */
d3.selection.prototype.enter = function() {};

/**
 * @return {!d3.selection}
 */
d3.selection.prototype.exit = function() {};

/**
 * @param {* | function(this:Element, ?, number, !IArrayLike<!Element>)=}
 *     value
 * @return {?}
 */
d3.selection.prototype.datum = function(value) {};

// Handling Events

/**
 * @param {string} typenames
 * @param {?function(this:Element, ?, number, !IArrayLike<!Element>)=}
 *     listener
 * @param {boolean=} capture
 * @return {?} d3.selection (2+ arguments), listener function (1 argument) or
 *     undefined (1 argument).
 */
d3.selection.prototype.on = function(typenames, listener, capture) {};

/**
 * @record
 */
d3.EventParameters = function() {};

/**
 * @type {boolean | undefined}
 */
d3.EventParameters.prototype.bubbles;

/**
 * @type {boolean | undefined}
 */
d3.EventParameters.prototype.cancelable;

/**
 * @type {*}
 */
d3.EventParameters.prototype.detail;

/**
 * @param {string} type
 * @param {!d3.EventParameters |
 *     function(this:Element, ?, number, !NodeList<!Element>):
 *         !d3.EventParameters=} parameters
 */
d3.selection.prototype.dispatch = function(type, parameters) {};

/**
 * @type {?Event |
 *     {type: string, target: ?, sourceEvent: ?{type: string, target: ?}}}
 */
d3.event;

/**
 * @param {{type: string, target: ?}} event
 * @param {function(this:T, ...?): R} listener
 * @param {T=} that
 * @param {?Array | Arguments=} args
 * @return {R}
 * @template T, R
 */
d3.customEvent = function(event, listener, that, args) {};

/**
 * @param {!Element} container
 * @param {!Event} event Mouse, touch or gesture event with clientX and clientY
 *     properties.
 * @return {!Array<number>}
 */
d3.clientPoint = function(container, event) {};

/**
 * @param {!Element} container
 * @return {!Array<number>}
 */
d3.mouse = function(container) {};

/**
 * @param {!Element} container
 * @param {!TouchList | string} touchesOrIdentifier
 * @param {string=} identifier
 * @return {?Array<!Array<number>>}
 */
d3.touch = function(container, touchesOrIdentifier, identifier) {};

/**
 * @param {!Element} container
 * @param {!TouchList=} touches
 * @return {!Array<!Array<number>>}
 */
d3.touches = function(container, touches) {};

// Control Flow

/**
 * @param {function(this:Element, ?, number, !IArrayLike<!Element>)} callback
 * @return {!d3.selection}
 */
d3.selection.prototype.each = function(callback) {};

/**
 * @param {!Function} callback
 * @param {...?} var_args
 */
d3.selection.prototype.call = function(callback, var_args) {};

/**
 * @return {boolean}
 */
d3.selection.prototype.empty = function() {};

/**
 * @return {!Array<!Element>}
 */
d3.selection.prototype.nodes = function() {};

/**
 * @return {?Element}
 */
d3.selection.prototype.node = function() {};

/**
 * @return {number}
 */
d3.selection.prototype.size = function() {};

// Local Variables

/**
 * @return {!d3.local}
 * @constructor
 * @template T
 */
d3.local = function() {};

/**
 * @param {!Element} node
 * @param {T} value
 * @return {!Element}
 */
d3.local.prototype.set = function(node, value) {};

/**
 * @param {!Element} node
 * @return {T | undefined} value
 */
d3.local.prototype.get = function(node) {};

/**
 * @param {!Element} node
 * @return {boolean}
 */
d3.local.prototype.remove = function(node) {};

/**
 * @override
 * @return {string}
 */
d3.local.prototype.toString = function() {};

// Namespaces

/**
 * @param {string} name
 * @return {{local: string, space: string} | string}
 */
d3.namespace = function(name) {};

/**
 * @type {{
 *   svg: string,
 *   xhtml: string,
 *   xlink: string,
 *   xml: string,
 *   xmlns: string
 * }}
 */
d3.namespaces;

////////////////////////////////////////////////////////////////////////////////
// Shapes
// https://github.com/d3/d3-shape
////////////////////////////////////////////////////////////////////////////////

// API Reference

// Arcs

/**
 * @return {!d3.Arc}
 */
d3.arc = function() {};

/**
 * @typedef {function(...?)}
 */
d3.Arc;

/**
 * @private {!d3.Arc}
 */
d3.Arc_;

/**
 * @param {...?} var_args
 * @return {!Array<number>}
 */
d3.Arc_.centroid = function(var_args) {};

/**
 * @param {number | function(...?): number=} radius
 */
d3.Arc_.innerRadius = function(radius) {};

/**
 * @param {number | function(...?): number=} radius
 */
d3.Arc_.outerRadius = function(radius) {};

/**
 * @param {number | function(...?): number=} radius
 */
d3.Arc_.cornerRadius = function(radius) {};

/**
 * @param {number | function(...?): number=} angle
 */
d3.Arc_.startAngle = function(angle) {};

/**
 * @param {number | function(...?): number=} angle
 */
d3.Arc_.endAngle = function(angle) {};

/**
 * @param {number | function(...?): number=} angle
 */
d3.Arc_.padAngle = function(angle) {};

/**
 * @param {number | function(...?): number=} radius
 */
d3.Arc_.padRadius = function(radius) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.Arc_.context = function(context) {};

// Pies

/**
 * @return {!d3.Pie}
 */
d3.pie = function() {};

/**
 * @typedef {function(!Array, ...?): !Array<{
 *   data: ?,
 *   value: number,
 *   index: number,
 *   startAngle: number,
 *   endAngle: number,
 *   padAngle: number
 * }>}
 */
d3.Pie;

/**
 * @private {!d3.Pie}
 */
d3.Pie_;

/**
 * @param {number | function(T, number, !Array<T>): number=} value
 * @template T
 */
d3.Pie_.value = function(value) {};

/**
 * @param {?function(?, ?): number=} compare
 */
d3.Pie_.sort = function(compare) {};

/**
 * @param {?function(?, ?): number=} compare
 */
d3.Pie_.sortValues = function(compare) {};

/**
 * @param {number | function(!Array, ...?): number=} angle
 */
d3.Pie_.startAngle = function(angle) {};

/**
 * @param {number | function(!Array, ...?): number=} angle
 */
d3.Pie_.endAngle = function(angle) {};

/**
 * @param {number | function(!Array, ...?): number=} angle
 */
d3.Pie_.padAngle = function(angle) {};

// Lines

/**
 * @return {!d3.Line}
 */
d3.line = function() {};

/**
 * @typedef {function(!Array)}
 */
d3.Line;

/**
 * @private {!d3.Line}
 */
d3.Line_;

/**
 * @param {number | function(T, number, !Array<T>): number=} x
 * @template T
 */
d3.Line_.x = function(x) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} y
 * @template T
 */
d3.Line_.y = function(y) {};

/**
 * @param {boolean | function(T, number, !Array<T>): boolean=} defined
 * @template T
 */
d3.Line_.defined = function(defined) {};

/**
 * @param {function(!CanvasPathMethods): !d3.Curve=} curve
 */
d3.Line_.curve = function(curve) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.Line_.context = function(context) {};

/**
 * @return {!d3.RadialLine}
 * @deprecated Use d3.lineRadial
 */
d3.radialLine = function() {};

/**
 * @return {!d3.RadialLine}
 */
d3.lineRadial = function() {};

/**
 * @typedef {function(!Array)}
 */
d3.RadialLine;

/**
 * @private {!d3.RadialLine}
 */
d3.RadialLine_;

/**
 * @param {number | function(T, number, !Array<T>): number=} angle
 * @template T
 */
d3.RadialLine_.angle = function(angle) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} radius
 * @template T
 */
d3.RadialLine_.radius = function(radius) {};

/**
 * @param {boolean | function(T, number, !Array<T>): boolean=} defined
 * @template T
 */
d3.RadialLine_.defined = function(defined) {};

/**
 * @param {function(!CanvasPathMethods): !d3.Curve=} curve
 */
d3.RadialLine_.curve = function(curve) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.RadialLine_.context = function(context) {};

// Areas

/**
 * @return {!d3.Area}
 */
d3.area = function() {};

/**
 * @typedef {function(!Array)}
 */
d3.Area;

/**
 * @private {!d3.Area}
 */
d3.Area_;

/**
 * @param {number | function(T, number, !Array<T>): number=} x
 * @template T
 */
d3.Area_.x = function(x) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} x
 * @template T
 */
d3.Area_.x0 = function(x) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} x
 * @template T
 */
d3.Area_.x1 = function(x) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} y
 * @template T
 */
d3.Area_.y = function(y) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} y
 * @template T
 */
d3.Area_.y0 = function(y) {};

/**
 * @param {number | function(T, number, !Array<T>): number=} y
 * @template T
 */
d3.Area_.y1 = function(y) {};

/**
 * @param {boolean | function(T, number, !Array<T>): boolean=} defined
 * @template T
 */
d3.Area_.defined = function(defined) {};

/**
 * @param {function(!CanvasPathMethods): !d3.Curve2d=} curve
 */
d3.Area_.curve = function(curve) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.Area_.context = function(context) {};

/**
 * @return {!d3.Line}
 */
d3.Area_.lineX0 = function() {};

/**
 * @return {!d3.Line}
 */
d3.Area_.lineY0 = function() {};

/**
 * @return {!d3.Line}
 */
d3.Area_.lineX1 = function() {};

/**
 * @return {!d3.Line}
 */
d3.Area_.lineY1 = function() {};

/**
 * @return {!d3.RadialArea}
 */
d3.areaRadial = function() {};

/**
 * @return {!d3.RadialArea}
 * @deprecated Use d3.areaRadial
 */
d3.radialArea = function() {};

/**
 * @typedef {function(!Array)}
 */
d3.RadialArea;

/**
 * @private {!d3.RadialArea}
 */
d3.RadialArea_;

/**
 * @param {number | function(!Array, ...?): number=} angle
 */
d3.RadialArea_.angle = function(angle) {};

/**
 * @param {number | function(!Array, ...?): number=} angle
 */
d3.RadialArea_.startAngle = function(angle) {};

/**
 * @param {number | function(!Array, ...?): number=} angle
 */
d3.RadialArea_.endAngle = function(angle) {};

/**
 * @param {number | function(!Array, ...?): number=} radius
 */
d3.RadialArea_.radius = function(radius) {};

/**
 * @param {number | function(!Array, ...?): number=} radius
 */
d3.RadialArea_.innerRadius = function(radius) {};

/**
 * @param {number | function(!Array, ...?): number=} radius
 */
d3.RadialArea_.outerRadius = function(radius) {};

/**
 * @param {boolean | function(T, number, !Array<T>): boolean=} defined
 * @template T
 */
d3.RadialArea_.defined = function(defined) {};

/**
 * @param {function(!CanvasPathMethods): !d3.Curve2d=} curve
 */
d3.RadialArea_.curve = function(curve) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.RadialArea_.context = function(context) {};

/**
 * @return {!d3.RadialLine}
 */
d3.RadialArea_.lineStartAngle = function() {};

/**
 * @return {!d3.RadialLine}
 */
d3.RadialArea_.lineInnerRadius = function() {};

/**
 * @return {!d3.RadialLine}
 */
d3.RadialArea_.lineEndAngle = function() {};

/**
 * @return {!d3.RadialLine}
 */
d3.RadialArea_.lineOuterRadius = function() {};

// Curves

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveBasis = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveBasisClosed = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveBasisOpen = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve}
 */
d3.curveBundle = function(context) {};

/**
 * @param {number} beta
 * @return {function(!CanvasPathMethods): !d3.Curve}
 */
d3.curveBundle.beta = function(beta) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveCardinal = function(context) {};

/**
 * @param {number} tension
 * @return {function(!CanvasPathMethods): !d3.Curve2d}
 */
d3.curveCardinal.tension = function(tension) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveCardinalClosed = function(context) {};

/**
 * @param {number} tension
 * @return {function(!Object): !d3.Curve2d}
 */
d3.curveCardinalClosed.tension = function(tension) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveCardinalOpen = function(context) {};

/**
 * @param {number} tension
 * @return {function(!CanvasPathMethods): !d3.Curve2d}
 */
d3.curveCardinalOpen.tension = function(tension) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveCatmullRom = function(context) {};

/**
 * @param {number} alpha
 * @return {function(!CanvasPathMethods): !d3.Curve2d}
 */
d3.curveCatmullRom.alpha = function(alpha) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveCatmullRomClosed = function(context) {};

/**
 * @param {number} alpha
 * @return {function(!Object): !d3.Curve2d}
 */
d3.curveCatmullRomClosed.alpha = function(alpha) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveCatmullRomOpen = function(context) {};

/**
 * @param {number} alpha
 * @return {function(!CanvasPathMethods): !d3.Curve2d}
 */
d3.curveCatmullRomOpen.alpha = function(alpha) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveLinear = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveLinearClosed = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveMonotoneX = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveMonotoneY = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveNatural = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveStep = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveStepAfter = function(context) {};

/**
 * @param {!CanvasPathMethods} context
 * @return {!d3.Curve2d}
 */
d3.curveStepBefore = function(context) {};

// Custom Curves

/**
 * @interface
 */
d3.Curve = function() {};

/**
 * @return {void}
 */
d3.Curve.prototype.lineStart = function() {};

/**
 * @return {void}
 */
d3.Curve.prototype.lineEnd = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @return {void}
 */
d3.Curve.prototype.point = function(x, y) {};

/**
 * @interface
 * @extends {d3.Curve}
 */
d3.Curve2d = function() {};

/**
 * @return {void}
 */
d3.Curve2d.prototype.areaStart = function() {};

/**
 * @return {void}
 */
d3.Curve2d.prototype.areaEnd = function() {};

// Links

/**
 * @return {!d3.LinkShape}
 */
d3.linkHorizontal = function() {};

/**
 * @return {!d3.LinkShape}
 */
d3.linkVertical = function() {};

/**
 * @typedef {function(...?)}
 */
d3.LinkShape;

/**
 * @private {!d3.LinkShape}
 */
d3.LinkShape_;

/**
 * @param {!Function=} source
 * @return {!Function} Source accessor (0 arguments) or this (1 argument).
 */
d3.LinkShape_.source = function(source) {};

/**
 * @param {!Function=} target
 * @return {!Function} Target accessor (0 arguments) or this (1 argument).
 */
d3.LinkShape_.target = function(target) {};

/**
 * @param {!Function=} x
 * @return {!Function} x-accessor (0 arguments) or this (1 argument).
 */
d3.LinkShape_.x = function(x) {};

/**
 * @param {!Function=} y
 * @return {!Function} y-accessor (0 arguments) or this (1 argument).
 */
d3.LinkShape_.y = function(y) {};

/**
 * @param {?CanvasPathMethods=} context
 * @return {?} Context (0 arguments) or this (1 argument).
 */
d3.LinkShape_.context = function(context) {};

/**
 * @return {!d3.RadialLink}
 */
d3.linkRadial = function() {};

/**
 * @typedef {function(...?)}
 */
d3.RadialLink;

/**
 * @private {!d3.RadialLink}
 */
d3.RadialLink_;

/**
 * @param {!Function=} source
 * @return {!Function} Source accessor (0 arguments) or this (1 argument).
 */
d3.RadialLink_.source = function(source) {};

/**
 * @param {!Function=} target
 * @return {!Function} Target accessor (0 arguments) or this (1 argument).
 */
d3.RadialLink_.target = function(target) {};

/**
 * @param {!Function=} angle
 * @return {!Function} Angle accessor (0 arguments) or this (1 argument).
 */
d3.RadialLink_.angle = function(angle) {};

/**
 * @param {!Function=} radius
 * @return {!Function} Radius accessor (0 arguments) or this (1 argument).
 */
d3.RadialLink_.radius = function(radius) {};

/**
 * @param {?CanvasPathMethods=} context
 * @return {?} Context (0 arguments) or this (1 argument).
 */
d3.RadialLink_.context = function(context) {};

// Symbols

/**
 * @return {!d3.Symbol}
 */
d3.symbol = function() {};

/**
 * @typedef {function(...?)}
 */
d3.Symbol;

/**
 * @private {!d3.Symbol}
 */
d3.Symbol_;

/**
 * @param {!d3.SymbolType | function(...?): !d3.SymbolType=} type
 */
d3.Symbol_.type = function(type) {};

/**
 * @param {number | function(...?): number=} size
 */
d3.Symbol_.size = function(size) {};

/**
 * @param {?CanvasPathMethods=} context
 */
d3.Symbol_.context = function(context) {};

/**
 * @type {!Array<!d3.SymbolType>}
 */
d3.symbols;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolCircle;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolCross;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolDiamond;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolSquare;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolStar;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolTriangle;

/**
 * @type {!d3.SymbolType}
 */
d3.symbolWye;

/**
 * @param {number} angle
 * @param {number} radius
 * @return {!Array<number>}
 */
d3.pointRadial = function(angle, radius) {};

// Custom Symbol Types

/**
 * @interface
 */
d3.SymbolType = function() {};

/**
 * @param {!CanvasPathMethods} context
 * @param {number} size
 */
d3.SymbolType.prototype.draw = function(context, size) {};

// Stacks

/**
 * @constructor
 * @extends {Array<number>}
 */
d3.SeriesPoint = function() {};

/**
 * @type {number}
 */
d3.SeriesPoint.prototype.index;

/**
 * @type {?}
 */
d3.SeriesPoint.prototype.data;

/**
 * @constructor
 * @extends {Array<!d3.SeriesPoint>}
 */
d3.Series = function() {};

/**
 * @type {?}
 */
d3.Series.prototype.key;

/**
 * @return {!d3.Stack}
 */
d3.stack = function() {};

/**
 * @typedef {function(!Array, ...?): !Array<!d3.Series>}
 */
d3.Stack;

/**
 * @private {!d3.Stack}
 */
d3.Stack_;

/**
 * @param {!Array | function(!Array, ...?): !Array=} keys
 */
d3.Stack_.keys = function(keys) {};

/**
 * @param {number | function(?, ?, number, !Array): number=} value
 */
d3.Stack_.value = function(value) {};

/**
 * @param {?Array<number> | function(!d3.Series): !Array<number>=} order
 */
d3.Stack_.order = function(order) {};

/**
 * @param {?function(!d3.Series, !Array<number>): void=} offset
 */
d3.Stack_.offset = function(offset) {};

// Stack Orders

/**
 * @param {!d3.Series} series
 * @return {!Array<number>}
 */
d3.stackOrderAscending = function(series) {};

/**
 * @param {!d3.Series} series
 * @return {!Array<number>}
 */
d3.stackOrderDescending = function(series) {};

/**
 * @param {!d3.Series} series
 * @return {!Array<number>}
 */
d3.stackOrderInsideOut = function(series) {};

/**
 * @param {!d3.Series} series
 * @return {!Array<number>}
 */
d3.stackOrderNone = function(series) {};

/**
 * @param {!d3.Series} series
 * @return {!Array<number>}
 */
d3.stackOrderReverse = function(series) {};

// Stack Offsets

/**
 * @param {!d3.Series} series
 * @param {!Array<number>} order
 * @return {void}
 */
d3.stackOffsetExpand = function(series, order) {};

/**
 * @param {!d3.Series} series
 * @param {!Array<number>} order
 * @return {void}
 */
d3.stackOffsetDiverging = function(series, order) {};

/**
 * @param {!d3.Series} series
 * @param {!Array<number>} order
 * @return {void}
 */
d3.stackOffsetNone = function(series, order) {};

/**
 * @param {!d3.Series} series
 * @param {!Array<number>} order
 * @return {void}
 */
d3.stackOffsetSilhouette = function(series, order) {};

/**
 * @param {!d3.Series} series
 * @param {!Array<number>} order
 * @return {void}
 */
d3.stackOffsetWiggle = function(series, order) {};

////////////////////////////////////////////////////////////////////////////////
// Time Formats
// https://github.com/d3/d3-time-format
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @param {string} specifier
 * @return {function((!Date | number)): string}
 */
d3.timeFormat = function(specifier) {};

/**
 * @param {string} specifier
 * @return {function(string): ?Date}
 */
d3.timeParse = function(specifier) {};

/**
 * @param {string} specifier
 * @return {function((!Date | number)): string}
 */
d3.utcFormat = function(specifier) {};

/**
 * @param {string} specifier
 * @return {function(string): ?Date}
 */
d3.utcParse = function(specifier) {};

/**
 * @type {function((!Date | number)): string}
 */
d3.isoFormat;

/**
 * @type {function(string): ?Date}
 */
d3.isoParse;

// Locales

/**
 * @record
 */
d3.TimeLocaleDefinition = function() {};

/**
 * @type {string}
 */
d3.TimeLocaleDefinition.prototype.dateTime;

/**
 * @type {string}
 */
d3.TimeLocaleDefinition.prototype.date;

/**
 * @type {string}
 */
d3.TimeLocaleDefinition.prototype.time;

/**
 * @type {!Array<string>}
 */
d3.TimeLocaleDefinition.prototype.periods;

/**
 * @type {!Array<string>}
 */
d3.TimeLocaleDefinition.prototype.days;

/**
 * @type {!Array<string>}
 */
d3.TimeLocaleDefinition.prototype.shortDays;

/**
 * @type {!Array<string>}
 */
d3.TimeLocaleDefinition.prototype.months;

/**
 * @type {!Array<string>}
 */
d3.TimeLocaleDefinition.prototype.shortMonths;

/**
 * @interface
 */
d3.TimeLocale = function() {};

/**
 * @param {string} specifier
 * @return {function((!Date | number)): string}
 */
d3.TimeLocale.prototype.format = function(specifier) {};

/**
 * @param {string} specifier
 * @return {function(string): ?Date}
 */
d3.TimeLocale.prototype.parse = function(specifier) {};

/**
 * @param {string} specifier
 * @return {function((!Date | number)): string}
 */
d3.TimeLocale.prototype.utcFormat = function(specifier) {};

/**
 * @param {string} specifier
 * @return {function(string): ?Date}
 */
d3.TimeLocale.prototype.utcParse = function(specifier) {};

/**
 * @param {!d3.TimeLocaleDefinition} definition
 * @return {!d3.TimeLocale}
 */
d3.timeFormatLocale = function(definition) {};

/**
 * @param {!d3.TimeLocaleDefinition} definition
 * @return {!d3.TimeLocale}
 */
d3.timeFormatDefaultLocale = function(definition) {};

////////////////////////////////////////////////////////////////////////////////
// Time Intervals
// https://github.com/d3/d3-time
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @typedef {function((!Date | number)): !Date}
 */
d3.Interval;

/**
 * @private {!d3.Interval}
 */
d3.Interval_;

/**
 * @param {!Date | number} date
 * @return {!Date}
 */
d3.Interval_.floor = function(date) {};

/**
 * @param {!Date | number} date
 * @return {!Date}
 */
d3.Interval_.round = function(date) {};

/**
 * @param {!Date | number} date
 * @return {!Date}
 */
d3.Interval_.ceil = function(date) {};

/**
 * @param {!Date | number} date
 * @param {number} step
 * @return {!Date}
 */
d3.Interval_.offset = function(date, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.Interval_.range = function(start, stop, step) {};

/**
 * @param {function(!Date): boolean} test
 * @return {!d3.Interval}
 */
d3.Interval_.filter = function(test) {};

/**
 * @param {number} step
 * @return {!d3.Interval}
 */
d3.Interval_.every = function(step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} end
 * @return {number}
 */
d3.Interval_.count = function(start, end) {};

/**
 * @param {function(!Date): void} floor
 * @param {function(!Date, number): void} offset
 * @param {function(!Date, !Date): number=} count
 * @param {function(!Date): number=} field
 * @return {!d3.Interval}
 */
d3.timeInterval = function(floor, offset, count, field) {};

// Intervals

/**
 * @type {!d3.Interval}
 */
d3.timeMillisecond;

/**
 * @type {!d3.Interval}
 */
d3.timeSecond;

/**
 * @type {!d3.Interval}
 */
d3.timeMinute;

/**
 * @type {!d3.Interval}
 */
d3.timeHour;

/**
 * @type {!d3.Interval}
 */
d3.timeDay;

/**
 * @type {!d3.Interval}
 */
d3.timeWeek;

/**
 * @type {!d3.Interval}
 */
d3.timeSunday;

/**
 * @type {!d3.Interval}
 */
d3.timeMonday;

/**
 * @type {!d3.Interval}
 */
d3.timeTuesday;

/**
 * @type {!d3.Interval}
 */
d3.timeWednesday;

/**
 * @type {!d3.Interval}
 */
d3.timeThursday;

/**
 * @type {!d3.Interval}
 */
d3.timeFriday;

/**
 * @type {!d3.Interval}
 */
d3.timeSaturday;

/**
 * @type {!d3.Interval}
 */
d3.timeMonth;

/**
 * @type {!d3.Interval}
 */
d3.timeYear;

/**
 * @type {!d3.Interval}
 */
d3.utcMillisecond;

/**
 * @type {!d3.Interval}
 */
d3.utcSecond;

/**
 * @type {!d3.Interval}
 */
d3.utcMinute;

/**
 * @type {!d3.Interval}
 */
d3.utcHour;

/**
 * @type {!d3.Interval}
 */
d3.utcDay;

/**
 * @type {!d3.Interval}
 */
d3.utcWeek;

/**
 * @type {!d3.Interval}
 */
d3.utcSunday;

/**
 * @type {!d3.Interval}
 */
d3.utcMonday;

/**
 * @type {!d3.Interval}
 */
d3.utcTuesday;

/**
 * @type {!d3.Interval}
 */
d3.utcWednesday;

/**
 * @type {!d3.Interval}
 */
d3.utcThursday;

/**
 * @type {!d3.Interval}
 */
d3.utcFriday;

/**
 * @type {!d3.Interval}
 */
d3.utcSaturday;

/**
 * @type {!d3.Interval}
 */
d3.utcMonth;

/**
 * @type {!d3.Interval}
 */
d3.utcYear;

// Ranges

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeMilliseconds = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeSeconds = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeMinutes = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeHours = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeDays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeWeeks = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeSundays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeMondays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeTuesdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeWednesdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeThursdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeFridays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeSaturdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeMonths = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.timeYears = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcMilliseconds = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcSeconds = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcMinutes = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcHours = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcDays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcWeeks = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcSundays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcMondays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcTuesdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcWednesdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcThursdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcFridays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcSaturdays = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcMonths = function(start, stop, step) {};

/**
 * @param {!Date | number} start
 * @param {!Date | number} stop
 * @param {number=} step
 * @return {!Array<!Date>}
 */
d3.utcYears = function(start, stop, step) {};

////////////////////////////////////////////////////////////////////////////////
// Timers
// https://github.com/d3/d3-timer
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @return {number}
 */
d3.now = function() {};

/**
 * @param {function(number): void} callback
 * @param {number=} delay
 * @param {number=} time
 * @return {!d3.timer}
 * @constructor
 */
d3.timer = function(callback, delay, time) {};

/**
 * @param {function(number): void} callback
 * @param {number=} delay
 * @param {number=} time
 * @return {void}
 */
d3.timer.prototype.restart = function(callback, delay, time) {};

/**
 * @return {void}
 */
d3.timer.prototype.stop = function() {};

/**
 * @return {void}
 */
d3.timerFlush = function() {};

/**
 * @param {function(number): void} callback
 * @param {number=} delay
 * @param {number=} time
 * @return {!d3.timer}
 */
d3.timeout = function(callback, delay, time) {};

/**
 * @param {function(number): void} callback
 * @param {number=} delay
 * @param {number=} time
 * @return {!d3.timer}
 */
d3.interval = function(callback, delay, time) {};

////////////////////////////////////////////////////////////////////////////////
// Transitions
// https://github.com/d3/d3-transition
////////////////////////////////////////////////////////////////////////////////

// Selecting Elements

/**
 * @param {?string | d3.transition=} nameOrTransition
 * @return {!d3.transition}
 */
d3.selection.prototype.transition = function(nameOrTransition) {};

/**
 * @param {?string=} name
 * @return {!d3.selection}
 */
d3.selection.prototype.interrupt = function(name) {};

/**
 * @param {!Element} node
 * @param {?string=} name
 */
d3.interrupt = function(node, name) {};

/**
 * @param {?string | d3.transition=} nameOrTransition
 * @return {!d3.transition}
 * @constructor
 */
d3.transition = function(nameOrTransition) {};

/**
 * @param {?string} selector
 * @return {!d3.transition}
 */
d3.transition.prototype.select = function(selector) {};

/**
 * @param {?string} selector
 * @return {!d3.transition}
 */
d3.transition.prototype.selectAll = function(selector) {};

/**
 * @param {string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): boolean} filter
 * @return {!d3.transition}
 */
d3.transition.prototype.filter = function(filter) {};

/**
 * @param {!d3.transition} other
 * @return {!d3.transition}
 */
d3.transition.prototype.merge = function(other) {};

/**
 * @return {!d3.transition}
 */
d3.transition.prototype.transition = function() {};

/**
 * @return {!d3.selection}
 */
d3.transition.prototype.selection = function() {};

/**
 * @param {!Element} node
 * @param {?string=} name
 * @return {!d3.transition}
 */
d3.active = function(node, name) {};

// Modifying Elements

/**
 * @param {string} name
 * @param {?string | number |
 *     function(this:Element, ?, number, !IArrayLike<!Element>):
 *         ?(string | number)} value
 * @return {!d3.transition}
 */
d3.transition.prototype.attr = function(name, value) {};

/**
 * @param {string} name
 * @param {?function(this:Element, ?, number, !IArrayLike<!Element>):
 *     function(number): (string | number)=} value
 */
d3.transition.prototype.attrTween = function(name, value) {};

/**
 * @param {string} name
 * @param {?string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): ?string} value
 * @param {?string=} priority
 * @return {!d3.transition}
 */
d3.transition.prototype.style = function(name, value, priority) {};

/**
 * @param {string} name
 * @param {?function(this:Element, ?, number, !IArrayLike<!Element>):
 *     function(number): string=} value
 * @param {?string=} priority
 */
d3.transition.prototype.styleTween = function(name, value, priority) {};

/**
 * @param {?string |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): ?string} value
 * @return {!d3.transition}
 */
d3.transition.prototype.text = function(value) {};

/**
 * @return {!d3.transition}
 */
d3.transition.prototype.remove = function() {};

/**
 * @param {string} name
 * @param {?function(this:Element, ?, number, !IArrayLike<!Element>):
 *     function(number)=} value
 */
d3.transition.prototype.tween = function(name, value) {};

// Timing

/**
 * @param {number |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): number=}
 *     value
 */
d3.transition.prototype.delay = function(value) {};

/**
 * @param {number |
 *     function(this:Element, ?, number, !IArrayLike<!Element>): number=}
 *     value
 */
d3.transition.prototype.duration = function(value) {};

/**
 * @param {function(number): number=} value
 */
d3.transition.prototype.ease = function(value) {};

// Control Flow

/**
 * @param {string} typenames
 * @param {?function(this:Element, ?, number, !IArrayLike<!Element>)=}
 *     listener
 * @return {!d3.selection}
 */
d3.transition.prototype.on = function(typenames, listener) {};

/**
 * @param {function(this:Element, ?, number, !IArrayLike<!Element>)} callback
 * @return {!d3.transition}
 */
d3.transition.prototype.each = function(callback) {};

/**
 * @param {!Function} callback
 * @param {...?} var_args
 */
d3.transition.prototype.call = function(callback, var_args) {};

/**
 * @return {boolean}
 */
d3.transition.prototype.empty = function() {};

/**
 * @return {!Array<!Element>}
 */
d3.transition.prototype.nodes = function() {};

/**
 * @return {?Element}
 */
d3.transition.prototype.node = function() {};

/**
 * @return {number}
 */
d3.transition.prototype.size = function() {};

////////////////////////////////////////////////////////////////////////////////
// Voronoi Diagrams
// https://github.com/d3/d3-voronoi
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @return {!d3.Voronoi}
 */
d3.voronoi = function() {};

/**
 * @typedef {function(!Array): !d3.VoronoiDiagram}
 */
d3.Voronoi;

/**
 * @private {!d3.Voronoi}
 */
d3.Voronoi_;

/**
 * @param {function(?): number=} x
 */
d3.Voronoi_.x = function(x) {};

/**
 * @param {function(?): number=} y
 */
d3.Voronoi_.y = function(y) {};

/**
 * @param {!Array<!Array<number>>=} extent
 */
d3.Voronoi_.extent = function(extent) {};

/**
 * @param {!Array<number>=} size
 */
d3.Voronoi_.size = function(size) {};

/**
 * @param {!Array<T>} data
 * @return {!Array<!d3.VoronoiPolygon<T>>}
 * @template T
 */
d3.Voronoi_.polygons = function(data) {};

/**
 * @param {!Array<T>} data
 * @return {!Array<!Array<T>>}
 * @template T
 */
d3.Voronoi_.triangles = function(data) {};

/**
 * @param {!Array<T>} data
 * @return {!Array<!d3.VoronoiLink<T>>}
 * @template T
 */
d3.Voronoi_.links = function(data) {};

// Voronoi Diagrams

/**
 * @interface
 */
d3.VoronoiDiagram = function() {};

/**
 * @type {!Array<!d3.VoronoiEdge>}
 */
d3.VoronoiDiagram.prototype.edges;

/**
 * @type {!Array<!d3.VoronoiCell | undefined>}
 */
d3.VoronoiDiagram.prototype.cells;

/**
 * @return {!Array<!d3.VoronoiPolygon | undefined>}
 */
d3.VoronoiDiagram.prototype.polygons = function() {};

/**
 * @return {!Array<!Array>}
 */
d3.VoronoiDiagram.prototype.triangles = function() {};

/**
 * @return {!Array<!Array<!d3.VoronoiLink<?>>>}
 */
d3.VoronoiDiagram.prototype.links = function() {};

/**
 * @param {number} x
 * @param {number} y
 * @param {number=} radius
 * @return {?d3.VoronoiSite}
 */
d3.VoronoiDiagram.prototype.find = function(x, y, radius) {};

/**
 * @record
 */
d3.VoronoiCell = function() {};

/**
 * @type {!d3.VoronoiSite}
 */
d3.VoronoiCell.prototype.site;

/**
 * @type {!Array<number>}
 */
d3.VoronoiCell.prototype.halfedges;

/**
 * @record
 * @extends {IArrayLike<number>}
 */
d3.VoronoiSite = function() {};

/**
 * @type {number}
 */
d3.VoronoiSite.prototype.index;

/**
 * @type {?}
 */
d3.VoronoiSite.prototype.data;

/**
 * @record
 * @extends {IArrayLike<?Array<number>>}
 */
d3.VoronoiEdge = function() {};

/**
 * @type {!d3.VoronoiSite}
 */
d3.VoronoiEdge.prototype.left;

/**
 * @type {?d3.VoronoiSite}
 */
d3.VoronoiEdge.prototype.right;

/**
 * @record
 * @extends {IArrayLike<?Array<number>>}
 * @template T
 */
d3.VoronoiPolygon = function() {};

/**
 * @type {T}
 */
d3.VoronoiPolygon.prototype.data;

/**
 * @record
 * @template T
 */
d3.VoronoiLink = function() {};

/**
 * @type {T}
 */
d3.VoronoiLink.prototype.source;

/**
 * @type {T}
 */
d3.VoronoiLink.prototype.target;

////////////////////////////////////////////////////////////////////////////////
// Zooming
// https://github.com/d3/d3-zoom
////////////////////////////////////////////////////////////////////////////////

// API Reference

/**
 * @return {!d3.Zoom}
 */
d3.zoom = function() {};

/**
 * @typedef {function(!d3.selection): void}
 */
d3.Zoom;

/**
 * @private {!d3.Zoom}
 */
d3.Zoom_;

/**
 * @param {!d3.selection | !d3.transition} selection
 * @param {!d3.zoomTransform |
 *     function(this:Element, T, number, !Array<T>): !d3.zoomTransform}
 *     transform
 * @return {void}
 * @template T
 */
d3.Zoom_.transform = function(selection, transform) {};

/**
 * @param {!d3.selection | !d3.transition} selection
 * @param {number | function(this:Element, T, number, !Array<T>): number} x
 * @param {number | function(this:Element, T, number, !Array<T>): number} y
 * @return {void}
 * @template T
 */
d3.Zoom_.translateBy = function(selection, x, y) {};

/**
 * @param {!d3.selection | !d3.transition} selection
 * @param {number | function(this:Element, T, number, !Array<T>): number} x
 * @param {number | function(this:Element, T, number, !Array<T>): number} y
 * @return {void}
 * @template T
 */
d3.Zoom_.translateTo = function(selection, x, y) {};

/**
 * @param {!d3.selection | !d3.transition} selection
 * @param {number | function(this:Element, T, number, !Array<T>): number} k
 * @return {void}
 * @template T
 */
d3.Zoom_.scaleBy = function(selection, k) {};

/**
 * @param {!d3.selection | !d3.transition} selection
 * @param {number | function(this:Element, T, number, !Array<T>): number} k
 * @return {void}
 * @template T
 */
d3.Zoom_.scaleTo = function(selection, k) {};

/**
 * @param {function(!d3.zoomTransform,
 *                  !Array<!Array<number>>,
 *                  !Array<!Array<number>>): !d3.zoomTransform=} constrain
 * @return {!Function}
 */
d3.Zoom_.constrain = function(constrain) {};

/**
 * @param {function(this:Element, T, number, !Array<T>): boolean=} filter
 * @template T
 */
d3.Zoom_.filter = function(filter) {};

/**
 * @param {function(this:Element): boolean=} touchable
 * @return {!Function}
 */
d3.Zoom_.touchable = function(touchable) {};

/**
 * @param {function(): number=} delta
 * @return {!Function}
 */
d3.Zoom_.wheelDelta = function(delta) {};

/**
 * @param {!Array<!Array<number>> |
 *     function(this:Element, T, number, !Array<T>): !Array<!Array<number>>=}
 *     extent
 * @template T
 */
d3.Zoom_.extent = function(extent) {};

/**
 * @param {!Array<number>=} extent
 */
d3.Zoom_.scaleExtent = function(extent) {};

/**
 * @param {!Array<!Array<number>>=} extent
 */
d3.Zoom_.translateExtent = function(extent) {};

/**
 * @param {number=} distance
 * @return {?} Distance (0 arguments) or this (1 argument).
 */
d3.Zoom_.clickDistance = function(distance) {};

/**
 * @param {number=} duration
 */
d3.Zoom_.duration = function(duration) {};

/**
 * @param {function(!Array<number>, !Array<number>):
 *     function(number): !Array<number>=} interpolator
 */
d3.Zoom_.interpolate = function(interpolator) {};

/**
 * @param {string} typenames
 * @param {?function(this:Element, T, number, !Array<T>): void=} listener
 * @template T
 */
d3.Zoom_.on = function(typenames, listener) {};

// Zoom Events

/**
 * @record
 */
d3.ZoomEvent = function() {};

/**
 * @type {!d3.Zoom}
 */
d3.ZoomEvent.prototype.target;

/**
 * @type {string}
 */
d3.ZoomEvent.prototype.type;

/**
 * @type {!d3.zoomTransform}
 */
d3.ZoomEvent.prototype.transform;

/**
 * @type {!Event}
 */
d3.ZoomEvent.prototype.sourceEvent;

// Zoom Transforms

/**
 * @param {!Element} node
 * @return {!d3.zoomTransform}
 * @constructor
 */
d3.zoomTransform = function(node) {};

/**
 * @const {number}
 */
d3.zoomTransform.prototype.x;

/**
 * @const {number}
 */
d3.zoomTransform.prototype.y;

/**
 * @const {number}
 */
d3.zoomTransform.prototype.k;

/**
 * @param {number} k
 * @return {!d3.zoomTransform}
 */
d3.zoomTransform.prototype.scale = function(k) {};

/**
 * @param {number} x
 * @param {number} y
 * @return {!d3.zoomTransform}
 */
d3.zoomTransform.prototype.translate = function(x, y) {};

/**
 * @param {!Array<number>} point
 * @return {!Array<number>}
 */
d3.zoomTransform.prototype.apply = function(point) {};

/**
 * @param {number} x
 * @return {number}
 */
d3.zoomTransform.prototype.applyX = function(x) {};

/**
 * @param {number} y
 * @return {number}
 */
d3.zoomTransform.prototype.applyY = function(y) {};

/**
 * @param {!Array<number>} point
 * @return {!Array<number>}
 */
d3.zoomTransform.prototype.invert = function(point) {};

/**
 * @param {number} x
 * @return {number}
 */
d3.zoomTransform.prototype.invertX = function(x) {};

/**
 * @param {number} y
 * @return {number}
 */
d3.zoomTransform.prototype.invertY = function(y) {};

/**
 * @param {function(?): ?} x Invertible continuous scale.
 * @return {function(?): ?}
 */
d3.zoomTransform.prototype.rescaleX = function(x) {};

/**
 * @param {function(?): ?} y Invertible continuous scale.
 * @return {function(?): ?}
 */
d3.zoomTransform.prototype.rescaleY = function(y) {};

/**
 * @override
 * @return {string}
 */
d3.zoomTransform.prototype.toString = function() {};

/**
 * @type {!d3.zoomTransform}
 */
d3.zoomIdentity;
