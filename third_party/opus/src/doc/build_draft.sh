#!/bin/sh

# Copyright (c) 2011-2012 Xiph.Org Foundation and Mozilla Corporation
#
#  This file is extracted from RFC6716. Please see that RFC for additional
#  information.
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
#  - Neither the name of Internet Society, IETF or IETF Trust, nor the
#  names of specific contributors, may be used to endorse or promote
#  products derived from this software without specific prior written
#  permission.
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

toplevel=".."
destdir="opus_source"

echo packaging source code
rm -rf "${destdir}"
mkdir "${destdir}"
mkdir "${destdir}/src"
mkdir "${destdir}/silk"
mkdir "${destdir}/silk/float"
mkdir "${destdir}/silk/fixed"
mkdir "${destdir}/silk/fixed/x86"
mkdir "${destdir}/silk/fixed/arm"
mkdir "${destdir}/silk/fixed/mips"
mkdir "${destdir}/silk/x86"
mkdir "${destdir}/silk/arm"
mkdir "${destdir}/silk/mips"
mkdir "${destdir}/celt"
mkdir "${destdir}/celt/x86"
mkdir "${destdir}/celt/arm"
mkdir "${destdir}/celt/mips"
mkdir "${destdir}/include"
for f in `cat "${toplevel}"/opus_sources.mk "${toplevel}"/celt_sources.mk \
 "${toplevel}"/silk_sources.mk "${toplevel}"/opus_headers.mk \
 "${toplevel}"/celt_headers.mk "${toplevel}"/silk_headers.mk \
 | grep '\.[ch]' | sed -e 's/^.*=//' -e 's/\\\\//'` ; do
  cp -a "${toplevel}/${f}" "${destdir}/${f}"
done
cp -a "${toplevel}"/src/opus_demo.c "${destdir}"/src/
cp -a "${toplevel}"/src/opus_compare.c "${destdir}"/src/
cp -a "${toplevel}"/celt/opus_custom_demo.c "${destdir}"/celt/
cp -a "${toplevel}"/Makefile.unix "${destdir}"/Makefile
cp -a "${toplevel}"/opus_sources.mk "${destdir}"/
cp -a "${toplevel}"/celt_sources.mk "${destdir}"/
cp -a "${toplevel}"/silk_sources.mk "${destdir}"/
cp -a "${toplevel}"/README.draft "${destdir}"/README
cp -a "${toplevel}"/COPYING "${destdir}"/COPYING
cp -a "${toplevel}"/tests/run_vectors.sh "${destdir}"/

GZIP=-9 tar --owner=root --group=root --format=v7 -czf opus_source.tar.gz "${destdir}"
echo building base64 version
cat opus_source.tar.gz| base64 | tr -d '\n' | fold -w 64 | \
 sed -e 's/^/\<spanx style="vbare"\>###/' -e 's/$/\<\/spanx\>\<vspace\/\>/' > \
 opus_source.base64


#echo '<figure>' > opus_compare_escaped.c
#echo '<artwork>' >> opus_compare_escaped.c
#echo '<![CDATA[' >> opus_compare_escaped.c
#cat opus_compare.c >> opus_compare_escaped.c
#echo ']]>' >> opus_compare_escaped.c
#echo '</artwork>' >> opus_compare_escaped.c
#echo '</figure>' >> opus_compare_escaped.c

if [ ! -d ../opus_testvectors ] ; then
  echo "Downloading test vectors..."
  wget 'http://opus-codec.org/testvectors/opus_testvectors.tar.gz'
  tar -C .. -xvzf opus_testvectors.tar.gz
fi
echo '<figure>' > testvectors_sha1
echo '<artwork>' >> testvectors_sha1
echo '<![CDATA[' >> testvectors_sha1
(cd ../opus_testvectors; sha1sum *.bit *.dec) >> testvectors_sha1
#cd opus_testvectors
#sha1sum *.bit *.dec >> ../testvectors_sha1
#cd ..
echo ']]>' >> testvectors_sha1
echo '</artwork>' >> testvectors_sha1
echo '</figure>' >> testvectors_sha1

echo running xml2rfc
xml2rfc draft-ietf-codec-opus.xml draft-ietf-codec-opus.html &
xml2rfc draft-ietf-codec-opus.xml
wait
