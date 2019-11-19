// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Logic to process PDFs
 *
 * This file runs when ChromeVox is injected into Chrome's component
 * extension that implements PDF support. The extension wraps a
 * PDFium plug-in object.
 */

goog.provide('cvox.PdfProcessor');
goog.require('cvox.QueueMode');

/**
 * The current PDF we're processing, or null if we're not processing one.
 * @type {Element}
 */
cvox.PdfProcessor.pdfEmbed = null;

/**
 * The number of pages in the current PDF, or null if we haven't
 * retreived that yet.
 * @type {?number}
 */
cvox.PdfProcessor.pageCount = null;

/**
 * The 0-based index of the page we're currently retrieving, or null
 * if we don't know how many pages there are yet.
 * @type {?number}
 */
cvox.PdfProcessor.pageIndex = null;

/**
 * The element on the page where all of the generated content from the PDF
 * will go, or null if we're not currently processing a PDF.
 * @type {Element}
 */
cvox.PdfProcessor.documentDiv = null;

/**
 * Process PDFs created with Chrome's built-in PDF plugin, which has an
 * accessibility hook.
 */
cvox.PdfProcessor.processEmbeddedPdfs = function() {
  if (window.location.hash == '#original') {
    return;
  }

  var pdfEmbeds = document.querySelectorAll(
      'embed[type="application/x-google-chrome-pdf"]');
  if (pdfEmbeds.length == 0) {
    return;
  }
  cvox.PdfProcessor.pdfEmbed = pdfEmbeds[0];

  // Install our event listener for responses.
  window.addEventListener('message',
      /** @type {EventListener} */(cvox.PdfProcessor.onMessage));

  // Start processing the pdf.
  cvox.PdfProcessor.process();
};

/**
 * Send a message to the plug-in to begin processing. If there are no more
 * elements in the array, remove the event listener and reset
 * NavigationManager so that it lands at the top of the now-modified page.
 */
cvox.PdfProcessor.process = function() {
  cvox.PdfProcessor.pageCount = null;
  cvox.PdfProcessor.pageIndex = null;
  window.postMessage({'type': 'getAccessibilityJSON'}, '*');
};

/**
 * Called after finishing the pdf.
 */
cvox.PdfProcessor.finish = function() {
  window.removeEventListener('message',
      /** @type {EventListener} */(cvox.PdfProcessor.onMessage));
  cvox.PdfProcessor.pdfEmbed = null;
  cvox.ChromeVox.navigationManager.reset();
};

/**
 * Handler for the 'message' event on the window, which is how we get responses
 * from Chrome's PDF plugin.
 *
 * @param {{data: {type: string, json: string}}} message The message from the
 *     PDF plugin containing a type identifier and JSON string.
 */
cvox.PdfProcessor.onMessage = function(message) {
  // Exit if it's not an accessibility JSON reply message.
  if (message.data.type != 'getAccessibilityJSONReply') {
    return;
  }

  // Exit if we aren't in the middle of processing a PDF.
  if (!cvox.PdfProcessor.pdfEmbed) {
    return;
  }

  // Parse the JSON.
  var info = /** @type {PDFAccessibilityJSONReply} */(
      JSON.parse(message.data.json));

  // If we already know how many pages are in the doc, we expect this message
  // contains the data for one particular page.
  if (cvox.PdfProcessor.pageCount !== null) {
    cvox.PdfProcessor.processOnePage(info);
    return;
  }

  // If not, we expect this message contains the info about the PDF overall:
  // whether it's loaded, whether it's copyable, and how many total pages
  // there are.
  if (!info.loaded) {
    cvox.PdfProcessor.pdfEmbed = null;
    window.setTimeout(cvox.PdfProcessor.finish, 100);
    return;
  }

  // Create the initial HTML skeleton.
  cvox.PdfProcessor.documentDiv = document.createElement('DIV');
  var headerDiv = document.createElement('DIV');
  headerDiv.style.position = 'relative';
  headerDiv.style.background = 'white';
  headerDiv.style.margin = '20pt';
  headerDiv.style.padding = '20pt';
  headerDiv.style.border = '1px solid #000';
  var src = cvox.PdfProcessor.pdfEmbed.src;
  var filename = src.substr(src.lastIndexOf('/') + 1);
  document.title = filename;
  var html = Msgs.getMsg('pdf_header', [filename, src + '#original']);
  headerDiv.innerHTML = html;
  // Set up a handler to reload the page when 'Show original' is clicked.
  var showLink = headerDiv.getElementsByTagName('a')[0];
  showLink.addEventListener('click', function() {
    window.location.href = src + '#original';
    window.location.reload();
  }, true);
  cvox.PdfProcessor.documentDiv.appendChild(headerDiv);
  cvox.PdfProcessor.documentDiv.style.position = 'relative';
  cvox.PdfProcessor.documentDiv.style.background = '#CCC';
  cvox.PdfProcessor.documentDiv.style.paddingTop = '1pt';
  cvox.PdfProcessor.documentDiv.style.paddingBottom = '1pt';
  cvox.PdfProcessor.documentDiv.style.width = '100%';
  cvox.PdfProcessor.documentDiv.style.minHeight = '100%';

  if (!info.copyable) {
    var alert = document.createElement('div');
    alert.setAttribute('role', 'alert');
    alert.innerText = Msgs.getMsg('copy_protected_pdf');
    cvox.PdfProcessor.documentDiv.appendChild(alert);
    cvox.PdfProcessor.pdfEmbed.parentElement.appendChild(
        cvox.PdfProcessor.documentDiv);
    return;
  }

  // Start processing the first page.
  cvox.PdfProcessor.pageCount = info.numberOfPages;
  cvox.PdfProcessor.pageIndex = -1;
  cvox.PdfProcessor.getNextPage();
};

