# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Support for formatting an RC file for compilation.
'''

import os
import re
from functools import partial

from grit import util
from grit.node import misc


def Format(root, lang='en', output_dir='.'):
  from grit.node import empty, include, message, structure

  yield _FormatHeader(root, lang, output_dir)

  for item in root.ActiveDescendants():
    if isinstance(item, empty.MessagesNode):
      # Write one STRINGTABLE per <messages> container.
      # This is hacky: it iterates over the children twice.
      yield 'STRINGTABLE\nBEGIN\n'
      for subitem in item.ActiveDescendants():
        if isinstance(subitem, message.MessageNode):
          with subitem:
            yield FormatMessage(subitem, lang)
      yield 'END\n\n'
    elif isinstance(item, include.IncludeNode):
      with item:
        yield FormatInclude(item, lang, output_dir)
    elif isinstance(item, structure.StructureNode):
      with item:
        yield FormatStructure(item, lang, output_dir)


'''
This dictionary defines the language charset pair lookup table, which is used
for replacing the GRIT expand variables for language info in Product Version
resource. The key is the language ISO country code, and the value
is the language and character-set pair, which is a hexadecimal string
consisting of the concatenation of the language and character-set identifiers.
The first 4 digit of the value is the hex value of LCID, the remaining
4 digits is the hex value of character-set id(code page)of the language.

LCID resource: http://msdn.microsoft.com/en-us/library/ms776294.aspx
Codepage resource: http://www.science.co.il/language/locale-codes.asp

We have defined three GRIT expand_variables to be used in the version resource
file to set the language info. Here is an example how they should be used in
the VS_VERSION_INFO section of the resource file to allow GRIT to localize
the language info correctly according to product locale.

VS_VERSION_INFO VERSIONINFO
...
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
      BLOCK "[GRITVERLANGCHARSETHEX]"
        BEGIN
        ...
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", [GRITVERLANGID], [GRITVERCHARSETID]
    END
END

