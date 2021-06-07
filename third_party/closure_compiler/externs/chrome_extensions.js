//    SSSSSSSSSSSSSSS TTTTTTTTTTTTTTTTTTTTTTT     OOOOOOOOO     PPPPPPPPPPPPPPPPP
//  SS:::::::::::::::ST:::::::::::::::::::::T   OO:::::::::OO   P::::::::::::::::P
// S:::::SSSSSS::::::ST:::::::::::::::::::::T OO:::::::::::::OO P::::::PPPPPP:::::P
// S:::::S     SSSSSSST:::::TT:::::::TT:::::TO:::::::OOO:::::::OPP:::::P     P:::::P
// S:::::S            TTTTTT  T:::::T  TTTTTTO::::::O   O::::::O  P::::P     P:::::P
// S:::::S                    T:::::T        O:::::O     O:::::O  P::::P     P:::::P
//  S::::SSSS                                                     P::::PPPPPP:::::P
//   SS::::::SSSSS       This file is generated. To update it,    P:::::::::::::PP
//     SSS::::::::SS          run roll_closure_compiler.          P::::PPPPPPPPP
//        SSSSSS::::S                                             P::::P
//             S:::::S        T:::::T        O:::::O     O:::::O  P::::P
//             S:::::S        T:::::T        O::::::O   O::::::O  P::::P
// SSSSSSS     S:::::S      TT:::::::TT      O:::::::OOO:::::::OPP::::::PP
// S::::::SSSSSS:::::S      T:::::::::T       OO:::::::::::::OO P::::::::P
// S:::::::::::::::SS       T:::::::::T         OO:::::::::OO   P::::::::P
//  SSSSSSSSSSSSSSS         TTTTTTTTTTT           OOOOOOOOO     PPPPPPPPPP
/*
 * Copyright 2009 The Closure Compiler Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview Definitions for the Chromium extensions API.
 *
 * This is the externs file for the Chrome Extensions API.
 * See http://developer.chrome.com/extensions/
 *
 * There are several problematic issues regarding Chrome extension APIs and
 * this externs files, including:
 * A. When to add packages to this file
 * B. Optional parameters
 * C. Pseudo-types
 * D. Events
 * E. Nullability
 * F. Private APIs
 * G. Enums
 *
 * The best practices for each are described in more detail below. It should be
 * noted that, due to historical reasons, and the evolutionary nature of this
 * file, much this file currently violates the best practices described below.
 * As changes are made, the changes should adhere to the best practices.
 *
 * A. When to Add Packages to this File?
 * Packages in chrome.experimental.* should *not* be added to this file. The
 * experimental APIs change very quickly, so rather than add them here, make a
 * separate externs file for your project, then move the API here when it moves
 * out of experimental.
 *
 * Some non-experimental APIs are still evolving or are not full documented. It
 * is still advantageous to include these in this file as doing so avoids a
 * proliferation of project-private externs files containing duplicated info. In
 * these cases, use comments to describe the situation.
 *
 * B. Optional Parameters
 * The Chrome extension APIs make extensive use of interior optional parameters
 * that are not at the end of the parameter list, while the JS Compiler's type
 * system requires optional parameters to be at the end. This creates a bit of
 * tension:
 *
 * 1. If a method has N required params, then the parameter declarations
 *    should have N required params.
 * 2. If, due to interior optional params, a parameter can be of more than
 *    one type, its at-param should:
 *    a. be named to indicate both possibilities, eg, extensionIdOrRequest,
 *       or getInfoOrCallback.
 *    b. the type should include both types, in the same order as the parts
 *       of the name, even when one type subsumes the other, eg, {string|*}
 *       or {Object|function(string)}.
 * See chrome.runtime.sendMessage for a complex example as sendMessage takes
 * three params with the first and third being optional.
 *
 * C. Pseudo-types
 * The Chrome APIs define many types that are actually pseudo-types, that
 * is, they can't be instantiated by name. The extension APIs also pass untyped
 * objects (a bag of properties) to callbacks.
 *
 * The Chrome extension APIs include at least three different situations:
 *
 * 1. an object that must be created by an extension developer and passed
 *    into a Chrome extension API and for which there is no constructor.
 * 2. an instance of a type that is created inside the extension libraries
 *    and passed out to a callback/listener or returned by an extension API
 *    (the constructor implicity lives within the library).
 * 3. like #2, but a bag-of-properties object that is passed out to a
 *    callback/listener or returned by an extension API so there is no
 *    defined type.
 *
 * For #1, use a record type so object literals and objects created via
 * goog.object are acceptable. (Note: a named record type may be declared using
 * the at-record syntax; an anonymous record type may be described using the
 * {foo: !Foo, ...} syntax. Anonymous record types may be named using an
 * at-typedef annotation. See
 * https://github.com/google/closure-compiler/wiki/Types-in-the-Closure-Type-System
 * for more information.) For example, the Permissions type defined at
 * http://developer.chrome.com/extensions/permissions.html#type-Permissions
 * could be:
 *
 *   / **
 *     * at-typedef {?{
 *     *   permissions: (!Array<string>|undefined),
 *     *   origins: (!Array<string>|undefined),
 *     * }}
 *     * /
 *   chrome.permissions.Permissions;
 *
 * Using record types provides type-safety for the fields that are defined in
 * the object literal and also defined in the record type. Note that record
 * types define a minimal interface and will not complain about extraneous
 * (often misspelled) fields.
 *
 * Also, record types are non-nullable by default. The "{?{"
 * creates a nullable record-type typedef so ! has the same meaning in usages
 * as it does for real types.
 *
 * For #2, use a standard constructor, even though no constructor is provided
 * and extension writers will never instantiate an instance, as using a first
 * class type provides the strongest type checking. For example, see the Port
 * type defined at http://developer.chrome.com/apps/runtime.html#type-Port.
 * Always qualify the type name to reduce top-level pollution in this file:
 *
 *   Do:
 *        chrome.runtime.Port = function() {}
 *   Don't:
 *        function Port() {}
 *
 * Note that, unfortunately, the actual Port class definition in this file
 * does not follow this recommendation.
 *
 * For #3, the introduction of arrow functions that are frequently used for
 * callbacks has changed things. Prior to arrow functions, the Best Practices
 * recommended using !Object since that's what in the docs. It was tempting to
 * define a real type within this file and treat this situation as identical to
 * #2, but that meant a new type was being defined in this file and developers
 * did not expect to find required new types in extension files.
 *
 * Arrow functions change things. The common use of them does not include
 * specifying a type for the callback's param, so the compiler infers the type
 * from the externs file. This is good and happens automatically with no actions
 * required of the developer. Futhermore, since the param has a type, field
 * references can use dot access, obj.someField, vs bracket access,
 * obj['someField'] as required for !Object.
 *
 * So, for #3, the best practice is to define a record type for the parameter.
 * See chrome.proxy.settings.GetResponse for an example. As mentioned above in
 * the section for #1, there are three common ways to introduce a record type
 * and any of them are acceptable. Of course, if a type is going to be used more
 * than once, it should be named.
 *
 * While the externs file will define an undocumented record type for a
 * callback's param, using !Object as specified in the docs will continue to
 * work. This is crucial when a callback is a regular function, as opposed to an
 * arrow function (whose parameter's type can be inferred).
 *
 * D. Events
 * Most packages define a set of events with the standard set of methods:
 * addListener, removeListener, hasListener and hasListeners. ChromeVoidEvent
 * is the appropriate type when an event's listeners do not take any
 * parameters, however, many events take parameters specific to that event.
 *
 * For those cases, create a record type for the event that extends
 * ChromeBaseEvent with the correct listener function type filled in.
 * See any of the Event type definitions below that extend ChromeBaseEvent for
 * an example.
 *
 * In some cases the addListener method takes more than one argument, so
 * inheritance from ChromeBaseEvent will not work. In those cases extend
 * ChromeBaseEventNoListeners instead and add the listener interfaces
 * explicitly. See any of the Event type definitions below that extend
 * ChromeBaseEventNoListeners for an example.
 *
 * E. Nullability
 * We treat the Chrome Extension API pages as "the truth". Not-null types should
 * be used in the following situations:
 *
 * 1. Parameters and return values that are not explicitly declared to handle
 *    null.
 * 2. Static event instances, for example, chrome.runtime.onConnect's type
 *    should be: !chrome.runtime.PortEvent.
 * 3. Optional params as there is little value to passing null when the
 *    parameter can be omitted, of course, if null is explicitly declared
 *    to be meaningful, then a nullable type should be used.
 *
 * F. Private APIs
 * Private Chrome APIs (such as those that end in "Private") should go at the
 * bottom of this file.
 *
 * G. Enums
 * An enum's type name and the name of its members must be included in an
 * externs file, but the values of its members are ignored by the compiler.
 * To make it clear enums are not being *defined* in this file, we set
 * string enum values to the empty string (at this time, there are no
 * known enums of other types).
 *
 * As of Mar 2016, the chrome extension docs are incomplete wrt to enums
 * as they don't list the member names, only their string values. This means
 * extension authors will tend to use string literals. Therefore, whereever
 * an enum type should be used, we support either the enum or a string. Once
 * the docs are complete, new uses of enums will no longer need the "or string"
 * portion of the type.
 *
 * Finally, most places in this file where enums should be used are using only
 * string. This is historical and is no longer the recommended practice.
 *
 * See enum chrome.wallpaper.WallpaperLayout and chrome.wallpaper.setWallpaper's
 * param for examples.
 *
 * @externs
 *
 */


/*
 * Ensure projects don't execute this file.
 * The throw is to catch executions of this file, however, without the guard,
 * the compiler's flow analysis stops at the throw, even for an externs file.
 * Therefore, the Math.random() guard fools the compiler during externs
 * processing.
 */
if (Math.random() < 1) {  // always true but the compiler doesn't know that
  throw 'Externs file "chrome_extensions.js" should not be executed';
}



/**
 * @const
 * @see http://developer.chrome.com/apps/app.runtime.html
 */
chrome.app.runtime = {};



/**
 * @constructor
 * @see http://developer.chrome.com/apps/app_runtime.html
 */
chrome.app.runtime.LaunchItem = function() {};


/** @type {!FileEntry} */
chrome.app.runtime.LaunchItem.prototype.entry;


/** @type {string} */
chrome.app.runtime.LaunchItem.prototype.type;


/** @type {!ChromeObjectEvent} */
chrome.app.runtime.onEmbedRequested;


/**
 * @constructor
 * @see https://developer.chrome.com/apps/app_runtime#event-onLaunched
 */
chrome.app.runtime.LaunchData = function() {};


/** @type {!Array<{entry: !FileEntry, type: (string|undefined)}>|undefined} */
chrome.app.runtime.LaunchData.prototype.items;


/** @type {boolean|undefined} */
chrome.app.runtime.LaunchData.prototype.isKioskSession;


/** @type {string|undefined} */
chrome.app.runtime.LaunchData.prototype.source;


/** @type {!ChromeBaseEvent<function(!chrome.app.runtime.LaunchData)>} */
chrome.app.runtime.onLaunched;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.runtime.html#event-onRestarted
 */
chrome.app.runtime.onRestarted;


/**
 * @const
 * @see http://developer.chrome.com/apps/app.window.html
 */
chrome.app.window = {};


/**
 * @see https://developer.chrome.com/apps/app_window#method-getAll
 * @return {!Array<!chrome.app.window.AppWindow>}
 */
chrome.app.window.getAll = function() {};


/**
 * @see https://developer.chrome.com/apps/app_window#method-get
 * @param {string} id
 * @return {chrome.app.window.AppWindow}
 */
chrome.app.window.get = function(id) {};



/**
 * @constructor
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 */
chrome.app.window.AppWindow = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.focus = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.fullscreen = function() {};


/**
 * @return {boolean}
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 */
chrome.app.window.AppWindow.prototype.isFullscreen = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.minimize = function() {};


/**
 * @return {boolean}
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 */
chrome.app.window.AppWindow.prototype.isMinimized = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.maximize = function() {};


/**
 * @return {boolean}
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 */
chrome.app.window.AppWindow.prototype.isMaximized = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.restore = function() {};


/**
 * @param {number} left The new left position, in pixels.
 * @param {number} top The new top position, in pixels.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.moveTo = function(left, top) {};


/**
 * @param {number} width The new width, in pixels.
 * @param {number} height The new height, in pixels.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.resizeTo = function(width, height) {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.drawAttention = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.clearAttention = function() {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.close = function() {};


/**
 * @param {boolean=} opt_focus Should the window be focused? Defaults to true.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.show = function(opt_focus) {};


/**
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.hide = function() {};


/**
 * @return {!chrome.app.window.ContentBounds} The current window bounds.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 */
chrome.app.window.AppWindow.prototype.getBounds = function() {};


/**
 * @param {!chrome.app.window.ContentBounds} bounds The new window bounds.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.setBounds = function(bounds) {};


/**
 * @return {boolean}
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 */
chrome.app.window.AppWindow.prototype.isAlwaysOnTop = function() {};


/**
 * @param {boolean} alwaysOnTop Set whether the window should stay above most
 *     other windows.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.setAlwaysOnTop = function(alwaysOnTop) {};


/**
 * @param {boolean} alwaysVisible Set whether the window is visible on all
 *     workspaces.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.setVisibleOnAllWorkspaces = function(
    alwaysVisible) {};


/**
 * @param {boolean} wantAllKeys Set whether the window should get all keyboard
 *     events including system keys that are usually not sent.
 * @see http://developer.chrome.com/apps/app.window.html#type-AppWindow
 * @return {undefined}
 */
chrome.app.window.AppWindow.prototype.setInterceptAllKeys = function(
    wantAllKeys) {};


/** @type {!ChromeEvent} */
chrome.app.window.AppWindow.prototype.onBoundsChanged;


/** @type {!ChromeEvent} */
chrome.app.window.AppWindow.prototype.onClosed;


/** @type {!ChromeEvent} */
chrome.app.window.AppWindow.prototype.onFullscreened;


/** @type {!ChromeEvent} */
chrome.app.window.AppWindow.prototype.onMinimized;


/** @type {!ChromeEvent} */
chrome.app.window.AppWindow.prototype.onMaximized;


/** @type {!ChromeEvent} */
chrome.app.window.AppWindow.prototype.onRestored;


/** @type {!Window} */
chrome.app.window.AppWindow.prototype.contentWindow;


/** @type {string} */
chrome.app.window.AppWindow.prototype.id;


/** @type {!chrome.app.window.Bounds} */
chrome.app.window.AppWindow.prototype.innerBounds;


/** @type {!chrome.app.window.Bounds} */
chrome.app.window.AppWindow.prototype.outerBounds;


/**
 * @typedef {?{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number,
 *   minWidth: (number|undefined),
 *   minHeight: (number|undefined),
 *   maxWidth: (number|undefined),
 *   maxHeight: (number|undefined),
 *   setPosition: function(number, number),
 *   setSize: function(number, number),
 *   setMinimumSize: function(number, number),
 *   setMaximumSize: function(number, number)
 * }}
 * @see http://developer.chrome.com/apps/app.window.html#type-Bounds
 */
chrome.app.window.Bounds;


/**
 * @typedef {?{
 *   left: (number|undefined),
 *   top: (number|undefined),
 *   width: (number|undefined),
 *   height: (number|undefined),
 *   minWidth: (number|undefined),
 *   minHeight: (number|undefined),
 *   maxWidth: (number|undefined),
 *   maxHeight: (number|undefined)
 * }}
 * @see http://developer.chrome.com/apps/app_window#type-BoundsSpecification
 */
chrome.app.window.BoundsSpecification;


/**
 * @typedef {?{
 *   left: (number|undefined),
 *   top: (number|undefined),
 *   width: (number|undefined),
 *   height: (number|undefined)
 * }}
 * @see http://developer.chrome.com/apps/app_window#type-ContentBounds
 */
chrome.app.window.ContentBounds;


/**
 * @typedef {?{
 *   type: (string|undefined),
 *   color: (string|undefined),
 *   activeColor: (string|undefined),
 *   inactiveColor: (string|undefined)
 * }}
 * @see http://developer.chrome.com/apps/app_window#type-FrameOptions
 */
chrome.app.window.FrameOptions;


/**
 * @typedef {?{
 *   id: (string|undefined),
 *   innerBounds: (!chrome.app.window.BoundsSpecification|undefined),
 *   outerBounds: (!chrome.app.window.BoundsSpecification|undefined),
 *   minWidth: (number|undefined),
 *   minHeight: (number|undefined),
 *   maxWidth: (number|undefined),
 *   maxHeight: (number|undefined),
 *   frame: (!chrome.app.window.FrameOptions|string|undefined),
 *   bounds: (!chrome.app.window.ContentBounds|undefined),
 *   state: (string|undefined),
 *   hidden: (boolean|undefined),
 *   resizable: (boolean|undefined),
 *   singleton: (boolean|undefined),
 *   alwaysOnTop: (boolean|undefined),
 *   focused: (boolean|undefined),
 *   visibleOnAllWorkspaces: (boolean|undefined)
 * }}
 * @see http://developer.chrome.com/apps/app.window.html#method-create
 */
chrome.app.window.CreateWindowOptions;


/**
 * @param {string} url URL to create.
 * @param {!chrome.app.window.CreateWindowOptions=} opt_options The options for
 *     the new window.
 * @param {function(!chrome.app.window.AppWindow)=} opt_createWindowCallback
 *     Callback to be run.
 * @see http://developer.chrome.com/apps/app.window.html#method-create
 * @return {undefined}
 */
chrome.app.window.create = function(
    url, opt_options, opt_createWindowCallback) {};


/**
 * Returns an AppWindow object for the current script context (ie JavaScript
 * 'window' object).
 * @return {!chrome.app.window.AppWindow}
 * @see http://developer.chrome.com/apps/app.window.html#method-current
 */
chrome.app.window.current = function() {};


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.window.html#event-onBoundsChanged
 */
chrome.app.window.onBoundsChanged;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.window.html#event-onClosed
 */
chrome.app.window.onClosed;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.window.html#event-onFullscreened
 */
chrome.app.window.onFullscreened;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.window.html#event-onMaximized
 */
chrome.app.window.onMaximized;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.window.html#event-onMinimized
 */
chrome.app.window.onMinimized;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/apps/app.window.html#event-onRestored
 */
chrome.app.window.onRestored;


/**
 * Private API.
 *
 * @const
 * @see https://code.google.com/p/chromium/codesearch#chromium/src/chrome/common/extensions/api/audio_modem.idl
 * @see go/chrome-modem
 */
chrome.audioModem = {};


/**
 * @typedef {?{
 *   tokenLength: number,
 *   crc: (boolean|undefined),
 *   parity: (boolean|undefined)
 * }}
 */
chrome.audioModem.TokenEncoding;


/**
 * @typedef {?{
 *   timeoutMillis: number,
 *   band: string,
 *   encoding: !chrome.audioModem.TokenEncoding
 * }}
 */
chrome.audioModem.RequestParams;


/** @constructor */
chrome.audioModem.ReceivedToken = function() {};


/** @type {!ArrayBuffer} */
chrome.audioModem.ReceivedToken.prototype.token;


/** @type {string} */
chrome.audioModem.ReceivedToken.prototype.band;


/**
 * @param {!chrome.audioModem.RequestParams} params
 * @param {!ArrayBuffer} token
 * @param {function(string)} callback
 * @return {undefined}
 */
chrome.audioModem.transmit = function(params, token, callback) {};


/**
 * @param {string} band
 * @param {function(string)} callback
 * @return {undefined}
 */
chrome.audioModem.stopTransmit = function(band, callback) {};


/**
 * @param {!chrome.audioModem.RequestParams} params
 * @param {function(string)} callback
 * @return {undefined}
 */
chrome.audioModem.receive = function(params, callback) {};


/**
 * @param {string} band
 * @param {function(string)} callback
 * @return {undefined}
 */
chrome.audioModem.stopReceive = function(band, callback) {};


/**
 * @interface
 * @extends {ChromeBaseEvent<function(!Array<!chrome.audioModem.ReceivedToken>)>}
 */
chrome.audioModem.ReceivedEvent = function() {};


/** @type {!chrome.audioModem.ReceivedEvent} */
chrome.audioModem.onReceived;


/** @type {!ChromeStringEvent} */
chrome.audioModem.onTransmitFail;


/**
 * @see https://developer.chrome.com/apps/bluetooth
 */
chrome.bluetooth = {};



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetooth#type-AdapterState
 */
chrome.bluetooth.AdapterState = function() {};


/** @type {string} */
chrome.bluetooth.AdapterState.prototype.address;


/** @type {string} */
chrome.bluetooth.AdapterState.prototype.name;


/** @type {boolean} */
chrome.bluetooth.AdapterState.prototype.powered;


/** @type {boolean} */
chrome.bluetooth.AdapterState.prototype.available;


/** @type {boolean} */
chrome.bluetooth.AdapterState.prototype.discovering;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetooth#type-Device
 */
chrome.bluetooth.Device = function() {};


/** @type {string} */
chrome.bluetooth.Device.prototype.address;


/** @type {string|undefined} */
chrome.bluetooth.Device.prototype.name;


/** @type {number|undefined} */
chrome.bluetooth.Device.prototype.deviceClass;


/** @type {string|undefined} */
chrome.bluetooth.Device.prototype.vendorIdSource;


/** @type {string|undefined} */
chrome.bluetooth.Device.prototype.vendorId;


/** @type {number|undefined} */
chrome.bluetooth.Device.prototype.productId;


/** @type {number|undefined} */
chrome.bluetooth.Device.prototype.deviceId;


/** @type {string|undefined} */
chrome.bluetooth.Device.prototype.type;


/** @type {boolean|undefined} */
chrome.bluetooth.Device.prototype.paired;


/** @type {boolean|undefined} */
chrome.bluetooth.Device.prototype.connected;


/** @type {boolean|undefined} */
chrome.bluetooth.Device.prototype.connecting;


/** @type {boolean|undefined} */
chrome.bluetooth.Device.prototype.connectable;


/** @type {!Array<string>|undefined} */
chrome.bluetooth.Device.prototype.uuids;


/** @type {number|undefined} */
chrome.bluetooth.Device.prototype.inquiryRssi;


/** @type {number|undefined} */
chrome.bluetooth.Device.prototype.inquiryTxPower;


/**
 * @param {function(!chrome.bluetooth.AdapterState)} callback
 * @see https://developer.chrome.com/apps/bluetooth#method-getAdapterState
 * @return {undefined}
 */
chrome.bluetooth.getAdapterState = function(callback) {};


/**
 * @param {string} deviceAddress
 * @param {function(!chrome.bluetooth.Device)} callback
 * @see https://developer.chrome.com/apps/bluetooth#method-getDevice
 * @return {undefined}
 */
chrome.bluetooth.getDevice = function(deviceAddress, callback) {};


/**
 * @param {function(!Array<!chrome.bluetooth.Device>)} callback
 * @see https://developer.chrome.com/apps/bluetooth#method-getDevices
 * @return {undefined}
 */
chrome.bluetooth.getDevices = function(callback) {};


/**
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetooth#method-startDiscovery
 * @return {undefined}
 */
chrome.bluetooth.startDiscovery = function(opt_callback) {};


/**
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetooth#method-stopDiscovery
 * @return {undefined}
 */
chrome.bluetooth.stopDiscovery = function(opt_callback) {};



/**
 * Event whose listeners take an AdapaterState parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetooth.AdapterState)>}
 */
chrome.bluetooth.AdapterStateEvent = function() {};


/**
 * @type {!chrome.bluetooth.AdapterStateEvent}
 * @see https://developer.chrome.com/apps/bluetooth#event-onAdapterStateChanged
 */
chrome.bluetooth.onAdapterStateChanged;


/**
 * Event whose listeners take an Device parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetooth.Device)>}
 */
chrome.bluetooth.DeviceEvent = function() {};


/**
 * @type {!chrome.bluetooth.DeviceEvent}
 * @see https://developer.chrome.com/apps/bluetooth#event-onDeviceAdded
 */
chrome.bluetooth.onDeviceAdded;


/**
 * @type {!chrome.bluetooth.DeviceEvent}
 * @see https://developer.chrome.com/apps/bluetooth#event-onDeviceChanged
 */
chrome.bluetooth.onDeviceChanged;


/**
 * @type {!chrome.bluetooth.DeviceEvent}
 * @see https://developer.chrome.com/apps/bluetooth#event-onDeviceRemoved
 */
chrome.bluetooth.onDeviceRemoved;


/**
 * @const
 * @see https://developer.chrome.com/apps/bluetoothSocket
 */
chrome.bluetoothSocket = {};


/**
 * @typedef {{
 *   persistent: (boolean|undefined),
 *   name: (string|undefined),
 *   bufferSize: (number|undefined)
 * }}
 * @see https://developer.chrome.com/apps/bluetoothSocket#type-SocketProperties
 */
chrome.bluetoothSocket.SocketProperties;


/**
 * @typedef {{
 *   channel: (number|undefined),
 *   psm: (number|undefined),
 *   backlog: (number|undefined)
 * }}
 * @see https://developer.chrome.com/apps/bluetoothSocket#type-ListenOptions
 */
chrome.bluetoothSocket.ListenOptions;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetoothSocket#type-SocketInfo
 */
chrome.bluetoothSocket.SocketInfo = function() {};


/** @type {number} */
chrome.bluetoothSocket.SocketInfo.prototype.socketId;


/** @type {boolean} */
chrome.bluetoothSocket.SocketInfo.prototype.persistent;


/** @type {string|undefined} */
chrome.bluetoothSocket.SocketInfo.prototype.name;


/** @type {number|undefined} */
chrome.bluetoothSocket.SocketInfo.prototype.bufferSize;


/** @type {boolean} */
chrome.bluetoothSocket.SocketInfo.prototype.paused;


/** @type {boolean} */
chrome.bluetoothSocket.SocketInfo.prototype.connected;


/** @type {string|undefined} */
chrome.bluetoothSocket.SocketInfo.prototype.address;


/** @type {string|undefined} */
chrome.bluetoothSocket.SocketInfo.prototype.uuid;


/**
 * @param {!chrome.bluetoothSocket.SocketProperties|
 *     function(!{socketId: number})} propertiesOrCallback
 * @param {function(!{socketId: number})=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-create
 * @return {undefined}
 */
chrome.bluetoothSocket.create = function(propertiesOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!chrome.bluetoothSocket.SocketProperties} properties
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-update
 * @return {undefined}
 */
chrome.bluetoothSocket.update = function(socketId, properties, opt_callback) {};


/**
 * @param {number} socketId
 * @param {boolean} paused
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-setPaused
 * @return {undefined}
 */
chrome.bluetoothSocket.setPaused = function(socketId, paused, opt_callback) {};


/**
 * @param {number} socketId
 * @param {string} uuid
 * @param {!chrome.bluetoothSocket.ListenOptions|function()} optionsOrCallback
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-listenUsingRfcomm
 * @return {undefined}
 */
