// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sprites = (function() {
  var objs;
  var width;
  var height;
  var SPRITE_SPEED = 2;

  var sprites = {};
  sprites.init = function(w, h) {
    objs = [];
    width = w;
    height = h;
  };
  sprites.add = function(img) {
    var obj = { img: img,
                  x: Math.random() * (width - img.width),
                  y: Math.random() * (height - img.height),
                 dx: SPRITE_SPEED * (Math.random() < .5 ? -1 : 1),
                 dy: SPRITE_SPEED * (Math.random() < .5 ? -1 : 1) };
    objs.push(obj);
  };
  sprites.draw = function(context) {
    for (var i = 0, len = objs.length; i < len; ++i) {
      var obj = objs[i];

      obj.x += obj.dx;
      if ((obj.x > (width - obj.img.width)) || (obj.x < 0))
        obj.dx *= -1;

      obj.y += obj.dy;
      if ((obj.y > (height - obj.img.height)) || (obj.y < 0))
        obj.dy *= -1;

      context.drawImage(obj.img, obj.x, obj.y);
  };
  };
  return sprites;
})();
