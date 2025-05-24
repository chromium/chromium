/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Functions for setting, getting and deleting cookies.
 */


goog.provide('goog.net.Cookies');

goog.require('goog.string');



/**
 * A class for handling browser cookies.
 * @param {?Document} context The context document to get/set cookies on.
 * @constructor
 * @final
 */
goog.net.Cookies = function(context) {
  'use strict';
  /**
   * The context document to get/set cookies on. If no document context is
   * passed, use a fake one with only the "cookie" attribute. This allows
   * this class to be instantiated safely in web worker environments.
   * @private {{cookie: string}}
   */
  this.document_ = context || {cookie: ''};
};


/**
 * Static constant for the size of cookies. Per the spec, there's a 4K limit
 * to the size of a cookie. To make sure users can't break this limit, we
 * should truncate long cookies at 3950 bytes, to be extra careful with dumb
 * browsers/proxies that interpret 4K as 4000 rather than 4096.
 * @const {number}
 */
goog.net.Cookies.MAX_COOKIE_LENGTH = 3950;


/**
 * The name of the test cookie to set.
 *
 *
 * @private @const {string}
 */
goog.net.Cookies.TEST_COOKIE_NAME_ = 'TESTCOOKIESENABLED';


/**
 * The value of the test cookie to set.
 * @private @const {string}
 */
goog.net.Cookies.TEST_COOKIE_VALUE_ = '1';


/**
 * Max age of the test cookie in seconds.
 * @private @const {number}
 */
goog.net.Cookies.TEST_COOKIE_MAX_AGE_ = 60;


/**
 * Returns true if cookies are enabled.
 *
 * navigator.cookieEnabled is an unreliable API in some browsers such as
 * Internet Explorer. It will return true even when cookies are actually
 * blocked. To work around this, check for the presence of cookies, or attempt
 * to manually set and retrieve a cookie, which is the ultimate test of whether
 * or not a browser supports cookies.
 *
 * @return {boolean} True if cookies are enabled.
 */
goog.net.Cookies.prototype.isEnabled = function() {
  'use strict';
  if (!goog.global.navigator.cookieEnabled) {
    return false;
  }

  if (!this.isEmpty()) {
    // There are some cookies already set for the current domain, so cookies
    // can't be totally blocked.
    return true;
  }

  // Try setting and reading back a cookie to see if cookies are enabled.
  this.set(
      goog.net.Cookies.TEST_COOKIE_NAME_, goog.net.Cookies.TEST_COOKIE_VALUE_,
      {maxAge: goog.net.Cookies.TEST_COOKIE_MAX_AGE_});
  if (this.get(goog.net.Cookies.TEST_COOKIE_NAME_) !==
      goog.net.Cookies.TEST_COOKIE_VALUE_) {
    return false;
  }

  // Clean up the test cookie.
  this.remove(goog.net.Cookies.TEST_COOKIE_NAME_);

  return true;
};


/**
 * We do not allow '=', ';', or white space in the name.
 *
 * NOTE: The following are allowed by this method, but should be avoided for
 * cookies handled by the server.
 * - any name starting with '$'
 * - 'Comment'
 * - 'Domain'
 * - 'Expires'
 * - 'Max-Age'
 * - 'Path'
 * - 'Secure'
 * - 'Version'
 *
 * @param {string} name Cookie name.
 * @return {boolean} Whether name is valid.
 *
 * @see <a href="http://tools.ietf.org/html/rfc2109">RFC 2109</a>
 * @see <a href="http://tools.ietf.org/html/rfc2965">RFC 2965</a>
 */
goog.net.Cookies.prototype.isValidName = function(name) {
  'use strict';
  return !(/[;=\s]/.test(name));
};


/**
 * We do not allow ';' or line break in the value.
 *
 * Spec does not mention any illegal characters, but in practice semi-colons
 * break parsing and line breaks truncate the name.
 *
 * @param {string} value Cookie value.
 * @return {boolean} Whether value is valid.
 *
 * @see <a href="http://tools.ietf.org/html/rfc2109">RFC 2109</a>
 * @see <a href="http://tools.ietf.org/html/rfc2965">RFC 2965</a>
 */
goog.net.Cookies.prototype.isValidValue = function(value) {
  'use strict';
  return !(/[;\r\n]/.test(value));
};


/**
 * Sets a cookie.  The max_age can be -1 to set a session cookie. To remove and
 * expire cookies, use remove() instead.
 *
 * Neither the `name` nor the `value` are encoded in any way. It is
 * up to the callers of `get` and `set` (as well as all the other
 * methods) to handle any possible encoding and decoding.
 *
 * @throws {!Error} If the `name` fails #goog.net.cookies.isValidName.
 * @throws {!Error} If the `value` fails #goog.net.cookies.isValidValue.
 *
 * @param {string} name  The cookie name.
 * @param {string} value  The cookie value.
 * @param {!goog.net.Cookies.SetOptions=} options  The options object.
 */
