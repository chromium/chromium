// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

function toCamelCase(property) {
  for (var i = property.length - 2; i > 0; --i) {
    if (property[i] === '-') {
      property = property.substring(0, i) +
        property[i + 1].toUpperCase() +
        property.substring(i + 2);
    }
  }
  return property;
}

function perfTestCSSValue(options) {
  var svgTag = options.svgTag;
  var property = options.property;
  var from = options.from;
  var to = options.to;
  console.assert(property);
  console.assert(from);
  console.assert(to);
  console.assert(PerfTestHelper);
  console.assert(document.getElementById('container'));

  var duration = 15000;

  var N = PerfTestHelper.getN(1000);
  var targets = [];
  for (var i = 0; i < N; i++) {
    var target;
    if (svgTag) {
      target = document.createElementNS("http://www.w3.org/2000/svg", svgTag);
    } else {
      target = document.createElement('target');
    }
    targets.push(target);
    container.appendChild(target);
  }

  var tag = svgTag || 'target';

  var api = PerfTestHelper.getParameter('api');
  switch (api) {
  case 'css_animations':
    var style = document.createElement('style');
    style.textContent = '' +
      '@-webkit-keyframes anim {\n' +
      '  from {' + property + ': ' + from + ';}\n' +
      '  to {' + property + ': ' + to + ';}\n' +
      '}\n' +
      tag + ' {\n' +
      '  -webkit-animation: anim ' + duration + 'ms linear infinite;\n' +
      '}\n';
    document.head.appendChild(style);
    break;
  case 'web_animations':
    var keyframes = [{}, {}];
    keyframes[0][toCamelCase(property)] = from;
    keyframes[1][toCamelCase(property)] = to;
    targets.forEach(function(target) {
      target.animate(keyframes, {
        duration: duration,
        iterations: Infinity,
      });
    });
    break;
  default:
    throw 'Invalid api: ' + api;
  }

  PerfTestHelper.signalReady();
}
