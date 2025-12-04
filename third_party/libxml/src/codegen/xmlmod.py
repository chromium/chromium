# Symbol to module mapping
#
# This relies on a few tables and some regexes.

import re

moduleMap = {
    'HTMLtree': 'HTML',
    'HTMLparser': 'HTML',
    'c14n': 'C14N',
    'catalog': 'CATALOG',
    'debugXML': 'DEBUG',
    'nanohttp': 'HTTP',
    'pattern': 'PATTERN',
    'relaxng': 'RELAXNG',
    'schemasInternals': 'SCHEMAS',
    'schematron': 'SCHEMATRON',
    'xinclude': 'XINCLUDE',
    'xlink': 'XPTR',
    'xmlautomata': 'REGEXP',
    'xmlmodule': 'MODULES',
    'xmlreader': 'READER',
    'xmlregexp': 'REGEXP',
    'xmlsave': 'OUTPUT',
    'xmlschemas': 'SCHEMAS',
    'xmlschemastypes': 'SCHEMAS',
    'xmlwriter': 'WRITER',
    'xpath': 'XPATH',
    'xpathInternals': 'XPATH',
    'xpointer': 'XPTR',
}

symbolMap1 = {
    # not VALID
    'xmlValidateNCName': '',
    'xmlValidateNMToken': '',
    'xmlValidateName': '',
    'xmlValidateQName': '',

    'htmlDefaultSAXHandlerInit': 'HTML',
    'xmlSAX2InitHtmlDefaultSAXHandler': 'HTML',

    'xmlRegisterHTTPPostCallbacks': 'HTTP',

    '__xmlOutputBufferCreateFilename': 'OUTPUT',
    'xmlAttrSerializeTxtContent': 'OUTPUT',
    'xmlUTF8ToIsolat1': 'OUTPUT',
    'xmlSprintfElementContent': 'OUTPUT',

    'xmlCreatePushParserCtxt': 'PUSH',
    'xmlParseChunk': 'PUSH',

    'xmlParseBalancedChunkMemory': 'SAX1',
    'xmlParseBalancedChunkMemoryRecover': 'SAX1',
    'xmlParseDoc': 'SAX1',
    'xmlParseEntity': 'SAX1',
    'xmlParseExternalEntity': 'SAX1',
    'xmlParseFile': 'SAX1',
    'xmlParseMemory': 'SAX1',
    'xmlSAXDefaultVersion': 'SAX1',
    'xmlSetupParserForBuffer': 'SAX1',

    'xmlCtxtGetValidCtxt': 'VALID',
    'xmlFreeValidCtxt': 'VALID',
    'xmlNewValidCtxt': 'VALID',

    'xmlCatalogConvert': 'SGML_CATALOG',
    'xmlConvertSGMLCatalog': 'SGML_CATALOG',
    'xmlLoadSGMLSuperCatalog': 'SGML_CATALOG',
}

symbolMap2 = {
    # not OUTPUT (should be fixed in xmlIO.h)
    'xmlOutputBufferCreateFilenameDefault': '',

    'xmlXPathDebugDumpCompExpr': 'DEBUG',
    'xmlXPathDebugDumpObject': 'DEBUG',
    'xmlSchemaDump': 'DEBUG',
    'xmlRelaxNGDump': 'DEBUG',

    'xmlACatalogDump': 'OUTPUT',
    'xmlCatalogDump': 'OUTPUT',
    'xmlIOHTTPOpenW': 'OUTPUT',
    'xmlNanoHTTPSave': 'OUTPUT',
    'xmlRegisterHTTPPostCallbacks': 'OUTPUT',
    'xmlRelaxNGDumpTree': 'OUTPUT',

    'xmlTextReaderPreservePattern': 'PATTERN',

    'htmlCreatePushParserCtxt': 'PUSH',
    'htmlParseChunk': 'PUSH',

    'xmlValidBuildContentModel': 'REGEXP',
    'xmlValidatePopElement': 'REGEXP',
    'xmlValidatePushCData': 'REGEXP',
    'xmlValidatePushElement': 'REGEXP',

    'xmlTextReaderRelaxNGSetSchema': 'RELAXNG',
    'xmlTextReaderRelaxNGValidate': 'RELAXNG',
    'xmlTextReaderRelaxNGValidateCtxt': 'RELAXNG',

    'xmlTextReaderSchemaValidate': 'SCHEMAS',
    'xmlTextReaderSchemaValidateCtxt': 'SCHEMAS',
    'xmlTextReaderSetSchema': 'SCHEMAS',

    'xmlTextReaderReadInnerXml': 'WRITER',
    'xmlTextReaderReadOuterXml': 'WRITER',
}

outputRegex = '|'.join((
    '^(html|xml(Buf)?)(Doc(Content|Format)?|Elem|Node)Dump',
    '^(html|xml)Save(Format)?File',
    '^xmlDump.*(Decl|Table)',
    '^xml(Alloc)?OutputBuffer',
    '^xml.*OutputCallbacks',
))

def findModules(filename, symbol):
    module1 = symbolMap1.get(symbol)

    if module1 is None:
        module1 = moduleMap.get(filename)

    if module1 is None:
        if re.search('^xml(Ctxt)?Valid|Parse(DTD|Dtd)', symbol):
            module1 = 'VALID'
        elif re.search('^xml(Recover|SAX(User)?Parse)', symbol):
            module1 = 'SAX1'
        elif re.search('^xmlIOHTTP', symbol):
            module1 = 'HTTP'

    module2 = symbolMap2.get(symbol)

    if module2 is None:
        if re.search(outputRegex, symbol):
            if module1 is None:
                module1 = 'OUTPUT'
            else:
                module2 = 'OUTPUT'

    if module1 is None:
        module1 = ''
    if module2 is None:
        module2 = ''

    return module1, module2