goog.net.Cookies.prototype.set = function(name, value, options) {
  'use strict';
  /** @type {number|undefined} */
  let maxAge;
  /** @type {string|undefined} */
  let path;
  /** @type {string|undefined} */
  let domain;
  /** @type {boolean} */
  let secure = false;
  /** @type {!goog.net.Cookies.SameSite|undefined} */
  let sameSite;

  if (typeof options === 'object') {
    sameSite = options.sameSite;
    secure = options.secure || false;
    domain = options.domain || undefined;
    path = options.path || undefined;
    maxAge = options.maxAge;
  }
  if (!this.isValidName(name)) {
    throw new Error('Invalid cookie name "' + name + '"');
  }
  if (!this.isValidValue(value)) {
    throw new Error('Invalid cookie value "' + value + '"');
  }

  if (maxAge === undefined) {
    maxAge = -1;
  }

  const domainStr = domain ? ';domain=' + domain : '';
  const pathStr = path ? ';path=' + path : '';
  const secureStr = secure ? ';secure' : '';

  let expiresStr;

  // Case 1: Set a session cookie.
  if (maxAge < 0) {
    expiresStr = '';

    // Case 2: Remove the cookie.
    // Note: We don't tell people about this option in the function doc because
    // we prefer people to use remove() to remove cookies.
  } else if (maxAge == 0) {
    // Note: Don't use Jan 1, 1970 for date because NS 4.76 will try to convert
    // it to local time, and if the local time is before Jan 1, 1970, then the
    // browser will ignore the Expires attribute altogether.
    const pastDate = new Date(1970, 1 /*Feb*/, 1);  // Feb 1, 1970
    expiresStr = ';expires=' + pastDate.toUTCString();

    // Case 3: Set a persistent cookie.
  } else {
    const futureDate = new Date(Date.now() + maxAge * 1000);
    expiresStr = ';expires=' + futureDate.toUTCString();
  }

  const sameSiteStr = sameSite != null ? ';samesite=' + sameSite : '';

  this.setCookie_(
      name + '=' + value + domainStr + pathStr + expiresStr + secureStr +
      sameSiteStr);
};


/**
 * Returns the value for the first cookie with the given name.
 * @param {string} name  The name of the cookie to get.
 * @param {string=} opt_default  If not found this is returned instead.
 * @return {string|undefined}  The value of the cookie. If no cookie is set this
 *     returns opt_default or undefined if opt_default is not provided.
 */
goog.net.Cookies.prototype.get = function(name, opt_default) {
  'use strict';
  const nameEq = name + '=';
  const parts = this.getParts_();
  for (let i = 0, part; i < parts.length; i++) {
    part = goog.string.trim(parts[i]);
    // startsWith
    if (part.lastIndexOf(nameEq, 0) == 0) {
      return part.slice(nameEq.length);
    }
    if (part == name) {
      return '';
    }
  }
  return opt_default;
};


/**
 * Removes and expires a cookie.
 * @param {string} name  The cookie name.
 * @param {?string=} opt_path  The path of the cookie. If null or not present,
 *     expires the cookie set at the full request path.
 * @param {?string=} opt_domain  The domain of the cookie, or null to expire a
 *     cookie set at the full request host name. If not provided, the default is
 *     null (i.e. cookie at full request host name).
 * @return {boolean} Whether the cookie existed before it was removed.
 */
goog.net.Cookies.prototype.remove = function(name, opt_path, opt_domain) {
  'use strict';
  const rv = this.containsKey(name);
  this.set(name, '', {maxAge: 0, path: opt_path, domain: opt_domain});
  return rv;
};


/**
 * Gets the names for all the cookies.
 * @return {!Array<string>} An array with the names of the cookies.
 */
goog.net.Cookies.prototype.getKeys = function() {
  'use strict';
  return this.getKeyValues_().keys;
};


/**
 * Gets the values for all the cookies.
 * @return {!Array<string>} An array with the values of the cookies.
 */
goog.net.Cookies.prototype.getValues = function() {
  'use strict';
  return this.getKeyValues_().values;
};


/**
 * @return {boolean} Whether there are any cookies for this document.
 */
goog.net.Cookies.prototype.isEmpty = function() {
  'use strict';
  return !this.getCookie_();
};


/**
 * @return {number} The number of cookies for this document.
 */
goog.net.Cookies.prototype.getCount = function() {
  'use strict';
  const cookie = this.getCookie_();
  if (!cookie) {
    return 0;
  }
  return this.getParts_().length;
};


/**
 * Returns whether there is a cookie with the given name.
 * @param {string} key The name of the cookie to test for.
 * @return {boolean} Whether there is a cookie by that name.
 */
