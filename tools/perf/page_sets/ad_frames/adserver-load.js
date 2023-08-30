// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to load ads into the loader.html page. This file name was
// deliberately chosen because it matches an entry on the ads easylist, and
// will be marked by Chrome as an ad frame.

function LoadFrame(type, url) {
  const frame = document.createElement(type);
  if (type == "fencedframe") {
    frame.config = new FencedFrameConfig(url);
  } else {
    frame.src = url;
  }
  const dimensions = url.split(";sz=")[1].split("x");
  frame.width = dimensions[0];
  frame.height = dimensions[1];
  document.body.appendChild(frame);
}