/**
 * Send a message to the PDF plugin to get the next page. If we've finished
 * getting all of the pages, clean up and get the next PDF in the document.
 */
cvox.PdfProcessor.getNextPage = function() {
  cvox.PdfProcessor.pageIndex++;
  if (cvox.PdfProcessor.pageIndex >= cvox.PdfProcessor.pageCount) {
    cvox.PdfProcessor.pdfEmbed.style.display = 'none';
    cvox.PdfProcessor.pdfEmbed.parentElement.appendChild(
        cvox.PdfProcessor.documentDiv);
    cvox.PdfProcessor.finish();
    return;
  }

  cvox.PdfProcessor.pdfEmbed.postMessage(
      {'type': 'getAccessibilityJSON',
       'page': cvox.PdfProcessor.pageIndex});
};

/**
 * Process one page in the PDF file and turn it into HTML.
 * @param {PDFAccessibilityJSONReply} info The data from one page of the PDF.
 */
cvox.PdfProcessor.processOnePage = function(info) {
  var pageDiv = document.createElement('DIV');
  var pageAnchor = document.createElement('A');

  // Page Achor Setup
  pageAnchor.name = 'page' + cvox.PdfProcessor.pageIndex;

  // Page Styles
  pageDiv.style.position = 'relative';
  pageDiv.style.background = 'white';
  pageDiv.style.margin = 'auto';
  pageDiv.style.marginTop = '20pt';
  pageDiv.style.marginBottom = '20pt';
  pageDiv.style.height = info.height + 'pt';
  pageDiv.style.width = info.width + 'pt';
  pageDiv.style.boxShadow = '0pt 0pt 10pt #333';

  // Text Nodes
  var texts = info.textBox;
  for (var j = 0; j < texts.length; j++) {
    var text = texts[j];
    var textSpan = document.createElement('Span');

    // Text Styles
    textSpan.style.position = 'absolute';
    textSpan.style.left = text.left + 'pt';
    textSpan.style.top = text.top + 'pt';
    textSpan.style.fontSize = (0.8 * text.height) + 'pt';

    // Text Content
    for (var k = 0; k < text.textNodes.length; k++) {
      var node = text.textNodes[k];
      if (node.type == 'text') {
        textSpan.appendChild(document.createTextNode(node.text));
      } else if (node.type == 'url') {
        var a = document.createElement('A');
        a.href = node.url;
        a.appendChild(document.createTextNode(node.text));
        textSpan.appendChild(a);
      }
    }

    pageDiv.appendChild(textSpan);
  }
  cvox.PdfProcessor.documentDiv.appendChild(pageAnchor);
  cvox.PdfProcessor.documentDiv.appendChild(pageDiv);

  // Now get the next page.
  cvox.PdfProcessor.getNextPage();
};