chrome.bluetoothSocket.listenUsingRfcomm = function(
    socketId, uuid, optionsOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {string} uuid
 * @param {!chrome.bluetoothSocket.ListenOptions|function()} optionsOrCallback
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-listenUsingL2cap
 * @return {undefined}
 */
chrome.bluetoothSocket.listenUsingL2cap = function(
    socketId, uuid, optionsOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {string} address
 * @param {string} uuid
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-connect
 * @return {undefined}
 */
chrome.bluetoothSocket.connect = function(socketId, address, uuid, callback) {};


/**
 * @param {number} socketId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-disconnect
 * @return {undefined}
 */
chrome.bluetoothSocket.disconnect = function(socketId, opt_callback) {};


/**
 * @param {number} socketId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-close
 * @return {undefined}
 */
chrome.bluetoothSocket.close = function(socketId, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!ArrayBuffer} data
 * @param {function(number)=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-send
 * @return {undefined}
 */
chrome.bluetoothSocket.send = function(socketId, data, opt_callback) {};


/**
 * @param {number} socketId
 * @param {function(!chrome.bluetoothSocket.SocketInfo)} callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-getInfo
 * @return {undefined}
 */
chrome.bluetoothSocket.getInfo = function(socketId, callback) {};


/**
 * @param {function(!Array<!chrome.bluetoothSocket.SocketInfo>)} callback
 * @see https://developer.chrome.com/apps/bluetoothSocket#method-getSockets
 * @return {undefined}
 */
chrome.bluetoothSocket.getSockets = function(callback) {};



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetoothSocket#event-onAccept
 */
chrome.bluetoothSocket.AcceptEventData = function() {};


/** @type {number} */
chrome.bluetoothSocket.AcceptEventData.prototype.socketId;


/** @type {number} */
chrome.bluetoothSocket.AcceptEventData.prototype.clientSocketId;



/**
 * Event whose listeners take a AcceptEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothSocket.AcceptEventData)>}
 */
chrome.bluetoothSocket.AcceptEvent = function() {};


/** @type {!chrome.bluetoothSocket.AcceptEvent} */
chrome.bluetoothSocket.onAccept;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetoothSocket#event-onAcceptError
 */
chrome.bluetoothSocket.AcceptErrorEventData = function() {};


/** @type {number} */
chrome.bluetoothSocket.AcceptErrorEventData.prototype.socketId;


/** @type {string} */
chrome.bluetoothSocket.AcceptErrorEventData.prototype.errorMessage;


/** @type {string} */
chrome.bluetoothSocket.AcceptErrorEventData.prototype.error;



/**
 * Event whose listeners take a AcceptErrorEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothSocket.AcceptErrorEventData)>}
 */
chrome.bluetoothSocket.AcceptErrorEvent = function() {};


/** @type {!chrome.bluetoothSocket.AcceptErrorEvent} */
chrome.bluetoothSocket.onAcceptError;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetoothSocket#event-onReceive
 */
chrome.bluetoothSocket.ReceiveEventData = function() {};


/** @type {number} */
chrome.bluetoothSocket.ReceiveEventData.prototype.socketId;


/** @type {!ArrayBuffer} */
chrome.bluetoothSocket.ReceiveEventData.prototype.data;



/**
 * Event whose listeners take a ReceiveEventData parameter.
 * @interface
 * @extends
 *     {ChromeBaseEvent<function(!chrome.bluetoothSocket.ReceiveEventData)>}
 */
chrome.bluetoothSocket.ReceiveEvent = function() {};


/** @type {!chrome.bluetoothSocket.ReceiveEvent} */
chrome.bluetoothSocket.onReceive;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetoothSocket#event-onReceiveError
 */
chrome.bluetoothSocket.ReceiveErrorEventData = function() {};


/** @type {number} */
chrome.bluetoothSocket.ReceiveErrorEventData.prototype.socketId;


/** @type {string} */
chrome.bluetoothSocket.ReceiveErrorEventData.prototype.errorMessage;


/** @type {string} */
chrome.bluetoothSocket.ReceiveErrorEventData.prototype.error;



/**
 * Event whose listeners take a ReceiveErrorEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<
 *     function(!chrome.bluetoothSocket.ReceiveErrorEventData)>}
 */
chrome.bluetoothSocket.ReceiveErrorEvent = function() {};


/** @type {!chrome.bluetoothSocket.ReceiveErrorEvent} */
chrome.bluetoothSocket.onReceiveError;


/**
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy
 * @const
 */
chrome.bluetoothLowEnergy = {};


/**
 * @typedef {?{
 *   uuid: string,
 *   isPrimary: boolean,
 *   instanceId: (string|undefined),
 *   deviceAddress: (string|undefined)
 * }}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#type-Service
 */
chrome.bluetoothLowEnergy.Service;


/**
 * @constructor
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#type-Request
 */
chrome.bluetoothLowEnergy.Request = function() {};


/** @type {number} */
chrome.bluetoothLowEnergy.Request.prototype.requestId;


/** @type {!Object} */
chrome.bluetoothLowEnergy.Request.prototype.device;


/** @type {!ArrayBuffer|undefined} */
chrome.bluetoothLowEnergy.Request.prototype.value;


/**
 * @typedef {?{
 *   uuid: string,
 *   service: (!chrome.bluetoothLowEnergy.Service|undefined),
 *   properties: !Array<string>,
 *   instanceId: (string|undefined),
 *   value: (!ArrayBuffer|undefined)
 * }}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#type-Characteristic
 */
chrome.bluetoothLowEnergy.Characteristic;


/**
 * @typedef {?{
 *   uuid: string,
 *   characteristic: (!chrome.bluetoothLowEnergy.Characteristic|undefined),
 *   permissions: !Array<string>,
 *   instanceId: (string|undefined),
 *   value: (!ArrayBuffer|undefined)
 * }}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#type-Descriptor
 */
chrome.bluetoothLowEnergy.Descriptor;


/**
 * @typedef {?{
 *   persistent: boolean
 * }}
 */
chrome.bluetoothLowEnergy.ConnectionProperties;


/**
 * @param {string} deviceAddress
 * @param {!chrome.bluetoothLowEnergy.ConnectionProperties|function()}
 *     propertiesOrCallback
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-connect
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.connect = function(
    deviceAddress, propertiesOrCallback, opt_callback) {};

/**
 * @param {string} deviceAddress
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-disconnect
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.disconnect = function(deviceAddress, opt_callback) {};


/**
 * @param {string} serviceId
 * @param {function(!chrome.bluetoothLowEnergy.Service)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getService
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getService = function(serviceId, callback) {};


/**
 * @param {!chrome.bluetoothLowEnergy.Service} service
 * @param {function(string)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-createService
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.createService = function(service, callback) {};


/**
 * @param {string} deviceAddress
 * @param {function(!Array<!chrome.bluetoothLowEnergy.Service>)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getServices
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getServices = function(deviceAddress, callback) {};


/**
 * @param {string} characteristicId
 * @param {function(!chrome.bluetoothLowEnergy.Characteristic)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getCharacteristic
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getCharacteristic = function(
    characteristicId, callback) {};


/**
 * @param {!chrome.bluetoothLowEnergy.Characteristic} characteristic
 * @param {string} serviceId
 * @param {function(string)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-createCharacteristic
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.createCharacteristic = function(
    characteristic, serviceId, callback) {};


/**
 * @param {string} serviceId
 * @param {function(!Array<!chrome.bluetoothLowEnergy.Characteristic>)}
 * callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getCharacteristics
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getCharacteristics = function(serviceId, callback) {};


/**
 * @param {string} serviceId
 * @param {function(!Array<!chrome.bluetoothLowEnergy.Service>)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getIncludedServices
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getIncludedServices = function(
    serviceId, callback) {};


/**
 * @param {string} descriptorId
 * @param {function(!chrome.bluetoothLowEnergy.Descriptor)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getDescriptor
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getDescriptor = function(descriptorId, callback) {};


/**
 * @param {!chrome.bluetoothLowEnergy.Descriptor} descriptor
 * @param {string} characteristicId
 * @param {function(string)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-createDescriptor
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.createDescriptor = function(
    descriptor, characteristicId, callback) {};


/**
 * @param {string} characteristicId
 * @param {function(!Array<!chrome.bluetoothLowEnergy.Descriptor>)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-getDescriptors
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.getDescriptors = function(
    characteristicId, callback) {};


/**
 * @param {string} characteristicId
 * @param {function(!chrome.bluetoothLowEnergy.Characteristic)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-readCharacteristicValue
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.readCharacteristicValue = function(
    characteristicId, callback) {};


/**
 * @param {string} characteristicId
 * @param {!ArrayBuffer} value
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-writeCharacteristicValue
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.writeCharacteristicValue = function(
    characteristicId, value, callback) {};


/**
 * @typedef {?{
 *   persistent: boolean
 * }}
 */
chrome.bluetoothLowEnergy.NotificationSessionProperties;

/**
 * @param {string} characteristicId
 * @param {!chrome.bluetoothLowEnergy.NotificationSessionProperties|function()}
 *     propertiesOrCallback
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-startCharacteristicNotifications
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.startCharacteristicNotifications = function(
    characteristicId, propertiesOrCallback, opt_callback) {};


/**
 * @param {string} characteristicId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-stopCharacteristicNotifications
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.stopCharacteristicNotifications = function(
    characteristicId, opt_callback) {};


/**
 * @typedef {?{
 *   value: !ArrayBuffer,
 *   shouldIndicate: (boolean|undefined)
 * }}
 */
chrome.bluetoothLowEnergy.NotificationIndication;


/**
 * @param {string} characteristicId
 * @param {!chrome.bluetoothLowEnergy.NotificationIndication} notification
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-notifyCharacteristicValueChanged
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.notifyCharacteristicValueChanged = function(
    characteristicId, notification, callback) {};


/**
 * @param {string} descriptorId
 * @param {function(!chrome.bluetoothLowEnergy.Descriptor)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-readDescriptorValue
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.readDescriptorValue = function(
    descriptorId, callback) {};


/**
 * @param {string} descriptorId
 * @param {!ArrayBuffer} value
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-writeDescriptorValue
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.writeDescriptorValue = function(
    descriptorId, value, callback) {};


/**
 * @param {string} serviceId
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-registerService
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.registerService = function(serviceId, callback) {};


/**
 * @param {string} serviceId
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-unregisterService
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.unregisterService = function(serviceId, callback) {};


/**
 * @param {string} serviceId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-removeService
 * @return {undefined}
 */
chrome.bluetoothLowEnergy.removeService = function(serviceId, opt_callback) {};


/**
 * @typedef {?{
 *   type: string,
 *   serviceUuids: (!Array<string>|undefined),
 *   manufacturerData: (!Array<{id: number, data: !Array<number>}>|undefined),
 *   solicitUuids: (!Array<string>|undefined),
 *   serviceData: (!Array<{uuid: string, data: !Array<number>}>|undefined)
 * }}
 */
chrome.bluetoothLowEnergy.Advertisement;


/**
 * @param {!chrome.bluetoothLowEnergy.Advertisement} advertisement
 * @param {function(number)} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-registerAdvertisement
 */
chrome.bluetoothLowEnergy.registerAdvertisement = function(
    advertisement, callback) {};


/**
 * @param {number} advertisementId
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-unregisterAdvertisement
 */
chrome.bluetoothLowEnergy.unregisterAdvertisement = function(
    advertisementId, callback) {};


/**
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-resetAdvertising
 */
chrome.bluetoothLowEnergy.resetAdvertising = function(callback) {};


/**
 * @param {number} minInterval
 * @param {number} maxInterval
 * @param {function()} callback
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#method-setAdvertisingInterval
 */
chrome.bluetoothLowEnergy.setAdvertisingInterval = function(
    minInterval, maxInterval, callback) {};


/**
 * @typedef {?{
 *   requestId: number,
 *   isError: boolean,
 *   value: (!ArrayBuffer|undefined)
 * }}
 */
chrome.bluetoothLowEnergy.RequestResponse;


/** @param {!chrome.bluetoothLowEnergy.RequestResponse} response */
chrome.bluetoothLowEnergy.sendRequestResponse = function(response) {};


/**
 * Event whose listeners take a Service parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothLowEnergy.Service)>}
 */
chrome.bluetoothLowEnergy.ServiceEvent = function() {};


/**
 * @type {!chrome.bluetoothLowEnergy.ServiceEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onServiceAdded
 */
chrome.bluetoothLowEnergy.onServiceAdded;


/**
 * @type {!chrome.bluetoothLowEnergy.ServiceEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onServiceChanged
 */
chrome.bluetoothLowEnergy.onServiceChanged;


/**
 * @type {!chrome.bluetoothLowEnergy.ServiceEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onServiceRemoved
 */
chrome.bluetoothLowEnergy.onServiceRemoved;


/**
 * Event whose listeners take a Characteristic parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothLowEnergy.Characteristic)>}
 */
chrome.bluetoothLowEnergy.CharacteristicEvent = function() {};


/**
 * @type {!chrome.bluetoothLowEnergy.CharacteristicEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onCharacteristicValueChanged
 */
chrome.bluetoothLowEnergy.onCharacteristicValueChanged;


/**
 * Event whose listeners take a Descriptor parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothLowEnergy.Descriptor)>}
 */
chrome.bluetoothLowEnergy.DescriptorEvent = function() {};


/**
 * @type {!chrome.bluetoothLowEnergy.DescriptorEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onDescriptorValueChanged
 */
chrome.bluetoothLowEnergy.onDescriptorValueChanged;


/**
 * Event whose listeners take a Request parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothLowEnergy.Request,
 * string)>}
 */
chrome.bluetoothLowEnergy.RequestEvent = function() {};


/**
 * @type {!chrome.bluetoothLowEnergy.RequestEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onCharacteristicReadRequest
 */
chrome.bluetoothLowEnergy.onCharacteristicReadRequest;


/**
 * @type {!chrome.bluetoothLowEnergy.RequestEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onCharacteristicWriteRequest
 */
chrome.bluetoothLowEnergy.onCharacteristicWriteRequest;


/**
 * @type {!chrome.bluetoothLowEnergy.RequestEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onDescriptorReadRequest
 */
chrome.bluetoothLowEnergy.onDescriptorReadRequest;


/**
 * @type {!chrome.bluetoothLowEnergy.RequestEvent}
 * @see https://developer.chrome.com/apps/bluetoothLowEnergy#event-onDescriptorWriteRequest
 */
chrome.bluetoothLowEnergy.onDescriptorWriteRequest;

/**
 * @const
 * @see https://developer.chrome.com/extensions/browsingData
 */
chrome.browsingData = {};

/**
 * @typedef {?{
 *   since: (number|undefined),
 *   originTypes: ({
 *     unprotectedWeb: (boolean|undefined),
 *     protectedWeb: (boolean|undefined),
 *     extension: (boolean|undefined)
 *   }|undefined),
 * }}
 * @see https://developer.chrome.com/extensions/browsingData#type-RemovalOptions
 */
chrome.browsingData.RemovalOptions;

/**
 * @typedef {?{
 *   appcache: (boolean|undefined),
 *   cache: (boolean|undefined),
 *   cookies: (boolean|undefined),
 *   downloads: (boolean|undefined),
 *   fileSystems: (boolean|undefined),
 *   formData: (boolean|undefined),
 *   history: (boolean|undefined),
 *   indexedDB: (boolean|undefined),
 *   localStorage: (boolean|undefined),
 *   serverBoundCertificates: (boolean|undefined),
 *   passwords: (boolean|undefined),
 *   pluginData: (boolean|undefined),
 *   serviceWorkers: (boolean|undefined),
 *   webSQL: (boolean|undefined),
 * }}
 * @see https://developer.chrome.com/extensions/browsingData#type-DataTypeSet
 */
chrome.browsingData.DataTypeSet;

/**
 * @typedef {?{
 *   options: !chrome.browsingData.RemovalOptions,
 *   dataToRemove: !chrome.browsingData.DataTypeSet,
 *   dataRemovalPermitted: !chrome.browsingData.DataTypeSet,
 * }}
 */
chrome.browsingData.SettingsResponse;

/**
 * @param {function(!chrome.browsingData.SettingsResponse): void} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-settings
 */
chrome.browsingData.settings = function(callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {!chrome.browsingData.DataTypeSet} dataToRemove
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-remove
 */
chrome.browsingData.remove = function(options, dataToRemove, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeAppcache
 */
chrome.browsingData.removeAppcache = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeCache
 */
chrome.browsingData.removeCache = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeCookies
 */
chrome.browsingData.removeCookies = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeDownloads
 */
chrome.browsingData.removeDownloads = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeFileSystems
 */
chrome.browsingData.removeFileSystems = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeFormData
 */
chrome.browsingData.removeFormData = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeHistory
 */
chrome.browsingData.removeHistory = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeIndexedDB
 */
chrome.browsingData.removeIndexedDB = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeLocalStorage
 */
chrome.browsingData.removeLocalStorage = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removePluginData
 */
chrome.browsingData.removePluginData = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removePasswords
 */
chrome.browsingData.removePasswords = function(options, callback) {};

/**
 * @param {!chrome.browsingData.RemovalOptions} options
 * @param {function(): void=} callback
 * @see https://developer.chrome.com/extensions/browsingData#method-removeWebSQL
 */
chrome.browsingData.removeWebSQL = function(options, callback) {};

/**
 * @see http://developer.chrome.com/extensions/commands.html
 * @const
 */
chrome.commands = {};


/**
 * @param {function(Array<string>): void} callback Callback function.
 * @return {undefined}
 */
chrome.commands.getAll = function(callback) {};


/** @type {!ChromeEvent} */
chrome.commands.onCommand;


/**
 * @see https://developer.chrome.com/apps/copresence
 * @const
 */
chrome.copresence = {};


/**
 * @typedef {?{
 *   lowPower: (boolean|undefined),
 *   onlyBroadcast: (boolean|undefined),
 *   onlyScan: (boolean|undefined),
 *   audible: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-Strategy
 */
chrome.copresence.Strategy;


/**
 * @typedef {?{
 *   type: string,
 *   payload: ArrayBuffer
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-Message
 */
chrome.copresence.Message;


/**
 * @typedef {?{
 *   onlyEarshot: (boolean|undefined)
 * }}
 * https://developer.chrome.com/apps/copresence#type-AccessPolicy
 */
chrome.copresence.AccessPolicy;


/**
 * @typedef {?{
 *   id: string,
 *   message: !chrome.copresence.Message,
 *   timeToLiveMillis: (number|undefined),
 *   policy: (!chrome.copresence.AccessPolicy|undefined),
 *   strategies: (!chrome.copresence.Strategy|undefined)
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-PublishOperation
 */
chrome.copresence.PublishOperation;


/** @typedef {?{type: string}} */
chrome.copresence.SubscriptionFilter;


/**
 * @typedef {?{
 *   id: string,
 *   filter: !chrome.copresence.SubscriptionFilter,
 *   timeToLiveMillis: (number|undefined),
 *   strategies: (!chrome.copresence.Strategy|undefined)
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-SubscribeOperation
 */
chrome.copresence.SubscribeOperation;


/**
 * @typedef {?{
 *   unpublishId: string
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-UnpublishOperation
 */
chrome.copresence.UnpublishOperation;


/**
 * @typedef {?{
 *   unsubscribeId: string
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-UnsubscribeOperation
 */
chrome.copresence.UnsubscribeOperation;


/**
 * @typedef {?{
 *   publish: (!chrome.copresence.PublishOperation|undefined),
 *   subscribe: (!chrome.copresence.SubscribeOperation|undefined),
 *   unpublish: (!chrome.copresence.UnpublishOperation|undefined),
 *   unsubscribe: (!chrome.copresence.UnsubscribeOperation|undefined)
 * }}
 * @see https://developer.chrome.com/apps/copresence#type-Operation
 */
chrome.copresence.Operation;


/**
 * @param {!Array<!chrome.copresence.Operation>} operations
 * @param {function(string): void} callback
 * @see https://developer.chrome.com/apps/copresence#method-execute
 * @return {undefined}
 */
chrome.copresence.execute = function(operations, callback) {};



/**
 * Event whose listeners take a subscription id and received messages as a
 * parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(string,
 * !Array<!chrome.copresence.Message>)>}
 * @see https://developer.chrome.com/apps/copresence#event-onMessagesReceived
 */
chrome.copresence.MessagesReceivedEvent = function() {};


/**
 * @type {!chrome.copresence.MessagesReceivedEvent}
 * @see https://developer.chrome.com/apps/copresence#event-onMessagesReceived
 */
chrome.copresence.onMessagesReceived;


/**
 * @type {!ChromeStringEvent}
 * @see https://developer.chrome.com/apps/copresence#event-onStatusUpdated
 */
chrome.copresence.onStatusUpdated;


/** @const */
chrome.devtools = {};


/**
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow
 * @const
 */
chrome.devtools.inspectedWindow = {};


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#type-Resource
 */
chrome.devtools.inspectedWindow.Resource = function() {};

/** @type {string} */
chrome.devtools.inspectedWindow.Resource.prototype.url;

/** @param {function(string, string): void} callback */
chrome.devtools.inspectedWindow.Resource.prototype.getContent = function(
    callback) {};

/**
 * @param {string} content
 * @param {boolean} commit
 * @param {function(!Object): void=} callback
 */
chrome.devtools.inspectedWindow.Resource.prototype.setContent = function(
    content, commit, callback) {};


/**
 * @type {number}
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#property-tabId
 */
chrome.devtools.inspectedWindow.tabId;


/**
 * @typedef {?{
 *   frameUrl: (string|undefined),
 *   useContentScriptContext: (boolean|undefined),
 *   contextSecurityOrigin: (string|undefined)
 * }}
 */
chrome.devtools.inspectedWindow.EvalOptions;

/**
 * @param {string} expression
 * @param {!chrome.devtools.inspectedWindow.EvalOptions=} options
 * @param {function(!Object, !Object): void=} callback
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#method-eval
 */
chrome.devtools.inspectedWindow.eval = function(
    expression, options, callback) {};


/**
 * @typedef {?{
 *   ignoreCache: (boolean|undefined),
 *   userAgent: (string|undefined),
 *   injectedScript: (string|undefined)
 * }}
 */
chrome.devtools.inspectedWindow.ReloadOptions;

/**
 * @param {!chrome.devtools.inspectedWindow.ReloadOptions=} reloadOptions
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#method-reload
 */
chrome.devtools.inspectedWindow.reload = function(reloadOptions) {};


/**
 * @param {function(!Array<!chrome.devtools.inspectedWindow.Resource>): void}
 *     callback
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#method-getResources
 */
chrome.devtools.inspectedWindow.getResources = function(callback) {};


/**
 * @interface
 * @extends {ChromeBaseEvent<
 *     function(!chrome.devtools.inspectedWindow.Resource)>}
 */
chrome.devtools.inspectedWindow.ResourceEvent = function() {};

/**
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#event-onResourceAdded
 * @type {!chrome.devtools.inspectedWindow.ResourceEvent}
 */
chrome.devtools.inspectedWindow.onResourceAdded;


/**
 * @interface
 * @extends {ChromeBaseEvent<
 *     function(!chrome.devtools.inspectedWindow.Resource, string)>}
 */
chrome.devtools.inspectedWindow.ResourceContentEvent = function() {};

/**
 * @see https://developer.chrome.com/extensions/devtools_inspectedWindow#event-onResourceContentCommitted
 * @type {!chrome.devtools.inspectedWindow.ResourceContentEvent}
 */
chrome.devtools.inspectedWindow.onResourceContentCommitted;


/**
 * @see https://developer.chrome.com/docs/extensions/reference/devtools_network/
 * @const
 */
chrome.devtools.network = {};

/**
 * @see https://developer.chrome.com/docs/extensions/reference/devtools_network/#type-Request
 * @constructor
 */
chrome.devtools.network.Request = function() {};

/**
 * @interface
 * @extends {ChromeBaseEvent<
 *     function(!chrome.devtools.network.Request)>}
 */
chrome.devtools.network.RequestEvent = function() {};

/**
 * @see https://developer.chrome.com/docs/extensions/reference/devtools_network/#event-onRequestFinished
 * @type {!chrome.devtools.network.RequestEvent}
 */
chrome.devtools.network.onRequestFinished


/**
 * @see https://developer.chrome.com/extensions/enterprise_platformKeys
 * @const
 */
chrome.enterprise = {};


/**
 * @constructor
 * deviceAttributes allows for reading device attributes.
 * @see https://developer.chrome.com/extensions/enterprise_deviceAttributes.
 */
chrome.enterprise.deviceAttributes = function() {};


/**
 * @param {function(string): void} callback Called with the device identifier
 *     of the directory API when received.
 * @return {undefined}
 */
chrome.enterprise.deviceAttributes.getDirectoryDeviceId = function(callback) {};


/**
 * @param {function(string): void} callback Called with the serial number of the
 *     device.
 * @return {undefined}
 */
chrome.enterprise.deviceAttributes.getDeviceSerialNumber = function(
    callback) {};


/**
 * @param {function(string): void} callback Called with the Asset ID of the
 *     device.
 * @return {undefined}
 */
chrome.enterprise.deviceAttributes.getDeviceAssetId = function(callback) {};


/**
 * @param {function(string): void} callback Called with the Annotated Location
 *     of the device.
 * @return {undefined}
 */
chrome.enterprise.deviceAttributes.getDeviceAnnotatedLocation = function(
    callback) {};


/**
 * @constructor
 * platformKeys allows for generating hardware-backed keys and the installation
 * of certificates for these keys.
 * @see https://developer.chrome.com/extensions/enterprise_platformKeys.
 */
chrome.enterprise.platformKeys = function() {};


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/enterprise_platformKeys#type-Token
 */
chrome.enterprise.Token = function() {};


/**
 * @type {string} Unique id for the Token, either "user" or "system."
 */
chrome.enterprise.Token.prototype.id;


/**
 * @type {!webCrypto.SubtleCrypto} Implements the WebCrypto's
 *     SubtleCrypto interface. The cryptographic operations, including key
 *     generation, are hardware-backed.
 */
chrome.enterprise.Token.prototype.subtleCrypto;


/**
 * @param {!ArrayBuffer} challenge A challenge as emitted by the Verified Access
 *     Web API.
 * @param {boolean|function(!ArrayBuffer): void} registerKeyOrCallback Either a
 *     flag indicating whether to register the key, in which case the callback
 *     is passed as the next arg, or the callback. If a flag is set, the current
 *     Enterprise Machine Key is registered with the "system" token and
 *     relinquishes the Enterprise Machine Key role. The key can then be
 *     associated with a certificate and used like any other signing key. This
 *     key is 2048-bit RSA. Subsequent calls to this function will then generate
 *     a new Enterprise Machine Key.
 * @param {function(!ArrayBuffer=): void=} callback The callback (called back
 *     with the challenge response), if arg2 was the registerKey flag.
 * @return {undefined}
 */
chrome.enterprise.platformKeys.challengeMachineKey = function(
    challenge, registerKeyOrCallback, callback) {};


/**
 * @param {!ArrayBuffer} challenge A challenge as emitted by the Verified Access
 *     Web API.
 * @param {boolean} registerKey If set, the current Enterprise User Key is
 *     registered with the "user" token and relinquishes the Enterprise User
 *     Key role. The key can then be associated with a certificate and used like
 *     any other signing key. This key is 2048-bit RSA. Subsequent calls to this
 *     function will then generate a new Enterprise User Key.
 * @param {function(!ArrayBuffer): void} callback Called back with the
 *     challenge response.
 * @return {undefined}
 */
chrome.enterprise.platformKeys.challengeUserKey = function(
    challenge, registerKey, callback) {};


/**
 * @param {function(!Array<!chrome.enterprise.Token>): void} callback Called
 * with an array of Tokens.
 * @return {undefined}
 */
chrome.enterprise.platformKeys.getTokens = function(callback) {};


/**
 * @param {string} tokenId Id of cetificate token either "user" or "system".
 * @param {(function(!Array<!ArrayBuffer>): void)} callback Array of DER
 *     encoded x.509 certificates.
 * @return {undefined}
 */
chrome.enterprise.platformKeys.getCertificates = function(tokenId, callback) {};


/**
 * @param {string} tokenId The id of a Token returned by getTokens.
 * @param {!ArrayBuffer} certificate The DER encoding of a X.509 certificate.
 * @param {function(): void=} opt_callback Called back when this operation is
 *     finished.
 * @return {undefined}
 */
chrome.enterprise.platformKeys.importCertificate = function(
    tokenId, certificate, opt_callback) {};


/**
 * @param {string} tokenId The id of a Token returned by getTokens.
 * @param {!ArrayBuffer} certificate The DER encoding of a X.509 certificate.
 * @param {(function(): void)=} opt_callback Called back when this operation is
 *     finished.
 * @return {undefined}
 */
chrome.enterprise.platformKeys.removeCertificate = function(
    tokenId, certificate, opt_callback) {};

/**
 * reportingPrivate is a Private API for reporting Chrome browser status to
 * admin console.
 * @see https://cs.chromium.org/chromium/src/chrome/common/extensions/api/enterprise_reporting_private.idl
 */
chrome.enterprise.reportingPrivate = {};

/**
 * Uploads the status of Chrome browser to the admin console by sending
 * request to the DMServer. Sets runtime.lastError on failure.
 * @param {!Object} report Object to report to admin console.
 * @param {(function(): void)=} callback Called back when this operation is
 *     finished.
 */
chrome.enterprise.reportingPrivate.uploadChromeDesktopReport = function(
    report, callback) {};

/**
 * Gets the identity of device that Chrome browser is running on. The ID is
 * retrieved from the local device and used by the Google admin console.
 * @param {(function(!string): void)=} callback Called with the device ID.
 */
chrome.enterprise.reportingPrivate.getDeviceId = function(callback) {};

/**
 * Returns a random secret stored in a platform specific storage.
 * @param {(function(!ArrayBuffer): void)=} callback Called with the result.
 */
chrome.enterprise.reportingPrivate.getPersistentSecret = function(callback) {};

/**
 * Returns byte string associated with the data item stored in a platform
 * specific storage.
 * @param {!string} item Item name (can have containers separated by '/').
 * @param {(function((!ArrayBuffer|undefined)): void)=} callback Called back with the
 *     response.
 */
chrome.enterprise.reportingPrivate.getDeviceData = function(item, callback) {};

/**
 * Stores byte string associated with the data item in a platform
 * specific storage.
 * @param {!string} item Item name (can have containers separated by '/').
 * @param {!ArrayBuffer|undefined} data Byte string to associate with the data item.
 * @param {(function(): void)=} callback Called back with the response.
 */
chrome.enterprise.reportingPrivate.setDeviceData = function(
    item, data, callback) {};

/**
 * Represents a device info property type.
 * @enum {string}
 */
chrome.enterprise.reportingPrivate.SettingValue = {
  UNKNOWN: '',
  DISABLED: '',
  ENABLED: '',
};

/**
 * Type of the object returned by getDeviceInfo.
 * @typedef {?{
 *   osName: string,
 *   osVersion: string,
 *   deviceHostName: string,
 *   deviceModel: string,
 *   serialNumber: string,
 *   screenLockSecured: chrome.enterprise.reportingPrivate.SettingValue,
 *   diskEncrypted: chrome.enterprise.reportingPrivate.SettingValue,
 *   macAddresses: (!Array<string>|undefined),
 * }}
 */
chrome.enterprise.reportingPrivate.DeviceInfo;

/**
 * Returns the device information object.
 * @param {(function(!chrome.enterprise.reportingPrivate.DeviceInfo): void)=}
 *     callback Called back with the response.
 */
chrome.enterprise.reportingPrivate.getDeviceInfo = function(callback) {};

/**
 * Represents possible states for the EnterpriseRealTimeUrlCheckMode policy
 * @enum {number}
 */
chrome.enterprise.reportingPrivate.RealtimeUrlCheckMode = {
  DISABLED: 0,
  ENABLED_MAIN_FRAME: 0,
};

/**
 * Type of the object returned by getContextInfo.
 * @typedef {?{
 *   browserAffiliationIds: (!Array<string>|undefined),
 *   profileAffiliationIds: (!Array<string>|undefined),
 *   onFileAttachedProviders: (!Array<string>|undefined),
 *   onFileDownloadedProviders: (!Array<string>|undefined),
 *   onBulkDataEntryProviders: (!Array<string>|undefined),
 *   onSecurityEventProviders: (!Array<string>|undefined),
 *   realtimeUrlCheckMode: chrome.enterprise.reportingPrivate.RealtimeUrlCheckMode,
 *   browserVersion: string,
 * }}
 */
chrome.enterprise.reportingPrivate.ContextInfo;

/**
 * Returns the context information object.
 * @param {(function(!chrome.enterprise.reportingPrivate.ContextInfo): void)}
 *     callback Called back with the response.
 */
chrome.enterprise.reportingPrivate.getContextInfo = function(callback) {};


/**
 * Possible states for the Certificate status.
 * @enum {number}
 */
chrome.enterprise.reportingPrivate.CertificateStatus = {
  OK: 0,
  POLICY_UNSET: 1,
};

/**
 * Type of the object returned by getCertificate.
 * @typedef {?{
 *   status: chrome.enterprise.reportingPrivate.CertificateStatus,
 *   encodedCertificate: (!ArrayBuffer|undefined),
 * }}
 */
chrome.enterprise.reportingPrivate.Certificate;

/**
 * Returns the certificate object.
 * @param {!string} url URL for which certificate needs to be fetched.
 * @param {(function(!chrome.enterprise.reportingPrivate.Certificate): void)}
 *     callback Called back with the response.
 */
chrome.enterprise.reportingPrivate.getCertificate = function(url, callback) {};

/**
 * @see https://developer.chrome.com/extensions/extension.html
 * @const
 */
chrome.extension = {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/extension#type-ViewType
 */
chrome.extension.ViewType = {
  TAB: '',
  POPUP: '',
};


/** @type {{message:(string|undefined)}|undefined} */
chrome.extension.lastError = {};


/** @type {boolean|undefined} */
chrome.extension.inIncognitoContext;


// TODO: change Object to !Object when it's clear nobody is passing in null
// TODO: change Port to !Port since it should never be null
/**
 * @param {string|Object<string>=} opt_extensionIdOrConnectInfo Either the
 *     extensionId to connect to, in which case connectInfo params can be
 *     passed in the next optional argument, or the connectInfo params.
 * @param {Object<string>=} opt_connectInfo The connectInfo object,
 *     if arg1 was the extensionId to connect to.
 * @return {Port} New port.
 */
chrome.extension.connect = function(
    opt_extensionIdOrConnectInfo, opt_connectInfo) {};


/**
 * @return {Window} The global JS object for the background page.
 */
chrome.extension.getBackgroundPage = function() {};


/**
 * @param {string} path A path to a resource within an extension expressed
 *     relative to it's install directory.
 * @return {string} The fully-qualified URL to the resource.
 */
chrome.extension.getURL = function(path) {};


/**
 * @typedef {?{
 *   type: (!chrome.extension.ViewType|string|undefined),
 *   windowId: (number|undefined),
 *   tabId: (number|undefined)
 * }}
 */
chrome.extension.ViewInfo;

/**
 * @param {?chrome.extension.ViewInfo=} opt_fetchProperties
 * @return {Array<Window>} The global JS objects for each content view.
 */
chrome.extension.getViews = function(opt_fetchProperties) {};


/**
 * @param {function(boolean): void} callback Callback function.
 * @return {undefined}
 */
chrome.extension.isAllowedFileSchemeAccess = function(callback) {};


/**
 * @param {function(boolean): void} callback Callback function.
 * @return {undefined}
 */
chrome.extension.isAllowedIncognitoAccess = function(callback) {};


/**
 * @param {string|*} extensionIdOrRequest Either the extensionId to send the
 *     request to, in which case the request is passed as the next arg, or the
 *     request.
 * @param {*=} opt_request The request value, if arg1 was the extensionId.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 * @return {undefined}
 */
chrome.extension.sendMessage = function(
    extensionIdOrRequest, opt_request, opt_callback) {};


/**
 * @param {number|*=} opt_arg1 Either the extensionId to send the request to,
 *     in which case the request is passed as the next arg, or the request.
 * @param {*=} opt_request The request value, if arg1 was the extensionId.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 * @return {undefined}
 */
chrome.extension.sendRequest = function(opt_arg1, opt_request, opt_callback) {};


/**
 * @param {string} data
 * @return {undefined}
 */
chrome.extension.setUpdateUrlData = function(data) {};


/** @type {!ChromeEvent} */
chrome.extension.onConnect;


/** @type {!ChromeEvent} */
chrome.extension.onConnectExternal;


/** @type {!ChromeEvent} */
chrome.extension.onMessage;


/** @type {!ChromeEvent} */
chrome.extension.onRequest;


/** @type {!ChromeEvent} */
chrome.extension.onRequestExternal;

/**
 * @see https://developer.chrome.com/extensions/runtime.html
 * @const
 */
chrome.runtime = {};


/** @type {{message:(string|undefined)}|undefined} */
chrome.runtime.lastError;


/**
 * @param {string|!Object=} opt_extensionIdOrConnectInfo Either the
 *     extensionId to connect to, in which case connectInfo params can be
 *     passed in the next optional argument, or the connectInfo params.
 * @param {!Object=} opt_connectInfo The connectInfo object,
 *     if arg1 was the extensionId to connect to.
 * @return {!Port} New port.
 */
chrome.runtime.connect = function(
    opt_extensionIdOrConnectInfo, opt_connectInfo) {};


/**
 * @param {string|*} extensionIdOrMessage Either the extensionId to send the
 *     message to, in which case the message is passed as the next arg, or the
 *     message itself.
 * @param {(*|!Object|function(*): void)=} opt_messageOrOptsOrCallback
 *     One of:
 *     The message, if arg1 was the extensionId.
 *     The options for message sending, if arg1 was the message and this
 *     argument is not a function.
 *     The callback, if arg1 was the message and this argument is a function.
 * @param {(!Object|function(*): void)=} opt_optsOrCallback
 *     Either the options for message sending, if arg2 was the message,
 *     or the callback.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 * @return {undefined}
 */
chrome.runtime.sendMessage = function(
    extensionIdOrMessage, opt_messageOrOptsOrCallback, opt_optsOrCallback,
    opt_callback) {};


/** @type {string} */
chrome.runtime.id;


/**
 * @param {function(!Window=): void} callback Callback function.
 * @return {undefined}
 */
chrome.runtime.getBackgroundPage = function(callback) {};


/**
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.runtime.openOptionsPage = function(opt_callback) {};


/**
 * Manifest information returned from chrome.runtime.getManifest. See
 * https://developer.chrome.com/extensions/manifest.html and
 * https://developer.chrome.com/apps/manifest.html
 *
 * Note that:
 * 1) There are several other fields not included here. They should be
 *    added to these externs as needed.
 * 2) There are some distinct fields for app vs extension manifests. The
 *    below manifest definition contains the union of all fields; unique
 *    fields should be annotated as such (see e.g. kiosk_only property
 *    below).
 * @constructor
 */
chrome.runtime.Manifest = function() {};


/** @type {string} */
chrome.runtime.Manifest.prototype.name;


/** @type {string} */
chrome.runtime.Manifest.prototype.version;


/** @type {string|undefined} */
chrome.runtime.Manifest.prototype.version_name;


/** @type {number|undefined} */
chrome.runtime.Manifest.prototype.manifest_version;


/** @type {string|undefined} */
chrome.runtime.Manifest.prototype.description;


/** @type {!Object<string, string>} */
chrome.runtime.Manifest.prototype.icons;


/** @type {!chrome.runtime.Manifest.Oauth2|undefined} */
chrome.runtime.Manifest.prototype.oauth2;


/** @type {!Array<(string|!Object)>} */
chrome.runtime.Manifest.prototype.permissions;


/**
 * App-only manifest field.
 * @see https://developer.chrome.com/apps/manifest
 * @type {boolean|undefined}
 */
chrome.runtime.Manifest.prototype.kiosk_only;

/**
 * App-only manifest field.
 * @see https://developer.chrome.com/apps/manifest
 * @type {boolean|undefined}
 */
chrome.runtime.Manifest.prototype.kiosk_enabled;

/** @type {string|undefined} */
chrome.runtime.Manifest.prototype.replacement_web_app;



/**
 * Oauth2 info in the manifest.
 * See http://developer.chrome.com/apps/app_identity.html#update_manifest.
 * @constructor
 */
chrome.runtime.Manifest.Oauth2 = function() {};


/** @type {string} */
chrome.runtime.Manifest.Oauth2.prototype.client_id;


/**@type {!Array<string>} */
chrome.runtime.Manifest.Oauth2.prototype.scopes;


/**
 * https://developer.chrome.com/extensions/runtime.html#method-getManifest
 * https://developer.chrome.com/apps/runtime#method-getManifest
 * @return {!chrome.runtime.Manifest} The full manifest file of the app or
 *     extension.
 */
chrome.runtime.getManifest = function() {};


/**
 * @param {string} path A path to a resource within an extension expressed
 *     relative to it's install directory.
 * @return {string} The fully-qualified URL to the resource.
 */
chrome.runtime.getURL = function(path) {};


/**
 * @param {string} url This may be used to clean up server-side data, do
 *     analytics, and implement surveys. Maximum 255 characters.
 * @return {undefined}
 */
chrome.runtime.setUninstallURL = function(url) {};


/**
 * Reloads the app or extension.
 * @return {undefined}
 */
chrome.runtime.reload = function() {};


/**
 * @see https://developer.chrome.com/apps/runtime#method-requestUpdateCheck
 * @param {function(string, !{version: string}=): void} callback Called with
 *     "throttled", "no_update", or "update_available". If an update is
 *     available, the object contains more information about the available
 *     update.
 * @return {undefined}
 */
chrome.runtime.requestUpdateCheck = function(callback) {};


/**
 * Restart the ChromeOS device when the app runs in kiosk mode. Otherwise, it's
 * no-op.
 * @return {undefined}
 */
chrome.runtime.restart = function() {};


/**
 * @see https://developer.chrome.com/extensions/runtime#method-restartAfterDelay
 * @param {number} seconds Time to wait in seconds before rebooting the device,
 *     or -1 to cancel a scheduled reboot.
 * @param {function():void=} opt_callback A callback to be invoked when a
 *     restart request was successfully rescheduled.
 * @return {undefined}
 */
chrome.runtime.restartAfterDelay = function(seconds, opt_callback) {};


/**
 * @see http://developer.chrome.com/extensions/runtime.html#method-connectNative
 * @param {string} application Name of the registered native messaging host to
 *     connect to, like 'com.google.your_product'.
 * @return {!Port} New port.
 */
chrome.runtime.connectNative = function(application) {};


/**
 * @see http://developer.chrome.com/extensions/runtime.html#method-sendNativeMessage
 * @param {string} application Name of the registered native messaging host to
 *     connect to, like 'com.google.your_product'.
 * @param {Object} message The message that will be passed to the native
 *     messaging host.
 * @param {function(*)=} opt_callback Called with the response message sent by
 *     the native messaging host. If an error occurs while connecting to the
 *     native messaging host, the callback will be called with no arguments and
 *     chrome.runtime.lastError will be set to the error message.
 * @return {undefined}
 */
chrome.runtime.sendNativeMessage = function(
    application, message, opt_callback) {};


/**
 * The operating system chrome is running on.
 * @see https://developer.chrome.com/apps/runtime#type-PlatformOs
 * @enum {string}
 */
chrome.runtime.PlatformOs = {
  ANDROID: '',
  CROS: '',
  LINUX: '',
  MAC: '',
  OPENBSD: '',
  WIN: '',
};


/**
 * The machine's processor architecture.
 * @see https://developer.chrome.com/apps/runtime#type-PlatformArch
 * @enum {string}
 */
chrome.runtime.PlatformArch = {
  ARM: '',
  ARM64: '',
  MIPS: '',
  MIPS64: '',
  X86_32: '',
  X86_64: '',
};


/**
 * The native client architecture.
 * @see https://developer.chrome.com/apps/runtime#type-PlatformNaclArch
 * @enum {string}
 */
chrome.runtime.PlatformNaclArch = {
  ARM: '',
  MIPS: '',
  MIPS64: '',
  X86_32: '',
  X86_64: '',
};


/**
 * @see https://developer.chrome.com/apps/runtime#type-PlatformInfo
 * @typedef {{
 *   os: !chrome.runtime.PlatformOs,
 *   arch: !chrome.runtime.PlatformArch,
 *   nacl_arch: !chrome.runtime.PlatformNaclArch,
 * }}
 */
chrome.runtime.PlatformInfo;


/**
 * @see https://developer.chrome.com/extensions/runtime#type-PlatformInfo
 * @param {function(!chrome.runtime.PlatformInfo)} callback
 * @return {undefined}
 */
chrome.runtime.getPlatformInfo = function(callback) {};


/**
 * @param {function(!DirectoryEntry)} callback
 * @return {undefined}
 */
chrome.runtime.getPackageDirectoryEntry = function(callback) {};


/** @type {!chrome.runtime.PortEvent} */
chrome.runtime.onConnect;


/** @type {!chrome.runtime.PortEvent} */
chrome.runtime.onConnectExternal;


/** @type {!chrome.runtime.PortEvent} */
chrome.runtime.onConnectNative;


/** @type {!ChromeObjectEvent} */
chrome.runtime.onInstalled;


/** @type {!chrome.runtime.MessageSenderEvent} */
chrome.runtime.onMessage;


/** @type {!chrome.runtime.MessageSenderEvent} */
chrome.runtime.onMessageExternal;


/** @type {!ChromeEvent} */
chrome.runtime.onStartup;


/** @type {!ChromeEvent} */
chrome.runtime.onSuspend;


/** @type {!ChromeEvent} */
chrome.runtime.onSuspendCanceled;


/** @type {!ChromeObjectEvent} */
chrome.runtime.onUpdateAvailable;


/** @type {!ChromeStringEvent} */
chrome.runtime.onRestartRequired;



/**
 * Event whose listeners take a Port parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!Port)>}
 */
chrome.runtime.PortEvent = function() {};


/**
 * Event whose listeners take a MessageSender and additional parameters.
 * @see http://developer.chrome.com/dev/apps/runtime.html#event-onMessage
 * @interface
 * @extends {ChromeBaseEvent<function(*, !MessageSender, function(*): void):
 * (boolean|undefined)>}
 */
chrome.runtime.MessageSenderEvent = function() {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/tabs
 */
chrome.tabs = {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/tabs#type-TabStatus
 */
chrome.tabs.TabStatus = {
  COMPLETE: '',
  LOADING: '',
};


/**
 * @const {number}
 * @see https://developer.chrome.com/extensions/tabs#property-TAB_ID_NONE
 */
chrome.tabs.TAB_ID_NONE = -1;


/**
 * @typedef {?{
 *   code: (string|undefined),
 *   file: (string|undefined),
 *   allFrames: (boolean|undefined),
 *   matchAboutBlank: (boolean|undefined),
 *   runAt: (string|undefined)
 * }}
 */
chrome.tabs.InjectDetails;


/**
 * @see https://developer.chrome.com/extensions/tabs#method-captureVisibleTab
 * @param {number|!chrome.types.ImageDetails|function(string):void}
 *     windowIdOrOptionsOrCallback One of:
 *     The target window.
 *     An object defining details about the format and quality of an image, in
 *     which case the window defaults to the current window.
 *     A callback function which accepts the data URL string of a JPEG encoding
 *     of the visible area of the captured tab.
 * @param {(!chrome.types.ImageDetails|function(string):void)=}
 *     opt_optionsOrCallback Either an object defining details about the
 *     format and quality of an image, or a callback function which accepts the
 *     data URL string of a JPEG encoding of the visible area of the captured
 *     tab.
 * @param {function(string):void=} opt_callback A callback function which
 *     accepts the data URL string of a JPEG encoding of the visible area of the
 *     captured tab.
 * @return {undefined}
 */
chrome.tabs.captureVisibleTab = function(
    windowIdOrOptionsOrCallback, opt_optionsOrCallback, opt_callback) {};


/**
 * @param {number} tabId Tab Id.
 * @param {{name: (string|undefined)}=} connectInfo Info Object.
 * @return {Port} New port.
 */
chrome.tabs.connect = function(tabId, connectInfo) {};


/**
 * @typedef {?{
 *   windowId: (number|undefined),
 *   index: (number|undefined),
 *   url: (string|undefined),
 *   active: (boolean|undefined),
 *   pinned: (boolean|undefined),
 *   openerTabId: (number|undefined)
 * }}
 */
chrome.tabs.CreateProperties;


/**
 * @param {!chrome.tabs.CreateProperties} createProperties Info object.
 * @param {function(!Tab): void=} opt_callback The callback function.
 * @return {undefined}
 */
chrome.tabs.create = function(createProperties, opt_callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#method-detectLanguage
 * @param {number|function(string): void} tabIdOrCallback The tab id, or a
 *     callback function that will be invoked with the language of the active
 *     tab in the current window.
 * @param {function(string): void=} opt_callback An optional callback function
 *     that will be invoked with the language of the tab specified as first
 *     argument.
 * @return {undefined}
 */
chrome.tabs.detectLanguage = function(tabIdOrCallback, opt_callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#method-discard
 * @param {number|function(!Tab): void} tabIdOrCallback
 * @param {function(!Tab): void=} opt_callback
 */
chrome.tabs.discard;


/**
 * @see https://developer.chrome.com/extensions/tabs#method-executeScript
 * @param {number|!chrome.tabs.InjectDetails} tabIdOrDetails
 *     Either the id of the tab in which to run the script, or an object
 *     containing the details of the script to run, in which case the script
 *     will be executed in the active tab of the current window.
 * @param {(!chrome.tabs.InjectDetails|function(!Array<*>):void)=}
 *     opt_detailsOrCallback Either an object containing the details of the
 *     script to run, if the tab id was speficied as first argument, or a
 *     callback that will be invoked with the result of the execution of the
 *     script in every injected frame.
 * @param {function(!Array<*>):void=} opt_callback A callback that will be
 *     invoked with the result of the execution of the script in every
 *     injected frame.
 * @return {undefined}
 */
chrome.tabs.executeScript = function(
    tabIdOrDetails, opt_detailsOrCallback, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {function(!Tab): void} callback Callback.
 * @return {undefined}
 */
chrome.tabs.get = function(tabId, callback) {};


/**
 * Note (2014-05-21): Because this function is deprecated, the types of it's
 * parameters were not upgraded to make the first parameter optional and to mark
 * the Array and Tab in the callback as non-null.
 *
 * @param {number?} windowId Window id.
 * @param {function(Array<Tab>): void} callback Callback.
 * @deprecated Please use tabs.query {windowId: windowId}.
 * @return {undefined}
 */
chrome.tabs.getAllInWindow = function(windowId, callback) {};


/**
 * @param {function(!Tab=): void} callback Callback.
 * @return {undefined}
 */
chrome.tabs.getCurrent = function(callback) {};


/**
 * Note (2014-05-21): Because this function is deprecated, the types of it's
 * parameters were not upgraded to make the first parameter optional and to mark
 * the Array and Tab in the callback as non-null.
 *
 * @param {number?} windowId Window id.
 * @param {function(Tab): void} callback Callback.
 * @deprecated Please use tabs.query({active: true}).
 * @return {undefined}
 */
chrome.tabs.getSelected = function(windowId, callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#method-getZoom
 * @param {number|function(number): void} tabIdOrCallback
 * @param {function(number): void=} opt_callback
 * @return {undefined}
 */
chrome.tabs.getZoom = function(tabIdOrCallback, opt_callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#type-ZoomSettings
 * @typedef {?{
 *   mode: (string|undefined),
 *   scope: (string|undefined),
 *   defaultZoomFactor: (number|undefined),
 * }}
 */
chrome.tabs.ZoomSettings;


/**
 * @see https://developer.chrome.com/extensions/tabs#method-getZoomSettings
 * @param {number|function(!chrome.tabs.ZoomSettings): void} tabIdOrCallback
 * @param {function(!chrome.tabs.ZoomSettings): void=} opt_callback
 */
chrome.tabs.getZoomSettings = function(tabIdOrCallback, opt_callback) {};


/**
 * @typedef {?{
 *   windowId: (number|undefined),
 *   tabs: (number|!Array<number>)
 * }}
 */
chrome.tabs.HighlightInfo;


/**
 * @see https://developer.chrome.com/extensions/tabs#method-highlight
 * @param {!chrome.tabs.HighlightInfo} highlightInfo
 * @param {function(!ChromeWindow): void=} callback Callback function invoked
 *    for each window whose tabs were highlighted.
 * @return {undefined}
 */
chrome.tabs.highlight = function(highlightInfo, callback) {};


/**
 * @link https://developer.chrome.com/extensions/tabs#method-insertCSS
 * @param {number|!chrome.tabs.InjectDetails} tabIdOrDetails
 *     Either the id of the tab in which to run the script, or an object
 *     containing the details of the CSS to insert, in which case the script
 *     will be executed in the active tab of the current window.
 * @param {(!chrome.tabs.InjectDetails|function():void)=}
 *     opt_detailsOrCallback Either an object containing the details of the
 *     CSS to insert, if the tab id was speficied as first argument, or a
 *     callback that will be invoked after the CSS has been injected.
 * @param {function():void=} opt_callback A callback that will be invoked after
 *     the CSS has been injected.
 * @return {undefined}
 */
chrome.tabs.insertCSS = function(
    tabIdOrDetails, opt_detailsOrCallback, opt_callback) {};


/**
 * @typedef {?{
 *   windowId: (number|undefined),
 *   index: number
 * }}
 */
chrome.tabs.MoveProperties;


/**
 * @param {number|!Array<number>} tabId Tab id or array of tab ids.
 * @param {!chrome.tabs.MoveProperties} moveProperties
 * @param {function((!Tab|!Array<!Tab>)): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.tabs.move = function(tabId, moveProperties, opt_callback) {};


/**
 * @typedef {?{
 *   active: (boolean|undefined),
 *   pinned: (boolean|undefined),
 *   audible: (boolean|undefined),
 *   muted: (boolean|undefined),
 *   highlighted: (boolean|undefined),
 *   discarded: (boolean|undefined),
 *   autoDiscardable: (boolean|undefined),
 *   currentWindow: (boolean|undefined),
 *   lastFocusedWindow: (boolean|undefined),
 *   status: (!chrome.tabs.TabStatus|string|undefined),
 *   title: (string|undefined),
 *   url: (!Array<string>|string|undefined),
 *   windowId: (number|undefined),
 *   windowType: (string|undefined),
 *   index: (number|undefined)
 * }}
 */
chrome.tabs.QueryInfo;


/**
 * @param {!chrome.tabs.QueryInfo} queryInfo
 * @param {function(!Array<!Tab>): void} callback Callback.
 * @return {undefined}
 */
chrome.tabs.query = function(queryInfo, callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#method-query
 * @param {number} tabId The ID of the tab which is to be duplicated.
 * @param {(function(!Tab=):void)=} opt_callback A callback to be invoked with
 *     details about the duplicated tab.
 * @return {undefined}
 */
chrome.tabs.duplicate = function(tabId, opt_callback) {};


/**
 * @typedef {?{
 *   bypassCache: (boolean|undefined)
 * }}
 */
chrome.tabs.ReloadProperties;


/**
 * @see https://developer.chrome.com/extensions/tabs#method-reload
 * @param {(number|!chrome.tabs.ReloadProperties|function():void)=}
 *     opt_tabIdOrReloadPropertiesOrCallback One of:
 *     The ID of the tab to reload; defaults to the selected tab of the current
 *     window.
 *     An object specifying boolean flags to customize the reload operation.
 *     A callback to be invoked when the reload is complete.
 * @param {(!chrome.tabs.ReloadProperties|function():void)=}
 *     opt_reloadPropertiesOrCallback Either an object specifying boolean flags
 *     to customize the reload operation, or a callback to be invoked when the
 *     reload is complete, if no object needs to be specified.
 * @param {function():void=} opt_callback  A callback to be invoked when the
 *     reload is complete.
 * @return {undefined}
 */
chrome.tabs.reload = function(
    opt_tabIdOrReloadPropertiesOrCallback, opt_reloadPropertiesOrCallback,
    opt_callback) {};


/**
 * @param {number|!Array<number>} tabIds A tab ID or an array of tab IDs.
 * @param {function(): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.tabs.remove = function(tabIds, opt_callback) {};


/**
 * @typedef {?{
 *   frameId: (number|undefined)
 * }}
 */
chrome.tabs.SendMessageOptions;


/**
 * @param {number} tabId Tab id.
 * @param {*} request The request value of any type.
 * @param {(!chrome.tabs.SendMessageOptions|function(*): void)=}
 *     opt_optionsOrCallback The object with an optional "frameId" or the
 *     callback function.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 * @return {undefined}
 */
chrome.tabs.sendMessage = function(
    tabId, request, opt_optionsOrCallback, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {*} request The request value of any type.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 * @deprecated Please use runtime.sendMessage.
 * @return {undefined}
 */
chrome.tabs.sendRequest = function(tabId, request, opt_callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#method-setZoom
 * @param {number} tabIdOrZoomFactor
 * @param {(number|function(): void)=} opt_zoomFactorOrCallback
 * @param {function(): void=} opt_callback
 * @return {undefined}
 */
chrome.tabs.setZoom = function(
    tabIdOrZoomFactor, opt_zoomFactorOrCallback, opt_callback) {};


/**
 * @see https://developer.chrome.com/extensions/tabs#method-setZoomSettings
 * @param {number|!chrome.tabs.ZoomSettings} tabIdOrZoomSettings
 * @param {(!chrome.tabs.ZoomSettings|function(): void)=}
 *     opt_zoomSettingsOrCallback
 * @param {function(): void=} opt_callback
 */
chrome.tabs.setZoomSettings = function(
    tabIdOrZoomSettings, opt_zoomSettingsOrCallback, opt_callback) {};


/**
 * @typedef {?{
 *   url: (string|undefined),
 *   active: (boolean|undefined),
 *   highlighted: (boolean|undefined),
 *   pinned: (boolean|undefined),
 *   openerTabId: (number|undefined)
 * }}
 */
chrome.tabs.UpdateProperties;


/**
 * @see https://developer.chrome.com/extensions/tabs#method-update
 * @param {number|!chrome.tabs.UpdateProperties} tabIdOrUpdateProperties
 *     Either the id of the tab to update, or an object with new property
 *     values, in which case the selected tab of the current window will be
 *     updated.
 * @param {(!chrome.tabs.UpdateProperties|function(Tab):void)=}
 *     opt_updatePropertiesOrCallback Either an object with new property values,
 *     if the tabId was specified as first parameter, or an optional callback
 *     that will be invoked with information about the tab being updated.
 * @param {function(!Tab=): void=} opt_callback An optional callback that will
 *     be invoked with information about the tab being updated.
 * @return {undefined}
 */
chrome.tabs.update = function(
    tabIdOrUpdateProperties, opt_updatePropertiesOrCallback, opt_callback) {};


/**
 * @type {!ChromeEvent}
 * @deprecated Please use tabs.onActivated.
 */
chrome.tabs.onActiveChanged;


/** @type {!ChromeEvent} */
chrome.tabs.onActivated;


/** @type {!ChromeEvent} */
chrome.tabs.onAttached;


/** @type {!ChromeEvent} */
chrome.tabs.onCreated;


/** @type {!ChromeEvent} */
chrome.tabs.onDetached;


/**
 * @type {!ChromeEvent}
 * @deprecated Please use tabs.onHighlighted.
 */
chrome.tabs.onHighlightChanged;


/**
 * @type {!ChromeEvent}
 */
chrome.tabs.onHighlighted;


/** @type {!ChromeEvent} */
chrome.tabs.onMoved;


/** @type {!ChromeEvent} */
chrome.tabs.onRemoved;


/** @type {!ChromeEvent} */
chrome.tabs.onUpdated;


/** @type {!ChromeEvent} */
chrome.tabs.onReplaced;

// DEPRECATED:
// TODO(user): Remove once all usage has been confirmed to have ended.


/**
 * @type {!ChromeEvent}
 * @deprecated Please use tabs.onActivated.
 */
chrome.tabs.onSelectionChanged;


/**
 * @see https://developer.chrome.com/extensions/tabs#event-onZoomChange
 * @type {!ChromeObjectEvent}
 */
chrome.tabs.onZoomChange;


/**
 * @see https://developer.chrome.com/extensions/topSites
 * @const
 */
chrome.topSites = {};



/**
 * @constructor
 * @see https://developer.chrome.com/extensions/topSites#type-MostVisitedURL
 */
chrome.topSites.MostVisitedURL = function() {};


/** @type {string} */
chrome.topSites.MostVisitedURL.prototype.url;


/** @type {string} */
chrome.topSites.MostVisitedURL.prototype.title;


/**
 * Gets a list of top sites.
 * @param {function(!Array<!chrome.topSites.MostVisitedURL>)} callback Invoked
 *     with a list of most visited URLs.
 * @see https://developer.chrome.com/extensions/topSites#method-get
 * @return {undefined}
 */
chrome.topSites.get = function(callback) {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/windows.html
 */
chrome.windows = {};


/**
 * @param {Object=} opt_createData May have many keys to specify parameters.
 *     Or the callback.
 * @param {function(ChromeWindow): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.windows.create = function(opt_createData, opt_callback) {};


/**
 * @param {number} id Window id.
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(!ChromeWindow): void=} opt_callback Callback when
 *     opt_getInfo is an object.
 * @return {undefined}
 */
chrome.windows.get = function(id, opt_getInfo, opt_callback) {};


/**
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(!Array<!ChromeWindow>): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.windows.getAll = function(opt_getInfo, opt_callback) {};


/**
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(ChromeWindow): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.windows.getCurrent = function(opt_getInfo, opt_callback) {};


/**
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(ChromeWindow): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.windows.getLastFocused = function(opt_getInfo, opt_callback) {};


/**
 * @param {number} tabId Tab Id.
 * @param {function(): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.windows.remove = function(tabId, opt_callback) {};


/**
 * @param {number} tabId Tab Id.
 * @param {Object} updateProperties An object which may have many keys for
 *     various options.
 * @param {function(!ChromeWindow): void=} opt_callback Callback.
 * @return {undefined}
 */
chrome.windows.update = function(tabId, updateProperties, opt_callback) {};


/** @type {!ChromeEvent} */
chrome.windows.onCreated;


/** @type {!ChromeEvent} */
chrome.windows.onFocusChanged;


/** @type {!ChromeEvent} */
chrome.windows.onRemoved;


/**
 * @see https://developer.chrome.com/extensions/windows.html#property-WINDOW_ID_NONE
 * @type {number}
 */
chrome.windows.WINDOW_ID_NONE;


/**
 * @see https://developer.chrome.com/extensions/windows.html#property-WINDOW_ID_CURRENT
 * @type {number}
 */
chrome.windows.WINDOW_ID_CURRENT;


/**
 * @const
 * @see https://developer.chrome.com/extensions/i18n.html
 */
chrome.i18n = {};


/**
 * @param {function(Array<string>): void} callback The callback function which
 *     accepts an array of the accept languages of the browser, such as
 *     'en-US','en','zh-CN'.
 * @return {undefined}
 */
chrome.i18n.getAcceptLanguages = function(callback) {};


/**
 * @param {string} messageName
 * @param {(string|Array<string>)=} opt_args
 * @param {{escapeLt: boolean}=} opt_options
 * @return {string}
 * @see https://developer.chrome.com/extensions/i18n#method-getMessage
 */
chrome.i18n.getMessage = function(messageName, opt_args, opt_options) {};


/**
 * @return {string}
 */
chrome.i18n.getUILanguage = function() {};


/**
 * @param {string} text User input string to be detected.
 * @param {function(!Object)} callback The callback for passing back the
 *     language detection result.
 * @return {undefined}
 */
chrome.i18n.detectLanguage = function(text, callback) {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/pageAction.html
 */
chrome.pageAction = {};


/**
 * @param {number} tabId Tab Id.
 * @return {undefined}
 */
chrome.pageAction.hide = function(tabId) {};


/**
 * @param {Object} details An object which has 'tabId' and either
 *     'imageData' or 'path'.
 * @param {(function(): void)=} callback The callback function. If an error
 * occurs setting the icon, chrome.runtime.lastError will be set to the error
 * message.
 * @return {undefined}
 */
chrome.pageAction.setIcon = function(details, callback) {};


/**
 * @param {Object} details An object which may have 'popup' or 'tabId' as keys.
 * @return {undefined}
 */
chrome.pageAction.setPopup = function(details) {};


/**
 * @param {Object} details An object which has 'tabId' and 'title'.
 * @return {undefined}
 */
chrome.pageAction.setTitle = function(details) {};


/**
 * @param {number} tabId Tab Id.
 * @param {(function(): void)=} callback The callback function. If an error
 * occurs showing the pageAction, chrome.runtime.lastError will be set to the
 * error message.
 * @return {undefined}
 */
chrome.pageAction.show = function(tabId, callback) {};


/** @type {!ChromeEvent} */
chrome.pageAction.onClicked;


/**
 * @const
 * @see https://developer.chrome.com/apps/browser
 */
chrome.browser = {};


/**
 * @param {{url: string}} details An object with a single 'url' key.
 * @param {(function(): void)=} opt_callback The callback function. If an error
 * occurs opening the URL, chrome.runtime.lastError will be set to the error
 * message.
 * @return {undefined}
 */
chrome.browser.openTab = function(details, opt_callback) {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/browserAction.html
 */
chrome.browserAction = {};


/**
 * @typedef {?{
 *   tabId: (number|undefined)
 * }}
 */
chrome.browserAction.Tab;


/**
 * @typedef {Array<number>}
 * @see https://developer.chrome.com/extensions/browserAction#type-ColorArray
 */
chrome.browserAction.ColorArray;


/**
 * @typedef {{
 *   imageData: (!ImageData|!Object<number, !ImageData>|undefined),
 *   path: (string|!Object<number, string>|undefined),
 *   tabId: (number|undefined)
 * }}
 */
chrome.browserAction.SetIconImageData;


/**
 * @param {{
 *   title: string,
 *   tabId: (number|undefined)
 * }} details
 * @see https://developer.chrome.com/extensions/browserAction#method-setTitle
 * @return {undefined}
 */
chrome.browserAction.setTitle = function(details) {};


/**
 * @param {!chrome.browserAction.Tab} details
 * @param {function(string): void} callback
 * @see https://developer.chrome.com/extensions/browserAction#method-getTitle
 * @return {undefined}
 */
chrome.browserAction.getTitle = function(details, callback) {};


/**
 * @param {!chrome.browserAction.SetIconImageData} details
 * @param {function(): void=} opt_callback
 * @see https://developer.chrome.com/extensions/browserAction#method-setIcon
 * @return {undefined}
 */
chrome.browserAction.setIcon = function(details, opt_callback) {};


/**
 * @param {{
 *   tabId: (number|undefined),
 *   popup: string
 * }} details
 * @see https://developer.chrome.com/extensions/browserAction#method-setPopup
 * @return {undefined}
 */
chrome.browserAction.setPopup = function(details) {};


/**
 * @param {!chrome.browserAction.Tab} details
 * @param {function(string): void} callback
 * @see https://developer.chrome.com/extensions/browserAction#method-getPopup
 * @return {undefined}
 */
chrome.browserAction.getPopup = function(details, callback) {};


/**
 * @param {{
 *   text: string,
 *   tabId: (number|undefined)
 * }} details
 * @see https://developer.chrome.com/extensions/browserAction#method-setBadgeText
 * @return {undefined}
 */
chrome.browserAction.setBadgeText = function(details) {};


/**
 * @param {!chrome.browserAction.Tab} details
 * @param {function(string): void} callback
 * @see https://developer.chrome.com/extensions/browserAction#method-getBadgeText
 * @return {undefined}
 */
chrome.browserAction.getBadgeText = function(details, callback) {};


/**
 * @param {{
 *   color: (string|chrome.browserAction.ColorArray),
 *   tabId: (number|undefined)
 * }} details
 * @see https://developer.chrome.com/extensions/browserAction#method-setBadgeBackgroundColor
 * @return {undefined}
 */
chrome.browserAction.setBadgeBackgroundColor = function(details) {};


/**
 * @param {!chrome.browserAction.Tab} details
 * @param {function(chrome.browserAction.ColorArray): void} callback
 * @see https://developer.chrome.com/extensions/browserAction#method-getBadgeBackgroundColor
 * @return {undefined}
 */
chrome.browserAction.getBadgeBackgroundColor = function(details, callback) {};


/**
 * @param {number=} opt_tabId
 * @see https://developer.chrome.com/extensions/browserAction#method-enable
 * @return {undefined}
 */
chrome.browserAction.enable = function(opt_tabId) {};


/**
 * @param {number=} opt_tabId
 * @see https://developer.chrome.com/extensions/browserAction#method-disable
 * @return {undefined}
 */
chrome.browserAction.disable = function(opt_tabId) {};


/**
 * @interface
 * @extends {ChromeBaseEvent<function(!Tab)>}
 */
chrome.browserAction.BrowserActionTabEvent = function() {};


/**
 * @type {!chrome.browserAction.BrowserActionTabEvent}
 * @see https://developer.chrome.com/extensions/browserAction#event-onClicked
 */
chrome.browserAction.onClicked;


/**
 * @const
 * @see https://developer.chrome.com/extensions/bookmarks.html
 */
chrome.bookmarks = {};


/**
 * @typedef {?{
 *   parentId: (string|undefined),
 *   index: (number|undefined),
 *   url: (string|undefined),
 *   title: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/bookmarks#method-create
 */
chrome.bookmarks.CreateDetails;


/**
 * @typedef {?{
 *   query: (string|undefined),
 *   url: (string|undefined),
 *   title: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/bookmarks#method-search
 */
chrome.bookmarks.SearchDetails;


/**
 * @param {(string|Array<string>)} idOrIdList
 * @param {function(Array<BookmarkTreeNode>): void} callback The
 *     callback function which accepts an array of BookmarkTreeNode.
 * @return {Array<BookmarkTreeNode>}
 */
chrome.bookmarks.get = function(idOrIdList, callback) {};


/**
 * @param {string} id
 * @param {function(Array<BookmarkTreeNode>): void} callback The
 *     callback function which accepts an array of BookmarkTreeNode.
 * @return {Array<BookmarkTreeNode>}
 */
chrome.bookmarks.getChildren = function(id, callback) {};


/**
 * @param {number} numberOfItems The number of items to return.
 * @param {function(Array<BookmarkTreeNode>): void} callback The
 *     callback function which accepts an array of BookmarkTreeNode.
 * @return {Array<BookmarkTreeNode>}
 */
chrome.bookmarks.getRecent = function(numberOfItems, callback) {};


/**
 * @param {function(Array<BookmarkTreeNode>): void} callback The
 *     callback function which accepts an array of BookmarkTreeNode.
 * @return {Array<BookmarkTreeNode>}
 */
chrome.bookmarks.getTree = function(callback) {};


/**
 * @param {string} id The ID of the root of the subtree to retrieve.
 * @param {function(Array<BookmarkTreeNode>): void} callback The
 *     callback function which accepts an array of BookmarkTreeNode.
 * @return {Array<BookmarkTreeNode>}
 */
chrome.bookmarks.getSubTree = function(id, callback) {};


/**
 * @param {string|!chrome.bookmarks.SearchDetails} query
 * @param {function(Array<BookmarkTreeNode>): void} callback
 * @return {Array<BookmarkTreeNode>}
 */
chrome.bookmarks.search = function(query, callback) {};


/**
 * @param {chrome.bookmarks.CreateDetails} bookmark
 * @param {function(BookmarkTreeNode): void=} opt_callback The
 *     callback function which accepts a BookmarkTreeNode object.
 * @return {undefined}
 */
chrome.bookmarks.create = function(bookmark, opt_callback) {};


/**
 * @param {string} id
 * @param {Object} destination An object which has optional 'parentId' and
 *     optional 'index'.
 * @param {function(BookmarkTreeNode): void=} opt_callback
 *     The callback function which accepts a BookmarkTreeNode object.
 * @return {undefined}
 */
chrome.bookmarks.move = function(id, destination, opt_callback) {};


/**
 * @param {string} id
 * @param {Object} changes An object which may have 'title' as a key.
 * @param {function(BookmarkTreeNode): void=} opt_callback The
 *     callback function which accepts a BookmarkTreeNode object.
 * @return {undefined}
 */
chrome.bookmarks.update = function(id, changes, opt_callback) {};


/**
 * @param {string} id
 * @param {function(): void=} opt_callback
 * @return {undefined}
 */
chrome.bookmarks.remove = function(id, opt_callback) {};


/**
 * @param {string} id
 * @param {function(): void=} opt_callback
 * @return {undefined}
 */
chrome.bookmarks.removeTree = function(id, opt_callback) {};


/**
 * @param {function(): void=} opt_callback
 * @return {undefined}
 */
chrome.bookmarks.import = function(opt_callback) {};


/**
 * @param {function(): void=} opt_callback
 * @return {undefined}
 */
chrome.bookmarks.export = function(opt_callback) {};


/** @type {!ChromeEvent} */
chrome.bookmarks.onChanged;


/** @type {!ChromeEvent} */
chrome.bookmarks.onChildrenReordered;


/** @type {!ChromeEvent} */
chrome.bookmarks.onCreated;


/** @type {!ChromeEvent} */
chrome.bookmarks.onImportBegan;


/** @type {!ChromeEvent} */
chrome.bookmarks.onImportEnded;


/** @type {!ChromeEvent} */
chrome.bookmarks.onMoved;


/** @type {!ChromeEvent} */
chrome.bookmarks.onRemoved;


/**
 * @typedef {?{
 *   content: string,
 *   description: string
 * }}
 */
var SuggestResult;


/**
 * @const
 * @see https://developer.chrome.com/extensions/omnibox.html
 */
chrome.omnibox = {};



/**
 * @interface
 * @extends {ChromeBaseEvent<function(string,
 * function(!Array<!SuggestResult>))>}
 */
chrome.omnibox.InputChangedEvent = function() {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/omnibox#type-OnInputEnteredDisposition
 */
chrome.omnibox.OnInputEnteredDisposition = {
  CURRENT_TAB: '',
  NEW_BACKGROUND_TAB: '',
  NEW_FOREGROUND_TAB: '',
};


/**
 * @interface
 * @extends {ChromeBaseEvent<
 * function(string,!chrome.omnibox.OnInputEnteredDisposition)>}
 */
chrome.omnibox.InputEnteredEvent = function() {};


/**
 * @param {{description: string}} suggestion A partial SuggestResult object.
 * @return {undefined}
 */
chrome.omnibox.setDefaultSuggestion = function(suggestion) {};


/** @type {!ChromeEvent} */
chrome.omnibox.onInputCancelled;


/** @type {!chrome.omnibox.InputChangedEvent} */
chrome.omnibox.onInputChanged;


/** @type {!chrome.omnibox.InputEnteredEvent} */
chrome.omnibox.onInputEntered;


/** @type {!ChromeEvent} */
chrome.omnibox.onInputStarted;


/**
 * @const
 * @see https://developer.chrome.com/extensions/dev/contextMenus.html
 */
chrome.contextMenus = {};


/**
 * @typedef {?{
 *   type: (string|undefined),
 *   id: (string|undefined),
 *   title: (string|undefined),
 *   checked: (boolean|undefined),
 *   contexts: (!Array<string>|undefined),
 *   onclick: (function(!Object, !Tab)|undefined),
 *   parentId: (number|string|undefined),
 *   documentUrlPatterns: (!Array<string>|undefined),
 *   targetUrlPatterns: (!Array<string>|undefined),
 *   enabled: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/contextMenus#method-create
 */
chrome.contextMenus.CreateProperties;


/**
 * @typedef {?{
 *   type: (string|undefined),
 *   title: (string|undefined),
 *   checked: (boolean|undefined),
 *   contexts: (!Array<string>|undefined),
 *   onclick: (function(!Object, !Tab)|undefined),
 *   parentId: (number|string|undefined),
 *   documentUrlPatterns: (!Array<string>|undefined),
 *   targetUrlPatterns: (!Array<string>|undefined),
 *   enabled: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/contextMenus#method-update
 */
chrome.contextMenus.UpdateProperties;


/**
 * @param {!chrome.contextMenus.CreateProperties} createProperties
 * @param {function()=} opt_callback
 * @return {(number|string)} The id of the newly created window.
 * @see https://developer.chrome.com/extensions/contextMenus#method-create
 */
chrome.contextMenus.create = function(createProperties, opt_callback) {};


/**
 * @param {(number|string)} id
 * @param {!chrome.contextMenus.UpdateProperties} updateProperties
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/extensions/contextMenus#method-update
 * @return {undefined}
 */
chrome.contextMenus.update = function(id, updateProperties, opt_callback) {};


/**
 * @param {(number|string)} menuItemId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/extensions/contextMenus#method-remove
 * @return {undefined}
 */
chrome.contextMenus.remove = function(menuItemId, opt_callback) {};


/**
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/extensions/contextMenus#method-removeAll
 * @return {undefined}
 */
chrome.contextMenus.removeAll = function(opt_callback) {};


/**
 * @interface
 * @extends {ChromeBaseEvent<function(!OnClickData, !Tab=)>}
 * @see https://developer.chrome.com/extensions/contextMenus#event-onClicked
 */
chrome.contextMenus.ClickedEvent = function() {};


/**
 * @type {!chrome.contextMenus.ClickedEvent}
 * @see https://developer.chrome.com/extensions/contextMenus#event-onClicked
 */
chrome.contextMenus.onClicked;


/**
 * @const
 * @see https://developer.chrome.com/extensions/dev/cookies.html
 */
chrome.cookies = {};


/**
 * This typedef is used for the parameters to chrome.cookies.get,
 * chrome.cookies.remove, and for the parameter to remove's callback. These uses
 * all identify a single cookie uniquely without specifying its content, and the
 * objects are identical except for the storeId being optional vs required.
 * If greater divergence occurs, then going to two typedefs is recommended.
 *
 * @typedef {?{
 *   url: string,
 *   name: string,
 *   storeId: (string|undefined)
 * }}
 */
chrome.cookies.CookieIdentifier;


/**
 * @param {!chrome.cookies.CookieIdentifier} details
 * @param {function(Cookie=): void} callback
 * @return {undefined}
 */
chrome.cookies.get = function(details, callback) {};


/**
 * @param {Object} details
 * @param {function(Array<Cookie>): void} callback
 * @return {undefined}
 */
chrome.cookies.getAll = function(details, callback) {};


/**
 * @param {function(Array<CookieStore>): void} callback
 * @return {undefined}
 */
chrome.cookies.getAllCookieStores = function(callback) {};


/**
 * @param {!chrome.cookies.CookieIdentifier} details
 * @param {function(chrome.cookies.CookieIdentifier): void=} opt_callback If
 *     removal failed for any reason, the parameter will be "null", and
 *     "chrome.runtime.lastError" will be set.
 * @return {undefined}
 */
chrome.cookies.remove = function(details, opt_callback) {};


/**
 * @typedef {?{
 *   url: string,
 *   name: (string|undefined),
 *   value: (string|undefined),
 *   domain: (string|undefined),
 *   path: (string|undefined),
 *   secure: (boolean|undefined),
 *   httpOnly: (boolean|undefined),
 *   expirationDate: (number|undefined),
 *   storeId: (string|undefined)
 * }}
 */
chrome.cookies.CookieSetDetails;


/**
 * @param {!chrome.cookies.CookieSetDetails} details
 * @param {function(Cookie): void=} opt_callback If setting failed for any
 *    reason, the parameter will be "null", and "chrome.runtime.lastError" will
 *    be set.
 * @return {undefined}
 */
chrome.cookies.set = function(details, opt_callback) {};


/**
 * @see https://developer.chrome.com/extensions/cookies.html#event-onChanged
 * @type {!ChromeEvent}
 */
chrome.cookies.onChanged;



/** @constructor */
function CookieChangeInfo() {}


/** @type {boolean} */
CookieChangeInfo.prototype.removed;


/** @type {Cookie} */
CookieChangeInfo.prototype.cookie;


/** @type {string} */
CookieChangeInfo.prototype.cause;


/**
 * @const
 * @see https://developer.chrome.com/extensions/management
 */
chrome.management = {};


/**
 * @typedef {?{
 *   showConfirmDialog: (boolean|undefined)
 * }}
 */
chrome.management.InstallOptions;


/**
 * @param {function(!Array<!ExtensionInfo>): void=} opt_callback Optional
 *     callback function.
 * @return {undefined}
 */
chrome.management.getAll = function(opt_callback) {};


/**
 * @param {string} id
 * @param {function(!ExtensionInfo): void=} opt_callback Optional callback
 *     function.
 * @return {undefined}
 */
chrome.management.get = function(id, opt_callback) {};


/**
 * @param {function(!ExtensionInfo): void=} opt_callback Optional
 *     callback function
 * @return {undefined}
 */
chrome.management.getSelf = function(opt_callback) {};


/**
 * @param {string} id The id of an already installed extension.
 * @param {function(!Array<string>)=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.getPermissionWarningsById = function(id, opt_callback) {};


/**
 * @param {string} manifestStr Extension's manifest JSON string.
 * @param {function(!Array<string>)=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.getPermissionWarningsByManifest = function(
    manifestStr, opt_callback) {};


/**
 * @param {string} id The id of an already installed extension.
 * @param {boolean} enabled Whether this item should be enabled.
 * @param {function(): void=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.setEnabled = function(id, enabled, opt_callback) {};


/**
 * @param {string} id The id of an already installed extension.
 * @param {(!chrome.management.InstallOptions|function(): void)=}
 *     opt_optionsOrCallback An optional uninstall options object or an optional
 *     callback function.
 * @param {function(): void=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.uninstall = function(
    id, opt_optionsOrCallback, opt_callback) {};


/**
 * @param {(!chrome.management.InstallOptions|function(): void)=}
 *     opt_optionsOrCallback An optional uninstall options object or an optional
 *     callback function.
 * @param {function(): void=} opt_callback An optional callback function.
 * @return {undefined}
 */
chrome.management.uninstallSelf = function(
    opt_optionsOrCallback, opt_callback) {};


/**
 * @param {string} id The id of an already installed extension.
 * @param {function(): void=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.launchApp = function(id, opt_callback) {};


/**
 * @param {string} id The id of an already installed extension.
 * @param {function(): void=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.createAppShortcut = function(id, opt_callback) {};


/**
 * @param {string} id The id of an already installed extension.
 * @param {string} launchType The LaunchType enum value to set. Make sure this
 *     value is in ExtensionInfo.availableLaunchTypes because the available
 *     launch types vary on different platforms and configurations.
 * @param {function(): void=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.management.setLaunchType = function(id, launchType, opt_callback) {};


/**
 * @param {string} url The URL of a web page. The scheme of the URL can only be
 *     "http" or "https".
 * @param {string} title The title of the generated app.
 * @param {function(!ExtensionInfo): void=} opt_callback Optional callback
 *     function.
 * @return {undefined}
 */
chrome.management.generateAppForLink = function(url, title, opt_callback) {};


/**
 * @param {function():void=} callback
 * @return {undefined}
 */
chrome.management.installReplacementWebApp = function(callback) {};

/**
 * Event whose listeners take an ExtensionInfo parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!ExtensionInfo)>}
 */
function ChromeExtensionInfoEvent() {};


/** @type {!ChromeExtensionInfoEvent} */
chrome.management.onInstalled;


/** @type {!ChromeStringEvent} */
chrome.management.onUninstalled;


/** @type {!ChromeExtensionInfoEvent} */
chrome.management.onEnabled;


/** @type {!ChromeExtensionInfoEvent} */
chrome.management.onDisabled;


/**
 * @const
 * @see https://developer.chrome.com/extensions/idle.html
 */
chrome.idle = {};


/**
 * @param {number} thresholdSeconds Threshold in seconds, used to determine
 *     when a machine is in the idle state.
 * @param {function(string): void} callback Callback to handle the state.
 * @return {undefined}
 */
chrome.idle.queryState = function(thresholdSeconds, callback) {};


/**
 * @param {number} intervalInSeconds Threshold, in seconds, used to determine
 *    when the system is in an idle state.
 * @return {undefined}
 */
chrome.idle.setDetectionInterval = function(intervalInSeconds) {};

/**
 * @param {function(number): void} callback Callback to receive the delay
 * @return {undefined}
 */
chrome.idle.getAutoLockDelay = function(callback) {};

/** @type {!ChromeEvent} */
chrome.idle.onStateChanged;


/**
 * @const
 * @see https://developer.chrome.com/extensions/contentSettings.html
 */
chrome.contentSettings = {};


/** @type {!ContentSetting} */
chrome.contentSettings.cookies;


/** @type {!ContentSetting} */
chrome.contentSettings.images;


/** @type {!ContentSetting} */
chrome.contentSettings.javascript;


/** @type {!ContentSetting} */
chrome.contentSettings.plugins;


/** @type {!ContentSetting} */
chrome.contentSettings.popups;


/** @type {!ContentSetting} */
chrome.contentSettings.notifications;


/**
 * @const
 * @see https://developer.chrome.com/extensions/fileBrowserHandler
 */
chrome.fileBrowserHandler = {};


/**
 * @typedef {?{
 *   suggestedName: string,
 *   allowedFileExtensions: (!Array<string>|undefined)
 * }}
 */
chrome.fileBrowserHandler.SelectFileSelectionParams;


/**
 * Prompts user to select file path under which file should be saved.
 * @see https://developer.chrome.com/extensions/fileBrowserHandler#method-selectFile
 * @param {!chrome.fileBrowserHandler.SelectFileSelectionParams} selectionParams
 *     Parameters that will be used while selecting the file.
 * @param {function(!Object)} callback Function called upon completion.
 * @return {undefined}
 */
chrome.fileBrowserHandler.selectFile = function(selectionParams, callback) {};


/**
 * @interface
 * @extends {ChromeBaseEvent<function(string, !FileHandlerExecuteEventDetails)>}
 * @see https://developer.chrome.com/extensions/fileBrowserHandler#event-onExecute
 */
chrome.fileBrowserHandler.ExecuteEvent = function() {};


/**
 * Fired when file system action is executed from ChromeOS file browser.
 * @see https://developer.chrome.com/extensions/fileBrowserHandler#event-onExecute
 * @type {!chrome.fileBrowserHandler.ExecuteEvent}
 */
chrome.fileBrowserHandler.onExecute;


/**
 * @const
 * @see https://developer.chrome.com/extensions/gcm
 */
chrome.gcm = {};


/**
 * @see https://developer.chrome.com/extensions/gcm#property-MAX_MESSAGE_SIZE
 * @type {number}
 */
chrome.gcm.MAX_MESSAGE_SIZE;


/**
 * Registers the application with GCM. The registration ID will be returned by
 * the callback. If register is called again with the same list of senderIds,
 * the same registration ID will be returned.
 * @see https://developer.chrome.com/extensions/gcm#method-register
 * @param {!Array<string>} senderIds A list of server IDs that are allowed to
 *     send messages to the application.
 * @param {function(string): void} callback Function called when
 *     registration completes with registration ID as argument.
 * @return {undefined}
 */
chrome.gcm.register = function(senderIds, callback) {};


/**
 * Unregisters the application from GCM.
 * @see https://developer.chrome.com/extensions/gcm#method-unregister
 * @param {function(): void} callback Called when unregistration is done.
 * @return {undefined}
 */
chrome.gcm.unregister = function(callback) {};


/**
 * Sends an upstream message using GCM.
 * @see https://developer.chrome.com/extensions/gcm#method-send
 * @param {!chrome.gcm.Message} message Message to be sent.
 * @param {function(string): void} callback Called with message ID.
 * @return {undefined}
 */
chrome.gcm.send = function(message, callback) {};


/**
 * Outgoing message.
 * @typedef {?{
 *   destinationId: string,
 *   messageId: string,
 *   timeToLive: (number|undefined),
 *   data: !Object<string, string>
 * }}
 */
chrome.gcm.Message;


/**
 * An event, fired when a message is received through GCM.
 * @see https://developer.chrome.com/extensions/gcm#event-onMessage
 * @type {!chrome.gcm.OnMessageEvent}
 */
chrome.gcm.onMessage;


/**
 * An event, fired when GCM server had to delete messages to the application
 * from its queue in order to manage its size.
 * @see https://developer.chrome.com/extensions/gcm#event-onMessagesDeleted
 * @type {!ChromeEvent}
 */
chrome.gcm.onMessagesDeleted;


/**
 * An event indicating problems with sending messages.
 * @see https://developer.chrome.com/extensions/gcm#event-onSendError
 * @type {!chrome.gcm.OnSendErrorEvent}
 */
chrome.gcm.onSendError;



/**
 * @interface
 * @extends {ChromeBaseEvent<function(!Object)>}
 */
chrome.gcm.OnMessageEvent = function() {};


/**
 * @interface
 * @extends {ChromeBaseEvent<function(!Object)>}
 */
chrome.gcm.OnSendErrorEvent = function() {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/history.html
 */
chrome.history = {};


/**
 * @param {Object<string, string>} details Object with a 'url' key.
 * @return {undefined}
 */
chrome.history.addUrl = function(details) {};


/**
 * @param {function(): void} callback Callback function.
 * @return {undefined}
 */
chrome.history.deleteAll = function(callback) {};


/**
 * @param {Object<string, string>} range Object with 'startTime'
 *     and 'endTime' keys.
 * @param {function(): void} callback Callback function.
 * @return {undefined}
 */
chrome.history.deleteRange = function(range, callback) {};


/**
 * @param {Object<string, string>} details Object with a 'url' key.
 * @return {undefined}
 */
chrome.history.deleteUrl = function(details) {};


/**
 * @param {Object<string, string>} details Object with a 'url' key.
 * @param {function(!Array<!VisitItem>): void} callback Callback function.
 * @return {!Array<!VisitItem>}
 */
chrome.history.getVisits = function(details, callback) {};


/**
 * @param {Object<string, string>} query Object with a 'text' (string)
 *     key and optional 'startTime' (number), 'endTime' (number) and
 *     'maxResults' keys.
 * @param {function(!Array<!HistoryItem>): void} callback Callback function.
 * @return {!Array<!HistoryItem>}
 */
chrome.history.search = function(query, callback) {};


/** @type {!ChromeEvent} */
chrome.history.onVisitRemoved;


/** @type {!ChromeEvent} */
chrome.history.onVisited;


/**
 * @const
 * @see http://developer.chrome.com/apps/identity.html
 */
chrome.identity = {};


/** @typedef {?{id: string}} */
chrome.identity.AccountInfo;


/**
 * @param {function(!Array<!chrome.identity.AccountInfo>): void} callback
 * @return {undefined}
 */
chrome.identity.getAccounts = function(callback) {};


/**
 * @param {(!chrome.identity.TokenDetails|function(string=): void)}
 *     detailsOrCallback Token options or a callback function if no options are
 *     specified.
 * @param {function(string=): void=} opt_callback A callback function if options
 *     are specified.
 * @return {undefined}
 */
chrome.identity.getAuthToken = function(detailsOrCallback, opt_callback) {};


/**
 * @typedef {?{
 *   interactive: (boolean|undefined),
 *   account: (!chrome.identity.AccountInfo|undefined),
 *   scopes: (!Array<string>|undefined)
 * }}
 */
chrome.identity.TokenDetails;


/**
 * @param {!chrome.identity.InvalidTokenDetails} details
 * @param {function(): void=} opt_callback
 * @return {undefined}
 */
chrome.identity.removeCachedAuthToken = function(details, opt_callback) {};


/** @typedef {?{token: string}} */
chrome.identity.InvalidTokenDetails;


/**
 * @param {!chrome.identity.WebAuthFlowDetails} details
 * @param {function(string=): void} callback
 * @return {undefined}
 */
chrome.identity.launchWebAuthFlow = function(details, callback) {};


/** @typedef {?{url: string, interactive: (boolean|undefined)}} */
chrome.identity.WebAuthFlowDetails;


/** @typedef {?{id: string, email: string}} */
chrome.identity.ProfileUserInfo;

/**
 * @enum {string}
 * See https://developer.chrome.com/docs/extensions/reference/identity/#type-AccountStatus
 */
chrome.identity.AccountStatus = {
  SYNC: '',
  ANY: '',
};

/**
 * See https://developer.chrome.com/docs/extensions/reference/identity/#type-ProfileDetails
 * @typedef {{accountStatus: (!chrome.identity.AccountStatus|undefined)}}
 */
chrome.identity.ProfileDetails;

/**
 * @param {!chrome.identity.ProfileDetails|function(!chrome.identity.ProfileUserInfo):void} accountStatusOrCallback
 *     Either the accountStatus of the primary profile account or the callback
 * @param {function(!chrome.identity.ProfileUserInfo):void=} opt_callback if
 *     the accountStatus is provided
 * @return {undefined}
 */
chrome.identity.getProfileUserInfo = function(accountStatusOrCallback, opt_callback) {};



/**
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.identity.AccountInfo, boolean)>}
 */
chrome.identity.OnSignInChangedEvent = function() {};


/** @type {!chrome.identity.OnSignInChangedEvent} */
chrome.identity.onSignInChanged;


/**
 * @param {string=} opt_path
 * @return {string}
 */
chrome.identity.getRedirectURL = function(opt_path) {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/input.ime.html
 */
chrome.input = {};


/** @const */
chrome.input.ime = {};


/**
 * @typedef {?{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 * @see https://developer.chrome.com/extensions/input_ime#type-CreateWindowOptions
 */
chrome.input.ime.Bounds;


/**
 * @typedef {?{
 *   windowType: chrome.input.ime.WindowType,
 *   url: (string|undefined),
 *   bounds: (chrome.input.ime.Bounds|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/input_ime#type-CreateWindowOptions
 */
chrome.input.ime.CreateWindowOptions;


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/system.display#type-MirrorMode
 */
chrome.input.ime.WindowType = {
  NORMAL: '',
  FOLLOW_CURSOR: '',
};


/**
 * The OnKeyEvent event takes an extra argument.
 *
 * The addListener() method for this class takes more than one argument, so it
 * isn't possible to just extend ChromeBaseEvent here.
 * @interface
 * @extends {ChromeBaseEventNoListeners}
 */
function ChromeInputImeOnKeyEventEvent() {}


/**
 * @param {function(string, !ChromeKeyboardEvent): (boolean|undefined)} callback
 *     callback.
 * @param {Array<string>=} opt_extraInfoSpec Array of extra information.
 * @return {undefined}
 */
ChromeInputImeOnKeyEventEvent.prototype.addListener = function(
    callback, opt_extraInfoSpec) {};


/**
 * @param {function(string, !ChromeKeyboardEvent): (boolean|undefined)} callback
 *     callback.
 * @return {undefined}
 */
ChromeInputImeOnKeyEventEvent.prototype.removeListener = function(callback) {};


/**
 * @param {function(string, !ChromeKeyboardEvent): (boolean|undefined)} callback
 *     callback.
 * @return {boolean}
 */
ChromeInputImeOnKeyEventEvent.prototype.hasListener = function(callback) {};


/**
 * @return {boolean}
 */
ChromeInputImeOnKeyEventEvent.prototype.hasListeners = function() {};


/**
 * @param {function(boolean): void=} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.activate = function(callback) {};


/**
 * @param {!Object<string,number>} parameters An object with a
 *     'contextID' (number) key.
 * @param {function(boolean): void=} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.clearComposition = function(parameters, callback) {};


/**
 * @param {!Object<string,(string|number)>} parameters An object with
 *     'contextID' (number) and 'text' (string) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.commitText = function(parameters, opt_callback) {};


/**
 * @param {!chrome.input.ime.CreateWindowOptions} options Options of the created
 *     IME window.
 * @param {function(Window): void=} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.createWindow = function(options, callback) {};


/**
 * @param {function(boolean): void} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.deactivate = function(callback) {};


/**
 * @param {!Object<string,(string|number)>} parameters An object with
 *     'contextID' (number) and 'text' (string) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.deleteSurroundingText = function(parameters, opt_callback) {};


/**
 * @return {undefined}
 */
chrome.input.ime.hideInputView = function() {};


/**
 * @param {function(boolean): void=} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.hideWindow = function(callback) {};


/**
 * @param {!Object<number,Array<KeyboardEvent>>}
 *     parameters An object with 'contextID' (number) and 'keyData'
 *     (Array<KeyboardEvent>) keys.
 * @param {function(boolean): void=} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.sendKeyEvents = function(parameters, callback) {};


/**
 * @param {!Object<string,(number|Object<string,(string|number|boolean)>)>}
 *     parameters An object with 'engineID' (string) and 'properties'
 *     (Object) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.setCandidateWindowProperties = function(
    parameters, opt_callback) {};


/**
 * @param {!Object<string,(number|Object<string,(string|number)>)>}
 *     parameters An object with 'contextID' (number) and 'candidates'
 *     (array of object) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.setCandidates = function(parameters, opt_callback) {};


/**
 * @param {!Object<string,(string|number|Object<string,(string|number)>)>}
 *     parameters An object with 'contextID' (number), 'text' (string),
 *     'selectionStart (number), 'selectionEnd' (number), 'cursor' (number),
 *     and 'segments' (array of object) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.setComposition = function(parameters, opt_callback) {};


/**
 * @param {!Object<string,number>} parameters An object with
 *     'contextID' (number) and 'candidateID' (number) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.setCursorPosition = function(parameters, opt_callback) {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/input.ime#type-AssistiveWindowType
 */
chrome.input.ime.AssistiveWindowType = {
  UNDO: '',
};


/**
 * Properties of the assistive window.
 * @typedef {{
 *   type: !chrome.input.ime.AssistiveWindowType,
 *   visible: boolean
 * }}
 * @see https://developer.chrome.com/extensions/input.ime#type-AssistiveWindowProperties
 */
chrome.input.ime.AssistiveWindowProperties;


/**
 * Shows/Hides an assistive window with the given properties.
 * @param {{
 *   contextID: number,
 *   properties: !chrome.input.ime.AssistiveWindowProperties
 * }} parameters
 * @param {function(boolean): void=} callback Called when the operation
 *     completes.
 * @see https://developer.chrome.com/extensions/input.ime#method-setAssistiveWindowProperties
 */
chrome.input.ime.setAssistiveWindowProperties = function(
    parameters, callback) {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/input.ime#type-AssistiveWindowButton
 */
chrome.input.ime.AssistiveWindowButton = {
  UNDO: '',
  ADD_TO_DICTIONARY: '',
};


/**
 * This event is sent when a button in an assistive window is clicked.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/input.ime#event-onAssistiveWindowButtonClicked
 */
chrome.input.ime.onAssistiveWindowButtonClicked;


/**
 * @param {!Object<string,(string|Array<Object<string,(string|boolean)>>)>}
 *     parameters An object with 'engineID' (string) and 'items'
 *     (array of object) keys.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.setMenuItems = function(parameters, opt_callback) {};


/**
 * @param {function(boolean): void=} callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.showWindow = function(callback) {};


/**
 * @param {!Object<string,(string|Array<Object<string,(string|boolean)>>)>}
 *     parameters An object with  'engineID' (string) and 'items'
 *     (array of object) keys.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.input.ime.updateMenuItems = function(parameters, opt_callback) {};


/**
 * @param {string} requestId Request id of the event that was handled. This
 *     should come from keyEvent.requestId.
 * @param {boolean} response True if the keystroke was handled, false if not.
 * @return {undefined}
 */
chrome.input.ime.keyEventHandled = function(requestId, response) {};


/** @type {!ChromeEvent} */
chrome.input.ime.onActivate;


/** @type {!ChromeEvent} */
chrome.input.ime.onBlur;


/** @type {!ChromeEvent} */
chrome.input.ime.onCandidateClicked;


/** @type {!ChromeEvent} */
chrome.input.ime.onDeactivated;


/** @type {!ChromeEvent} */
chrome.input.ime.onFocus;


/** @type {!ChromeEvent} */
chrome.input.ime.onInputContextUpdate;


/** @type {!ChromeInputImeOnKeyEventEvent} */
chrome.input.ime.onKeyEvent;


/** @type {!ChromeEvent} */
chrome.input.ime.onMenuItemActivated;


/** @type {!ChromeEvent} */
chrome.input.ime.onReset;


/** @type {!ChromeEvent} */
chrome.input.ime.onSurroundingTextChanged;


/**
 * namespace
 * @see http://developer.chrome.com/apps/mediaGalleries
 * @const
 */
chrome.mediaGalleries = {};


/**
 * @param {{interactive: (string|undefined)}|function(!Array<!FileSystem>)}
 *     detailsOrCallback A details object for whether the request should be
 *     interactive if permissions haven't been granted yet or the callback.
 * @param {function(!Array<!FileSystem>)=} opt_callback A success callback if
 *     no details were supplied as arg1.
 * @return {undefined}
 */
chrome.mediaGalleries.getMediaFileSystems = function(
    detailsOrCallback, opt_callback) {};


/**
 * @param {function(!Array<!FileSystem>, string)} callback Callback function.
 * @return {undefined}
 */
chrome.mediaGalleries.addUserSelectedFolder = function(callback) {};


/**
 * @param {string} galleryId ID of the media gallery.
 * @param {function()=} opt_callback Optional callback function.
 * @return {undefined}
 */
chrome.mediaGalleries.dropPermissionForMediaFileSystem = function(
    galleryId, opt_callback) {};


chrome.mediaGalleries.startMediaScan = function() {};


chrome.mediaGalleries.cancelMediaScan = function() {};


/**
 * @param {function(!Array<!FileSystem>)} callback Callback function.
 * @return {undefined}
 */
chrome.mediaGalleries.addScanResults = function(callback) {};


/**
 * @typedef {{
 *   name: string,
 *   galleryId: string,
 *   deviceId: (string|undefined),
 *   isRemovable: boolean,
 *   isMediaDevice: boolean,
 *   isAvailable: boolean
 * }}
 */
chrome.mediaGalleries.MediaFileSystemMetadata;


/**
 * @param {!FileSystem} mediaFileSystem The file system to get metadata for.
 * @return {!chrome.mediaGalleries.MediaFileSystemMetadata}
 */
chrome.mediaGalleries.getMediaFileSystemMetadata = function(mediaFileSystem) {};


/**
 * @param {function(!Array<!chrome.mediaGalleries.MediaFileSystemMetadata>)}
 *     callback Callback function.
 * @return {undefined}
 */
chrome.mediaGalleries.getAllMediaFileSystemMetadata = function(callback) {};


/**
 * @typedef {{
 *   mimeType: string,
 *   height: (number|undefined),
 *   width: (number|undefined),
 *   xResolution: (number|undefined),
 *   yResolution: (number|undefined),
 *   duration: (number|undefined),
 *   rotation: (number|undefined),
 *   cameraMake: (string|undefined),
 *   cameraModel: (string|undefined),
 *   exposureTimeSeconds: (number|undefined),
 *   flashFired: (boolean|undefined),
 *   fNumber: (number|undefined),
 *   focalLengthMm: (number|undefined),
 *   isoEquivalent: (number|undefined),
 *   album: (string|undefined),
 *   artist: (string|undefined),
 *   comment: (string|undefined),
 *   copyright: (string|undefined),
 *   disc: (number|undefined),
 *   genre: (string|undefined),
 *   language: (string|undefined),
 *   title: (string|undefined),
 *   track: (number|undefined),
 *   rawTags: !Array<!chrome.mediaGalleries.metadata.RawTag>,
 *   attachedImages: !Array<!Blob>
 * }}
 */
chrome.mediaGalleries.MetaData;


/** @const */
chrome.mediaGalleries.metadata = {};


/** @constructor */
chrome.mediaGalleries.metadata.RawTag = function() {};


/** @type {string} */
chrome.mediaGalleries.metadata.RawTag.prototype.type;


/** @type {!Object<string, string>} */
chrome.mediaGalleries.metadata.RawTag.prototype.tags;


/**
 * @param {!Blob} mediaFile The media file for which to get metadata.
 * @param {{metadataType: (string|undefined)}|
 *     function(!chrome.mediaGalleries.MetaData)} optionsOrCallback The options
 *     for the metadata to retrieve or the callback to invoke with the metadata.
 *     The metadataType should either be 'all' or 'mimeTypeOnly'. Defaults to
 *     'all' if the metadataType is omitted.
 * @param {function(!chrome.mediaGalleries.MetaData)=} opt_callback If options
 *     were passed as arg2, the callback to invoke with the metadata.
 * @return {undefined}
 */
chrome.mediaGalleries.getMetadata = function(
    mediaFile, optionsOrCallback, opt_callback) {};


/**
 * @typedef {function({galleryId: string, success: boolean}): void}
 */
chrome.mediaGalleries.AddGalleryWatchCallback;


/**
 * @param {string} galleryId The media gallery's ID.
 * @param {!chrome.mediaGalleries.AddGalleryWatchCallback} callback Fired with
 *     success or failure result.
 * @return {undefined}
 */
chrome.mediaGalleries.addGalleryWatch = function(galleryId, callback) {};


/**
 * @param {string} galleryId The media gallery's ID.
 * @return {undefined}
 */
chrome.mediaGalleries.removeGalleryWatch = function(galleryId) {};


/**
 * @param {function(!Array<string>): void} callback Callback function notifies
 *     which galleries are being watched.
 * @return {undefined}
 */
chrome.mediaGalleries.getAllGalleryWatch = function(callback) {};


chrome.mediaGalleries.removeAllGalleryWatch = function() {};



/**
 * @interface
 * @extends {ChromeBaseEvent<!chrome.mediaGalleries.GalleryChangeCallback>}
 */
chrome.mediaGalleries.GalleryChangeEvent = function() {};


/**
 * @typedef {function({type: string, galleryId: string}): void}
 */
chrome.mediaGalleries.GalleryChangeCallback;


/**
 * @type {!chrome.mediaGalleries.GalleryChangeEvent}
 */
chrome.mediaGalleries.onGalleryChanged;


/**
 * @typedef {{
 *   type: string,
 *   galleryCount: (number|undefined),
 *   audioCount: (number|undefined),
 *   imageCount: (number|undefined),
 *   videoCount: (number|undefined)
 * }}
 */
chrome.mediaGalleries.OnScanProgressDetails;



/**
 * Event whose listeners take a chrome.mediaGalleries.OnScanProgressDetails
 * parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.mediaGalleries.OnScanProgressDetails)>}
 */
chrome.mediaGalleries.ScanProgressEvent = function() {};


/** @type {!chrome.mediaGalleries.ScanProgressEvent} */
chrome.mediaGalleries.onScanProgress;


/**
 * @const
 * @see https://developer.chrome.com/extensions/pageCapture.html
 */
chrome.pageCapture = {};


/**
 * @param {Object<string, number>} details Object with a 'tabId' (number) key.
 * @param {function(Blob=): void} callback Callback function.
 * @return {undefined}
 */
chrome.pageCapture.saveAsMHTML = function(details, callback) {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/permissions.html
 */
chrome.permissions = {};


/**
 * @typedef {{
 *   permissions: (Array<string>|undefined),
 *   origins: (Array<string>|undefined)
 * }}
 * @see http://developer.chrome.com/extensions/permissions.html#type-Permissions
 */
chrome.permissions.Permissions;


/**
 * @param {!chrome.permissions.Permissions} permissions
 * @param {function(boolean): void} callback Callback function.
 * @return {undefined}
 */
chrome.permissions.contains = function(permissions, callback) {};


/**
 * @param {function(!chrome.permissions.Permissions): void} callback
 *     Callback function.
 * @return {undefined}
 */
chrome.permissions.getAll = function(callback) {};


/**
 * @param {!chrome.permissions.Permissions} permissions
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.permissions.remove = function(permissions, opt_callback) {};


/**
 * @param {!chrome.permissions.Permissions} permissions
 * @param {function(boolean): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.permissions.request = function(permissions, opt_callback) {};


/** @type {!ChromeEvent} */
chrome.permissions.onAdded;


/** @type {!ChromeEvent} */
chrome.permissions.onRemoved;


/**
 * @const
 * @see https://developer.chrome.com/extensions/platformKeys
 */
chrome.platformKeys = {};


/**
 * @see https://developer.chrome.com/extensions/platformKeys#type-Match
 * @constructor
 */
chrome.platformKeys.Match = function() {};


/** @type {!ArrayBuffer} */
chrome.platformKeys.Match.prototype.certificate;


/** @type {!Object} */
chrome.platformKeys.Match.prototype.keyAlgorithm;


/**
 * @typedef {?{
 *   certificateTypes: !Array<string>,
 *   certificateAuthorities: !Array<!ArrayBuffer>
 * }}
 */
chrome.platformKeys.SelectCertificateDetailRequest;


/**
 * @typedef {?{
 *   request: !chrome.platformKeys.SelectCertificateDetailRequest,
 *   clientCerts: (!Array<!ArrayBuffer>|undefined),
 *   interactive: boolean
 * }}
 *
 * @see https://developer.chrome.com/extensions/platformKeys#method-selectClientCertificates
 */
chrome.platformKeys.SelectCertificateDetails;


/**
 * @param {!chrome.platformKeys.SelectCertificateDetails} details
 * @param {!function(!Array<!chrome.platformKeys.Match>)} callback
 */
chrome.platformKeys.selectClientCertificates = function(details, callback) {};


/**
 * @param {!ArrayBuffer} certificate
 * @param {!Object} parameters
 * @param {!function(!Object, ?Object)} callback
 */
chrome.platformKeys.getKeyPair = function(certificate, parameters, callback) {};


/**
 * @return {!Object}
 */
chrome.platformKeys.subtleCrypto = function() {};


/**
 * @typedef {?{
 *   serverCertificateChain: !Array<!ArrayBuffer>,
 *   hostname: string
 * }}
 *
 * @see https://developer.chrome.com/extensions/platformKeys#method-verifyTLSServerCertificate
 */
chrome.platformKeys.VerifyServerCertificateDetails;


/**
 * @param {!chrome.platformKeys.VerifyServerCertificateDetails} details
 * @param {!function(!Object)} callback
 */
chrome.platformKeys.verifyTLSServerCertificate = function(details, callback) {};


/**
 * @const
 * @see http://developer.chrome.com/dev/extensions/power.html
 */
chrome.power = {};


/**
 * @param {string} level A string describing the degree to which power
 *     management should be disabled, should be either "system" or "display".
 * @return {undefined}
 */
chrome.power.requestKeepAwake = function(level) {};


/**
 * Releases a request previously made via requestKeepAwake().
 * @return {undefined}
 */
chrome.power.releaseKeepAwake = function() {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/privacy.html
 */
chrome.privacy = {};


/** @type {!Object<string,!ChromeSetting>} */
chrome.privacy.network;


/** @type {!Object<string,!ChromeSetting>} */
chrome.privacy.services;


/** @type {!Object<string,!ChromeSetting>} */
chrome.privacy.websites;


/**
 * @const
 * @see https://developer.chrome.com/extensions/proxy.html
 */
chrome.proxy = {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/proxy#property-settings
 */
chrome.proxy.settings = {};


/**
 * @typedef {?{
 *   incognito: (boolean|undefined),
 * }}
 */
chrome.proxy.settings.GetParameter;


/**
 * @typedef {{
 *   value: *,
 *   levelOfControl: string,
 *   incognitoSpecific: (boolean|undefined),
 * }}
 */
chrome.proxy.settings.GetResponse;


/**
 * @param {!chrome.proxy.settings.GetParameter} details
 * @param {(function(!chrome.proxy.settings.GetResponse): void)=} callback
 */
chrome.proxy.settings.get = function(details, callback) {};


/**
 * @param {{
 *   value: *,
 *   scope: (string|undefined),
 * }} details
 * @param {(function(): void)=} callback
 */
chrome.proxy.settings.set = function(details, callback) {};


/**
 * @param {{
 *   scope: (string|undefined),
 * }} details
 * @param {(function(): void)=} callback
 */
chrome.proxy.settings.clear = function(details, callback) {};


/** @type {!ChromeEvent} */
chrome.proxy.onProxyError;


/**
 * @const
 * @see http://developer.chrome.com/apps/socket.html
 */
chrome.socket = {};



/**
 * @constructor
 */
chrome.socket.CreateInfo = function() {};


/** @type {number} */
chrome.socket.CreateInfo.prototype.socketId;



/**
 * @constructor
 */
chrome.socket.ReadInfo = function() {};


/** @type {number} */
chrome.socket.ReadInfo.prototype.resultCode;


/** @type {!ArrayBuffer} */
chrome.socket.ReadInfo.prototype.data;



/**
 * @constructor
 */
chrome.socket.WriteInfo = function() {};


/** @type {number} */
chrome.socket.WriteInfo.prototype.bytesWritten;



/**
 * @constructor
 */
chrome.socket.RecvFromInfo = function() {};


/** @type {number} */
chrome.socket.RecvFromInfo.prototype.resultCode;


/** @type {!ArrayBuffer} */
chrome.socket.RecvFromInfo.prototype.data;


/** @type {string} */
chrome.socket.RecvFromInfo.prototype.address;


/** @type {number} */
chrome.socket.RecvFromInfo.prototype.port;



/**
 * @constructor
 */
chrome.socket.AcceptInfo = function() {};


/** @type {number} */
chrome.socket.AcceptInfo.prototype.resultCode;


/** @type {(number|undefined)} */
chrome.socket.AcceptInfo.prototype.socketId;



/**
 * @constructor
 */
chrome.socket.SocketInfo = function() {};


/** @type {string} */
chrome.socket.SocketInfo.prototype.socketType;


/** @type {boolean} */
chrome.socket.SocketInfo.prototype.connected;


/** @type {(string|undefined)} */
chrome.socket.SocketInfo.prototype.peerAddress;


/** @type {(number|undefined)} */
chrome.socket.SocketInfo.prototype.peerPort;


/** @type {(string|undefined)} */
chrome.socket.SocketInfo.prototype.localAddress;


/** @type {(number|undefined)} */
chrome.socket.SocketInfo.prototype.localPort;



/**
 * @constructor
 */
chrome.socket.NetworkAdapterInfo = function() {};


/** @type {string} */
chrome.socket.NetworkAdapterInfo.prototype.name;


/** @type {string} */
chrome.socket.NetworkAdapterInfo.prototype.address;


/**
 * @param {string} type The type of socket to create. Must be 'tcp' or 'udp'.
 * @param {(Object|function(!chrome.socket.CreateInfo))} optionsOrCallback The
 *     socket options or callback.
 * @param {function(!chrome.socket.CreateInfo)=} opt_callback Called when the
 *     socket has been created.
 * @return {undefined}
 */
chrome.socket.create = function(type, optionsOrCallback, opt_callback) {};


/**
 * @param {number} socketId The id of the socket to destroy.
 * @return {undefined}
 */
chrome.socket.destroy = function(socketId) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {string} hostname The hostname or IP address of the remote machine.
 * @param {number} port The port of the remote machine.
 * @param {function(number)} callback Called when the connection attempt is
 *     complete.
 * @return {undefined}
 */
chrome.socket.connect = function(socketId, hostname, port, callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {string} address The address of the local machine.
 * @param {number} port The port of the local machine.
 * @param {function(number)} callback Called when the bind attempt is complete.
 * @return {undefined}
 */
chrome.socket.bind = function(socketId, address, port, callback) {};


/**
 * @param {number} socketId The id of the socket to disconnect.
 * @return {undefined}
 */
chrome.socket.disconnect = function(socketId) {};


/**
 * @param {number} socketId The id of the socket to read from.
 * @param {(number|function(!chrome.socket.ReadInfo))} bufferSizeOrCallback The
 *     read buffer size or the callback.
 * @param {function(!chrome.socket.ReadInfo)=} opt_callback Called with data
 *     that was available to be read without blocking.
 * @return {undefined}
 */
chrome.socket.read = function(socketId, bufferSizeOrCallback, opt_callback) {};


/**
 * @param {number} socketId The id of the socket to write to.
 * @param {!ArrayBuffer} data The data to write.
 * @param {function(!chrome.socket.WriteInfo)} callback Called when the write
 *     operation completes without blocking or an error occurs.
 * @return {undefined}
 */
chrome.socket.write = function(socketId, data, callback) {};


/**
 * @param {number} socketId The id of the socket to read from.
 * @param {(number|function(!chrome.socket.RecvFromInfo))} bufferSizeOrCallback
 *     The read buffer size or the callback.
 * @param {function(!chrome.socket.RecvFromInfo)=} opt_callback Called with data
 *     that was available to be read without blocking.
 * @return {undefined}
 */
chrome.socket.recvFrom = function(
    socketId, bufferSizeOrCallback, opt_callback) {};


/**
 * @param {number} socketId The id of the socket to write to.
 * @param {!ArrayBuffer} data The data to write.
 * @param {string} address The address of the remote machine.
 * @param {number} port The port of the remote machine.
 * @param {function(!chrome.socket.WriteInfo)} callback Called when the write
 *     operation completes without blocking or an error occurs.
 * @return {undefined}
 */
chrome.socket.sendTo = function(socketId, data, address, port, callback) {};


/**
 * @param {number} socketId The id of the socket to listen on.
 * @param {string} address The address of the local machine to listen on. Use
 *     '0' to listen on all addresses.
 * @param {number} port The port of the local machine.
 * @param {(number|function(number))} backlogOrCallback The length of the
 *     socket's listen queue or the callback.
 * @param {function(number)=} opt_callback Called when the listen operation
 *     completes.
 * @return {undefined}
 */
chrome.socket.listen = function(
    socketId, address, port, backlogOrCallback, opt_callback) {};


/**
 * @param {number} socketId The id of the socket to accept a connection on.
 * @param {function(!chrome.socket.AcceptInfo)} callback Called when a new
 *     socket is accepted.
 * @return {undefined}
 */
chrome.socket.accept = function(socketId, callback) {};


/**
 * @param {number} socketId The id of the socket to listen on.
 * @param {boolean} enable If true, enable keep-alive functionality.
 * @param {(number|function(boolean))} delayOrCallback The delay in seconds
 *     between the last packet received and the first keepalive probe (default
 *     is 0) or the callback
 * @param {function(boolean)=} opt_callback Called when the setKeepAlive attempt
 *     is complete.
 * @return {undefined}
 */
chrome.socket.setKeepAlive = function(
    socketId, enable, delayOrCallback, opt_callback) {};


/**
 * @param {number} socketId The id of the socket to listen on.
 * @param {boolean} noDelay If true, disables Nagle's algorithm.
 * @param {function(boolean)} callback Called when the setNoDelay attempt is
 *     complete.
 * @return {undefined}
 */
chrome.socket.setNoDelay = function(socketId, noDelay, callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {function(!chrome.socket.SocketInfo)} callback Called when the state
 *     is available.
 * @return {undefined}
 */
chrome.socket.getInfo = function(socketId, callback) {};


/**
 * @param {function(!Array<!chrome.socket.NetworkAdapterInfo>)} callback Called
 *     when local adapter information is available.
 * @return {undefined}
 */
chrome.socket.getNetworkList = function(callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {string} address The group address to join. Domain names are not
 *     supported.
 * @param {function(number)} callback Called when the join operation is done.
 * @return {undefined}
 */
chrome.socket.joinGroup = function(socketId, address, callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {string} address The group address to leave. Domain names are not
 *     supported.
 * @param {function(number)} callback Called when the leave operation is done.
 * @return {undefined}
 */
chrome.socket.leaveGroup = function(socketId, address, callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {number} ttl The time-to-live value.
 * @param {function(number)} callback Called when the configuration operation is
 *     done.
 * @return {undefined}
 */
chrome.socket.setMulticastTimeToLive = function(socketId, ttl, callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {boolean} enabled True to enable loopback mode.
 * @param {function(number)} callback Called when the configuration operation is
 *     done.
 * @return {undefined}
 */
chrome.socket.setMulticastLoopbackMode = function(
    socketId, enabled, callback) {};


/**
 * @param {number} socketId The id of the socket.
 * @param {function(!Array<string>)} callback Called with an array of string
 *     groups.
 * @return {undefined}
 */
chrome.socket.getJoinedGroups = function(socketId, callback) {};


/**
 * @const
 */
chrome.sockets = {};


/**
 * @const
 * @see https://developer.chrome.com/apps/sockets_tcp
 */
chrome.sockets.tcp = {};



/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_tcp#type-SocketInfo
 */
chrome.sockets.tcp.SocketInfo = function() {};


/** @type {number} */
chrome.sockets.tcp.SocketInfo.prototype.socketId;


/** @type {boolean} */
chrome.sockets.tcp.SocketInfo.prototype.persistent;


/** @type {string|undefined} */
chrome.sockets.tcp.SocketInfo.prototype.name;


/** @type {number|undefined} */
chrome.sockets.tcp.SocketInfo.prototype.bufferSize;


/** @type {boolean} */
chrome.sockets.tcp.SocketInfo.prototype.paused;


/** @type {boolean} */
chrome.sockets.tcp.SocketInfo.prototype.connected;


/** @type {string|undefined} */
chrome.sockets.tcp.SocketInfo.prototype.localAddress;


/** @type {number|undefined} */
chrome.sockets.tcp.SocketInfo.prototype.localPort;


/** @type {string|undefined} */
chrome.sockets.tcp.SocketInfo.prototype.peerAddress;


/** @type {number|undefined} */
chrome.sockets.tcp.SocketInfo.prototype.peerPort;


/**
 * @typedef {?{
 *   persistent: (boolean|undefined),
 *   name: (string|undefined),
 *   bufferSize: (number|undefined)
 * }}
 * @see https://developer.chrome.com/apps/sockets_tcp#type-SocketProperties
 */
chrome.sockets.tcp.SocketProperties;


/**
 * @typedef {?{
 *   min: (string|undefined),
 *   max: (string|undefined)
 * }}
 * @see https://developer.chrome.com/apps/sockets_tcp#method-secure
 */
chrome.sockets.tcp.SecurePropertiesTlsVersion;


/**
 * @typedef {?{
 *   tlsVersion: (chrome.sockets.tcp.SecurePropertiesTlsVersion|undefined)
 * }}
 * @see https://developer.chrome.com/apps/sockets_tcp#method-secure
 */
chrome.sockets.tcp.SecureProperties;


/**
 * @param {!chrome.sockets.tcp.SocketProperties|
 *     function(!Object)} propertiesOrCallback
 * @param {function(!Object)=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-create
 * @return {undefined}
 */
chrome.sockets.tcp.create = function(propertiesOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!chrome.sockets.tcp.SocketProperties} properties
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-update
 * @return {undefined}
 */
chrome.sockets.tcp.update = function(socketId, properties, opt_callback) {};


/**
 * @param {number} socketId
 * @param {boolean} paused
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-setPaused
 * @return {undefined}
 */
chrome.sockets.tcp.setPaused = function(socketId, paused, opt_callback) {};


/**
 * @param {number} socketId
 * @param {boolean} enable
 * @param {(number|function(number))} delayOrCallback
 * @param {function(number)=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-setKeepAlive
 * @return {undefined}
 */
chrome.sockets.tcp.setKeepAlive = function(
    socketId, enable, delayOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {boolean} noDelay
 * @param {function(number)} callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-setNoDelay
 * @return {undefined}
 */
chrome.sockets.tcp.setNoDelay = function(socketId, noDelay, callback) {};


/**
 * @param {number} socketId
 * @param {string} peerAddress
 * @param {number} peerPort
 * @param {function(number)} callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-connect
 * @return {undefined}
 */
chrome.sockets.tcp.connect = function(
    socketId, peerAddress, peerPort, callback) {};


/**
 * @param {number} socketId The id of the socket to disconnect.
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-disconnect
 * @return {undefined}
 */
chrome.sockets.tcp.disconnect = function(socketId, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!chrome.sockets.tcp.SecureProperties|function(number)}
 *     optionsOrCallback
 * @param {function(number)=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-secure
 * @return {undefined}
 */
chrome.sockets.tcp.secure = function(
    socketId, optionsOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!ArrayBuffer} data
 * @param {function(!Object)} callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-send
 * @return {undefined}
 */
chrome.sockets.tcp.send = function(socketId, data, callback) {};


/**
 * @param {number} socketId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-close
 * @return {undefined}
 */
chrome.sockets.tcp.close = function(socketId, opt_callback) {};


/**
 * @param {number} socketId
 * @param {function(!chrome.sockets.tcp.SocketInfo)} callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-getInfo
 * @return {undefined}
 */
chrome.sockets.tcp.getInfo = function(socketId, callback) {};


/**
 * @param {function(!Array<!chrome.sockets.tcp.SocketInfo>)} callback
 * @see https://developer.chrome.com/apps/sockets_tcp#method-getSockets
 * @return {undefined}
 */
chrome.sockets.tcp.getSockets = function(callback) {};



/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_tcp#event-onReceive
 */
chrome.sockets.tcp.ReceiveEventData = function() {};


/** @type {number} */
chrome.sockets.tcp.ReceiveEventData.prototype.socketId;


/** @type {!ArrayBuffer} */
chrome.sockets.tcp.ReceiveEventData.prototype.data;



/**
 * Event whose listeners take a ReceiveEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.sockets.tcp.ReceiveEventData)>}
 */
chrome.sockets.tcp.ReceiveEvent = function() {};


/** @type {!chrome.sockets.tcp.ReceiveEvent} */
chrome.sockets.tcp.onReceive;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_tcp#event-onReceiveError
 */
chrome.sockets.tcp.ReceiveErrorEventData = function() {};


/** @type {number} */
chrome.sockets.tcp.ReceiveErrorEventData.prototype.socketId;


/** @type {number} */
chrome.sockets.tcp.ReceiveErrorEventData.prototype.resultCode;



/**
 * Event whose listeners take a ReceiveErrorEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.sockets.tcp.ReceiveErrorEventData)>}
 */
chrome.sockets.tcp.ReceiveErrorEvent = function() {};


/** @type {!chrome.sockets.tcp.ReceiveErrorEvent} */
chrome.sockets.tcp.onReceiveError;


/**
 * @const
 * @see https://developer.chrome.com/apps/sockets_tcpServer
 */
chrome.sockets.tcpServer = {};


/**
 * @typedef {?{
 *   persistent: (boolean|undefined),
 *   name: (string|undefined),
 * }}
 * @see https://developer.chrome.com/apps/sockets_tcpServer#type-SocketProperties
 */
chrome.sockets.tcpServer.SocketProperties;


/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_tcpServer#type-SocketInfo
 */
chrome.sockets.tcpServer.SocketInfo = function() {};


/** @type {number} */
chrome.sockets.tcpServer.SocketInfo.prototype.socketId;


/** @type {boolean} */
chrome.sockets.tcpServer.SocketInfo.prototype.persistent;


/** @type {string|undefined} */
chrome.sockets.tcpServer.SocketInfo.prototype.name;


/** @type {boolean} */
chrome.sockets.tcpServer.SocketInfo.prototype.paused;


/** @type {string|undefined} */
chrome.sockets.tcpServer.SocketInfo.prototype.localAddress;


/** @type {number|undefined} */
chrome.sockets.tcpServer.SocketInfo.prototype.localPort;


/**
 * @param {
 *   (!chrome.sockets.tcpServer.SocketProperties|function(!Object))
 * } propertiesOrCallback
 * @param {function(!Object)=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-create
 */
chrome.sockets.tcpServer.create = function(
    propertiesOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!chrome.sockets.tcpServer.SocketProperties} properties
 * @param {function(): void=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-update
 */
chrome.sockets.tcpServer.update = function(
    socketId, properties, opt_callback) {};


/**
 * @param {number} socketId
 * @param {boolean} paused
 * @param {function(): void=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-setPaused
 */
chrome.sockets.tcpServer.setPaused = function(
    socketId, paused, opt_callback) {};


/**
 * @param {number} socketId
 * @param {string} address
 * @param {number} port
 * @param {number|function(number)} backlogOrCallback
 * @param {function(number)=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-listen
 */
chrome.sockets.tcpServer.listen = function(
    socketId, address, port, backlogOrCallback, opt_callback) {};


/**
 * @param {number} socketId The id of the socket to disconnect.
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-disconnect
 * @return {undefined}
 */
chrome.sockets.tcpServer.disconnect = function(socketId, opt_callback) {};


/**
 * @param {number} socketId
 * @param {function()=} opt_callback
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-close
 * @return {undefined}
 */
chrome.sockets.tcpServer.close = function(socketId, opt_callback) {};


/**
 * @param {number} socketId
 * @param {function(!chrome.sockets.tcpServer.SocketInfo)} callback
 * @see https://developer.chrome.com/apps/sockets_tcpServer#method-getInfo
 * @return {undefined}
 */
chrome.sockets.tcpServer.getInfo = function(socketId, callback) {};


/**
 * @param {function(!Array<!chrome.sockets.tcpServer.SocketInfo>)} callback
 * @see https://developer.chrome.com/apps/sockets_tcpServerp#method-getSockets
 * @return {undefined}
 */
chrome.sockets.tcpServer.getSockets = function(callback) {};


/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_tcpServer#event-onAccept
 */
chrome.sockets.tcpServer.AcceptEventData = function() {};


/** @type {number} */
chrome.sockets.tcpServer.AcceptEventData.prototype.socketId;


/** @type {number} */
chrome.sockets.tcpServer.AcceptEventData.prototype.clientSocketId;


/**
 * Event whose listeners take a AcceptEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.sockets.tcpServer.AcceptEventData)>}
 */
chrome.sockets.tcpServer.AcceptEvent = function() {};


/** @type {!chrome.sockets.tcpServer.AcceptEvent} */
chrome.sockets.tcpServer.onAccept;


/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_tcpServer#event-onAcceptError
 */
chrome.sockets.tcpServer.AcceptErrorEventData = function() {};


/** @type {number} */
chrome.sockets.tcpServer.AcceptErrorEventData.prototype.socketId;


/** @type {number} */
chrome.sockets.tcpServer.AcceptErrorEventData.prototype.resultCode;


/**
 * Event whose listeners take a AcceptErrorEventData parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.sockets.tcpServer.AcceptErrorEventData)>}
 */
chrome.sockets.tcpServer.AcceptErrorEvent = function() {};


/** @type {!chrome.sockets.tcpServer.AcceptErrorEvent} */
chrome.sockets.tcpServer.onAcceptError;


/**
 * @see https://developer.chrome.com/apps/sockets_udp
 * @const
 */
chrome.sockets.udp = {};


/**
 * @typedef {?{
 *   persistent: (boolean|undefined),
 *   name: (string|undefined),
 *   bufferSize: (number|undefined)
 * }}
 * @see https://developer.chrome.com/apps/sockets_udp#type-SocketProperties
 */
chrome.sockets.udp.SocketProperties;


/**
 * @constructor
 * @see https://developer.chrome.com/apps/sockets_udp#type-SocketInfo
 */
chrome.sockets.udp.SocketInfo = function() {};


/** @type {number} */
chrome.sockets.udp.SocketInfo.prototype.socketId;


/** @type {boolean} */
chrome.sockets.udp.SocketInfo.prototype.persistent;


/** @type {string|undefined} */
chrome.sockets.udp.SocketInfo.prototype.name;


/** @type {number|undefined} */
chrome.sockets.udp.SocketInfo.prototype.bufferSize;


/** @type {boolean} */
chrome.sockets.udp.SocketInfo.prototype.paused;


/** @type {string|undefined} */
chrome.sockets.udp.SocketInfo.prototype.localAddress;


/** @type {number|undefined} */
chrome.sockets.udp.SocketInfo.prototype.localPort;


/**
 * @param {
 *   (!chrome.sockets.udp.SocketProperties|function(!Object))
 * } propertiesOrCallback
 * @param {function(!Object)=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-create
 */
chrome.sockets.udp.create = function(propertiesOrCallback, opt_callback) {};


/**
 * @param {number} socketId
 * @param {!chrome.sockets.udp.SocketProperties} properties
 * @param {function(): void=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-update
 */
chrome.sockets.udp.update = function(socketId, properties, opt_callback) {};


/**
 * @param {number} socketId
 * @param {boolean} paused
 * @param {function(): void=} opt_callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-setPaused
 */
chrome.sockets.udp.setPaused = function(socketId, paused, opt_callback) {};


/**
 * @param {number} socketId
 * @param {string} address
 * @param {number} port
 * @param {function(number): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-bind
 */
chrome.sockets.udp.bind = function(socketId, address, port, callback) {};


/**
 * @param {number} socketId
 * @param {!ArrayBuffer} data
 * @param {string} address
 * @param {number} port
 * @param {function(!Object): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-send
 */
chrome.sockets.udp.send = function(socketId, data, address, port, callback) {};


/**
 * @param {number} socketId
 * @param {function()} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-close
 */
chrome.sockets.udp.close = function(socketId, callback) {};


/**
 * @param {number} socketId
 * @param {function(!chrome.sockets.udp.SocketInfo): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-getInfo
 */
chrome.sockets.udp.getInfo = function(socketId, callback) {};


/**
 * @param {function(!Array<!chrome.sockets.udp.SocketInfo>): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-getSockets
 */
chrome.sockets.udp.getSockets = function(callback) {};


/**
 * @param {number} socketId
 * @param {string} address
 * @param {function(number): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-joinGroup
 */
chrome.sockets.udp.joinGroup = function(socketId, address, callback) {};


/**
 * @param {number} socketId
 * @param {string} address
 * @param {function(number): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-leaveGroup
 */
chrome.sockets.udp.leaveGroup = function(socketId, address, callback) {};


/**
 * @param {number} socketId
 * @param {number} ttl
 * @param {function(number): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-setMulticastTimeToLive
 */
chrome.sockets.udp.setMulticastTimeToLive = function(
    socketId, ttl, callback) {};


/**
 * @param {number} socketId
 * @param {boolean} enabled
 * @param {function(number): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-setMulticastLoopbackMode
 */
chrome.sockets.udp.setMulticastLoopbackMode = function(
    socketId, enabled, callback) {};


/**
 * @param {number} socketId
 * @param {function(!Array<string>): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-getJoinedGroups
 */
chrome.sockets.udp.getJoinedGroups = function(socketId, callback) {};


/**
 * @param {number} socketId
 * @param {boolean} enabled
 * @param {function(number): void} callback
 * @return {undefined}
 * @see https://developer.chrome.com/apps/sockets_udp#method-send
 */
chrome.sockets.udp.setBroadcast = function(socketId, enabled, callback) {};


/**
 * @type {!ChromeObjectEvent}
 * @see https://developer.chrome.com/apps/sockets_udp#event-onReceive
 */
chrome.sockets.udp.onReceive;


/**
 * @type {!ChromeObjectEvent}
 * @see https://developer.chrome.com/apps/sockets_udp#event-onReceiveError
 */
chrome.sockets.udp.onReceiveError;


/**
 * @const
 * @see https://developer.chrome.com/extensions/storage.html
 */
chrome.storage = {};


/** @type {!StorageArea} */
chrome.storage.sync;


/** @type {!StorageArea} */
chrome.storage.local;


/** @type {!StorageArea} */
chrome.storage.managed;


/** @type {!StorageChangeEvent} */
chrome.storage.onChanged;


/** @const */
chrome.system = {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/system_cpu.html
 */
chrome.system.cpu = {};


/**
 * @typedef {?{
 *   user: number,
 *   kernel: number,
 *   idle: number,
 *   total: number,
 * }}
 */
chrome.system.cpu.CpuTime;


/**
 * @typedef {?{
 *   usage: !chrome.system.cpu.CpuTime,
 * }}
 */
chrome.system.cpu.ProcessorInfo;


/**
 * @typedef {?{
 *   numOfProcessors: number,
 *   archName: string,
 *   modelName: string,
 *   features: !Array<string>,
 *   processors: !Array<!chrome.system.cpu.ProcessorInfo>,
 *   temperatures: !Array<number>,
 * }}
 */
chrome.system.cpu.CpuInfo;


/**
 * @param {function(!chrome.system.cpu.CpuInfo)} callback
 * @return {undefined}
 */
chrome.system.cpu.getInfo = function(callback) {};


/**
 * @const
 * @see http://developer.chrome.com/apps/system_display.html
 */
chrome.system.display = {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/system.display#type-MirrorMode
 */
chrome.system.display.MirrorMode = {
  OFF: '',
  NORMAL: '',
  MIXED: '',
};


/**
 * @typedef {!{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-Bounds
 */
chrome.system.display.Bounds;


/**
 * @typedef {!{
 *   left: number,
 *   top: number,
 *   right: number,
 *   bottom: number
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-Insets
 */
chrome.system.display.Insets;


/**
 * @typedef {!{
 *   x: number,
 *   y: number
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-Point
 */
chrome.system.display.Point;


/**
 * @typedef {!{
 *   displayPoint: !chrome.system.display.Point,
 *   touchPoint: !chrome.system.display.Point
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-TouchCalibrationPair
 */
chrome.system.display.TouchCalibrationPair;


/**
 * @typedef {!{
 *   pair1: !chrome.system.display.TouchCalibrationPair,
 *   pair2: !chrome.system.display.TouchCalibrationPair,
 *   pair3: !chrome.system.display.TouchCalibrationPair,
 *   pair4: !chrome.system.display.TouchCalibrationPair
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-TouchCalibrationPairQuad
 */
chrome.system.display.TouchCalibrationPairQuad;


/**
 * @typedef {!{
 *   width: number,
 *   height: number,
 *   widthInNativePixels: number,
 *   heightInNativePixels: number,
 *   uiScale: number,
 *   deviceScaleFactor: number,
 *   isNative: boolean,
 *   isSelected: boolean
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-DisplayMode
 */
chrome.system.display.DisplayMode;


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/system.display#type-LayoutPosition
 */
chrome.system.display.LayoutPosition = {
  TOP: '',
  RIGHT: '',
  BOTTOM: '',
  LEFT: '',
};


/**
 * @typedef {!{
 *   id: string,
 *   parentId: string,
 *   position: (!chrome.system.display.LayoutPosition|string),
 *   offset: number
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-DisplayLayout
 */
chrome.system.display.DisplayLayout;


/**
 * @typedef {!{
 *   isUnified: (boolean|undefined),
 *   mirroringSourceId: (string|undefined),
 *   mirroringDestinationIds: (!Array<string>|undefined),
 *   isPrimary: (boolean|undefined),
 *   overscan: (!chrome.system.display.Insets|undefined),
 *   rotation: (number|undefined),
 *   boundsOriginX: (number|undefined),
 *   boundsOriginY: (number|undefined),
 *   displayMode: (!chrome.system.display.DisplayMode|undefined),
 *   availableDisplayZoomFactors: (!Array<number>|undefined),
 *   displayZoomFactor: (number|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-DisplayProperties
 */
chrome.system.display.DisplayProperties;


/**
 * @typedef {!{
 *   singleUnified: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-GetInfoFlags
 */
chrome.system.display.GetInfoFlags;


/**
 * @typedef {!{
 *   mode: (!chrome.system.display.MirrorMode|string),
 *   mirroringSourceId: (string|undefined),
 *   mirroringDestinationIds: (!Array<string>|undefined),
 * }}
 * @see https://developer.chrome.com/extensions/system.display#type-MirrorModeInfo
 */
chrome.system.display.MirrorModeInfo;


/**
 * @typedef {!{
 *   manufacturerId: string,
 *   productId: string,
 *   yearOfManufacture: number,
 * }}
 */
chrome.system.display.Edid;


/**
 * An undocumented type that defines the objects passed to getInfo()'s callback.
 * @constructor
 */
chrome.system.display.DisplayUnitInfo = function() {};


/** @type {string} */
chrome.system.display.DisplayUnitInfo.prototype.id;


/** @type {string} */
chrome.system.display.DisplayUnitInfo.prototype.name;


/** @type {!chrome.system.display.Edid|undefined} */
chrome.system.display.DisplayUnitInfo.prototype.edid;


/** @type {string} */
chrome.system.display.DisplayUnitInfo.prototype.mirroringSourceId;


/** @type {!Array<string>} */
chrome.system.display.DisplayUnitInfo.prototype.mirroringDestinationIds;


/** @type {boolean} */
chrome.system.display.DisplayUnitInfo.prototype.isPrimary;


/** @type {boolean} */
chrome.system.display.DisplayUnitInfo.prototype.isInternal;


/** @type {boolean} */
chrome.system.display.DisplayUnitInfo.prototype.isEnabled;


/** @type {number} */
chrome.system.display.DisplayUnitInfo.prototype.dpiX;


/** @type {number} */
chrome.system.display.DisplayUnitInfo.prototype.dpiY;


/** @type {number} */
chrome.system.display.DisplayUnitInfo.prototype.rotation;


/** @type {!chrome.system.display.Bounds} */
chrome.system.display.DisplayUnitInfo.prototype.bounds;


/** @type {!chrome.system.display.Insets} */
chrome.system.display.DisplayUnitInfo.prototype.overscan;


/** @type {!chrome.system.display.Bounds} */
chrome.system.display.DisplayUnitInfo.prototype.workArea;


/** @type {!Array<!chrome.system.display.DisplayMode>} */
chrome.system.display.DisplayUnitInfo.prototype.modes;


/** @type {boolean} */
chrome.system.display.DisplayUnitInfo.prototype.hasTouchSupport;


/** @type {!Array<number>} */
chrome.system.display.DisplayUnitInfo.prototype.availableDisplayZoomFactors;


/** @type {number} */
chrome.system.display.DisplayUnitInfo.prototype.displayZoomFactor;


/**
 * @param {!chrome.system.display.GetInfoFlags|
 *     function(!Array<!chrome.system.display.DisplayUnitInfo>):void}
 *     flags Options affecting how the information is returned.
 * @param {function(!Array<!chrome.system.display.DisplayUnitInfo>):void=}
 *     callback The callback to invoke with the results.
 * @return {undefined}
 * @see https://developer.chrome.com/extensions/system.display#method-getInfo
 */
chrome.system.display.getInfo = function(flags, callback) {};


/**
 * @param {function(!Array<!chrome.system.display.DisplayLayout>):void} callback
 *     The callback to invoke with the results.
 * @see https://developer.chrome.com/extensions/system.display#method-getDisplayLayout
 */
chrome.system.display.getDisplayLayout = function(callback) {};


/**
 * @param {string} id The display's unique identifier.
 * @param {!chrome.system.display.DisplayProperties} info The information about
 *     display properties that should be changed. A property will be changed
 *     only if a new value for it is specified in info.
 * @param {function():void=} callback Empty function called when the function
 *     finishes. To find out whether the function succeeded, runtime.lastError
 *     should be queried.
 * @see https://developer.chrome.com/extensions/system.display#method-setDisplayProperties
 */
chrome.system.display.setDisplayProperties = function(id, info, callback) {};


/**
 * @param {!Array<!chrome.system.display.DisplayLayout>} layouts The layout
 *     information, required for all displays except the primary display.
 * @param {function():void=} callback Empty function called when the function
 *     finishes. To find out whether the function succeeded, runtime.lastError
 *     should be queried.
 * @see https://developer.chrome.com/extensions/system.display#method-setDisplayLayout
 */
chrome.system.display.setDisplayLayout = function(layouts, callback) {};


/**
 * @param {!chrome.system.display.MirrorModeInfo} info The information of the
 *     mirror mode that should be applied to the display mode.
 * @param {function():void=} callback Empty function called when the function
 *     finishes. To find out whether the function succeeded,
 *     $(ref:runtime.lastError) should be queried.
 * @see https://developer.chrome.com/extensions/system.display#method-setMirrorMode
 */
chrome.system.display.setMirrorMode = function(info, callback) {};


/**
 * @param {boolean} enabled True if unified desktop should be enabled.
 * @see https://developer.chrome.com/extensions/system.display#method-enableUnifiedDesktop
 */
chrome.system.display.enableUnifiedDesktop = function(enabled) {};


/**
 * @param {string} id The display's unique identifier.
 * @see https://developer.chrome.com/extensions/system.display#method-overscanCalibrationStart
 */
chrome.system.display.overscanCalibrationStart = function(id) {};


/**
 * @param {string} id The display's unique identifier.
 * @param {!chrome.system.display.Insets} delta The amount to change the
 *     overscan insets.
 * @see https://developer.chrome.com/extensions/system.display#method-overscanCalibrationAdjust
 */
chrome.system.display.overscanCalibrationAdjust = function(id, delta) {};


/**
 * @param {string} id The display's unique identifier.
 * @see https://developer.chrome.com/extensions/system.display#method-overscanCalibrationReset
 */
chrome.system.display.overscanCalibrationReset = function(id) {};


/**
 * @param {string} id The display's unique identifier.
 * @see https://developer.chrome.com/extensions/system.display#method-overscanCalibrationComplete
 */
chrome.system.display.overscanCalibrationComplete = function(id) {};


/**
 * @param {string} id The display's unique identifier.
 * @param {function(boolean):void=} callback Optional callback to inform the
 *     caller that the touch calibration has ended. The argument of the callback
 *     informs if the calibration was a success or not.
 * @see https://developer.chrome.com/extensions/system.display#method-showNativeTouchCalibration
 */
chrome.system.display.showNativeTouchCalibration = function(id, callback) {};


/**
 * @param {string} id The display's unique identifier.
 * @see https://developer.chrome.com/extensions/system.display#method-startCustomTouchCalibration
 */
chrome.system.display.startCustomTouchCalibration = function(id) {};


/**
 * @param {!chrome.system.display.TouchCalibrationPairQuad} pairs The pairs of
 * point used to calibrate the display.
 * @param {!chrome.system.display.Bounds} bounds Bounds of the display when the
 *     touch calibration was performed. |bounds.left| and |bounds.top| values
 *     are ignored.
 * @see https://developer.chrome.com/extensions/system.display#method-completeCustomTouchCalibration
 */
chrome.system.display.completeCustomTouchCalibration = function(
    pairs, bounds) {};


/**
 * @param {string} id The display's unique identifier.
 * @see https://developer.chrome.com/extensions/system.display#method-clearTouchCalibration
 */
chrome.system.display.clearTouchCalibration = function(id) {};


/**
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/system.display#event-onDisplayChanged
 */
chrome.system.display.onDisplayChanged;

/**
 * @const
 * @see https://developer.chrome.com/extensions/system_memory
 */
chrome.system.memory = {};

/**
 * Get physical memory information.
 * @param {function(!chrome.system.memory.MemoryInfo)} callback
 * @return {undefined}
 */
chrome.system.memory.getInfo = function(callback) {};

/**
 * @constructor
 */
chrome.system.memory.MemoryInfo = function() {};


/** @type {number} */
chrome.system.memory.MemoryInfo.prototype.capacity;


/** @type {number} */
chrome.system.memory.MemoryInfo.prototype.availableCapacity;

/**
 * @const
 * @see http://developer.chrome.com/apps/system_network.html
 */
chrome.system.network = {};

/**
 * @param {function(!Array<!chrome.system.network.NetworkInterface>)} callback
 * @return {undefined}
 */
chrome.system.network.getNetworkInterfaces = function(callback) {};

/**
 * @interface
 */
chrome.system.network.NetworkInterface = function() {};

/**
 * @const {string} The underlying name of the adapter. On *nix, this will
 *     typically be "eth0", "wlan0", etc.
 */
chrome.system.network.NetworkInterface.prototype.name;

/** @const {string} The available IPv4/6 address. */
chrome.system.network.NetworkInterface.prototype.address;

/** @const {number} The prefix length */
chrome.system.network.NetworkInterface.prototype.prefixLength;


/**
 * @const
 */
chrome.types = {};


/**
 * @typedef {?{
 *   format: (string|undefined),
 *   quality: (number|undefined)
 * }}
 */
chrome.types.ImageDetails;



/**
 * @const
 * @see https://developer.chrome.com/extensions/types.html
 */
chrome.chromeSetting = {};


/** @type {!ChromeEvent} */
chrome.chromeSetting.onChange;


/**
 * @const
 * @see https://developer.chrome.com/extensions/webNavigation.html
 */
chrome.webNavigation = {};


/**
 * @param {Object} details Object with a 'tabId' (number) key.
 * @param {function(!Array<Object<string, (boolean|number|string)>>)} callback
 *     Callback function.
 * @return {undefined}
 */
chrome.webNavigation.getAllFrames = function(details, callback) {};


/**
 * @param {Object} details Object with 'tabId' (number) and 'frameId' (number)
 *     keys.
 * @param {function(Object<string, (boolean|string)>)} callback
 *     Callback function.
 * @return {undefined}
 */
chrome.webNavigation.getFrame = function(details, callback) {};


/** @type {!ChromeEvent} */
chrome.webNavigation.onBeforeNavigate;


/** @type {!ChromeEvent} */
chrome.webNavigation.onCommitted;


/** @type {!ChromeEvent} */
chrome.webNavigation.onDOMContentLoaded;


/** @type {!ChromeEvent} */
chrome.webNavigation.onCompleted;


/** @type {!ChromeEvent} */
chrome.webNavigation.onErrorOccurred;


/** @type {!ChromeEvent} */
chrome.webNavigation.onCreatedNavigationTarget;


/** @type {!ChromeEvent} */
chrome.webNavigation.onReferenceFragmentUpdated;


/** @type {!ChromeEvent} */
chrome.webNavigation.onTabReplaced;


/** @type {!ChromeEvent} */
chrome.webNavigation.onHistoryStateUpdated;


/**
 * Base event type for WebRequest-releated events.
 *
 * The `addListener()` methods for these take more than one argument, so they
 * cannot just extend `ChromeBaseEvent`.
 * @interface
 * @extends {ChromeBaseEventNoListeners}
 * @template LISTENER
 */
function WebRequestBaseEvent() {}


/**
 * @param {LISTENER} listener Listener function.
 * @param {!RequestFilter} filter A set of filters that restrict
 *     the events that will be sent to this listener.
 * @param {!Array<string>=} opt_extraInfoSpec Array of extra information
 *     that should be passed to the listener function.
 * @return {undefined}
 */
WebRequestBaseEvent.prototype.addListener = function(
    listener, filter, opt_extraInfoSpec) {};


/**
 * @param {LISTENER} listener Listener function.
 * @return {undefined}
 */
WebRequestBaseEvent.prototype.removeListener = function(listener) {};


/**
 * @param {LISTENER} listener Listener function.
 * @return {boolean}
 */
WebRequestBaseEvent.prototype.hasListener = function(listener) {};


/**
 * @return {boolean}
 */
WebRequestBaseEvent.prototype.hasListeners = function() {};


/**
 * Most event listeners for WebRequest take extra arguments.
 * @see https://developer.chrome.com/extensions/webRequest
 * @interface
 * @extends {WebRequestBaseEvent<function(!Object)>}
 */
function WebRequestEvent() {}


/**
 * Some event listeners can be optionally synchronous.
 * @see https://developer.chrome.com/extensions/webRequest
 * @interface
 * @extends {WebRequestBaseEvent<function(!Object):
 * (undefined|!BlockingResponse)>}
 */
function WebRequestOptionallySynchronousEvent() {}


/**
 * The onAuthRequired event listener can be optionally synchronous, and can also
 * optionally take a callback.
 * @see https://developer.chrome.com/extensions/webRequest
 * @interface
 * @extends {WebRequestBaseEvent<
 *     function(
 *         !Object,
 *         function(!BlockingResponse)=): (undefined|!BlockingResponse)>}
 */
function WebRequestOnAuthRequiredEvent() {}


/**
 * The onErrorOccurred event takes one fewer parameter than the others.
 *
 * The `addListener()` methods for these take more than one argument, so they
 * cannot just extend `ChromeBaseEvent`.
 * @see https://developer.chrome.com/extensions/webRequest
 * @interface
 * @extends {ChromeBaseEventNoListeners}
 */
function WebRequestOnErrorOccurredEvent() {}


/**
 * @param {function(!Object): void} listener Listener function.
 * @param {!RequestFilter} filter A set of filters that restrict
 *     the events that will be sent to this listener.
 * @return {undefined}
 */
WebRequestOnErrorOccurredEvent.prototype.addListener = function(
    listener, filter) {};


/**
 * @param {function(!Object): void} listener Listener function.
 * @return {undefined}
 */
WebRequestOnErrorOccurredEvent.prototype.removeListener = function(listener) {};


/**
 * @param {function(!Object): void} listener Listener function.
 * @return {boolean}
 */
WebRequestOnErrorOccurredEvent.prototype.hasListener = function(listener) {};


/**
 * @return {boolean}
 */
WebRequestOnErrorOccurredEvent.prototype.hasListeners = function() {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/webRequest
 */
chrome.webRequest = {};


/**
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
chrome.webRequest.handlerBehaviorChanged = function(opt_callback) {};


/** @type {!WebRequestOnAuthRequiredEvent} */
chrome.webRequest.onAuthRequired;


/** @type {!WebRequestEvent} */
chrome.webRequest.onBeforeRedirect;


/** @type {!WebRequestOptionallySynchronousEvent} */
chrome.webRequest.onBeforeRequest;


/** @type {!WebRequestOptionallySynchronousEvent} */
chrome.webRequest.onBeforeSendHeaders;


/** @type {!WebRequestEvent} */
chrome.webRequest.onCompleted;


/** @type {!WebRequestOnErrorOccurredEvent} */
chrome.webRequest.onErrorOccurred;


/** @type {!WebRequestOptionallySynchronousEvent} */
chrome.webRequest.onHeadersReceived;


/** @type {!WebRequestEvent} */
chrome.webRequest.onResponseStarted;


/** @type {!WebRequestEvent} */
chrome.webRequest.onSendHeaders;


// Classes



/**
 * @see https://developer.chrome.com/extensions/management.html
 * @constructor
 */
function ExtensionInfo() {}


/** @type {string} */
ExtensionInfo.prototype.id;


/** @type {string} */
ExtensionInfo.prototype.name;


/** @type {string} */
ExtensionInfo.prototype.shortName;


/** @type {string} */
ExtensionInfo.prototype.description;


/** @type {string} */
ExtensionInfo.prototype.version;


/** @type {boolean} */
ExtensionInfo.prototype.mayDisable;


/** @type {boolean} */
ExtensionInfo.prototype.enabled;


/** @type {string|undefined} */
ExtensionInfo.prototype.disabledReason;


/** @type {boolean} */
ExtensionInfo.prototype.isApp;


/** @type {string} */
ExtensionInfo.prototype.type;


/** @type {string|undefined} */
ExtensionInfo.prototype.appLaunchUrl;


/** @type {string|undefined} */
ExtensionInfo.prototype.homepageUrl;


/** @type {string|undefined} */
ExtensionInfo.prototype.updateUrl;


/** @type {boolean} */
ExtensionInfo.prototype.offlineEnabled;


/** @type {string} */
ExtensionInfo.prototype.optionsUrl;


/** @type {!Array<!IconInfo>|undefined} */
ExtensionInfo.prototype.icons;


/** @type {!Array<string>} */
ExtensionInfo.prototype.permissions;


/** @type {!Array<string>} */
ExtensionInfo.prototype.hostPermissions;


/** @type {string} */
ExtensionInfo.prototype.installType;


/** @type {string|undefined} */
ExtensionInfo.prototype.launchType;


/** @type {!Array<string>|undefined} */
ExtensionInfo.prototype.availableLaunchTypes;



/**
 * @see https://developer.chrome.com/extensions/management.html
 * @constructor
 */
function IconInfo() {}


/** @type {number} */
IconInfo.prototype.size;


/** @type {string} */
IconInfo.prototype.url;



/**
 * @see https://developer.chrome.com/extensions/windows.html
 * @constructor
 */
function ChromeWindow() {}


/** @type {number} */
ChromeWindow.prototype.id;


/** @type {boolean} */
ChromeWindow.prototype.focused;


/** @type {number} */
ChromeWindow.prototype.top;


/** @type {number} */
ChromeWindow.prototype.left;


/** @type {number} */
ChromeWindow.prototype.width;


/** @type {number} */
ChromeWindow.prototype.height;


/** @type {Array<Tab>} */
ChromeWindow.prototype.tabs;


/** @type {boolean} */
ChromeWindow.prototype.incognito;


/** @type {string} */
ChromeWindow.prototype.type;


/** @type {string} */
ChromeWindow.prototype.state;


/** @type {boolean} */
ChromeWindow.prototype.alwaysOnTop;


/**
 * @see http://developer.chrome.com/extensions/pushMessaging.html
 * @const
 */
chrome.pushMessaging = {};


/**
 * @type {!chrome.pushMessaging.PushMessageEvent}
 */
chrome.pushMessaging.onMessage;


/**
 * @param {boolean|function(!chrome.pushMessaging.ChannelIdResult)}
 *     interactiveOrCallback Either a flag(optional), if set to true, user will
 *     be asked to log in if they are not already logged in, or, when he flag is
 *     not given, the callback.
 * @param {function(!chrome.pushMessaging.ChannelIdResult)=} opt_callback
 *     Callback.
 * @return {undefined}
 */
chrome.pushMessaging.getChannelId = function(
    interactiveOrCallback, opt_callback) {};



/**
 * Event whose listeners take a chrome.pushMessaging.Message parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.pushMessaging.Message)>}
 */
chrome.pushMessaging.PushMessageEvent = function() {};


/**
 * @see https://developer.chrome.com/extensions/bookmarks.html#type-BookmarkTreeNode
 * @constructor
 */
function BookmarkTreeNode() {}


/** @type {string} */
BookmarkTreeNode.prototype.id;


/** @type {string|undefined} */
BookmarkTreeNode.prototype.parentId;


/** @type {number|undefined} */
BookmarkTreeNode.prototype.index;


/** @type {string|undefined} */
BookmarkTreeNode.prototype.url;


/** @type {string} */
BookmarkTreeNode.prototype.title;


/** @type {number|undefined} */
BookmarkTreeNode.prototype.dateAdded;


/** @type {number|undefined} */
BookmarkTreeNode.prototype.dateGroupModified;


/** @type {string|undefined} */
BookmarkTreeNode.prototype.unmodifiable;


/** @type {!Array<!BookmarkTreeNode>|undefined} */
BookmarkTreeNode.prototype.children;



/**
 * @see https://developer.chrome.com/extensions/dev/cookies.html#type-Cookie
 * @constructor
 */
function Cookie() {}


/** @type {string} */
Cookie.prototype.name;


/** @type {string} */
Cookie.prototype.value;


/** @type {string} */
Cookie.prototype.domain;


/** @type {boolean} */
Cookie.prototype.hostOnly;


/** @type {string} */
Cookie.prototype.path;


/** @type {boolean} */
Cookie.prototype.secure;


/** @type {boolean} */
Cookie.prototype.httpOnly;


/** @type {boolean} */
Cookie.prototype.session;


/** @type {number} */
Cookie.prototype.expirationDate;


/** @type {string} */
Cookie.prototype.storeId;



/**
 * @see https://developer.chrome.com/extensions/dev/cookies.html#type-CookieStore
 * @constructor
 */
function CookieStore() {}


/** @type {string} */
CookieStore.prototype.id;


/** @type {Array<number>} */
CookieStore.prototype.tabIds;



/**
 * @see https://developer.chrome.com/docs/extensions/reference/contextMenus/#type-OnClickData
 * @constructor
 */
function OnClickData() {}


/** @type {number|string} */
OnClickData.prototype.menuItemId;


/** @type {number|string} */
OnClickData.prototype.parentMenuItemId;


/** @type {string} */
OnClickData.prototype.mediaType;


/** @type {string} */
OnClickData.prototype.linkUrl;


/** @type {string} */
OnClickData.prototype.srcUrl;


/** @type {string} */
OnClickData.prototype.pageUrl;


/** @type {string} */
OnClickData.prototype.frameUrl;


/** @type {number} */
OnClickData.prototype.frameId;


/** @type {string} */
OnClickData.prototype.selectionText;


/** @type {boolean} */
OnClickData.prototype.editable;


/** @type {boolean} */
OnClickData.prototype.wasChecked;


/** @type {boolean} */
OnClickData.prototype.checked;



/**
 * @see https://developer.chrome.com/extensions/debugger.html#type-Debuggee
 * @constructor
 */
function Debuggee() {}


/** @type {number} */
Debuggee.prototype.tabId;


/**
 * @see https://developer.chrome.com/extensions/contentSettings.html#type-ResourceIdentifier
 * @constructor
 */
function ResourceIdentifier() {}


/** @type {string} */
ResourceIdentifier.prototype.id;


/** @type {string} */
ResourceIdentifier.prototype.description;



/**
 * @see https://developer.chrome.com/extensions/contentSettings.html#type-ContentSetting
 * @constructor
 */
function ContentSetting() {}


/**
 * @param {!Object<string,string>} details Settings details.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
ContentSetting.prototype.clear = function(details, opt_callback) {};


/**
 * @param {!Object<string,(string|boolean|ResourceIdentifier)>} details
 *     Settings details.
 * @param {function({setting: *}): void} callback Callback function.
 * @return {undefined}
 */
ContentSetting.prototype.get = function(details, callback) {};


/**
 * @param {function(): void} callback Callback function.
 * @return {undefined}
 */
ContentSetting.prototype.getResourceIdentifiers = function(callback) {};


/**
 * @param {!Object<string,(string|ResourceIdentifier)>} details
 *     Settings details.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
ContentSetting.prototype.set = function(details, opt_callback) {};



/**
 * @see https://developer.chrome.com/extensions/history.html#type-HistoryItem
 * @constructor
 */
function HistoryItem() {}


/** @type {string} */
HistoryItem.prototype.id;


/** @type {string} */
HistoryItem.prototype.url;


/** @type {string} */
HistoryItem.prototype.title;


/** @type {number} */
HistoryItem.prototype.lastVisitTime;


/** @type {number} */
HistoryItem.prototype.visitCount;


/** @type {number} */
HistoryItem.prototype.typedCount;



/**
 * @see https://developer.chrome.com/extensions/history.html#type-VisitItem
 * @constructor
 */
function VisitItem() {}


/** @type {string} */
VisitItem.prototype.id;


/** @type {string} */
VisitItem.prototype.visitId;


/** @type {number} */
VisitItem.prototype.visitTime;


/** @type {string} */
VisitItem.prototype.referringVisitId;


/** @type {string} */
VisitItem.prototype.transition;



/**
 * @see https://developer.chrome.com/extensions/fileBrowserHandler.html#type-FileHandlerExecuteEventDetails
 * @constructor
 */
function FileHandlerExecuteEventDetails() {}


/** @type {!Array<!FileEntry>} */
FileHandlerExecuteEventDetails.prototype.entries;


/** @type {number|undefined} */
FileHandlerExecuteEventDetails.prototype.tab_id;



/**
 * @see https://developer.chrome.com/extensions/input.ime.html#type-KeyboardEvent
 * @constructor
 */
function ChromeKeyboardEvent() {}


/** @type {string} */
ChromeKeyboardEvent.prototype.type;


/** @type {string} */
ChromeKeyboardEvent.prototype.requestId;


/** @type {string|undefined} */
ChromeKeyboardEvent.prototype.extensionId;


/** @type {string} */
ChromeKeyboardEvent.prototype.key;


/** @type {string} */
ChromeKeyboardEvent.prototype.code;


/** @type {number|undefined} */
ChromeKeyboardEvent.prototype.keyCode;


/** @type {boolean|undefined} */
ChromeKeyboardEvent.prototype.altKey;


/** @type {boolean|undefined} */
ChromeKeyboardEvent.prototype.altgrKey;


/** @type {boolean|undefined} */
ChromeKeyboardEvent.prototype.ctrlKey;


/** @type {boolean|undefined} */
ChromeKeyboardEvent.prototype.shiftKey;


/** @type {boolean|undefined} */
ChromeKeyboardEvent.prototype.capsLock;



/**
 * @see https://developer.chrome.com/extensions/input.ime.html#type-InputContext
 * @constructor
 */
chrome.input.ime.InputContext = function() {};


/** @type {number} */
chrome.input.ime.InputContext.prototype.contextID;


/** @type {string} */
chrome.input.ime.InputContext.prototype.type;


/** @type {boolean} */
chrome.input.ime.InputContext.prototype.autoCorrect;


/** @type {boolean} */
chrome.input.ime.InputContext.prototype.autoComplete;


/** @type {string} */
chrome.input.ime.InputContext.prototype.autoCapitalize;


/** @type {boolean} */
chrome.input.ime.InputContext.prototype.spellCheck;


/** @type {boolean} */
chrome.input.ime.InputContext.prototype.shouldDoLearning;


/**
 * @see https://developer.chrome.com/extensions/proxy.html#type-ProxyServer
 * @constructor
 */
function ProxyServer() {}


/** @type {string} */
ProxyServer.prototype.scheme;


/** @type {string} */
ProxyServer.prototype.host;


/** @type {number} */
ProxyServer.prototype.port;



/**
 * @see https://developer.chrome.com/extensions/proxy.html#type-ProxyRules
 * @constructor
 */
function ProxyRules() {}


/** @type {ProxyServer} */
ProxyRules.prototype.singleProxy;


/** @type {ProxyServer} */
ProxyRules.prototype.proxyForHttp;


/** @type {ProxyServer} */
ProxyRules.prototype.proxyForHttps;


/** @type {ProxyServer} */
ProxyRules.prototype.proxyForFtp;


/** @type {ProxyServer} */
ProxyRules.prototype.fallbackProxy;


/** @type {!Array<string>} */
ProxyRules.prototype.bypassList;



/**
 * @see https://developer.chrome.com/extensions/proxy.html#type-PacScript
 * @constructor
 */
function PacScript() {}


/** @type {string} */
PacScript.prototype.url;


/** @type {string} */
PacScript.prototype.data;


/** @type {boolean} */
PacScript.prototype.mandatory;



/**
 * @see https://developer.chrome.com/extensions/proxy.html#type-ProxyConfig
 * @constructor
 */
function ProxyConfig() {}


/** @type {ProxyRules} */
ProxyConfig.prototype.rules;


/** @type {PacScript} */
ProxyConfig.prototype.pacScript;


/** @type {string} */
ProxyConfig.prototype.mode;



/**
 * The event listener for Storage receives an Object mapping each
 * key that changed to its corresponding StorageChange for that item.
 *
 * Listener will receive an object that maps each key to its StorageChange,
 * and the namespace ("sync" or "local") of the storage area the changes
 * are for.
 * @see https://developer.chrome.com/extensions/storage#event-onChanged
 * @interface
 * @extends {ChromeBaseEvent<function(!Object<string, !StorageChange>, string)>}
 */
function StorageChangeEvent() {}


/**
 * The event listener for StorageArea receives an Object mapping each
 * key that changed to its corresponding StorageChange for that item.
 *
 * Listener will receive an object that maps each key to its StorageChange.
 * @see https://developer.chrome.com/extensions/storage#type-StorageArea
 * @interface
 * @extends {ChromeBaseEvent<function(!Object<string, !StorageChange>)>}
 */
function StorageAreaChangeEvent() {}


/**
 * @see https://developer.chrome.com/extensions/storage.html#type-StorageChange
 * @constructor
 */
function StorageChange() {}


/** @type {?} */
StorageChange.prototype.oldValue;


/** @type {?} */
StorageChange.prototype.newValue;



/**
 * @see https://developer.chrome.com/extensions/storage.html#type-StorageArea
 * @constructor
 */
function StorageArea() {}


/**
 * @param {(string|!Array<string>|!Object|null|function(!Object))=}
 * keysOrCallback
 *    A single key to get, list of keys to get, or a dictionary
 *    specifying default values (see description of the
 *    object). An empty list or object will return an empty
 *    result object. Pass in null to get the entire contents of storage.
 * @param {function(!Object)=} opt_callback Callback with storage items, or null
 *    on failure.
 * @return {undefined}
 */
StorageArea.prototype.get = function(keysOrCallback, opt_callback) {};


/**
 * @param {(string|!Array<string>|null|function(!Object))=} keysOrCallback
 *    A single key or list of keys to get the total usage for. An empty list
 *    will return 0. Pass in null to get the total usage of all of storage.
 * @param {function(number)=} opt_callback
 *    Callback with the amount of space being used by storage.
 * @return {undefined}
 */
StorageArea.prototype.getBytesInUse = function(keysOrCallback, opt_callback) {};


/**
 * @param {!Object<string>} items
 *    Object specifying items to augment storage
 *    with. Values that cannot be serialized (functions, etc) will be ignored.
 * @param {function()=} opt_callback Callback.
 * @return {undefined}
 */
StorageArea.prototype.set = function(items, opt_callback) {};


/**
 * @param {(string|!Array<string>)} keys
 *    A single key or a list of keys for items to remove.
 * @param {function()=} opt_callback Callback.
 * @return {undefined}
 */
StorageArea.prototype.remove = function(keys, opt_callback) {};


/**
 * Removes all items from storage.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
StorageArea.prototype.clear = function(opt_callback) {};


/**
 * Fired when one or more items change.
 * @type {!StorageAreaChangeEvent}
 */
StorageArea.prototype.onChanged;


/**
 * @see https://developer.chrome.com/extensions/types.html#type-ChromeSetting
 * @constructor
 */
function ChromeSetting() {}


/**
 * @param {Object} details Object with a 'scope' (string) key.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
ChromeSetting.prototype.clear = function(details, opt_callback) {};


/**
 * @param {Object} details Object with an 'incognito' (boolean) key.
 * @param {function(Object<string, *>): void} callback Callback function.
 * @return {undefined}
 */
ChromeSetting.prototype.get = function(details, callback) {};


/**
 * @param {Object} details Object with a 'value' (*) key and an optional
 *     'scope' (string) key.
 * @param {function(): void=} opt_callback Callback function.
 * @return {undefined}
 */
ChromeSetting.prototype.set = function(details, opt_callback) {};


/** @type {!ChromeObjectEvent} */
ChromeSetting.prototype.onChange;



/**
 * @see https://developer.chrome.com/extensions/webRequest#type-RequestFilter
 * @typedef {?{
 *   urls: !Array<string>,
 *   types: (!Array<string>|undefined),
 *   tabId: (number|undefined),
 *   windowId: (number|undefined),
 * }}
 */
var RequestFilter;



/**
 * @see https://developer.chrome.com/extensions/webRequest#type-HttpHeaders
 * @typedef {?{
 *   name: string,
 *   value: (string|undefined),
 *   binaryValue: (!Array<number>|undefined),
 * }}
 */
var HttpHeader;



/**
 * @see https://developer.chrome.com/extensions/webRequest#type-HttpHeaders
 * @typedef {?Array<!HttpHeader>}
 */
chrome.webRequest.HttpHeaders;



/**
 * @see https://developer.chrome.com/extensions/webRequest#type-BlockingResponse
 * @typedef {?{
 *   cancel: (boolean|undefined),
 *   redirectUrl: (string|undefined),
 *   requestHeaders: (!chrome.webRequest.HttpHeaders|undefined),
 *   responseHeaders: (!chrome.webRequest.HttpHeaders|undefined),
 *   authCredentials: (!{username: string, password: string}|undefined),
 * }}
 */
var BlockingResponse;



/**
 * @see http://developer.chrome.com/extensions/pushMessaging.html#type-Message
 * @constructor
 */
chrome.pushMessaging.Message = function() {};


/**
 * @type {number}
 */
chrome.pushMessaging.Message.prototype.subchannelId;


/**
 * @type {string}
 */
chrome.pushMessaging.Message.prototype.payload;



/**
 * @see http://developer.chrome.com/extensions/pushMessaging.html#type-ChannelIdResult
 * @constructor
 */
chrome.pushMessaging.ChannelIdResult = function() {};


/**
 * @type {string}
 */
chrome.pushMessaging.ChannelIdResult.prototype.channelId;


/**
 * The `chrome.fileSystem` API makes use of the Entry and FileEntry types
 * defined in {@code javascript/externs/fileapi.js}.
 * @const
 * @see http://developer.chrome.com/apps/fileSystem.html
 */
chrome.fileSystem = {};


/**
 * @param {!Entry} entry The entry to get the display path for. The entry can
 *     originally be obtained through
 *     `chrome.fileSystem.chooseEntry` or
 *     `chrome.fileSystem.restoreEntry`.
 * @param {function(string)} callback A success callback.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-getDisplayPath
 * @return {undefined}
 */
chrome.fileSystem.getDisplayPath = function(entry, callback) {};


/**
 * @param {!Entry} entry The entry to get a writable entry for.
 * @param {function(!Entry)} callback A success callback.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-getWritableEntry
 * @return {undefined}
 */
chrome.fileSystem.getWritableEntry = function(entry, callback) {};


/**
 * @param {!Entry} entry The entry to query writability.
 * @param {function(boolean)} callback A success callback.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-isWritableEntry
 * @return {undefined}
 */
chrome.fileSystem.isWritableEntry = function(entry, callback) {};


/**
 * @typedef {{
 *   description: (string|undefined),
 *   mimeTypes: (!Array<string>|undefined),
 *   extensions: (!Array<string>|undefined)
 * }}
 * @see http://developer.chrome.com/apps/fileSystem.html#method-chooseEntry
 */
chrome.fileSystem.AcceptsOption;


/**
 * @typedef {{
 *   type: (string|undefined),
 *   suggestedName: (string|undefined),
 *   accepts: (!Array<!chrome.fileSystem.AcceptsOption>|undefined),
 *   acceptsAllTypes: (boolean|undefined),
 *   acceptsMultiple: (boolean|undefined)
 * }}
 * @see http://developer.chrome.com/apps/fileSystem.html#method-chooseEntry
 */
chrome.fileSystem.ChooseEntryOptions;


/**
 * @typedef {?{
 *   volumeId: string,
 *   writable: (boolean|undefined)
 * }}
 * @see http://developer.chrome.com/apps/fileSystem.html#method-requestFileSystem
 */
chrome.fileSystem.RequestFileSystemOptions;


/**
 * @see http://developer.chrome.com/apps/fileSystem.html#method-getVolumeList
 * @constructor
 */
chrome.fileSystem.Volume = function() {};


/** @type {string} */
chrome.fileSystem.Volume.prototype.volumeId;


/** @type {boolean} */
chrome.fileSystem.Volume.prototype.writable;


/**
 * @param {!chrome.fileSystem.ChooseEntryOptions|
 *     function(Entry=, !Array<!FileEntry>=)} optionsOrCallback The
 *     options for the file prompt or the callback.
 * @param {function(Entry=, !Array<!FileEntry>=)=} opt_callback A success
 *     callback, if arg1 is options.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-chooseEntry
 * @return {undefined}
 */
chrome.fileSystem.chooseEntry = function(optionsOrCallback, opt_callback) {};


/**
 * @param {string} id The ID of the file entry to restore.
 * @param {function(!Entry)} callback A success callback.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-restoreEntry
 * @return {undefined}
 */
chrome.fileSystem.restoreEntry = function(id, callback) {};


/**
 * @param {string} id The ID of the file entry to query restorability.
 * @param {function(boolean)} callback A success callback.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-isRestorable
 * @return {undefined}
 */
chrome.fileSystem.isRestorable = function(id, callback) {};


/**
 * @param {!Entry} entry The entry to regain access to.
 * @return {string} The ID that can be passed to restoreEntry to regain access
 *     to the given file entry.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-retainEntry
 */
chrome.fileSystem.retainEntry = function(entry) {};


/**
 * @param {!chrome.fileSystem.RequestFileSystemOptions} options Options for the
 *     request.
 * @param {function(!FileSystem=)} callback A completion callback with the file
 *     system in case of a success. Otherwise the error is passed as
 *     chrome.runtime.lastError.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-requestFileSystem
 * @return {undefined}
 */
chrome.fileSystem.requestFileSystem = function(options, callback) {};


/**
 * @param {function(!Array<!chrome.fileSystem.Volume>=)} callback A completion
 *     callback with the file system list in case of a success. Otherwise the
 *     error is passed as chrome.runtime.lastError.
 * @see http://developer.chrome.com/apps/fileSystem.html#method-getVolumeList
 * @return {undefined}
 */
chrome.fileSystem.getVolumeList = function(callback) {};


/**
 * @const
 * @see https://developer.chrome.com/apps/syncFileSystem
 */
chrome.syncFileSystem = {};


/**
 * Returns a syncable filesystem backed by Google Drive. The returned
 * DOMFileSystem instance can be operated on in the same way as
 * the Temporary and Persistent file systems (see
 * http://www.w3.org/TR/file-system-api/), except that the filesystem
 * object returned for Sync FileSystem does NOT support directory
 * operations (yet). You can get a list of file entries by reading
 * the root directory (by creating a new DirectoryReader),
 * but cannot create a new directory in it.
 *
 * <p>Calling this multiple times from the same app will return the same
 * handle to the same file system.
 *
 * <p>Note this call can fail. For example, if the user is not signed in
 * to Chrome or if there is no network operation. To handle these errors
 * it is important chrome.runtime.lastError is checked in the callback.
 *
 * @param {function(!FileSystem)} callback A callback type for
 *     requestFileSystem.
 * @see https://developer.chrome.com/apps/syncFileSystem#method-requestFileSystem
 * @return {undefined}
 */
chrome.syncFileSystem.requestFileSystem = function(callback) {};


/**
 * Sets the default conflict resolution policy for the 'syncable' file
 * storage for the app. By default it is set to 'last_write_win'.
 * When conflict resolution policy is set to 'last_write_win' conflicts
 * for existing files are automatically resolved next time the file is updated.
 * `callback` can be optionally given to know if the request has
 * succeeded or not.
 *
 * @param {string} policy Any of 'last_write_win' or 'manual'
 * @param {function()=} opt_callback
 *
 * @see https://developer.chrome.com/apps/syncFileSystem#method-setConflictResolutionPolicy
 * @return {undefined}
 */
chrome.syncFileSystem.setConflictResolutionPolicy = function(
    policy, opt_callback) {};


/**
 * Gets the current conflict resolution policy.
 *
 * @param {function(string)} callback Accepting any of 'last_write_win'
 *     or 'manual'.
 * @see https://developer.chrome.com/apps/syncFileSystem#method-getConflictResolutionPolicy
 * @return {undefined}
 */
chrome.syncFileSystem.getConflictResolutionPolicy = function(callback) {};


/**
 * Returns the current usage and quota in bytes for the 'syncable' file
 * storage for the app.
 *
 * @param {!FileSystem} fileSystem
 * @param {function(!Object)} callback Taking an object substantially similar
 *     to {@code {'usageBytes': number, quotaBytes: number}}.
 * @see https://developer.chrome.com/apps/syncFileSystem#method-getUsageAndQuota
 * @return {undefined}
 */
chrome.syncFileSystem.getUsageAndQuota = function(fileSystem, callback) {};


/**
 * Returns the FileStatus for the given fileEntry. The status value can be
 * 'synced', 'pending' or 'conflicting'. Note that 'conflicting' state only
 * happens when the service's conflict resolution policy is set to 'manual'.
 *
 * @param {!Entry} fileEntry
 * @param {function(string)} callback Called with any of 'synced', 'pending'
 *     or 'conflicting'.
 *
 * @see https://developer.chrome.com/apps/syncFileSystem#method-getFileStatus
 * @return {undefined}
 */
chrome.syncFileSystem.getFileStatus = function(fileEntry, callback) {};


/**
 * Returns each FileStatus for the given fileEntry array. Typically called
 * with the result from dirReader.readEntries().
 *
 * @param {!Array<!FileEntry>} fileEntries
 * @param {function(!Array<!Object>)} callback Each object will look like:
 *     {@code {'fileEntry': Entry, 'status': string, 'error': string?}}.
 *
 * @see https://developer.chrome.com/apps/syncFileSystem#method-getFileStatuses
 * @return {undefined}
 */
chrome.syncFileSystem.getFileStatuses = function(fileEntries, callback) {};


/**
 * Since Chrome 31.
 *
 * <p>Returns the current sync backend status.
 *
 * @param {function(string)} callback Arg is any of 'initializing', 'running',
 *     'authentication_required', 'temporary_unavailable', or 'disabled'.
 *
 * @see https://developer.chrome.com/apps/syncFileSystem#method-getServiceStatus
 * @return {undefined}
 */
chrome.syncFileSystem.getServiceStatus = function(callback) {};


/**
 * Fired when an error or other status change has happened in the sync
 * backend (for example, when the sync is temporarily disabled due
 * to network or authentication error).
 *
 * @type {!ChromeObjectEvent}
 *
 * @see https://developer.chrome.com/apps/syncFileSystem#event-onServiceStatusChanged
 */
chrome.syncFileSystem.onServiceStatusChanged;


/**
 * Fired when a file has been updated by the background sync service.
 *
 * @type {!ChromeObjectEvent}
 *
 * @see https://developer.chrome.com/apps/syncFileSystem#event-onFileStatusChanged
 */
chrome.syncFileSystem.onFileStatusChanged;


/**
 * @const
 * @see http://developer.chrome.com/extensions/alarms.html
 */
chrome.alarms = {};


/**
 * Creates an alarm. Near the time(s) specified by alarmInfo, the onAlarm event
 * is fired. If there is another alarm with the same name (or no name if none is
 * specified), it will be cancelled and replaced by this alarm.
 * @param {string|!chrome.alarms.AlarmCreateInfo} nameOrAlarmCreateInfo Either
 *     the name to identify this alarm or the info used to create the alarm. If
 *     no name is passed, the empty string is used to identify the alarm.
 * @param {!chrome.alarms.AlarmCreateInfo=} opt_alarmInfo If a name was passed
 *     as arg1, the info used to create the alarm.
 * @see http://developer.chrome.com/extensions/alarms.html#method-create
 * @return {undefined}
 */
chrome.alarms.create = function(nameOrAlarmCreateInfo, opt_alarmInfo) {};


/**
 * Retrieves details about the specified alarm.
 * @param {string|function(!chrome.alarms.Alarm)} nameOrCallback The name
 *     of the alarm to get or the callback to invoke with the alarm. If no name
 *     is passed, the empty string is used to get the alarm.
 * @param {function(!chrome.alarms.Alarm)=} opt_callback If a name was passed
 *     as arg1, the callback to invoke with the alarm.
 * @see http://developer.chrome.com/extensions/alarms.html#method-get
 * @return {undefined}
 */
chrome.alarms.get = function(nameOrCallback, opt_callback) {};


/**
 * Gets an array of all the alarms.
 * @param {function(!Array<!chrome.alarms.Alarm>)} callback
 * @see http://developer.chrome.com/extensions/alarms.html#method-getAll
 * @return {undefined}
 */
chrome.alarms.getAll = function(callback) {};


/**
 * Clears the alarm with the given name.
 * @param {string=} opt_name
 * @param {function(boolean)=} opt_callback A callback that will be called with
 *     a boolean for whether the alarm was cleared.
 * @see http://developer.chrome.com/extensions/alarms.html#method-clear
 * @return {undefined}
 */
chrome.alarms.clear = function(opt_name, opt_callback) {};


/**
 * Clears all alarms.
 * @param {function(boolean)=} opt_callback A callback that will be called with
 *     a boolean for whether the alarms were cleared.
 * @see http://developer.chrome.com/extensions/alarms.html#method-clearAll
 * @return {undefined}
 */
chrome.alarms.clearAll = function(opt_callback) {};


/**
 * Fired when an alarm has elapsed. Useful for event pages.
 * @type {!chrome.alarms.AlarmEvent}
 * @see http://developer.chrome.com/extensions/alarms.html#event-onAlarm
 */
chrome.alarms.onAlarm;



/**
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.alarms.Alarm)>}
 */
chrome.alarms.AlarmEvent = function() {};


/**
 * @interface
 * @see http://developer.chrome.com/extensions/alarms.html#type-Alarm
 */
chrome.alarms.Alarm = function() {};


/**
 * Name of this alarm.
 * @type {string}
 */
chrome.alarms.Alarm.prototype.name;


/**
 * Time at which this alarm was scheduled to fire, in milliseconds past the
 * epoch (e.g. Date.now() + n). For performance reasons, the alarm may have been
 * delayed an arbitrary amount beyond this.
 * @type {number}
 */
chrome.alarms.Alarm.prototype.scheduledTime;


/**
 * If not null, the alarm is a repeating alarm and will fire again in
 * periodInMinutes minutes.
 * @type {?number}
 */
chrome.alarms.Alarm.prototype.periodInMinutes;


/**
 * @typedef {{
 *   when: (number|undefined),
 *   delayInMinutes: (number|undefined),
 *   periodInMinutes: (number|undefined)
 * }}
 * @see http://developer.chrome.com/extensions/alarms.html#method-create
 */
chrome.alarms.AlarmCreateInfo;


/**
 * @see https://developer.chrome.com/apps/hid
 * @const
 */
chrome.hid = {};



/**
 * @constructor
 * @see https://developer.chrome.com/apps/hid#type-DeviceFilter
 */
chrome.hid.DeviceFilter = function() {};


/**
 * Device vendor ID.
 * @type {number|undefined}
 */
chrome.hid.DeviceFilter.prototype.vendorId;


/**
 * Device product ID, only checked if the vendor ID matches.
 * @type {number|undefined}
 */
chrome.hid.DeviceFilter.prototype.productId;


/**
 * HID usage page identifier.
 * @type {number|undefined}
 */
chrome.hid.DeviceFilter.prototype.usagePage;


/**
 * HID usage identifier, checked only if the HID usage page matches.
 * @type {number|undefined}
 */
chrome.hid.DeviceFilter.prototype.usage;


/**
 * @typedef {?{
 *   vendorId: (number|undefined),
 *   productId: (number|undefined),
 *   filters: (!Array<!chrome.hid.DeviceFilter>|undefined)
 * }}
 * Deprecated since Chrome 39: vendorId, productId
 * Since Chrome 39: filters
 * @see https://developer.chrome.com/apps/hid#method-getDevices
 */
chrome.hid.HidGetDevicesOptions;


/**
 * @typedef {?{
 *   multiple: (boolean|undefined),
 *   filters: (!Array<!chrome.hid.DeviceFilter>|undefined)
 * }}
 * @see https://developer.chrome.com/apps/hid#method-getUserSelectedDevices
 */
chrome.hid.HidGetUserSelectedDevicesOptions;


/**
 * @typedef {?{
 *   usagePage: number,
 *   usage: number,
 *   reportIds: !Array<number>
 * }}
 * @see https://developer.chrome.com/apps/hid#method-getDevices
 */
chrome.hid.HidDeviceUsage;



/**
 * @constructor
 * @see https://developer.chrome.com/apps/hid#type-HidDeviceInfo
 */
chrome.hid.HidDeviceInfo = function() {};


/**
 * Opaque device ID.
 * @type {number}
 */
chrome.hid.HidDeviceInfo.prototype.deviceId;


/**
 * Vendor ID.
 * @type {number}
 */
chrome.hid.HidDeviceInfo.prototype.vendorId;


/**
 * Product ID.
 * @type {number}
 */
chrome.hid.HidDeviceInfo.prototype.productId;


/**
 * The product name read from the device, if available.
 * Since Chrome 46.
 * @type {string}
 */
chrome.hid.HidDeviceInfo.prototype.productName;


/**
 * The serial number read from the device, if available.
 * Since Chrome 46.
 * @type {string}
 */
chrome.hid.HidDeviceInfo.prototype.serialNumber;


/**
 * Top-level collections from this device's report descriptors.
 * @type {!Array<!chrome.hid.HidDeviceUsage>}
 */
chrome.hid.HidDeviceInfo.prototype.collections;


/**
 * Top-level collection's maximum input report size.
 * @type {number}
 */
chrome.hid.HidDeviceInfo.prototype.maxInputReportSize;


/**
 * Top-level collection's maximum output report size.
 * @type {number}
 */
chrome.hid.HidDeviceInfo.prototype.maxOutputReportSize;


/**
 * Top-level collection's maximum feature report size.
 * @type {number}
 */
chrome.hid.HidDeviceInfo.prototype.maxFeatureReportSize;


/**
 * Raw device report descriptor (not available on Windows).
 * Since Chrome 42.
 * @type {!ArrayBuffer}
 */
chrome.hid.HidDeviceInfo.prototype.reportDescriptor;


/**
 * @typedef {?{
 *   connectionId: number
 * }}
 * @see https://developer.chrome.com/apps/hid#method-connect
 */
chrome.hid.HidConnectInfo;


/**
 * @see https://developer.chrome.com/apps/hid#method-getDevices
 * Enumerates all the connected HID devices specified by the
 * vendorId/productId/interfaceId tuple.
 * @param {!chrome.hid.HidGetDevicesOptions} options The properties to search
 *     for on target devices.
 * @param {function(!Array<!chrome.hid.HidDeviceInfo>)} callback Invoked with a
 *     list of |HidDeviceInfo|s on complete.
 * @return {undefined}
 */
chrome.hid.getDevices = function(options, callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-getUserSelectedDevices
 * Presents a device picker to the user and returns HidDeviceInfo objects for
 * the devices selected. If the user cancels the picker devices will be empty. A
 * user gesture is required for the dialog to display. Without a user gesture,
 * the callback will run as though the user cancelled. If multiple filters are
 * provided devices matching any filter will be displayed.
 * @param {!chrome.hid.HidGetUserSelectedDevicesOptions} options
 * @param {function(!Array<!chrome.hid.HidDeviceInfo>)} callback Invoked with a
 *     list of |HidDeviceInfo|s on complete.
 * @return {undefined}
 */
chrome.hid.getUserSelectedDevices = function(options, callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-connect
 * Opens a connection to a HID device for communication.
 * @param {number} deviceId The ID of the device to open.
 * @param {function(!Object=)} callback Invoked with an |HidConnectInfo| if the
 *     connection succeeds, or undefined if it fails.
 * @return {undefined}
 */
chrome.hid.connect = function(deviceId, callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-disconnect
 * Disconnects from a device.
 * @param {number} connectionId The connection to close.
 * @param {function()=} opt_callback The callback to invoke once the connection
 *     is closed.
 * @return {undefined}
 */
chrome.hid.disconnect = function(connectionId, opt_callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-receive
 * Receives an input report from an HID device.
 * @param {number} connectionId The connection from which to receive the report.
 * @param {function(number, !ArrayBuffer)} callback The callback to invoke with
 *     the received report.
 * @return {undefined}
 */
chrome.hid.receive = function(connectionId, callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-send
 * Sends an output report to an HID device.
 * @param {number} connectionId The connection to which to send the report.
 * @param {number} reportId The report ID to use, or 0 if none.
 * @param {!ArrayBuffer} data The report data.
 * @param {function()} callback The callback to invoke once the write is
 *     finished.
 * @return {undefined}
 */
chrome.hid.send = function(connectionId, reportId, data, callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-receiveFeatureReport
 * Receives a feature report from the device.
 * @param {number} connectionId The connection from which to read the feature
 *     report.
 * @param {number} reportId The report ID to use, or 0 if none.
 * @param {number} size The size of the feature report to receive.
 * @param {function(!ArrayBuffer)} callback The callback to invoke with the
 *     received report.
 * @return {undefined}
 */
chrome.hid.receiveFeatureReport = function(
    connectionId, reportId, size, callback) {};


/**
 * @see https://developer.chrome.com/apps/hid#method-sendFeatureReport
 * Sends a feature report to the device.
 * @param {number} connectionId The connection to which to send the feature
 *     report.
 * @param {number} reportId The report ID to use, or 0 if none.
 * @param {!ArrayBuffer} data The report data.
 * @param {function()} callback The callback to invoke once the write is
 *     finished.
 * @return {undefined}
 */
chrome.hid.sendFeatureReport = function(
    connectionId, reportId, data, callback) {};



/**
 * Event whose listeners take an HidDeviceInfo parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.hid.HidDeviceInfo)>}
 */
chrome.hid.DeviceAddedEvent = function() {};


/**
 * @type {!chrome.hid.DeviceAddedEvent}
 * @see https://developer.chrome.com/apps/hid#event-onDeviceAdded
 */
chrome.hid.onDeviceAdded;


/**
 * @type {!ChromeNumberEvent}
 * @see https://developer.chrome.com/apps/hid#event-onDeviceRemoved
 */
chrome.hid.onDeviceRemoved;


/**
 * @see http://developer.chrome.com/extensions/notifications.html
 * @const
 */
chrome.notifications = {};


/**
 * @typedef {{
 *   title: string,
 *   iconUrl: (string|undefined)
 * }}
 * @see http://developer.chrome.com/extensions/notifications.html#type-NotificationOptions
 */
chrome.notifications.NotificationButton;


/**
 * @typedef {{
 *   title: string,
 *   message: string
 * }}
 * @see http://developer.chrome.com/extensions/notifications.html#type-NotificationOptions
 */
chrome.notifications.NotificationItem;


/**
 * @typedef {{
 *   type: (string|undefined),
 *   iconUrl: (string|undefined),
 *   appIconMaskUrl: (string|undefined),
 *   title: (string|undefined),
 *   message: (string|undefined),
 *   contextMessage: (string|undefined),
 *   priority: (number|undefined),
 *   eventTime: (number|undefined),
 *   buttons: (!Array<!chrome.notifications.NotificationButton>|undefined),
 *   imageUrl: (string|undefined),
 *   items: (!Array<!chrome.notifications.NotificationItem>|undefined),
 *   progress: (number|undefined),
 *   isClickable: (boolean|undefined),
 *   requireInteraction: (boolean|undefined),
 *   silent: (boolean|undefined)
 * }}
 * @see http://developer.chrome.com/extensions/notifications.html#type-NotificationOptions
 */
chrome.notifications.NotificationOptions;


/**
 * @typedef {function(boolean): void}
 * @see http://developer.chrome.com/extensions/notifications.html#method-update
 * @see http://developer.chrome.com/extensions/notifications.html#method-clear
 */
chrome.notifications.BooleanCallback;


/**
 * @typedef {function(!Object): void}
 * @see http://developer.chrome.com/extensions/notifications.html#method-getAll
 */
chrome.notifications.ObjectCallback;


/**
 * @typedef {function(string, boolean): void}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onClosed
 */
chrome.notifications.ClosedCallback;


/**
 * @typedef {function(string, number): void}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onButtonClicked
 */
chrome.notifications.ButtonCallback;


/**
 * @param {string|!chrome.notifications.NotificationOptions}
 *     notificationIdOrOptions
 * @param {(!chrome.notifications.NotificationOptions|function(string): void)=}
 *     opt_optionsOrCallback
 * @param {(function(string): void)=} opt_callback
 * @see http://developer.chrome.com/extensions/notifications.html#method-create
 * @return {undefined}
 */
chrome.notifications.create = function(
    notificationIdOrOptions, opt_optionsOrCallback, opt_callback) {};


/**
 * @param {string} notificationId
 * @param {!chrome.notifications.NotificationOptions} options
 * @param {chrome.notifications.BooleanCallback=} opt_callback
 * @see http://developer.chrome.com/extensions/notifications.html#method-update
 * @return {undefined}
 */
chrome.notifications.update = function(
    notificationId, options, opt_callback) {};


/**
 * @param {string} notificationId
 * @param {!chrome.notifications.BooleanCallback=} opt_callback
 * @see http://developer.chrome.com/extensions/notifications.html#method-clear
 * @return {undefined}
 */
chrome.notifications.clear = function(notificationId, opt_callback) {};


/**
 * @see http://developer.chrome.com/extensions/notifications.html#method-getAll
 * @param {!chrome.notifications.ObjectCallback} callback
 * @return {undefined}
 */
chrome.notifications.getAll = function(callback) {};


/**
 * @see http://developer.chrome.com/extensions/notifications.html#method-getPermissionLevel
 * @param {function(string): void} callback takes 'granted' or 'denied'
 * @return {undefined}
 */
chrome.notifications.getPermissionLevel = function(callback) {};


/**
 * @type {!chrome.notifications.ClosedEvent}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onClosed
 */
chrome.notifications.onClosed;


/**
 * The user clicked a non-button area of the notification. Callback receives a
 * notificationId.
 * @type {!ChromeStringEvent}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onClicked
 */
chrome.notifications.onClicked;


/**
 * @type {!chrome.notifications.ButtonClickedEvent}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onButtonClicked
 */
chrome.notifications.onButtonClicked;


/**
 * Indicates permission level change. Callback should expect 'granted' or
 * 'denied'.
 * @type {!ChromeStringEvent}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onPermissionLevelChanged
 */
chrome.notifications.onPermissionLevelChanged;


/**
 * @type {!ChromeEvent}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onShowSettings
 */
chrome.notifications.onShowSettings;



/**
 * @interface
 * @extends {ChromeBaseEvent<!chrome.notifications.ClosedCallback>}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onClosed
 */
chrome.notifications.ClosedEvent = function() {};


/**
 * @interface
 * @extends {ChromeBaseEvent<!chrome.notifications.ButtonCallback>}
 * @see http://developer.chrome.com/extensions/notifications.html#event-onButtonClicked
 */
chrome.notifications.ButtonClickedEvent = function() {};


/**
 * @const
 * @see http://developer.chrome.com/apps/system_storage.html
 */
chrome.system.storage = {};



/** @constructor */
chrome.system.storage.StorageUnitInfo = function() {};


/** @type {string} */
chrome.system.storage.StorageUnitInfo.prototype.id;


/** @type {string} */
chrome.system.storage.StorageUnitInfo.prototype.name;


/** @type {string} Any of 'fixed', 'removable', or 'unknown' */
chrome.system.storage.StorageUnitInfo.prototype.type;


/** @type {number} */
chrome.system.storage.StorageUnitInfo.prototype.capacity;



/**
 * Event whose listeners take a StorageUnitInfoEvent parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.system.storage.StorageUnitInfo)>}
 */
chrome.system.storage.StorageUnitInfoEvent = function() {};


/** @type {chrome.system.storage.StorageUnitInfoEvent} */
chrome.system.storage.onAttached;


/** @type {!ChromeStringEvent} */
chrome.system.storage.onDetached;


/**
 * Gets the storage information from the system.
 * @param {function(!Array<!chrome.system.storage.StorageUnitInfo>)} callback
 * @return {undefined}
 */
chrome.system.storage.getInfo = function(callback) {};


/**
 * Ejects a removable storage device.
 * @param {string} id The transient device ID from StorageUnitInfo.
 * @param {function(string)} callback Callback function where the value
 *     is any of: "success", "in_use", "no_such_device", "failure"
 * @return {undefined}
 */
chrome.system.storage.ejectDevice = function(id, callback) {};


/**
 * Gets the available capacity of a specified storage device.
 * @param {string} id The transient device ID from StorageUnitInfo.
 * @param {function(Object<string, number>)} callback A callback function that
 *     accepts an object with `id` and `availableCapacity` fields.
 * @return {undefined}
 */
chrome.system.storage.getAvailableCapacity = function(id, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html
 * @const
 */
chrome.usb = {};



/** @constructor */
chrome.usb.Device = function Device() {};


/** @type {number} */
chrome.usb.Device.prototype.device;


/** @type {number} */
chrome.usb.Device.prototype.vendorId;


/** @type {number} */
chrome.usb.Device.prototype.productId;


/** @type {number} */
chrome.usb.Device.prototype.version;


/**
 * The product name read from the device, if available.
 * Since Chrome 46.
 * @type {string}
 */
chrome.usb.Device.prototype.productName;

/**
 * The manufacturer name read from the device, if available.
 * Since Chrome 46.
 * @type {string}
 */
chrome.usb.Device.prototype.manufacturerName;

/**
 * The serial number read from the device, if available.
 * Since Chrome 46.
 * @type {string}
 */
chrome.usb.Device.prototype.serialNumber;



/** @constructor */
chrome.usb.ConnectionHandle = function ConnectionHandle() {};


/** @type {number} */
chrome.usb.ConnectionHandle.prototype.handle;


/** @type {number} */
chrome.usb.ConnectionHandle.prototype.vendorId;


/** @type {number} */
chrome.usb.ConnectionHandle.prototype.productId;


/**
 * @typedef {?{
 *   direction: string,
 *   endpoint: number,
 *   length: (number|undefined),
 *   data: (!ArrayBuffer|undefined),
 *   timeout: (number|undefined)
 * }}
 */
chrome.usb.GenericTransferInfo;


/**
 * @typedef {?{
 *   direction: string,
 *   recipient: string,
 *   requestType: string,
 *   request: number,
 *   value: number,
 *   index: number,
 *   length: (number|undefined),
 *   data: (!ArrayBuffer|undefined),
 *   timeout: (number|undefined)
 * }}
 */
chrome.usb.ControlTransferInfo;



/** @constructor */
chrome.usb.TransferResultInfo = function() {};


/** @type {number|undefined} */
chrome.usb.TransferResultInfo.prototype.resultCode;


/** @type {!ArrayBuffer|undefined} */
chrome.usb.TransferResultInfo.prototype.data;


/**
 * @typedef {?{
 *   vendorId: number,
 *   productId: number,
 *   interfaceId: (number|undefined)
 * }}
 */
chrome.usb.FindDevicesOptions;


/**
 * @typedef {?{
 *   vendorId: (number|undefined),
 *   productId: (number|undefined),
 *   interfaceClass: (number|undefined),
 *   interfaceSubclass: (number|undefined),
 *   interfaceProtocol: (number|undefined)
 * }}
 */
chrome.usb.DeviceFilter;


/**
 * @typedef {?{
 *   vendorId: (number|undefined),
 *   productId: (number|undefined),
 *   filters: (!Array<!chrome.usb.DeviceFilter>|undefined)
 * }}
 */
chrome.usb.GetDevicesOptions;


/**
 * @typedef {?{
 *   multiple: (boolean|undefined),
 *   filters: (!Array<!chrome.usb.DeviceFilter>|undefined)
 * }}
 */
chrome.usb.GetUserSelectedDevicesOptions;


/**
 * @typedef {?{
 *   transferInfo: !chrome.usb.GenericTransferInfo,
 *   packets: number,
 *   packetLength: number
 * }}
 */
chrome.usb.IsochronousTransferInfo;



/** @constructor */
chrome.usb.EndpointDescriptor = function() {};


/** @type {number} */
chrome.usb.EndpointDescriptor.prototype.address;

/** @type {string} */
chrome.usb.EndpointDescriptor.prototype.type;


/** @type {string} */
chrome.usb.EndpointDescriptor.prototype.direction;


/** @type {number} */
chrome.usb.EndpointDescriptor.prototype.maximumPacketSize;


/** @type {(string|undefined)} */
chrome.usb.EndpointDescriptor.prototype.synchronization;


/** @type {(string|undefined)} */
chrome.usb.EndpointDescriptor.prototype.usage;


/** @type {(number|undefined)} */
chrome.usb.EndpointDescriptor.prototype.pollingInterval;


/** @type {!ArrayBuffer} */
chrome.usb.EndpointDescriptor.prototype.extra_data;



/** @constructor */
chrome.usb.InterfaceDescriptor = function() {};


/** @type {number} */
chrome.usb.InterfaceDescriptor.prototype.interfaceNumber;


/** @type {number} */
chrome.usb.InterfaceDescriptor.prototype.alternateSetting;


/** @type {number} */
chrome.usb.InterfaceDescriptor.prototype.interfaceClass;


/** @type {number} */
chrome.usb.InterfaceDescriptor.prototype.interfaceSubclass;


/** @type {number} */
chrome.usb.InterfaceDescriptor.prototype.interfaceProtocol;


/** @type {(string|undefined)} */
chrome.usb.InterfaceDescriptor.prototype.description;


/** @type {!Array.<!chrome.usb.EndpointDescriptor>} */
chrome.usb.InterfaceDescriptor.prototype.endpoints;


/** @type {!ArrayBuffer} */
chrome.usb.InterfaceDescriptor.prototype.extra_data;



/** @constructor */
chrome.usb.ConfigDescriptor = function() {};


/** @type {boolean} */
chrome.usb.ConfigDescriptor.prototype.active;


/** @type {number} */
chrome.usb.ConfigDescriptor.prototype.configurationValue;


/** @type {string|undefined} */
chrome.usb.ConfigDescriptor.prototype.description;


/** @type {boolean} */
chrome.usb.ConfigDescriptor.prototype.selfPowered;


/** @type {boolean} */
chrome.usb.ConfigDescriptor.prototype.remoteWakeup;


/** @type {number} */
chrome.usb.ConfigDescriptor.prototype.maxPower;


/** @type {!Array<!chrome.usb.InterfaceDescriptor>} */
chrome.usb.ConfigDescriptor.prototype.interfaces;


/** @type {!ArrayBuffer} */
chrome.usb.ConfigDescriptor.prototype.extra_data;


/**
 * @see http://developer.chrome.com/apps/usb.html#method-getDevices
 * @param {!chrome.usb.GetDevicesOptions} options The properties to
 *     search for on target devices.
 * @param {function(!Array<!chrome.usb.Device>)} callback Invoked with a list
 *     of |Device|s on complete.
 * @return {undefined}
 */
chrome.usb.getDevices = function(options, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-getUserSelectedDevices
 * @param {!chrome.usb.GetUserSelectedDevicesOptions} options Configuration of
 *     the device picker dialog box.
 * @param {function(!Array<!chrome.usb.Device>)} callback Invoked with a list
 *     of |Device|s on complete.
 * @return {undefined}
 */
chrome.usb.getUserSelectedDevices = function(options, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-getConfigurations
 * @param {!chrome.usb.Device} device The device to fetch descriptors from.
 * @param {function(!Array<!chrome.usb.ConfigDescriptor>)} callback Invoked with
 *     the full set of device configuration descriptors.
 * @return {undefined}
 */
chrome.usb.getConfigurations = function(device, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-requestAccess
 * @param {!chrome.usb.Device} device The device to request access to.
 * @param {number} interfaceId
 * @param {function(boolean)} callback
 * @return {undefined}
 */
chrome.usb.requestAccess = function(device, interfaceId, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-openDevice
 * @param {!chrome.usb.Device} device The device to open.
 * @param {function(!chrome.usb.ConnectionHandle)} callback Invoked with the
 *     created ConnectionHandle on complete.
 * @return {undefined}
 */
chrome.usb.openDevice = function(device, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-findDevices
 * @param {!chrome.usb.FindDevicesOptions} options The properties to search for
 *     on target devices.
 * @param {function(!Array<!chrome.usb.ConnectionHandle>)} callback Invoked
 *     with the opened ConnectionHandle on complete.
 * @return {undefined}
 */
chrome.usb.findDevices = function(options, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-closeDevice
 * @param {!chrome.usb.ConnectionHandle} handle The connection handle to close.
 * @param {function()=} opt_callback The callback to invoke once the device is
 *     closed.
 * @return {undefined}
 */
chrome.usb.closeDevice = function(handle, opt_callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-setConfiguration
 * @param {!chrome.usb.ConnectionHandle} handle The connection handle for which
 *     to select a device configuration.
 * @param {number} configurationValue The configuration to select.
 * @param {function()} callback The callback to invoke on complete.
 * @return {undefined}
 */
chrome.usb.setConfiguration = function(handle, configurationValue, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-getConfiguration
 * @param {!chrome.usb.ConnectionHandle} handle The connection handle for which
 *     to get the current device configuration descriptor.
 * @param {function(!chrome.usb.ConfigDescriptor)} callback The callback to
 *     invoke on complete.
 * @return {undefined}
 */
chrome.usb.getConfiguration = function(handle, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-listInterfaces
 * @param {!chrome.usb.ConnectionHandle} handle The device from which the
 *     interfaces should be listed.
 * @param {function(!Array<!chrome.usb.InterfaceDescriptor>)} callback The
 *     callback to invoke when the interfaces are enumerated.
 * @return {undefined}
 */
chrome.usb.listInterfaces = function(handle, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-claimInterface
 * @param {!chrome.usb.ConnectionHandle} handle The device on which the
 *     interface is to be claimed.
 * @param {number} interfaceNumber
 * @param {function()} callback The callback to invoke once the interface is
 *     claimed.
 * @return {undefined}
 */
chrome.usb.claimInterface = function(handle, interfaceNumber, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-releaseInterface
 * @param {!chrome.usb.ConnectionHandle} handle The device on which the
 *     interface is to be released.
 * @param {number} interfaceNumber
 * @param {function()} callback The callback to invoke once the interface is
 *     released.
 * @return {undefined}
 */
chrome.usb.releaseInterface = function(handle, interfaceNumber, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-setInterfaceAlternateSetting
 * @param {!chrome.usb.ConnectionHandle} handle The device on which the
 *     interface settings are to be set.
 * @param {number} interfaceNumber
 * @param {number} alternateSetting The alternate setting to set.
 * @param {function()} callback The callback to invoke once the interface
 *     setting is set.
 * @return {undefined}
 */
chrome.usb.setInterfaceAlternateSetting = function(
    handle, interfaceNumber, alternateSetting, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-controlTransfer
 * @param {!chrome.usb.ConnectionHandle} handle A connection handle to make the
 *     transfer on.
 * @param {!chrome.usb.ControlTransferInfo} transferInfo The parameters to the
 *     transfer.
 * @param {function(!chrome.usb.TransferResultInfo)} callback Invoked once the
 *     transfer has completed.
 * @return {undefined}
 */
chrome.usb.controlTransfer = function(handle, transferInfo, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-bulkTransfer
 * @param {!chrome.usb.ConnectionHandle} handle A connection handle to make
 *     the transfer on.
 * @param {!chrome.usb.GenericTransferInfo} transferInfo The parameters to the
 *     transfer. See GenericTransferInfo.
 * @param {function(!chrome.usb.TransferResultInfo)} callback Invoked once the
 *     transfer has completed.
 * @return {undefined}
 */
chrome.usb.bulkTransfer = function(handle, transferInfo, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-interruptTransfer
 * @param {!chrome.usb.ConnectionHandle} handle A connection handle to make the
 *     transfer on.
 * @param {!chrome.usb.GenericTransferInfo} transferInfo The parameters to the
 *     transfer. See GenericTransferInfo.
 * @param {function(!chrome.usb.TransferResultInfo)} callback Invoked once the
 *     transfer has completed.
 * @return {undefined}
 */
chrome.usb.interruptTransfer = function(handle, transferInfo, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-isochronousTransfer
 * @param {!chrome.usb.ConnectionHandle} handle A connection handle to make the
 *     transfer on.
 * @param {!chrome.usb.IsochronousTransferInfo} transferInfo The parameters to
 *     the transfer.
 * @param {function(!chrome.usb.TransferResultInfo)} callback Invoked once the
 *     transfer has been completed.
 * @return {undefined}
 */
chrome.usb.isochronousTransfer = function(handle, transferInfo, callback) {};


/**
 * @see http://developer.chrome.com/apps/usb.html#method-resetDevice
 * @param {!chrome.usb.ConnectionHandle} handle A connection handle to reset.
 * @param {function(boolean)} callback Invoked once the device is reset with a
 *     boolean indicating whether the reset completed successfully.
 * @return {undefined}
 */
chrome.usb.resetDevice = function(handle, callback) {};



/**
 * Event whose listeners take an chrome.usb.Device parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.usb.Device)>}
 */
chrome.usb.DeviceEvent = function() {};


/**
 * @type {!chrome.usb.DeviceEvent}
 * @see https://developer.chrome.com/apps/usb#event-onDeviceAdded
 */
chrome.usb.onDeviceAdded;


/**
 * @type {!chrome.usb.DeviceEvent}
 * @see https://developer.chrome.com/apps/usb#event-onDeviceRemoved
 */
chrome.usb.onDeviceRemoved;


/**
 * @see https://developer.chrome.com/apps/serial
 * @const
 */
chrome.serial = {};



/**
 * @typedef {?{
 *   persistent: (boolean|undefined),
 *   name: (string|undefined),
 *   bufferSize: (number|undefined),
 *   bitrate: (number|undefined),
 *   dataBits: (string|undefined),
 *   parityBit: (string|undefined),
 *   stopBits: (string|undefined),
 *   ctsFlowControl: (boolean|undefined),
 *   receiveTimeout: (number|undefined),
 *   sendTimeout: (number|undefined)
 * }}
 * @see https://developer.chrome.com/apps/serial#type-ConnectionOptions
 */
chrome.serial.ConnectionOptions;


/**
 * @typedef {?{
 *   connectionId: number,
 *   paused: boolean,
 *   persistent: boolean,
 *   name: string,
 *   bufferSize: number,
 *   receiveTimeout: number,
 *   sendTimeout: number,
 *   bitrate: (number|undefined),
 *   dataBits: (string|undefined),
 *   parityBit: (string|undefined),
 *   stopBits: (string|undefined),
 *   ctsFlowControl: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/apps/serial#type-ConnectionInfo
 */
chrome.serial.ConnectionInfo;


/**
 * Returns information about available serial devices on the system. The
 * list is regenerated each time this method is called.
 * @param {function(!Array<!Object>)} callback Invoked with a
 *     list of ports on complete.
 * @see https://developer.chrome.com/apps/serial#method-getDevices
 * @return {undefined}
 */
chrome.serial.getDevices = function(callback) {};


/**
 * Connects to a given serial port.
 * @param {string} path The system path of the serial port to open.
 * @param {!chrome.serial.ConnectionOptions|
 *         function(!chrome.serial.ConnectionInfo)} optionsOrCallback
 *     Port configuration options, or the callback invoked with the created
 *     ConnectionInfo on complete.
 * @param {function(!chrome.serial.ConnectionInfo)=} opt_callback Invoked with
 *     the created ConnectionInfo on complete.
 * @see https://developer.chrome.com/apps/serial#method-connect
 * @return {undefined}
 */
chrome.serial.connect = function(path, optionsOrCallback, opt_callback) {};


/**
 * Update the option settings on an open serial port connection.
 * @param {number} connectionId The id of the opened connection.
 * @param {!chrome.serial.ConnectionOptions} options Port configuration
 *     options.
 * @param {function(boolean)} callback Called when the configuration has
 *     completed.
 * @see https://developer.chrome.com/apps/serial#method-update
 * @return {undefined}
 */
chrome.serial.update = function(connectionId, options, callback) {};


/**
 * Disconnects from a serial port.
 * @param {number} connectionId The id of the opened connection.
 * @param {function(boolean)} callback Called when the connection
 *     has been closed.
 * @see https://developer.chrome.com/apps/serial#method-disconnect
 * @return {undefined}
 */
chrome.serial.disconnect = function(connectionId, callback) {};


/**
 * Pauses or unpauses an open connection.
 * @param {number} connectionId The id of the opened connection.
 * @param {boolean} paused Flag to indicate whether to pause or unpause.
 * @param {function()} callback Called when the configuration has completed.
 * @see https://developer.chrome.com/apps/serial#method-setPaused
 * @return {undefined}
 */
chrome.serial.setPaused = function(connectionId, paused, callback) {};


/**
 * Retrieves the state of a given connection.
 * @param {number} connectionId The id of the opened connection.
 * @param {function(!chrome.serial.ConnectionInfo)} callback
 *     Called with connection state information when available.
 * @see https://developer.chrome.com/apps/serial#method-getInfo
 * @return {undefined}
 */
chrome.serial.getInfo = function(connectionId, callback) {};


/**
 * Retrieves the list of currently opened serial port connections owned by
 * the application.
 * @param {function(!Array<!chrome.serial.ConnectionInfo>)} callback
 *     Called with the list of |ConnectionInfo|s when available.
 * @see https://developer.chrome.com/apps/serial#method-getConnections
 * @return {undefined}
 */
chrome.serial.getConnections = function(callback) {};


/**
 * Writes data to the given connection.
 * @param {number} connectionId The id of the opened connection.
 * @param {!ArrayBuffer} data The data to send.
 * @param {function(!Object)} callback Called when the operation has
 *     completed.
 * @see https://developer.chrome.com/apps/serial#method-send
 * @return {undefined}
 */
chrome.serial.send = function(connectionId, data, callback) {};


/**
 * Flushes all bytes in the given connection's input and output buffers.
 * @param {number} connectionId The id of the opened connection.
 * @param {function(boolean)} callback
 * @see https://developer.chrome.com/apps/serial#method-flush
 * @return {undefined}
 */
chrome.serial.flush = function(connectionId, callback) {};



/**
 * Retrieves the state of control signals on a given connection.
 * @param {number} connectionId The id of the opened connection.
 * @param {function(!Object)} callback
 * @see https://developer.chrome.com/apps/serial#method-getControlSignals
 * @return {undefined}
 */
chrome.serial.getControlSignals = function(connectionId, callback) {};


/**
 * @typedef {?{
 *   dtr: (boolean|undefined),
 *   rts: (boolean|undefined)
 * }}
 */
chrome.serial.ControlSignals;


/**
 * Sets the state of control signals on a given connection.
 * @param {number} connectionId The id of the opened connection.
 * @param {!chrome.serial.ControlSignals} signals
 *     The set of signal changes to send to the device.
 * @param {function(boolean)} callback Called once the control signals
 *     have been set.
 * @see https://developer.chrome.com/apps/serial#method-setControlSignals
 * @return {undefined}
 */
chrome.serial.setControlSignals = function(connectionId, signals, callback) {};


/**
 * Event raised when data has been read from the connection.
 * @type {!ChromeObjectEvent}
 * @see https://developer.chrome.com/apps/serial#event-onReceive
 */
chrome.serial.onReceive;


/**
 * Event raised when an error occurred while the runtime was waiting for
 * data on the serial port. Once this event is raised, the connection may
 * be set to paused. A "timeout" error does not pause the connection.
 * @type {!ChromeObjectEvent}
 * @see https://developer.chrome.com/apps/serial#event-onReceiveError
 */
chrome.serial.onReceiveError;



////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Chrome Private APIs ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


/**
 * @const
 */
chrome.musicManagerPrivate = {};


/**
 * @param {function(string): void} callback
 * @return {undefined}
 */
chrome.musicManagerPrivate.getDeviceId = function(callback) {};


/**
 * @const
 */
chrome.mediaGalleriesPrivate = {};


/**
 * @typedef {function({deviceId: string, deviceName: string}): void}
 */
chrome.mediaGalleriesPrivate.DeviceCallback;


/**
 * @typedef {function({galleryId: string}): void}
 */
chrome.mediaGalleriesPrivate.GalleryChangeCallback;


/**
 * @typedef {function({galleryId: string, success: boolean}): void}
 */
chrome.mediaGalleriesPrivate.AddGalleryWatchCallback;


/**
 * @param {string} galleryId
 * @param {!chrome.mediaGalleriesPrivate.AddGalleryWatchCallback} callback
 * @return {undefined}
 */
chrome.mediaGalleriesPrivate.addGalleryWatch = function(galleryId, callback) {};


/**
 * @type {!chrome.mediaGalleriesPrivate.DeviceEvent}
 * @deprecated Use {chrome.system.storage.onAttach}.
 */
chrome.mediaGalleriesPrivate.onDeviceAttached;


/**
 * @type {!chrome.mediaGalleriesPrivate.DeviceEvent}
 * @deprecated Use {chrome.system.storage.onDetach}.
 */
chrome.mediaGalleriesPrivate.onDeviceDetached;


/**
 * @type {!chrome.mediaGalleriesPrivate.GalleryChangeEvent}
 */
chrome.mediaGalleriesPrivate.onGalleryChanged;



/**
 * @interface
 * @extends {ChromeBaseEvent<!chrome.mediaGalleriesPrivate.DeviceCallback>}
 * @deprecated Use {chrome.system.storage.DeviceEvent}.
 */
chrome.mediaGalleriesPrivate.DeviceEvent = function() {};


/**
 * @interface
 * @extends {ChromeBaseEvent<!chrome.mediaGalleriesPrivate.GalleryChangeCallback>}
 */
chrome.mediaGalleriesPrivate.GalleryChangeEvent = function() {};


/**
 * safeBrowsingPrivate is a Private API for observing events and retrieving a
 * referrer chain.
 * @see https://cs.chromium.org/chromium/src/chrome/common/extensions/api/safe_browsing_private.idl
 */
chrome.safeBrowsingPrivate = {};

/**
 * @enum {string}
 */
chrome.safeBrowsingPrivate.URLType = {
  EVENT_URL: '',
  LANDING_PAGE: '',
  LANDING_REFERRER: '',
  CLIENT_REDIRECT: '',
  RECENT_NAVIGATION: '',
  REFERRER: '',
};

/**
 * @enum {string}
 */
chrome.safeBrowsingPrivate.NavigationInitiation = {
  BROWSER_INITIATED: '',
  RENDERER_INITIATED_WITHOUT_USER_GESTURE: '',
  RENDERER_INITIATED_WITH_USER_GESTURE: '',
};

/**
 * @typedef {{
 *   url: string,
 *   userName: string,
 *   isPhishingUrl: boolean
 * }}
 */
chrome.safeBrowsingPrivate.PolicySpecifiedPasswordReuse;

/**
 * @typedef {{
 *   url: string,
 *   fileName: string,
 *   downloadDigestSha256: string,
 *   userName: string
 * }}
 */
chrome.safeBrowsingPrivate.DangerousDownloadInfo;

/**
 * @typedef {{
 *   url: string,
 *   reason: string,
 *   netErrorCode: (string|undefined),
 *   userName: string
 * }}
 */
chrome.safeBrowsingPrivate.InterstitialInfo;

/**
 * @typedef {{
 *   url: (string|undefined)
 * }}
 */
chrome.safeBrowsingPrivate.ServerRedirect;

/**
 * @typedef {{
 *   url: string,
 *   mainFrameUrl: (string|undefined),
 *   urlType: !chrome.safeBrowsingPrivate.URLType,
 *   ipAddresses: (!Array<string>|undefined),
 *   referrerUrl: (string|undefined),
 *   referrerMainFrameUrl: (string|undefined),
 *   isRetargeting: (boolean|undefined),
 *   navigationTimeMs: (number|undefined),
 *   serverRedirectChain:
 * (!Array<!chrome.safeBrowsingPrivate.ServerRedirect>|undefined),
 *   navigationInitiation:
 * (!chrome.safeBrowsingPrivate.NavigationInitiation|undefined),
 *   maybeLaunchedByExternalApp: (boolean|undefined)
 * }}
 */
chrome.safeBrowsingPrivate.ReferrerChainEntry;

/**
 * Gets referrer chain for the specified tab.
 * @param {number} tabId Id of the tab from which to retrieve the referrer.
 * @param {function(!Array<!chrome.safeBrowsingPrivate.ReferrerChainEntry>):void}
 *     callback Called with the list of referrer chain entries.
 */
chrome.safeBrowsingPrivate.getReferrerChain = function(tabId, callback) {};

/**
 * Fired when Chrome detects a reuse of a policy specified password.
 * @type {!ChromeObjectEvent}
 */
chrome.safeBrowsingPrivate.onPolicySpecifiedPasswordReuseDetected;

/**
 * Fired when the user changed their policy specified password.
 * @type {!ChromeStringEvent}
 */
chrome.safeBrowsingPrivate.onPolicySpecifiedPasswordChanged;

/**
 * Fired when the user opened a dangerous download.
 * @type {!ChromeObjectEvent}
 */
chrome.safeBrowsingPrivate.onDangerousDownloadOpened;

/**
 * Fired when a security interstitial is shown to the user.
 * @type {!ChromeObjectEvent}
 */
chrome.safeBrowsingPrivate.onSecurityInterstitialShown;

/**
 * Fired when the user clicked-through a security interstitial.
 * @type {!ChromeObjectEvent}
 */
chrome.safeBrowsingPrivate.onSecurityInterstitialProceeded;

/**
 * WARNING(2014/08/14): This API is still under active initial development and
 * unstable. The types are not well defined or documented, and this API
 * definition here should not be used as an example for other APIs added to this
 * file. Please contact mednik@ for questions on and maintenance for this API.
 * @const
 * @see http://goo.gl/afV8wB
 */
chrome.mdns = {};


/**
 * Data type sent to the event handler of chrome.mdns.onServiceList.
 * @constructor
 */
chrome.mdns.MdnsService = function() {};


/** @type {string} */
chrome.mdns.MdnsService.prototype.serviceName;


/** @type {string} */
chrome.mdns.MdnsService.prototype.serviceHostPort;


/** @type {string} */
chrome.mdns.MdnsService.prototype.ipAddress;


/** @type {!Array<string>} */
chrome.mdns.MdnsService.prototype.serviceData;


/**
 * Event whose listeners take an array of MdnsService parameter.
 *
 * The `addListener()` methods for these take more than one argument, so they
 * cannot just extend `ChromeBaseEvent`.
 * @interface
 * @extends {ChromeBaseEventNoListeners}
 */
chrome.mdns.ServiceListEvent = function() {};


/**
 * @param {function(!Array<!chrome.mdns.MdnsService>): void} callback
 * @param {!Object=} opt_filter
 * @return {undefined}
 */
chrome.mdns.ServiceListEvent.prototype.addListener = function(
    callback, opt_filter) {};


/**
 * @param {function(!Array<!chrome.mdns.MdnsService>): void} callback
 * @return {undefined}
 */
chrome.mdns.ServiceListEvent.prototype.removeListener = function(callback) {};


/**
 * @param {function(!Array<!chrome.mdns.MdnsService>): void} callback
 * @return {boolean}
 */
chrome.mdns.ServiceListEvent.prototype.hasListener = function(callback) {};


/** @return {boolean} */
chrome.mdns.ServiceListEvent.prototype.hasListeners = function() {};


/** @type {!chrome.mdns.ServiceListEvent} */
chrome.mdns.onServiceList;


/**
 * @param {function()} callback
 * @return {undefined}
 */
chrome.mdns.forceDiscovery = function(callback) {};


/**
 * @const
 * @see http://goo.gl/79p5h5
 */
chrome.gcdPrivate = {};


/**
 * Represents a GCD device discovered locally or registered to a given user.
 * deviceId: Opaque device identifier to be passed to API.
 * setupType: How this device was discovered.
 * cloudId: Cloud identifier string.
 * deviceName: Device human readable name.
 * deviceType: Device type (camera, printer, etc).
 * deviceDescription: Device human readable description.
 * @typedef {?{
 *   deviceId: string,
 *   setupType: string,
 *   cloudId: (string|undefined),
 *   deviceType: string,
 *   deviceName: string,
 *   deviceDescription: string
 * }}
 */
chrome.gcdPrivate.Device;


/**
 * Returns the list of cloud devices visible locally or available in the
 * cloud for user account.
 * @param {function(!Array<!chrome.gcdPrivate.Device>): void} callback
 * @return {undefined}
 */
chrome.gcdPrivate.getCloudDeviceList = function(callback) {};


/**
 * Queries network for local devices. Triggers onDeviceStateChanged and
 * onDeviceRemoved events. Call this function *only* after registering for
 * onDeviceStateChanged and onDeviceRemoved events, or it will do nothing.
 * @return {undefined}
 */
chrome.gcdPrivate.queryForNewLocalDevices = function() {};


/**
 * Cache the WiFi password in the browser process for use during
 * provisioning. This is done to allow the gathering of the wifi password to
 * not be done while connected to the device's network. Callback is called
 * with true if wifi password was cached and false if it was unavailable.
 * @param {string} ssid
 * @param {function(boolean): void} callback
 * @return {undefined}
 */
chrome.gcdPrivate.prefetchWifiPassword = function(ssid, callback) {};


/**
 * Returns local device information.
 * @param {string} serviceName The mDNS service name of the device.
 * @param {function(string, !Object): void}
 *     callback Called when when the device info is available or on error.
 *     |status|: The status of the operation (success or type of error).
 *     |deviceInfo|: Content of /privet/info response.
 *     https://developers.google.com/cloud-devices/v1/reference/local-api/info
 * @return {undefined}
 */
chrome.gcdPrivate.getDeviceInfo = function(serviceName, callback) {};


/**
 * Create new pairing session.
 * @param {string} serviceName The mDNS service name of the device.
 * @param {function(number, string, !Array<string>): void}
 *     callback Called when the session is established or on error. 1st param,
 *     |sessionId|, is the session ID (identifies the session for future calls).
 *     2nd param, |status|, is the status (success or type of error). 3rd param,
 *     |pairingTypes|, is a list of pairing types supported by this device.
 * @return {undefined}
 */
chrome.gcdPrivate.createSession = function(serviceName, callback) {};


/**
 * Start pairing with the selected method.
 * @param {number} sessionId
 * @param {string} pairingType
 * @param {function(string): void} callback
 * @return {undefined}
 */
chrome.gcdPrivate.startPairing = function(sessionId, pairingType, callback) {};


/**
 * Confirm pairing code.
 * @param {number} sessionId
 * @param {string} code
 * @param {function(string): void} callback
 * @return {undefined}
 */
chrome.gcdPrivate.confirmCode = function(sessionId, code, callback) {};


/**
 * Send an encrypted message to the device. If the message is a setup message
 * with a wifi ssid specified but no password, the password cached from
 * prefetchWifiPassword() will be used and the call will fail if it's not
 * available. For open networks use an empty string as the password.
 * @param {number} sessionId
 * @param {string} api The API path.
 * @param {!Object} input The input message to be sent over the encrypted
 *     channel.
 * @param {function(string, ?Object): void} callback
 * @return {undefined}
 */
chrome.gcdPrivate.sendMessage = function(sessionId, api, input, callback) {};


/**
 * Terminate the session with the device.
 * @param {number} sessionId
 * @return {undefined}
 */
chrome.gcdPrivate.terminateSession = function(sessionId) {};


/**
 * Returns command definitions.
 * @param {string} deviceId The device to get command definitions for.
 * @param {function(!Object): void} callback The result callback.
 * @return {undefined}
 */
chrome.gcdPrivate.getCommandDefinitions = function(deviceId, callback) {};


/**
 * Creates and sends a new command.
 * @param {string} deviceId The device to send the command to.
 * @param {number} expireInMs The number of milliseconds since now before the
 *     command expires. An expired command should not be executed by the device.
 *     Acceptable values are 10 sec (10000 ms) to 30 days (2592000000 ms),
 *     inclusive. All values outside that range will be replaced by 30 days.
 * @param {!Object} command Described at
 *     https://developers.google.com/cloud-devices/v1/reference/commands.
 * @param {function(!Object): void} callback  The result callback.
 * @return {undefined}
 */
chrome.gcdPrivate.insertCommand = function(
    deviceId, expireInMs, command, callback) {};


/**
 * Returns a particular command.
 * @param {string} commandId Unique command ID.
 * @param {function(!Object): void} callback  The result callback.
 * @return {undefined}
 */
chrome.gcdPrivate.getCommand = function(commandId, callback) {};


/**
 * Cancels a command.
 * @param {string} commandId Unique command ID.
 * @param {function(!Object): void} callback  The result callback.
 * @return {undefined}
 */
chrome.gcdPrivate.cancelCommand = function(commandId, callback) {};


/**
 * Lists all commands in order of creation.
 * @param {string} deviceId The device to send the command to.
 * @param {string} byUser List all the commands issued by the user. Special
 *     value 'me' can be used to list by the current user.
 * @param {string} state Command state.
 * @param {function(!Array<!Object>): void} callback  The result callback.
 * @return {undefined}
 */
chrome.gcdPrivate.getCommandsList = function(
    deviceId, byUser, state, callback) {};



/**
 * Event whose listeners take a chrome.gcdPrivate.Device.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.gcdPrivate.Device)>}
 */
chrome.gcdPrivate.DeviceEvent = function() {};


/**
 * Fires when a device's state changes. When a listener is first added, this
 * event fires for all known devices on the network. Afterwards, it will fire
 * with device status updates.
 * @type {!chrome.gcdPrivate.DeviceEvent}
 */
chrome.gcdPrivate.onDeviceStateChanged;


/**
 * Fires when a given device disappears.
 * |deviceId| The device that has disappeared.
 * @type {!ChromeStringEvent}
 */
chrome.gcdPrivate.onDeviceRemoved;


/**
 * @const
 * @see https://cs.chromium.org/chromium/src/extensions/common/api/bluetooth_private.idl
 */
chrome.bluetoothPrivate = {};



/** @constructor */
chrome.bluetoothPrivate.PairingEvent = function() {};


/** @type {string} */
chrome.bluetoothPrivate.PairingEvent.prototype.pairing;


/** @type {!chrome.bluetooth.Device} */
chrome.bluetoothPrivate.PairingEvent.prototype.device;


/** @type {string|undefined} */
chrome.bluetoothPrivate.PairingEvent.prototype.pincode;


/** @type {number|undefined} */
chrome.bluetoothPrivate.PairingEvent.prototype.passkey;


/** @type {number|undefined} */
chrome.bluetoothPrivate.PairingEvent.prototype.enteredKey;


/**
 * @typedef {{
 *   name: (string|undefined),
 *   powered: (boolean|undefined),
 *   discoverable: (boolean|undefined)
 * }}
 */
chrome.bluetoothPrivate.NewAdapterState;


/**
 * @typedef {{
 *   device: !chrome.bluetooth.Device,
 *   response: (string|undefined),
 *   pincode: (string|undefined),
 *   passkey: (number|undefined),
 *   enteredKey: (number|undefined)
 * }}
 */
chrome.bluetoothPrivate.SetPairingResponseOptions;


/**
 * @param {!chrome.bluetoothPrivate.NewAdapterState} adapterState
 * @param {function()} callback
 * @return {undefined}
 */
chrome.bluetoothPrivate.setAdapterState = function(adapterState, callback) {};


/**
 * @param {!chrome.bluetoothPrivate.SetPairingResponseOptions} options
 * @param {function()} callback
 * @return {undefined}
 */
chrome.bluetoothPrivate.setPairingResponse = function(options, callback) {};


/**
 * @param {string} deviceAddress
 * @param {function():void=} callback
 */
chrome.bluetoothPrivate.disconnectAll = function(deviceAddress, callback) {};


/**
 * @param {string} deviceAddress
 * @param {function():void=} callback
 * @return {undefined}
 */
chrome.bluetoothPrivate.forgetDevice = function(deviceAddress, callback) {};


/**
 * @typedef {{
 *   transport: (!chrome.bluetoothPrivate.TransportType|undefined),
 *   uuids: ((string|!Array<string>)|undefined),
 *   rssi: (number|undefined),
 *   pathloss: (number|undefined)
 * }}
 */
chrome.bluetoothPrivate.DiscoveryFilter;


/**
 * Set or clear discovery filter.
 * @param {!chrome.bluetoothPrivate.DiscoveryFilter} discoveryFilter
 * @param {function():void=} callback
 */
chrome.bluetoothPrivate.setDiscoveryFilter = function(
    discoveryFilter, callback) {};


/**
 * Event whose listeners take a PairingEvent parameter.
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.bluetoothPrivate.PairingEvent)>}
 */
chrome.bluetoothPrivate.PairingEventEvent = function() {};


/** @type {!chrome.bluetoothPrivate.PairingEventEvent} */
chrome.bluetoothPrivate.onPairing;


/**
 * @param {string} deviceAddress
 * @param {function(): void=} callback
 */
chrome.bluetoothPrivate.pair = function(deviceAddress, callback) {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-PairingResponse
 */
chrome.bluetoothPrivate.PairingResponse = {
  CONFIRM: '',
  REJECT: '',
  CANCEL: '',
};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-PairingEventType
 */
chrome.bluetoothPrivate.PairingEventType = {
  REQUEST_PINCODE: '',
  DISPLAY_PINCODE: '',
  REQUEST_PASSKEY: '',
  DISPLAY_PASSKEY: '',
  KEYS_ENTERED: '',
  CONFIRM_PASSKEY: '',
  REQUEST_AUTHORIZATION: '',
  COMPLETE: '',
};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-ConnectResultType
 */
chrome.bluetoothPrivate.ConnectResultType = {
  ALREADY_CONNECTED: '',
  ATTRIBUTE_LENGTH_INVALID: '',
  AUTH_CANCELED: '',
  AUTH_FAILED: '',
  AUTH_REJECTED: '',
  AUTH_TIMEOUT: '',
  CONNECTION_CONGESTED: '',
  FAILED: '',
  IN_PROGRESS: '',
  INSUFFICIENT_ENCRYPTION: '',
  OFFSET_INVALID: '',
  READ_NOT_PERMITTED: '',
  REQUEST_NOT_SUPPORTED: '',
  SUCCESS: '',
  UNKNOWN_ERROR: '',
  UNSUPPORTED_DEVICE: '',
  WRITE_NOT_PERMITTED: '',
};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/bluetoothPrivate#type-TransportType
 */
chrome.bluetoothPrivate.TransportType = {
  LE: '',
  BREDR: '',
  DUAL: '',
};


/**
 * Connects to the given device. This will only throw an error if the device
 * address is invalid or the device is already connected. Otherwise this will
 * succeed and invoke |callback| with ConnectResultType.
 * @param {string} deviceAddress
 * @param {function(!chrome.bluetoothPrivate.ConnectResultType):void=} callback
 */
chrome.bluetoothPrivate.connect = function(deviceAddress, callback) {};


/**
 * @const
 * @see http://goo.gl/XmVdHm
 */
chrome.inlineInstallPrivate = {};


/**
 * Installs the given app ID.
 * @param {string} id
 * @param {function(string, string): void=} opt_callback Response callback that
 *     returns two string: (1) an error string (or empty string on success) and
 *     (2) an error code in case of error
 * @return {undefined}
 */
chrome.inlineInstallPrivate.install = function(id, opt_callback) {};


/**
 * @const
 * @see https://goo.gl/7dvJFW
 */
chrome.wallpaper = {};


/**
 * @enum {string}
 * @see https://goo.gl/7dvJFW#type-WallpaperLayout
 */
chrome.wallpaper.WallpaperLayout = {
  STRETCH: '',
  CENTER: '',
  CENTER_CROPPED: '',
};


/**
 * Sets wallpaper to the image at url or wallpaperData with the specified
 * layout.
 * @param {{
 *    data: (ArrayBuffer|undefined),
 *    url: (string|undefined),
 *    layout: (chrome.wallpaper.WallpaperLayout|string),
 *    filename: string,
 *    thumbnail: (boolean|undefined)
 *  }} details
 * @param {function(ArrayBuffer=)} callback
 * @return {undefined}
 * @see https://goo.gl/7dvJFW#method-setWallpaper
 */
chrome.wallpaper.setWallpaper = function(details, callback) {};


/**
 * @const
 * @see https://developer.chrome.com/extensions/downloads
 */
chrome.downloads = {};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/downloads#type-FilenameConflictAction
 */
chrome.downloads.FilenameConflictAction = {
  UNIQUIFY: '',
  OVERWRITE: '',
  PROMPT: ''
};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/downloads#type-InterruptReason
 */
chrome.downloads.InterruptReason = {
  FILE_FAILED: '',
  FILE_ACCESS_DENIED: '',
  FILE_NO_SPACE: '',
  FILE_NAME_TOO_LONG: '',
  FILE_TOO_LARGE: '',
  FILE_VIRUS_INFECTED: '',
  FILE_TRANSIENT_ERROR: '',
  FILE_BLOCKED: '',
  FILE_SECURITY_CHECK_FAILED: '',
  FILE_TOO_SHORT: '',
  FILE_HASH_MISMATCH: '',
  NETWORK_FAILED: '',
  NETWORK_TIMEOUT: '',
  NETWORK_DISCONNECTED: '',
  NETWORK_SERVER_DOWN: '',
  NETWORK_INVALID_REQUEST: '',
  SERVER_FAILED: '',
  SERVER_NO_RANGE: '',
  SERVER_BAD_CONTENT: '',
  SERVER_UNAUTHORIZED: '',
  SERVER_CERT_PROBLEM: '',
  SERVER_FORBIDDEN: '',
  SERVER_UNREACHABLE: '',
  USER_CANCELED: '',
  USER_SHUTDOWN: '',
  CRASH: '',
};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/downloads#type-DangerType
 */
chrome.downloads.DangerType = {
  FILE: '',
  URL: '',
  CONTENT: '',
  UNCOMMON: '',
  HOST: '',
  UNWANTED: '',
  SAFE: '',
  ACCEPTED: ''
};


/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/downloads#type-State
 */
chrome.downloads.State = {
  IN_PROGRESS: '',
  INTERRUPTED: '',
  COMPLETE: '',
};


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/downloads#type-DownloadItem
 */
chrome.downloads.DownloadItem = function() {};


/** @type {number} */
chrome.downloads.DownloadItem.prototype.id;


/** @type {string} */
chrome.downloads.DownloadItem.prototype.url;


/** @type {string} */
chrome.downloads.DownloadItem.prototype.finalUrl;


/** @type {string} */
chrome.downloads.DownloadItem.prototype.referrer;


/** @type {string} */
chrome.downloads.DownloadItem.prototype.filename;


/** @type {boolean} */
chrome.downloads.DownloadItem.prototype.incognito;


/** @type {!chrome.downloads.DangerType|string} */
chrome.downloads.DownloadItem.prototype.danger;


/** @type {string} */
chrome.downloads.DownloadItem.prototype.mime;


/** @type {string} */
chrome.downloads.DownloadItem.prototype.startTime;


/** @type {?string} */
chrome.downloads.DownloadItem.prototype.endTime;


/** @type {?string} */
chrome.downloads.DownloadItem.prototype.estimatedEndTime;


/** @type {!chrome.downloads.State|string} */
chrome.downloads.DownloadItem.prototype.state;


/** @type {boolean} */
chrome.downloads.DownloadItem.prototype.paused;


/** @type {boolean} */
chrome.downloads.DownloadItem.prototype.canResume;


/** @type {!chrome.downloads.InterruptReason|string|undefined} */
chrome.downloads.DownloadItem.prototype.error;


/** @type {number} */
chrome.downloads.DownloadItem.prototype.bytesReceived;


/** @type {number} */
chrome.downloads.DownloadItem.prototype.totalBytes;


/** @type {number} */
chrome.downloads.DownloadItem.prototype.fileSize;


/** @type {boolean} */
chrome.downloads.DownloadItem.prototype.exists;


/** @type {?string} */
chrome.downloads.DownloadItem.prototype.byExtensionId;


/** @type {?string} */
chrome.downloads.DownloadItem.prototype.byExtensionName;


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/downloads#type-StringDelta
 */
chrome.downloads.StringDelta = function() {};


/** @type {?string} */
chrome.downloads.StringDelta.prototype.previous;


/** @type {?string} */
chrome.downloads.StringDelta.prototype.current;


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/downloads#type-DoubleDelta
 */
chrome.downloads.DoubleDelta = function() {};


/** @type {?number} */
chrome.downloads.DoubleDelta.prototype.previous;


/** @type {?number} */
chrome.downloads.DoubleDelta.prototype.current;


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/downloads#type-BooleanDelta
 */
chrome.downloads.BooleanDelta = function() {};


/** @type {?boolean} */
chrome.downloads.BooleanDelta.prototype.previous;


/** @type {?boolean} */
chrome.downloads.BooleanDelta.prototype.current;


/**
 * @param {{
 *    url: string,
 *    filename: (string|undefined),
 *    conflictAction:
 *        (!chrome.downloads.FilenameConflictAction|string|undefined),
 *    saveAs: (boolean|undefined),
 *    method: (string|undefined),
 *    headers: (!Array<{name:string, value:string}>|undefined),
 *    body: (string|undefined)
 *  }} details
 * @param {function(number)=} opt_callback
 * @see https://developer.chrome.com/extensions/downloads#method-download
 */
chrome.downloads.download = function(details, opt_callback) {};


/**
 * @typedef {?{
 *    query: (!Array<string>|undefined),
 *    startedBefore: (string|undefined),
 *    startedAfter: (string|undefined),
 *    endedBefore: (string|undefined),
 *    endedAfter: (string|undefined),
 *    totalBytesGreater: (number|undefined),
 *    totalBytesLess: (number|undefined),
 *    filenameRegex: (string|undefined),
 *    urlRegex: (string|undefined),
 *    finalUrlRegex: (string|undefined),
 *    limit: (number|undefined),
 *    orderedBy: (!Array<string>|undefined),
 *    id: (number|undefined),
 *    url: (string|undefined),
 *    finalUrl: (string|undefined),
 *    filename: (string|undefined),
 *    danger: (!chrome.downloads.DangerType|string|undefined),
 *    mime: (string|undefined),
 *    startTime: (string|undefined),
 *    endTime: (string|undefined),
 *    state: (!chrome.downloads.State|string|undefined),
 *    paused: (boolean|undefined),
 *    error: (!chrome.downloads.InterruptReason|string|undefined),
 *    bytesReceived: (number|undefined),
 *    totalBytes: (number|undefined),
 *    fileSize: (number|undefined),
 *    exists: (boolean|undefined)
 *  }}
 */
chrome.downloads.Query;


/**
 * @param {!chrome.downloads.Query} query
 * @param {function(!Array<!chrome.downloads.DownloadItem>)} callback
 * @see https://developer.chrome.com/extensions/downloads#method-search
 */
chrome.downloads.search = function(query, callback) {};


/**
 * @param {number} id
 * @param {function()=} opt_callback
 *
 * @see https://developer.chrome.com/extensions/downloads#method-pause
 */
chrome.downloads.pause = function(id, opt_callback) {};


/**
 * @param {number} id
 * @param {function()=} opt_callback
 *
 * @see https://developer.chrome.com/extensions/downloads#method-resume
 */
chrome.downloads.resume = function(id, opt_callback) {};


/**
 * @param {number} id
 * @param {function()=} opt_callback
 *
 * @see https://developer.chrome.com/extensions/downloads#method-cancel
 */
chrome.downloads.cancel = function(id, opt_callback) {};


/**
 * @param {number} id
 * @param {{size:(number|undefined)}|function(string)} optionsOrCallback
 * @param {function(string)=} opt_callback
 *
 * @see https://developer.chrome.com/extensions/downloads#method-getFileIcon
 */
chrome.downloads.getFileIcon = function(id, optionsOrCallback, opt_callback) {};


/**
 * @param {number} id
 * @see https://developer.chrome.com/extensions/downloads#method-open
 */
chrome.downloads.open = function(id) {};


/**
 * @param {number} id
 * @see https://developer.chrome.com/extensions/downloads#method-show
 */
chrome.downloads.show = function(id) {};


/**
 * @see https://developer.chrome.com/extensions/downloads#method-showDefaultFolder
 */
chrome.downloads.showDefaultFolder = function() {};


/**
 * @param {!chrome.downloads.Query} query
 * @param {function(!Array<number>)} callback
 * @see https://developer.chrome.com/extensions/downloads#method-erase
 */
chrome.downloads.erase = function(query, callback) {};


/**
 * @param {number} id
 * @param {function()=} opt_callback
 *
 * @see https://developer.chrome.com/extensions/downloads#method-removeFile
 */
chrome.downloads.removeFile = function(id, opt_callback) {};


/**
 * @param {number} id
 * @param {function()=} opt_callback
 *
 * @see https://developer.chrome.com/extensions/downloads#method-acceptDanger
 */
chrome.downloads.acceptDanger = function(id, opt_callback) {};


/**
 * @param {number} id
 *
 * @see https://developer.chrome.com/extensions/downloads#method-drag
 */
chrome.downloads.drag = function(id) {};


/**
 * @param {boolean} enabled
 *
 * @see https://developer.chrome.com/extensions/downloads#method-setShelfEnabled
 */
chrome.downloads.setShelfEnabled = function(enabled) {};


/**
 * @interface
 * @extends {ChromeBaseEvent<function(!chrome.downloads.DownloadItem)>}
 * @see https://developer.chrome.com/extensions/downloads#event-onCreated
 */
chrome.downloads.CreatedEvent = function() {};


/**
 * @type {!chrome.downloads.CreatedEvent}
 * https://developer.chrome.com/extensions/downloads#event-onCreated
 */
chrome.downloads.onCreated;


/**
 * @type {!ChromeNumberEvent}
 * https://developer.chrome.com/extensions/downloads#event-onErased
 */
chrome.downloads.onErased;


/**
 * @type {!ChromeObjectEvent}
 * https://developer.chrome.com/extensions/downloads#event-onChanged
 */
chrome.downloads.onChanged;


/**
 * @typedef {?{
 *  filename: string,
 *  conflictAction: (!chrome.downloads.FilenameConflictAction|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/downloads#event-onDeterminingFilename
 */
chrome.downloads.FilenameSuggestion;


/**
 * @interface
 * @extends {ChromeBaseEvent<
 *     function(
 *         !chrome.downloads.DownloadItem,
 *         function(!chrome.downloads.FilenameSuggestion=))>}
 * @see https://developer.chrome.com/extensions/downloads#event-onDeterminingFilename
 */
chrome.downloads.DeterminingFilenameEvent = function() {};


/**
 * @type {!chrome.downloads.DeterminingFilenameEvent}
 * https://developer.chrome.com/extensions/downloads#event-onDeterminingFilename
 */
chrome.downloads.onDeterminingFilename;


/**
 * @const
 * @see https://developer.chrome.com/extensions/declarativeContent
 */
chrome.declarativeContent = {};


/**
 * @typedef {!{
 *   pageUrl: (!UrlFilter|undefined),
 *   css: (!Array<string>|undefined),
 *   isBookmarked: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/declarativeContent#type-PageStateMatcher
 */
chrome.declarativeContent.PageStateMatcherLiteral;


/**
 * @constructor
 * @param {!chrome.declarativeContent.PageStateMatcherLiteral=} literalValue
 * @see https://developer.chrome.com/extensions/declarativeContent#type-PageStateMatcher
 */
chrome.declarativeContent.PageStateMatcher = function(literalValue) {};


/**
 * @constructor
 * @see https://developer.chrome.com/extensions/declarativeContent#type-ShowPageAction
 */
chrome.declarativeContent.ShowPageAction = function() {};


/**
 * @typedef {!{
 *   imageData: (!ImageData|!Object<number, !ImageData>|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/declarativeContent#type-SetIcon
 */
chrome.declarativeContent.SetIconLiteral;


/**
 * @constructor
 * @param {!chrome.declarativeContent.SetIconLiteral} literalValue
 * @see https://developer.chrome.com/extensions/declarativeContent#type-SetIcon
 */
chrome.declarativeContent.SetIcon = function(literalValue) {};


/**
 * @typedef {!{
 *   css: (!Array<string>|undefined),
 *   js: (!Array<string>|undefined),
 *   allFrames: (boolean|undefined),
 *   matchAboutBlank: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/declarativeContent#type-RequestContentScript
 */
chrome.declarativeContent.RequestContentScriptLiteral;


/**
 * @constructor
 * @param {!chrome.declarativeContent.RequestContentScriptLiteral=} literalValue
 * @see https://developer.chrome.com/extensions/declarativeContent#type-RequestContentScript
 */
chrome.declarativeContent.RequestContentScript = function(literalValue) {};


/**
 * @type {!ChromeVoidEvent}
 * @see https://developer.chrome.com/extensions/declarativeContent#event-onPageChanged
 */
chrome.declarativeContent.onPageChanged;

/**
 * @see https://developer.chrome.com/extensions/instanceID
 * @const
 */
chrome.instanceID = {};

/**
 * @param {function(string)} callback
 * @see https://developer.chrome.com/extensions/instanceID#method-getID
 */
chrome.instanceID.getID = function(callback) {};

/**
 * @param {function(number)} callback
 * @see https://developer.chrome.com/extensions/instanceID#method-getCreationTime
 */
chrome.instanceID.getCreationTime = function(callback) {};

/**
 * @typedef {{
 *   authorizedEntity: string,
 *   scope: string,
 *   options: (!Object|undefined)
 * }}
 */
chrome.instanceID.GetTokenParams;

/**
 * @param {!chrome.instanceID.GetTokenParams} getTokenParams
 * @param {function(string)} callback
 * @see https://developer.chrome.com/extensions/instanceID#method-getToken
 */
chrome.instanceID.getToken = function(getTokenParams, callback) {};

/**
 * @typedef {{
 *   authorizedEntity: string,
 *   scope: string,
 * }}
 */
chrome.instanceID.DeleteTokenParams;

/**
 * @param {!chrome.instanceID.DeleteTokenParams} deleteTokenParams
 * @param {function()} callback
 * @see https://developer.chrome.com/extensions/instanceID#method-deleteToken
 */
chrome.instanceID.deleteToken = function(deleteTokenParams, callback) {};

/**
 * @param {function()} callback
 * @see https://developer.chrome.com/extensions/instanceID#method-deleteID
 */
chrome.instanceID.deleteID = function(callback) {};

/**
 * @const
 * @see https://developer.chrome.com/extensions/instanceID#event-onTokenRefresh
 */
chrome.instanceID.onTokenRefresh = {};

/**
 * @param {function()} callback
 * @see https://developer.chrome.com/extensions/instanceID#event-onTokenRefresh
 */
chrome.instanceID.onTokenRefresh.addListener = function(callback) {};
