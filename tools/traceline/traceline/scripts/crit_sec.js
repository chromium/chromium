// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// You should run this with v8, like v8_shell alloc.js datafile.json

function toHex(num) {
  var str = "";
  var table = "0123456789abcdef";
  while (num != 0) {
    str = table.charAt(num & 0xf) + str;
    num >>= 4;
  }
  return str;
}

function parseEvents(z) {
  var crits =  { }
  var calls = { }

  for (var i = 0, il = z.length; i < il; ++i) {
    var e = z[i];

    if (e['eventtype'] == 'EVENT_TYPE_ENTER_CS' ||
        e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS' ||
        e['eventtype'] == 'EVENT_TYPE_LEAVE_CS') {
      cs = e['critical_section'];
      if (!(cs in crits)) {
        crits[cs] = [ ];
      }
      crits[cs].push(e);
    }
  }

  // Verify that the locks get unlocked, and operations stay on the same thread.
  for (var key in crits) {
    var cs = key;
    var es = crits[key];

    var tid_stack = [ ];
    for (var j = 0, jl = es.length; j < jl; ++j) {
      var e = es[j];
      if (e['eventtype'] == 'EVENT_TYPE_ENTER_CS') {
        tid_stack.push(e);
      } else if (e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS') {
        if (e['retval'] != 0)
          tid_stack.push(e);
      } else if (e['eventtype'] == 'EVENT_TYPE_LEAVE_CS') {
        if (tid_stack.length == 0) {
          print('fail ' + e);
        }
        var tid = tid_stack.pop()
        if (tid['thread'] != e['thread']) {
          print('fail ' + tid);
        }
      }
    }
  }

  // Look for long-held / contended locks.  We can't really know it is
  // contended without looking if anyone is waiting on the embedded event...
  // Just look for locks are are held a long time?  Not so good...
  for (var key in crits) {
    var cs = key;
    var es = crits[key];

    var tid_stack = [ ];
    for (var j = 0, jl = es.length; j < jl; ++j) {
      var e = es[j];
      if (e['eventtype'] == 'EVENT_TYPE_ENTER_CS') {
        tid_stack.push(e);
      } else if (e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS') {
        if (e['retval'] != 0)
          tid_stack.push(e);
      } else if (e['eventtype'] == 'EVENT_TYPE_LEAVE_CS') {
        if (tid_stack.length == 0) {
          print('fail ' + e);
        }
        var tid = tid_stack.pop();
        var dur = e['ms'] - tid['ms'];
        if (dur > 0.1) {
          print('Lock: 0x' + toHex(cs) + ' for ' + dur + ' at: ' + e['ms']);
        }
      }
    }
  }
}
