#!/usr/bin/env python3
#
# generate a test program for the API
#

import xml.etree.ElementTree as etree
import os
import re
import sys

import xmlmod

# Globals

dtors = {
    'htmlDoc *': 'xmlFreeDoc',
    'htmlParserCtxt *': 'htmlFreeParserCtxt',
    'xmlAutomata *': 'xmlFreeAutomata',
    'xmlBuffer *': 'xmlBufferFree',
    'xmlCatalog *': 'xmlFreeCatalog',
    'xmlChar *': 'xmlFree',
    'xmlDOMWrapCtxt *': 'xmlDOMWrapFreeCtxt',
    'xmlDict *': 'xmlDictFree',
    'xmlDoc *': 'xmlFreeDoc',
    'xmlDtd *': 'xmlFreeDtd',
    'xmlEntitiesTable *': 'xmlFreeEntitiesTable',
    'xmlElementContent *': 'xmlFreeElementContent',
    'xmlEnumeration *': 'xmlFreeEnumeration',
    'xmlList *': 'xmlListDelete',
    'xmlModule *': 'xmlModuleFree',
    'xmlMutex *': 'xmlFreeMutex',
    'xmlNode *': 'xmlFreeNode',
    'xmlNodeSet *': 'xmlXPathFreeNodeSet',
    'xmlNs *': 'xmlFreeNs',
    'xmlOutputBuffer *': 'xmlOutputBufferClose',
    'xmlParserCtxt *': 'xmlFreeParserCtxt',
    'xmlParserInputBuffer *': 'xmlFreeParserInputBuffer',
    'xmlParserInput *': 'xmlFreeInputStream',
    'xmlRMutex *': 'xmlFreeRMutex',
    'xmlRelaxNGValidCtxt *': 'xmlRelaxNGFreeValidCtxt',
    'xmlSaveCtxt *': 'xmlSaveClose',
    'xmlSchemaFacet *': 'xmlSchemaFreeFacet',
    'xmlSchemaVal *': 'xmlSchemaFreeValue',
    'xmlSchemaValidCtxt *': 'xmlSchemaFreeValidCtxt',
    'xmlTextWriter *': 'xmlFreeTextWriter',
    'xmlURI *': 'xmlFreeURI',
    'xmlValidCtxt *': 'xmlFreeValidCtxt',
    'xmlXPathContext *': 'xmlXPathFreeContext',
    'xmlXPathParserContext *': 'xmlXPathFreeParserContext',
    'xmlXPathObject *': 'xmlXPathFreeObject',
}

blockList = {
    # init/cleanup
    'xmlCleanupParser': True,
    'xmlInitParser': True,

    # arg must be non-NULL
    'xmlMemStrdupLoc': True,
    'xmlMemoryStrdup': True,

    # Returns void pointer which must be freed
    'xmlMallocAtomicLoc': True,
    'xmlMallocLoc': True,
    'xmlMemMalloc': True,
    'xmlMemRealloc': True,
    'xmlReallocLoc': True,

    # Would reset the error handler
    'xmlSetStructuredErrorFunc': True,

    # Prints errors
    'xmlCatalogGetPublic': True,
    'xmlCatalogGetSystem': True,
    'xmlDebugDumpDTD': True,
    'xmlDebugDumpDocument': True,
    'xmlDebugDumpNode': True,
    'xmlDebugDumpString': True,
    'xmlParserError': True,
    'xmlParserWarning': True,
    'xmlParserValidityError': True,
    'xmlParserValidityWarning': True,

    # Internal parser unctions, ctxt must be non-NULL
    'xmlParseAttribute': True,
    'xmlParseAttributeListDecl': True,
    'xmlParseAttributeType': True,
    'xmlParseCDSect': True,
    'xmlParseCharData': True,
    'xmlParseCharRef': True,
    'xmlParseComment': True,
    'xmlParseDefaultDecl': True,
    'xmlParseDocTypeDecl': True,
    'xmlParseEndTag': True,
    'xmlParseElement': True,
    'xmlParseElementChildrenContentDecl': True,
    'xmlParseElementContentDecl': True,
    'xmlParseElementDecl': True,
    'xmlParseElementMixedContentDecl': True,
    'xmlParseEncName': True,
    'xmlParseEncodingDecl': True,
    'xmlParseEntityDecl': True,
    'xmlParseEntityValue': True,
    'xmlParseEnumeratedType': True,
    'xmlParseEnumerationType': True,
    'xmlParseExternalID': True,
    'xmlParseExternalSubset': True,
    'xmlParseMarkupDecl': True,
    'xmlParseMisc': True,
    'xmlParseName': True,
    'xmlParseNmtoken': True,
    'xmlParseNotationDecl': True,
    'xmlParseNotationType': True,
    'xmlParsePEReference': True,
    'xmlParsePI': True,
    'xmlParsePITarget': True,
    'xmlParsePubidLiteral': True,
    'xmlParseReference': True,
    'xmlParseSDDecl': True,
    'xmlParseStartTag': True,
    'xmlParseSystemLiteral': True,
    'xmlParseTextDecl': True,
    'xmlParseVersionInfo': True,
    'xmlParseVersionNum': True,
    'xmlParseXMLDecl': True,
    'xmlParserHandlePEReference': True,
    'xmlSkipBlankChars': True,

    # reads from stdin
    'htmlReadFd': True,
    'xmlReadFd': True,
    'xmlReaderForFd': True,
}

