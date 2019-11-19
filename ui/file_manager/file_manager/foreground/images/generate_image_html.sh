#!/bin/bash
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Search the current directory and its subdirectories for PNG and SVG images,
# and output an HTML page containing the images.
#
# Usage: ./generate_image_html.sh
#
source=$(pwd | sed -e "s|^.*chrome/src/||")

cat <<HTMLBEGIN
<!doctype html>
<html>
<head>
 <style>
  body {
    font-family: "Roboto", monospace;
    font-size: 16pt;
  }

  a[href] {
    text-decoration: none;
  }

  img {
     margin: 10px;
     background: #ddd;
     object-fit: contain;
     width: 100px;
     height: 100px;
  }

  p {
     text-align: top;
  }
 </style>
</head>
<body>
<h3>
image source: <a href="https://cs.chromium.org?q=${source}">${source}</a>
</h3>
HTMLBEGIN

output_html_image_element() {
  echo "<img src='${1}' title='${1}' class='${2}'>"
}

find_directories_containing_images() {
  find . | grep -e "\.svg$" -e "\.png$" | while read image; do
    echo $(dirname ${image})
  done | sort | uniq
}

for directory in $(find_directories_containing_images); do
  # generate HTML for the directory PNG images.
  echo "<h4>sub-directory ${directory} PNG images</h4><p>"
  ls ${directory} | grep -e "\.png$" | while read image; do
    output_html_image_element "${directory}/${image}" PNG
  done ; echo "</p>"

  # generate HTML for the directory SVG images.
  echo "<h4>sub-directory ${directory} SVG images</h4><p>"
  ls ${directory} | grep -e "\.svg$" | while read image; do
    output_html_image_element "${directory}/${image}" SVG
  done ; echo "</p>"
done

cat <<HTMLEND
</body>
</html>
HTMLEND
