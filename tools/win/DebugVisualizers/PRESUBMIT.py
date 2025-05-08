# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import xml.etree.ElementTree


def _IsNatvisFile(affected_file):
    return affected_file.Extension() == ".natvis"


def _CheckNatvisIsValidXml(input_api, output_api):
    """Enforce that DebugVisualizer files are valid XML.

    DebugVisualizer files have the .natvis file extension, which are XML files
    that follow the schema found at:

    {Visual Studio install}/Xml/Schemas/1033/natvis.xsd

    We can't easily validate against a schema, so this simply checks that
    .natvis files parse correctly as XML.
    """
    results = []
    for f in input_api.AffectedTestableFiles(file_filter=_IsNatvisFile):
        try:
            content = '\n'.join(f.NewContents())
            xml.etree.ElementTree.fromstring(content)
        except xml.etree.ElementTree.ParseError as e:
            results.append(
                output_api.PresubmitError(
                    f"{f.LocalPath()} has invalid XML:\n{str(e)}"))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CheckNatvisIsValidXml(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return []
