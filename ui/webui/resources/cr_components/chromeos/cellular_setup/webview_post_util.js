// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides utility methods used by the cellular activation flow.
 * Current: chrome://mobilesetup (mobile_setup.html/mobile_setup_portal.html)
 * New UI: chrome://cellular-setup (cellular_setup_dialog.html)
 */
cr.define('webviewPost.util', function() {
  /**
   * The script executed in the webview that is expected to be initialized
   * using POST request. The script parses the POST data (which is provided as
   * list of name value pairs) and adds appropriate input elements to the form,
   * sets the form action to the paymentUrl, and submits the form.
   *
   * The script should be run with the following arguments:
   * <code>form</code> - Form element that should be initialized and submitted.
   * <code>paymentUrl</code> - The target form action URL.
   * <code>postData</code> - The post request data submitted through the form.
   *    Example format of post data:
   *        <code>name1=value1&name2=value2&name3</code>
   *    Note that for <code>name3</code>, the value will be set to
   *    <code>true</code>.
   * @const {string}
   */
  const WEBVIEW_REDIRECT_SCRIPT = '(function(form, paymentUrl, postData) {' +
      'function addInputElement(form, name, value) {' +
      '  var input = document.createElement(\'input\');' +
      '  input.type = \'hidden\';' +
      '  input.name = name;' +
      '  input.value = value;' +
      '  form.appendChild(input);' +
      '}' +
      'function initFormFromPostData(form, postData) {' +
      '  if (!postData) return;' +
      '  var pairs = postData.split(\'&\');' +
      '  pairs.forEach(pairStr => {' +
      '    var pair = pairStr.split(\'=\');' +
      '    if (pair.length == 2)' +
      '      addInputElement(form, pair[0], pair[1]);' +
      '    else if (pair.length == 1)' +
      '      addInputElement(form, pair[0], true);' +
      '  });' +
      '}' +
      'form.action = unescape(paymentUrl);' +
      'form.method = \'POST\';' +
      'initFormFromPostData(form, unescape(postData));' +
      'form.submit();' +
      '})';

  /**
   * @const {string} The ID used for the form element in the initial webiew
   *     HTML.
   */
  const WEBVIEW_REDIRECT_FORM_ID = 'redirectForm';

  /**
   * @const {string} The initial webview HTML - this will be loaded into the
   *     webview using data URL before executing
   *     <code>WEBVIEW_REDIRECT_SCRIPT</code>.
   */
  const WEBVIEW_REDIRECT_HTML = '<html><body>' +
      '<form id="' + WEBVIEW_REDIRECT_FORM_ID + '"></form>' +
      '</body></html>';

  /**
   * Handles load commit event in the webview.
   * It runs <code>WEBVIEW_REDIRECT_SCRIPT</code> in the webview.
   * @param {!WebView} webview The targer webview element.
   * @param {string} paymentUrl URL to load.
   * @param {string} postData Data to pass.
   * @param {string} webviewSrc The intended webview URL - commit events that
   *     do not match this URL will be ignored.
   * @param {!Object} commitEvent The loadcommit event.
   */
  function initializeWebviewRedirectForm(
      webview, paymentUrl, postData, webviewSrc, commitEvent) {
    if (!commitEvent.isTopLevel || commitEvent.url != webviewSrc) {
      return;
    }

    webview.executeScript({
      code: WEBVIEW_REDIRECT_SCRIPT + '(' +
          'document.getElementById(\'' + WEBVIEW_REDIRECT_FORM_ID + '\'),' +
          ' \'' + escape(paymentUrl) + '\',' +
          ' \'' + escape(postData || '') + '\');'
    });
  }

  /**
   * Initialized webview using a POST request described in by
   * <code>paymentUrl</code> and <code>postData</code>.
   * @param {!WebView} webview The webview to be initialized.
   * @param {string} paymentUrl URL to load.
   * @param {string} postData Data to pass.
   */
  function postDeviceDataToWebview(webview, paymentUrl, postData) {
    const webviewSrc = 'data:text/html;charset=utf-8,' +
        encodeURIComponent(WEBVIEW_REDIRECT_HTML);
    webview.addEventListener(
        'loadcommit',
        initializeWebviewRedirectForm.bind(
            this, webview, paymentUrl, postData, webviewSrc));
    webview.src = webviewSrc;
  }

  return {postDeviceDataToWebview: postDeviceDataToWebview};
});
