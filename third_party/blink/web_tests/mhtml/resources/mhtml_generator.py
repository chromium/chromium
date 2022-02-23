#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (c) 2013, Opera Software ASA. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of Opera Software ASA nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

import email
import email.encoders
import email.generator
import email.mime.image
import email.mime.multipart
import email.mime.nonmultipart
import email.mime.text
import mimetypes
import os.path
import quopri
import sys

class ArgumentError(Exception):
    pass

def _encode_quopri(msg):
    """Own version of quopri isntead of email.encoders.quopri which seems to
       be buggy in python3"""
    orig = msg.get_payload()
    encdata = quopri.encodestring(orig, quotetabs=True)
    encdata.replace(b' ', b'=20')
    msg.set_payload(encdata.decode('ascii', 'surrogateescape'))
    msg['Content-Transfer-Encoding'] = 'quoted-printable'

def _encode_binary(msg):
    email.encoders.encode_noop(msg)
    msg['Content-Transfer-Encoding'] = 'binary'

TRANSFER_ENCODINGS = {
    "8bit": email.encoders.encode_7or8bit,
    "7bit": email.encoders.encode_7or8bit,
    "base64": email.encoders.encode_base64,
    "binary": _encode_binary,
    "none": email.encoders.encode_noop,
    "quoted-printable": _encode_quopri}

BASE = "http://test/"

def generate_message(parts):
    """Generate a mime message from the given parts"""

    main = email.mime.multipart.MIMEMultipart("related")
    main.add_header("Content-Location", BASE + parts[0]["name"])

    for part in parts:
        with open(part["name"], 'rb') as payload:
            sub = email.mime.text.MIMENonMultipart(*part["mime"].split("/"))
            sub.add_header("Content-Location", BASE + part["name"])
            sub.set_payload(payload.read())
            TRANSFER_ENCODINGS[part["transfer_encoding"]](sub)
            main.attach(sub)

    return main

def parse_arguments(args):
    """Parse arguments to extract file, transfer encoding, mime pairs"""

    parts = []
    current = {}

    for arg in args:
        if os.path.isfile(arg):
            if current:
                parts.append(current)
            current = {}
            current["name"] = arg
            current["transfer_encoding"] = "binary"
            current["mime"] = mimetypes.guess_type(arg)[0]
        elif arg.lower() in TRANSFER_ENCODINGS:
            current["transfer_encoding"] = arg.lower()
        elif "/" in arg:
            current["mime"] = arg.lower()
        else:
            raise ArgumentError("Unknown argument '" + arg + "'")

    if current:
        parts.append(current)

    return parts

def main():
    PARTS = parse_arguments(sys.argv[1:])
    MESSAGE = generate_message(PARTS)

    GENERATOR = email.generator.Generator(sys.stdout, mangle_from_=True,
                                          maxheaderlen=1000)
    GENERATOR.flatten(MESSAGE, linesep="\r\n")

if __name__ == "__main__":
    main()

