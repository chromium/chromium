#!/bin/sh

# Copyright (c) 2014 Xiph.Org Foundation and Mozilla Foundation
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  - Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#
#  - Redistributions in binary form must reproduce the above copyright
#  notice, this list of conditions and the following disclaimer in the
#  documentation and/or other materials provided with the distribution.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
#  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#Stop on errors
set -e
#Set the CWD to the location of this script
[ -n "${0%/*}" ] && cd "${0%/*}"

HTML=opus_in_isobmff.html

echo downloading updates...
CSS=${HTML%%.html}.css
wget -q http://vfrmaniac.fushizen.eu/contents/${HTML} -O ${HTML}
wget -q http://vfrmaniac.fushizen.eu/style.css -O ${CSS}

echo updating links...
cat ${HTML} | sed -e "s/\\.\\.\\/style.css/${CSS}/" > ${HTML}+ && mv ${HTML}+ ${HTML}

echo stripping...
cat ${HTML} | sed -e 's/<!--.*-->//g' > ${HTML}+ && mv ${HTML}+ ${HTML}
cat ${HTML} | sed -e 's/ *$//g' > ${HTML}+ && mv ${HTML}+ ${HTML}
cat ${CSS}  | sed -e 's/ *$//g' > ${CSS}+ && mv ${CSS}+ ${CSS}


VERSION=$(grep -F Version ${HTML} | sed 's/.*Version \([0-9]\.[0-9]\.[0-9]\).*/\1/')
echo Now at version ${VERSION}
