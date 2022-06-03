# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Sends a single message on opening containing the headers received from the
# browser. The header keys have been converted to lower-case, while the values
# retain the original case.

import json

from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    # Since python 3 does not lowercase the dictionary key, manually lower all
    # keys to maintain python 2/3 compatibility
    lowered_dict = {header.lower(): value for header, value in request.headers_in.items()}
    msgutil.send_message(request, json.dumps(lowered_dict))
