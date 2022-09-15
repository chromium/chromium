// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Point(x, y) {
  this.x = x;
  this.y = y;
}

function get_random_color_string() {
  var characters = "0123456789ABCDEF";
  var color = "#";

  for (var i = 1; i < 7; i++) {
    color += characters[Math.floor(Math.random() * characters.length)];
  }

  return color;
}

function rand_sgn(number) {
  return Math.floor(Math.random() * 2) ? 1*number : -1*number;
}

function swap(array, i, j) {
  var temp = array[i];
  array[i] = array[j];
  array[j] = temp;
}

function shuffle(l) {
  for (var i = 0; i < l.length -1; i++) {
    var num_left = l.length - i;
    var to_swap = i + Math.floor(Math.random() * num_left);
    swap(l, i, to_swap);
  }
}