'''

_LANGUAGE_CHARSET_PAIR = {
  # Language neutral LCID, unicode(1200) code page.
  'neutral'     : '000004b0',
  # LANG_USER_DEFAULT LCID, unicode(1200) code page.
  'userdefault' : '040004b0',
  'ar'          : '040104e8',
  'fi'          : '040b04e4',
  'ko'          : '041203b5',
  'es'          : '0c0a04e4',
  'bg'          : '040204e3',
  # No codepage for filipino, use unicode(1200).
  'fil'         : '046404e4',
  'fr'          : '040c04e4',
  'lv'          : '042604e9',
  'sv'          : '041d04e4',
  'ca'          : '040304e4',
  'de'          : '040704e4',
  'lt'          : '042704e9',
  # Do not use! This is only around for backwards
  # compatibility and will be removed - use fil instead
  'tl'          : '0c0004b0',
  'zh-CN'       : '080403a8',
  'zh-TW'       : '040403b6',
  'zh-HK'       : '0c0403b6',
  'el'          : '040804e5',
  'no'          : '001404e4',
  'nb'          : '041404e4',
  'nn'          : '081404e4',
  'th'          : '041e036a',
  'he'          : '040d04e7',
  'iw'          : '040d04e7',
  'pl'          : '041504e2',
  'tr'          : '041f04e6',
  'hr'          : '041a04e4',
  # No codepage for Hindi, use unicode(1200).
  'hi'          : '043904b0',
  'pt-PT'       : '081604e4',
  'pt-BR'       : '041604e4',
  'uk'          : '042204e3',
  'cs'          : '040504e2',
  'hu'          : '040e04e2',
  'ro'          : '041804e2',
  # No codepage for Urdu, use unicode(1200).
  'ur'          : '042004b0',
  'da'          : '040604e4',
  'is'          : '040f04e4',
  'ru'          : '041904e3',
  'vi'          : '042a04ea',
  'nl'          : '041304e4',
  'id'          : '042104e4',
  'sr'          : '081a04e2',
  'en-GB'       : '0809040e',
  'it'          : '041004e4',
  'sk'          : '041b04e2',
  'et'          : '042504e9',
  'ja'          : '041103a4',
  'sl'          : '042404e2',
  'en'          : '040904b0',
  # LCID for Mexico; Windows does not support L.A. LCID.
  'es-419'      : '080a04e4',
  # No codepage for Bengali, use unicode(1200).
  'bn'          : '044504b0',
  'fa'          : '042904e8',
  # No codepage for Gujarati, use unicode(1200).
  'gu'          : '044704b0',
  # No codepage for Kannada, use unicode(1200).
  'kn'          : '044b04b0',
  # Malay (Malaysia) [ms-MY]
  'ms'          : '043e04e4',
  # No codepage for Malayalam, use unicode(1200).
  'ml'          : '044c04b0',
  # No codepage for Marathi, use unicode(1200).
  'mr'          : '044e04b0',
  # No codepage for Oriya , use unicode(1200).
  'or'          : '044804b0',
  # No codepage for Tamil, use unicode(1200).
  'ta'          : '044904b0',
  # No codepage for Telugu, use unicode(1200).
  'te'          : '044a04b0',
  # No codepage for Amharic, use unicode(1200). >= Vista.
  'am'          : '045e04b0',
  'sw'          : '044104e4',
  'af'          : '043604e4',
  'eu'          : '042d04e4',
  'fr-CA'       : '0c0c04e4',
  'gl'          : '045604e4',
  # No codepage for Zulu, use unicode(1200).
  'zu'          : '043504b0',

  # Pseudolocales
  'ar-XB'       : '040d04e7',
  'en-XA'       : '040904b0',
}

# Language ID resource: http://msdn.microsoft.com/en-us/library/ms776294.aspx
#
# There is no appropriate sublang for Spanish (Latin America) [es-419], so we
# use Mexico. SUBLANG_DEFAULT would incorrectly map to Spain. Unlike other
# Latin American countries, Mexican Spanish is supported by VERSIONINFO:
# http://msdn.microsoft.com/en-us/library/aa381058.aspx

_LANGUAGE_DIRECTIVE_PAIR = {
  'neutral'     : 'LANG_NEUTRAL, SUBLANG_NEUTRAL',
  'userdefault' : 'LANG_NEUTRAL, SUBLANG_DEFAULT',
  'ar'          : 'LANG_ARABIC, SUBLANG_DEFAULT',
  'fi'          : 'LANG_FINNISH, SUBLANG_DEFAULT',
  'ko'          : 'LANG_KOREAN, SUBLANG_KOREAN',
  'es'          : 'LANG_SPANISH, SUBLANG_SPANISH_MODERN',
  'bg'          : 'LANG_BULGARIAN, SUBLANG_DEFAULT',
  # LANG_FILIPINO (100) not in VC 7 winnt.h.
  'fil'         : '100, SUBLANG_DEFAULT',
  'fr'          : 'LANG_FRENCH, SUBLANG_FRENCH',
  'lv'          : 'LANG_LATVIAN, SUBLANG_DEFAULT',
  'sv'          : 'LANG_SWEDISH, SUBLANG_SWEDISH',
  'ca'          : 'LANG_CATALAN, SUBLANG_DEFAULT',
  'de'          : 'LANG_GERMAN, SUBLANG_GERMAN',
  'lt'          : 'LANG_LITHUANIAN, SUBLANG_LITHUANIAN',
  # Do not use! See above.
  'tl'          : 'LANG_NEUTRAL, SUBLANG_DEFAULT',
  'zh-CN'       : 'LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED',
  'zh-TW'       : 'LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL',
  'zh-HK'       : 'LANG_CHINESE, SUBLANG_CHINESE_HONGKONG',
  'el'          : 'LANG_GREEK, SUBLANG_DEFAULT',
  'no'          : 'LANG_NORWEGIAN, SUBLANG_DEFAULT',
  'nb'          : 'LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL',
  'nn'          : 'LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK',
  'th'          : 'LANG_THAI, SUBLANG_DEFAULT',
  'he'          : 'LANG_HEBREW, SUBLANG_DEFAULT',
  'iw'          : 'LANG_HEBREW, SUBLANG_DEFAULT',
  'pl'          : 'LANG_POLISH, SUBLANG_DEFAULT',
  'tr'          : 'LANG_TURKISH, SUBLANG_DEFAULT',
  'hr'          : 'LANG_CROATIAN, SUBLANG_DEFAULT',
  'hi'          : 'LANG_HINDI, SUBLANG_DEFAULT',
  'pt-PT'       : 'LANG_PORTUGUESE, SUBLANG_PORTUGUESE',
  'pt-BR'       : 'LANG_PORTUGUESE, SUBLANG_DEFAULT',
  'uk'          : 'LANG_UKRAINIAN, SUBLANG_DEFAULT',
  'cs'          : 'LANG_CZECH, SUBLANG_DEFAULT',
  'hu'          : 'LANG_HUNGARIAN, SUBLANG_DEFAULT',
  'ro'          : 'LANG_ROMANIAN, SUBLANG_DEFAULT',
  'ur'          : 'LANG_URDU, SUBLANG_DEFAULT',
  'da'          : 'LANG_DANISH, SUBLANG_DEFAULT',
  'is'          : 'LANG_ICELANDIC, SUBLANG_DEFAULT',
  'ru'          : 'LANG_RUSSIAN, SUBLANG_DEFAULT',
  'vi'          : 'LANG_VIETNAMESE, SUBLANG_DEFAULT',
  'nl'          : 'LANG_DUTCH, SUBLANG_DEFAULT',
  'id'          : 'LANG_INDONESIAN, SUBLANG_DEFAULT',
  'sr'          : 'LANG_SERBIAN, SUBLANG_SERBIAN_LATIN',
  'en-GB'       : 'LANG_ENGLISH, SUBLANG_ENGLISH_UK',
  'it'          : 'LANG_ITALIAN, SUBLANG_DEFAULT',
  'sk'          : 'LANG_SLOVAK, SUBLANG_DEFAULT',
  'et'          : 'LANG_ESTONIAN, SUBLANG_DEFAULT',
  'ja'          : 'LANG_JAPANESE, SUBLANG_DEFAULT',
  'sl'          : 'LANG_SLOVENIAN, SUBLANG_DEFAULT',
  'en'          : 'LANG_ENGLISH, SUBLANG_ENGLISH_US',
  # No L.A. sublang exists.
  'es-419'      : 'LANG_SPANISH, SUBLANG_SPANISH_MEXICAN',
  'bn'          : 'LANG_BENGALI, SUBLANG_DEFAULT',
  'fa'          : 'LANG_PERSIAN, SUBLANG_DEFAULT',
  'gu'          : 'LANG_GUJARATI, SUBLANG_DEFAULT',
  'kn'          : 'LANG_KANNADA, SUBLANG_DEFAULT',
  'ms'          : 'LANG_MALAY, SUBLANG_DEFAULT',
  'ml'          : 'LANG_MALAYALAM, SUBLANG_DEFAULT',
  'mr'          : 'LANG_MARATHI, SUBLANG_DEFAULT',
  'or'          : 'LANG_ORIYA, SUBLANG_DEFAULT',
  'ta'          : 'LANG_TAMIL, SUBLANG_DEFAULT',
  'te'          : 'LANG_TELUGU, SUBLANG_DEFAULT',
  'am'          : 'LANG_AMHARIC, SUBLANG_DEFAULT',
  'sw'          : 'LANG_SWAHILI, SUBLANG_DEFAULT',
  'af'          : 'LANG_AFRIKAANS, SUBLANG_DEFAULT',
  'eu'          : 'LANG_BASQUE, SUBLANG_DEFAULT',
  'fr-CA'       : 'LANG_FRENCH, SUBLANG_FRENCH_CANADIAN',
  'gl'          : 'LANG_GALICIAN, SUBLANG_DEFAULT',
  'zu'          : 'LANG_ZULU, SUBLANG_DEFAULT',
  'pa'          : 'LANG_PUNJABI, SUBLANG_PUNJABI_INDIA',
  'sa'          : 'LANG_SANSKRIT, SUBLANG_SANSKRIT_INDIA',
  'si'          : 'LANG_SINHALESE, SUBLANG_SINHALESE_SRI_LANKA',
  'ne'          : 'LANG_NEPALI, SUBLANG_NEPALI_NEPAL',
  'ti'          : 'LANG_TIGRIGNA, SUBLANG_TIGRIGNA_ERITREA',

  # Pseudolocales
  'ar-XB'       : 'LANG_HEBREW, SUBLANG_DEFAULT',
  'en-XA'       : 'LANG_ENGLISH, SUBLANG_ENGLISH_US',
}

# A note on 'no-specific-language' in the following few functions:
# Some build systems may wish to call GRIT to scan for dependencies in
# a language-agnostic way, and can then specify this fake language as
# the output context.  It should never be used when output is actually
# being generated.

def GetLangCharsetPair(language):
  if language in _LANGUAGE_CHARSET_PAIR:
    return _LANGUAGE_CHARSET_PAIR[language]
  if language != 'no-specific-language':
    print('Warning:GetLangCharsetPair() found undefined language %s' % language)
  return ''

def GetLangDirectivePair(language):
  if language in _LANGUAGE_DIRECTIVE_PAIR:
    return _LANGUAGE_DIRECTIVE_PAIR[language]

  # We don't check for 'no-specific-language' here because this
  # function should only get called when output is being formatted,
  # and at that point we would not want to get
  # 'no-specific-language' passed as the language.
  print('Warning:GetLangDirectivePair() found undefined language %s' % language)
  return 'unknown language: see tools/grit/format/rc.py'

def GetLangIdHex(language):
  if language in _LANGUAGE_CHARSET_PAIR:
    langcharset = _LANGUAGE_CHARSET_PAIR[language]
    lang_id = '0x' + langcharset[0:4]
    return lang_id
  if language != 'no-specific-language':
    print('Warning:GetLangIdHex() found undefined language %s' % language)
  return ''


def GetCharsetIdDecimal(language):
  if language in _LANGUAGE_CHARSET_PAIR:
    langcharset = _LANGUAGE_CHARSET_PAIR[language]
    charset_decimal = int(langcharset[4:], 16)
    return str(charset_decimal)
  if language != 'no-specific-language':
    print('Warning:GetCharsetIdDecimal() found undefined language %s' % language)
  return ''


def GetUnifiedLangCode(language) :
  r = re.compile('([a-z]{1,2})_([a-z]{1,2})')
  if r.match(language) :
    underscore = language.find('_')
    return language[0:underscore] + '-' + language[underscore + 1:].upper()
  return language


def RcSubstitutions(substituter, lang):
  '''Add language-based substitutions for Rc files to the substitutor.'''
  unified_lang_code = GetUnifiedLangCode(lang)
  substituter.AddSubstitutions({
      'GRITVERLANGCHARSETHEX': GetLangCharsetPair(unified_lang_code),
      'GRITVERLANGID': GetLangIdHex(unified_lang_code),
      'GRITVERCHARSETID': GetCharsetIdDecimal(unified_lang_code)})


def _FormatHeader(root, lang, output_dir):
  '''Returns the required preamble for RC files.'''
  assert isinstance(lang, str)
  assert isinstance(root, misc.GritNode)
  # Find the location of the resource header file, so that we can include
  # it.
  resource_header = 'resource.h'  # fall back to this
  language_directive = ''
  for output in root.GetOutputFiles():
    if output.attrs['type'] == 'rc_header':
      resource_header = os.path.abspath(output.GetOutputFilename())
      resource_header = util.MakeRelativePath(output_dir, resource_header)
    if output.attrs['lang'] != lang:
      continue
    if output.attrs['language_section'] == '':
      # If no language_section is requested, no directive is added
      # (Used when the generated rc will be included from another rc
      # file that will have the appropriate language directive)
      language_directive = ''
    elif output.attrs['language_section'] == 'neutral':
      # If a neutral language section is requested (default), add a
      # neutral language directive
      language_directive = 'LANGUAGE LANG_NEUTRAL, SUBLANG_NEUTRAL'
    elif output.attrs['language_section'] == 'lang':
      language_directive = 'LANGUAGE %s' % GetLangDirectivePair(lang)
  resource_header = resource_header.replace('\\', '\\\\')
  return '''// This file is automatically generated by GRIT.  Do not edit.

