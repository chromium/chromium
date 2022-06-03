/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides CORS support for HTTP based RPC requests.
 *
 * As part of net.rpc package, CORS features provided by this class
 * depend on the server support. Please check related specs to decide how
 * to enable any of the features provided by this class.
 */

goog.module('goog.net.rpc.HttpCors');

const GoogUri = goog.require('goog.Uri');
const googObject = goog.require('goog.object');
const googString = goog.require('goog.string');
const googUriUtils = goog.require('goog.uri.utils');


/**
 * The default URL parameter name to overwrite http headers with a URL param
 * to avoid CORS preflight.
 *
 * See https://github.com/whatwg/fetch/issues/210#issue-129531743 for the spec.
 *
 * @type {string}
 */
exports.HTTP_HEADERS_PARAM_NAME = '$httpHeaders';


/**
 * The default URL parameter name to overwrite http method with a URL param
 * to avoid CORS preflight.
 *
 * See https://github.com/whatwg/fetch/issues/210#issue-129531743 for the spec.
 *
 * @type {string}
 */
exports.HTTP_METHOD_PARAM_NAME = '$httpMethod';


/**
 * Generates the URL parameter value with custom headers encoded as
 * HTTP/1.1 headers block.
 *
 * @param {!Object<string, string>} headers The custom headers.
 * @return {string} The URL param to overwrite custom HTTP headers.
 */
exports.generateHttpHeadersOverwriteParam = function(headers) {
  let result = '';
  googObject.forEach(headers, function(value, key) {
    result += key;
    result += ':';
    result += value;
    result += '\r\n';
  });
  return result;
};


/**
 * Generates the URL-encoded URL parameter value with custom headers encoded as
 * HTTP/1.1 headers block.
 *
 * @param {!Object<string, string>} headers The custom headers.
 * @return {string} The URL param to overwrite custom HTTP headers.
 */
exports.generateEncodedHttpHeadersOverwriteParam = function(headers) {
  return googString.urlEncode(
      exports.generateHttpHeadersOverwriteParam(headers));
};


/**
 * Sets custom HTTP headers via an overwrite URL param.
 *
 * @param {!GoogUri|string} url The URI object or a string path.
 * @param {string} urlParam The URL param name.
 * @param {!Object<string, string>} extraHeaders The HTTP headers.
 * @return {!GoogUri|string} The URI object or a string path with headers
 * encoded as a url param.
 */
exports.setHttpHeadersWithOverwriteParam = function(
    url, urlParam, extraHeaders) {
  if (googObject.isEmpty(extraHeaders)) {
    return url;
  }
  const httpHeaders = exports.generateHttpHeadersOverwriteParam(extraHeaders);
  if (typeof url === 'string') {
    return googUriUtils.appendParam(
        url, googString.urlEncode(urlParam), httpHeaders);
  } else {
    url.setParameterValue(urlParam, httpHeaders);  // duplicate removed!
    return url;
  }
};
