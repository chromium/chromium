// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.title = 'page cycler';

// The __pages is assumed an array which containing the directories for
// various pages to exercise. Some page cycler tests don't have this variable.

var initialPage;
var hasVariablePages = (typeof __pages != 'undefined') &&
                       (__pages instanceof Array);
if (hasVariablePages)
  initialPage = __pages[0];

document.cookie = '__navigated_to_report=0; path=/';
document.cookie = '__pc_done=0; path=/';
if (hasVariablePages)
  document.cookie = '__pc_pages=' + __pages + '; path=/';
document.cookie = '__pc_timings=; path=/';

var options = location.search.substring(1).split('&');

function getopt(name) {
  var r = new RegExp('^' + name + '=');
  for (var i = 0; i < options.length; ++i) {
    if (options[i].match(r)) {
      return options[i].substring(name.length + 1);
    }
  }
  return null;
}

function start() {
  var iterations = document.getElementById('iterations').value;
  window.resizeTo(800, 800);
  var ts = (new Date()).getTime();
  var url = '';
  if (hasVariablePages)
    url = initialPage + '/';
  url += 'index.html?n=' + iterations + '&i=0&p=0&ts=' + ts + '&td=0';
  window.location = url;
}

function render_form() {
  var form = document.createElement('FORM');
  form.onsubmit = function(e) {
    start();
    e.preventDefault();
  };

  var label = document.createTextNode('Iterations: ');
  form.appendChild(label);

  var input = document.createElement('INPUT');
  input.setAttribute('id', 'iterations');
  input.setAttribute('type', 'number');
  var iterations = getopt('iterations');
  input.setAttribute('value', iterations ? iterations : '5');
  form.appendChild(input);

  input = document.createElement('INPUT');
  input.setAttribute('type', 'submit');
  input.setAttribute('value', 'Start');
  form.appendChild(input);

  document.body.appendChild(form);
}

render_form();

// should we start automatically?
if (location.search.match('auto=1')) {
  start();
} else {
  if (!window.gc) {
    document.write('<h3 style=\'color:red\'>WARNING: window.gc is not ' +
                   'defined. Test results may be unreliable! You must ' +
                   'started chrome also with <tt>--js-flags=\"--expose_gc\"' +
                   '</tt> for this test to work manually</h3>');
  }
}
