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

function dump(obj) {
  for (var key in obj) {
    print('key: ' + key);
    print('  ' + obj[key]);
  }
}

function TopN(n) {
  this.n = n;
  this.min = 0;
  this.sorted = [ ];
}

TopN.prototype.add =
function(num, data) {
  if (num < this.min)
    return;

  this.sorted.push([num, data]);
  this.sorted.sort(function(a, b) { return b[0] - a[0] });
  if (this.sorted.length > this.n)
    this.sorted.pop();

  this.min = this.sorted[this.sorted.lenth - 1];
};

TopN.prototype.datas =
function() {
  var datas = [ ];
  for (var i = 0, il = this.sorted.length; i < il; ++i) {
    datas.push(this.sorted[i][1]);
  }
  return datas;
};

function parseEvents(z) {
  var topper = new TopN(1000);

  // Find the largest allocation.
  for (var i = 0, il = z.length; i < il; ++i) {
    var e = z[i];

    if (e['eventtype'] == 'EVENT_TYPE_ALLOCHEAP') {
      var size = e['heapsize'];
      topper.add(e['heapsize'], e);
    }
  }

  var datas = topper.datas();
  for (var i = 0, il = datas.length; i < il; ++i) {
    dump(datas[i]);
  }
}