#include "%s"
#include <winresrc.h>
#ifdef IDC_STATIC
#undef IDC_STATIC
#endif
#define IDC_STATIC (-1)

%s


''' % (resource_header, language_directive)
# end _FormatHeader() function


def FormatMessage(item, lang):
  '''Returns a single message of a string table.'''
  message = item.ws_at_start + item.Translate(lang) + item.ws_at_end
  # Escape quotation marks (RC format uses doubling-up
  message = message.replace('"', '""')
  # Replace linebreaks with a \n escape
  message = util.LINEBREAKS.sub(r'\\n', message)
  if hasattr(item.GetRoot(), 'GetSubstituter'):
    substituter = item.GetRoot().GetSubstituter()
    message = substituter.Substitute(message)

  name_attr = item.GetTextualIds()[0]

  return '  %-15s "%s"\n' % (name_attr, message)


def _FormatSection(item, lang, output_dir):
  '''Writes out an .rc file section.'''
  assert isinstance(lang, str)
  from grit.node import structure
  assert isinstance(item, structure.StructureNode)

  if item.IsExcludedFromRc():
    return ''

  text = item.gatherer.Translate(
      lang, skeleton_gatherer=item.GetSkeletonGatherer(),
      pseudo_if_not_available=item.PseudoIsAllowed(),
      fallback_to_english=item.ShouldFallbackToEnglish()) + '\n\n'

  # Replace the language expand_variables in version rc info.
  if item.ExpandVariables() and hasattr(item.GetRoot(), 'GetSubstituter'):
    substituter = item.GetRoot().GetSubstituter()
    text = substituter.Substitute(text)
  return text


def FormatInclude(item, lang, output_dir, type=None, process_html=False):
  '''Formats an item that is included in an .rc file (e.g. an ICON).

  Args:
    item: an IncludeNode or StructureNode
    lang, output_dir: standard formatter parameters
    type: .rc file resource type, e.g. 'ICON' (ignored unless item is a
          StructureNode)
    process_html: False/True (ignored unless item is a StructureNode)
  '''
  assert isinstance(lang, str)
  from grit.node import structure
  from grit.node import include
  assert isinstance(item, (structure.StructureNode, include.IncludeNode))

  if isinstance(item, include.IncludeNode):
    type = item.attrs['type'].upper()
    process_html = item.attrs['flattenhtml'] == 'true'
    filename_only = item.attrs['filenameonly'] == 'true'
    relative_path = item.attrs['relativepath'] == 'true'
  else:
    assert (isinstance(item, structure.StructureNode) and item.attrs['type'] in
        ['admin_template', 'chrome_html', 'chrome_scaled_image',
         'tr_html', 'txt'])
    filename_only = False
    relative_path = False

  # By default, we use relative pathnames to included resources so that
  # sharing the resulting .rc files is possible.
  #
  # The FileForLanguage() Function has the side effect of generating the file
  # if needed (e.g. if it is an HTML file include).
  file_for_lang = item.FileForLanguage(lang, output_dir)
  if file_for_lang is None:
    return ''

  filename = os.path.abspath(file_for_lang)
  if process_html:
    filename = item.Process(output_dir)
  elif filename_only:
    filename = os.path.basename(filename)
  elif relative_path:
    filename = util.MakeRelativePath(output_dir, filename)

  filename = filename.replace('\\', '\\\\')  # escape for the RC format

  if isinstance(item, structure.StructureNode) and item.IsExcludedFromRc():
    return ''

  name = item.attrs['name']
  item_id = item.GetRoot().GetIdMap()[name]
  return '// ID: %d\n%-18s %-18s "%s"\n' % (item_id, name, type, filename)


def _DoNotFormat(item, lang, output_dir):
  return ''


# Formatter instance to use for each type attribute
# when formatting Structure nodes.
_STRUCTURE_FORMATTERS = {
  'accelerators'        : _FormatSection,
  'dialog'              : _FormatSection,
  'menu'                : _FormatSection,
  'rcdata'              : _FormatSection,
  'version'             : _FormatSection,
  'admin_template'      : partial(FormatInclude, type='ADM'),
  'chrome_html'         : partial(FormatInclude, type='BINDATA',
                                                 process_html=True),
  'chrome_scaled_image' : partial(FormatInclude, type='BINDATA'),
  'tr_html'             : partial(FormatInclude, type='HTML'),
  'txt'                 : partial(FormatInclude, type='TXT'),
  'policy_template_metafile': _DoNotFormat,
}


def FormatStructure(item, lang, output_dir):
  formatter = _STRUCTURE_FORMATTERS[item.attrs['type']]
  return formatter(item, lang, output_dir)
