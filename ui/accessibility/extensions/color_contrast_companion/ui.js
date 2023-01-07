// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var maxScale = 32;

var scale = 4;
var fgtoggle = true;
var fgcolor = null;
var bgcolor = null;
var srcImage = null;
var imageData = null;
var prevScale = 1;

let close = document.getElementById('close');
close.addEventListener('click', () => {
  console.log('Sending message');
  chrome.runtime.sendMessage({'close': true});
});
let help = document.getElementById('help');
help.addEventListener('click', () => {
  window.open(chrome.runtime.getURL('help.html'), '_blank');
});

let canvas = document.querySelector('canvas');

function repaint() {
  console.log('Repaint ' + scale);
  let width = srcImage.naturalWidth;
  let height = srcImage.naturalHeight;
  let context = canvas.getContext('2d');
  context.imageSmoothingEnabled = false;
  canvas.style.transform = 'scale(' + scale + ')';

  canvas.width = 1;
  canvas.height = 1;
  canvas.offsetLeft;
  canvas.width = width;
  canvas.height = height;
  canvas.offsetLeft;

  context.drawImage(srcImage, 0, 0, width, height);
  imageData = context.getImageData(0, 0, width, height).data;
}


chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.imageDataUrl) {
    var img = document.createElement('img');
    img.addEventListener('load', () => {
      srcImage = img;
      repaint();
    });
    img.src = request.imageDataUrl;
  }
});

let hover = document.getElementById('hover');
let hoverrgb = document.getElementById('hoverrgb');
let fg = document.getElementById('fg');
let fgrgb = document.getElementById('fgrgb');
let bg = document.getElementById('bg');
let bgrgb = document.getElementById('bgrgb');
let scrollpanel = document.getElementById('img_panel');
let ratio = document.getElementById('ratio');
let details = document.getElementById('details');
let zoomin = document.getElementById('zoomin');
let zoomout = document.getElementById('zoomout');
let highlight = document.getElementById('highlight');

fg.classList.add('current');
bg.classList.remove('current');

function rgbToHex(color) {
  var r = color[0];
  var g = color[1];
  var b = color[2];
  return '#' + ((r << 16) | (g << 8) | b).toString(16);
}

function getRelativeLuminance(color) {
  var rSRGB = color[0] / 255;
  var gSRGB = color[1] / 255;
  var bSRGB = color[2] / 255;
  var r =
      rSRGB <= .03928 ? rSRGB / 12.92 : Math.pow((rSRGB + .055) / 1.055, 2.4);
  var g =
      gSRGB <= .03928 ? gSRGB / 12.92 : Math.pow((gSRGB + .055) / 1.055, 2.4);
  var b =
      bSRGB <= .03928 ? bSRGB / 12.92 : Math.pow((bSRGB + .055) / 1.055, 2.4);
  return .2126 * r + .7152 * g + .0722 * b;
};

function getContrast(color1, color2) {
  var c1lum = getRelativeLuminance(color1);
  var c2lum = getRelativeLuminance(color2);
  return (Math.max(c1lum, c2lum) + .05) / (Math.min(c1lum, c2lum) + .05);
}

let context = canvas.getContext('2d');
let bounds = canvas.getBoundingClientRect();

function getXY(evt) {
  var x = evt.clientX + scrollpanel.scrollLeft - bounds.left;
  var y = evt.clientY + scrollpanel.scrollTop - bounds.top;
  return [Math.floor(x / scale), Math.floor(y / scale)];
}

function getColor(x, y) {
  try {
    var pixelIndex = y * srcImage.naturalWidth + x;
    return imageData.slice(4 * pixelIndex, 4 * (pixelIndex + 1));
  } catch (e) {
    return [0, 0, 0, 0];
  }
}

function brightness(color) {
  return (color[0] + color[1] + color[2]) / 3;
}

function localMax(x, y) {
  // This needs to be optional. Doesn't always do what we want.
  return [x, y];

  if (x < 0 || x >= srcImage.naturalWidth || y < 0 ||
      y + j >= srcImage.naturalHeight) {
    return [x, y];
  }

  var ctr = getColor(x, y);
  var max = brightness(ctr);

  var max;
  var amax;
  for (var i = -2; i <= 2; i++) {
    for (var j = -2; j <= 2; j++) {
      if (x + i < 0 || x + i >= srcImage.naturalWidth)
        continue;
      if (y + j < 0 || y + j >= srcImage.naturalHeight)
        continue;
      var c = getColor(x + i, y + j);
      var cbright = brightness(c);
      if (max > 128 && cbright > max) {
        max = cbright;
        amax = [x + i, y + j];
      } else if (max < 128 && cbright < max) {
        max = cbright;
        amax = [x + i, y + j];
      }
    }
  }

  if (amax)
    return amax;
  else
    return [x, y];
}

