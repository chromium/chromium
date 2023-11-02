// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getArgValue(argname)
{
  var url = window.location.href;
  var parts = url.split("?");
  if (parts.length == 0)
  {
    return "";
  }
  var arglist = parts[1];
  var args = arglist.split("&");
  for (i=0;i<args.length;i++)
  {
    var parts = args[i].split("=");
    if (parts[0] == argname)
    {
      value = parts[1];
      value = unescape(value);
      return value;
    }
  }
  return "";
}