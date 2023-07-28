// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function () {
  const status = document.getElementById("status");
  if (!status) {
    return;
  }

  status.innerText = "Loaded";
}