canvas.addEventListener('mousemove', (evt) => {
  var x1, y1, x, y;
  [x1, y1] = getXY(evt);
  [x, y] = localMax(x1, y1);
  var color = getColor(x, y);
  var hex = rgbToHex(color);
  hover.style.backgroundColor = hex;
  hoverrgb.innerText = hex;

  if (scale >= 8) {
    highlight.style.display = 'block';
    highlight.style.left = (scale * x) + 'px';
    highlight.style.top = (scale * y) + 'px';
    highlight.style.width = scale + 'px';
    highlight.style.height = scale + 'px';
    highlight.style.top = (scale * y) + 'px';
  } else {
    highlight.style.display = 'none';
  }
});

canvas.addEventListener('mouseenter', (evt) => {
  highlight.style.display = 'block';
});

canvas.addEventListener('mouseleave', (evt) => {
  highlight.style.display = 'none';
});

canvas.addEventListener('click', (evt) => {
  var x1, y1, x, y;
  [x1, y1] = getXY(evt);
  [x, y] = localMax(x1, y1);
  var color = getColor(x, y);
  var hex = rgbToHex(color);
  if (fgtoggle) {
    fg.style.backgroundColor = hex;
    fgrgb.innerText = hex;
    fgcolor = color;
    bg.classList.add('current');
    fg.classList.remove('current');
  } else {
    bg.style.backgroundColor = hex;
    bgrgb.innerText = hex;
    bgcolor = color;
    fg.classList.add('current');
    bg.classList.remove('current');
  }
  fgtoggle = !fgtoggle;
  if (fgcolor && bgcolor) {
    var contrast = getContrast(fgcolor, bgcolor);
    ratio.innerText = contrast.toFixed(2);
    details.innerHTML = 'Foreground: ' + fgrgb.innerText + '\n' +
        'Background: ' + bgrgb.innerText + '\n' +
        'Ratio: ' + ratio.innerText;
    details.select();
  }
});

function updateZoom() {
  highlight.style.display = 'none';
  var prevScrollLeft = scrollpanel.scrollLeft;
  var prevScrollTop = scrollpanel.scrollTop;
  var panelBounds = scrollpanel.getBoundingClientRect();

  localStorage.setItem('scale', scale);
  zoomout.disabled = (scale == 1);
  zoomin.disabled = (scale >= maxScale);
  if (srcImage)
    repaint();

  console.log('prev: ' + prevScrollLeft + ', ' + prevScrollTop);
  console.log('factor: ' + (scale / prevScale));
  var newLeft = prevScrollLeft * (scale / prevScale);
  var newTop = prevScrollTop * (scale / prevScale);
  console.log('newLeft: ' + newLeft);
  console.log('newTop: ' + newTop);
  if (scale > prevScale) {
    newLeft += panelBounds.width / 2;
    newTop += panelBounds.height / 2;
    console.log('c newLeft: ' + newLeft);
    console.log('c newTop: ' + newTop);
  } else if (scale < prevScale) {
    newLeft -= panelBounds.width / 4;
    newTop -= panelBounds.height / 4;
    console.log('c newLeft: ' + newLeft);
    console.log('c newTop: ' + newTop);
  }
  scrollpanel.scrollLeft = newLeft;
  scrollpanel.scrollTop = newTop;
  prevScale = scale;
}

var scalevalue = localStorage.getItem('scale');
scale = parseInt(scalevalue, 10);
if (!scale || scale < 1 || scale > maxScale)
  scale = 4;
prevScale = scale;

updateZoom();

function onZoomIn() {
  if (scale < maxScale)
    scale *= 2;
  updateZoom();
}

function onZoomOut() {
  if (scale > 1)
    scale /= 2;
  updateZoom();
}

zoomin.addEventListener('click', () => {
  onZoomIn();
});

zoomout.addEventListener('click', () => {
  onZoomOut();
});

document.addEventListener('keydown', function(e) {
  if (e.key == '+' || e.key == '=') {
    onZoomIn();
  }
  if (e.key == '-') {
    onZoomOut();
  }

  console.log(e.key);
});