goog.net.Cookies.prototype.containsKey = function(key) {
  'use strict';
  // substring will return empty string if the key is not found, so the get
  // function will only return undefined
  return this.get(key) !== undefined;
};


/**
 * Returns whether there is a cookie with the given value. (This is an O(n)
 * operation.)
 * @param {string} value  The value to check for.
 * @return {boolean} Whether there is a cookie with that value.
 */
goog.net.Cookies.prototype.containsValue = function(value) {
  'use strict';
  // this O(n) in any case so lets do the trivial thing.
  const values = this.getKeyValues_().values;
  for (let i = 0; i < values.length; i++) {
    if (values[i] == value) {
      return true;
    }
  }
  return false;
};


/**
 * Removes all cookies for this document.  Note that this will only remove
 * cookies from the current path and domain.  If there are cookies set using a
 * subpath and/or another domain these will still be there.
 */
goog.net.Cookies.prototype.clear = function() {
  'use strict';
  const keys = this.getKeyValues_().keys;
  for (let i = keys.length - 1; i >= 0; i--) {
    this.remove(keys[i]);
  }
};


/**
 * Private helper function to allow testing cookies without depending on the
 * browser.
 * @param {string} s The cookie string to set.
 * @private
 */
goog.net.Cookies.prototype.setCookie_ = function(s) {
  'use strict';
  this.document_.cookie = s;
};


/**
 * Private helper function to allow testing cookies without depending on the
 * browser. IE6 can return null here.
 * @return {string} Returns the `document.cookie`.
 * @private
 */
goog.net.Cookies.prototype.getCookie_ = function() {
  'use strict';
  return this.document_.cookie;
};


/**
 * @return {!Array<string>} The cookie split on semi colons.
 * @private
 */
goog.net.Cookies.prototype.getParts_ = function() {
  'use strict';
  return (this.getCookie_() || '').split(';');
};


/**
 * Gets the names and values for all the cookies.
 * @return {{keys:!Array<string>, values:!Array<string>}} An object with keys
 *     and values.
 * @private
 */
goog.net.Cookies.prototype.getKeyValues_ = function() {
  'use strict';
  const parts = this.getParts_();
  const keys = [];
  const values = [];
  let index;
  let part;
  for (let i = 0; i < parts.length; i++) {
    part = goog.string.trim(parts[i]);
    index = part.indexOf('=');

    if (index == -1) {  // empty name
      keys.push('');
      values.push(part);
    } else {
      keys.push(part.substring(0, index));
      values.push(part.substring(index + 1));
    }
  }
  return {keys: keys, values: values};
};


/**
 * Options object for calls to Cookies.prototype.set.
 * @record
 */
goog.net.Cookies.SetOptions = function() {
  'use strict';
  /**
   * The max age in seconds (from now). Use -1 to set a session cookie. If not
   * provided, the default is -1 (i.e. set a session cookie).
   * @type {number|undefined}
   */
  this.maxAge;
  /**
   * The path of the cookie. If not present then this uses the full request
   * path.
   * @type {?string|undefined}
   */
  this.path;
  /**
   * The domain of the cookie, or null to not specify a domain attribute
   * (browser will use the full request host name). If not provided, the default
   * is null (i.e. let browser use full request host name).
   * @type {?string|undefined}
   */
  this.domain;
  /**
   * Whether the cookie should only be sent over a secure channel.
   * @type {boolean|undefined}
   */
  this.secure;
  /**
   * The SameSite attribute for the cookie (default is NONE).
   * @type {!goog.net.Cookies.SameSite|undefined}
   */
  this.sameSite;
};


/**
 * Valid values for the SameSite cookie attribute.  In 2019, browsers began the
 * process of changing the default from NONE to LAX.
 *
 * @see https://web.dev/samesite-cookies-explained
 * @see https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis-03#section-5.3.7
 * @enum {string}
 */
goog.net.Cookies.SameSite = {
  /**
   * The cookie will be sent in first-party contexts, including initial
   * navigation from external referrers.
   */
  LAX: 'lax',
  /**
   * The cookie will be sent in all first-party or third-party contexts. This
   * was the original default behavior of the web, but will need to be set
   * explicitly starting in 2020.
   */
  NONE: 'none',
  /**
   * The cookie will only be sent in first-party contexts. It will not be sent
   * on initial navigation from external referrers.
   */
  STRICT: 'strict',
};

/**
 * A static default instance.
 * @const {!goog.net.Cookies}
 * @private
 */
goog.net.Cookies.instance_ =
    new goog.net.Cookies(typeof document == 'undefined' ? null : document);

/**
 * Getter for the static instance of goog.net.Cookies.
 * @return {!goog.net.Cookies}
 */
goog.net.Cookies.getInstance = function() {
  'use strict';
  return goog.net.Cookies.instance_;
};