# Parse document

if len(sys.argv) > 1:
    buildDir = sys.argv[1]
else:
    buildDir = '.'

xmlDocDir = buildDir + '/doc/xml'

filenames = {}
functions = {}

for file in os.listdir(xmlDocDir):
    if not file.endswith('_8h.xml'):
        continue

    doc = etree.parse(xmlDocDir + '/' + file)

    compound = doc.find('compounddef')
    module = compound.find('compoundname').text
    if not module.endswith('.h'):
        continue
    module = module[:-2]

    for section in compound.findall('sectiondef'):
        if section.get('kind') != 'func':
            continue

        for func in section.findall('memberdef'):
            name = func.find('name').text
            if name in blockList:
                continue

            module1, module2 = xmlmod.findModules(module, name)

            cargs = []
            skip = False
            for arg in func.findall('param'):
                atype = etree.tostring(arg.find('type'),
                    method='text', encoding='unicode').rstrip()
                if atype == 'void':
                    continue
                if atype == 'va_list':
                    skip = True
                    break
                if re.search(r'(Ptr|\*)$', atype):
                    cargs.append('NULL')
                else:
                    cargs.append('0')

            if skip:
                continue

            mfunc = functions.get(module1)
            if mfunc is None:
                mfunc = {}
                functions[module1] = mfunc

            mmfunc = mfunc.get(module2)
            if mmfunc is None:
                mmfunc = {}
                mfunc[module2] = mmfunc

            code = f'{name}({', '.join(cargs)})'

            rtype = etree.tostring(func.find('type'),
                method='text', encoding='unicode').rstrip()
            dtor = dtors.get(rtype)
            if dtor is not None:
                code = f'{dtor}({code})'
            elif rtype == 'xmlHashTable *':
                code = f'xmlHashFree({code}, NULL)'

            mmfunc[name] = f'    {code};\n'

# Write output

test = open('testapi.c', 'w')

test.write("""/*
 * testapi.c: libxml2 API tester program.
 *
 * Automatically generated by gentest.py
 *
 * See Copyright for the status of this software.
 */

/* Disable deprecation warnings */
#define XML_DEPRECATED

#include "libxml.h"
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/c14n.h>
#include <libxml/catalog.h>
#include <libxml/debugXML.h>
#include <libxml/parserInternals.h>
#include <libxml/pattern.h>
#include <libxml/relaxng.h>
#include <libxml/schematron.h>
#include <libxml/uri.h>
#include <libxml/xinclude.h>
#include <libxml/xlink.h>
#include <libxml/xmlmodule.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlsave.h>
#include <libxml/xmlschemas.h>
#include <libxml/xmlschemastypes.h>
#include <libxml/xmlwriter.h>
#include <libxml/xpathInternals.h>
#include <libxml/xpointer.h>

static void
ignoreError(void *userData ATTRIBUTE_UNUSED,
            const xmlError *error ATTRIBUTE_UNUSED) {
}

int
main(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED) {
    xmlInitParser();
    xmlSetStructuredErrorFunc(NULL, ignoreError);

""")

for module1 in sorted(functions.keys()):
    mfunc = functions[module1]

    if module1 != '':
        test.write(f'#ifdef LIBXML_{module1}_ENABLED\n')

    for module2 in sorted(mfunc.keys()):
        mmfunc = mfunc[module2]

        if module2 != '':
            test.write(f'#ifdef LIBXML_{module2}_ENABLED\n')

        for name in sorted(mmfunc.keys()):
            test.write(mmfunc[name])

        if module2 != '':
            test.write(f'#endif /* LIBXML_{module2}_ENABLED */\n')

    if module1 != '':
        test.write(f'#endif /* LIBXML_{module1}_ENABLED */\n')

    test.write('\n')

test.write("""    xmlCleanupParser();
    return 0;
}
""")
