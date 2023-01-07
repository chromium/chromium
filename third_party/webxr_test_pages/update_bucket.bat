@echo off
:: Copyright 2019 The Chromium Authors
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This is a convenience wrapper to execute the script on Windows, since
:: .py files may not be directly executable on the command line. Alternatively,
:: you can also just run "python update_bucket.py ..." yourself.

python "%~dp0update_bucket.py" %*
