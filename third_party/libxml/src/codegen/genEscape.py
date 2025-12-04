#!/usr/bin/env python3

entities = [
    [ '',   '&#xFFFD;' ],
    [ '\t', '&#9;' ],
    [ '\n', '&#10;' ],
    [ '\r', '&#13;' ],
    [ '"',  '&quot;' ],
    [ '&',  '&amp;' ],
    [ '<',  '&lt;' ],
    [ '>',  '&gt;' ],
]

offset = [ None ] * 128

def gen_content(out):
    pos = 0
    r = ''

    for rec in entities:
        char, repl = rec

        if char:
            offset[ord(char)] = pos

        if pos % 12 == 0: r += '\n    '
        else: r += ' '
        r += '%3d,' % len(repl)
        pos += 1

        for c in repl:
            if pos % 12 == 0: r += '\n    '
            else: r += ' '
            r += "'%s'," % c
            pos += 1

    out.write('static const char xmlEscapeContent[] = {%s\n};\n\n' % r)

def gen_tab(out, name, escape, is_xml):
    r = ''

    for i in range(0x80):

        if chr(i) in escape:
            v = offset[i]
        elif i == 0:
            v = 0
        elif is_xml and i < 32 and i != 9 and i != 10:
            v = 0
        else:
            v = -1

        if i % 16 == 0: r += '\n    '
        else: r += ' '
        r += '%2d,' % v

    out.write('static const signed char %s[128] = {%s\n};\n\n' % (name, r))

with open('codegen/escape.inc', 'w') as out:
    gen_content(out)

    gen_tab(out, 'xmlEscapeTab', '\r&<>', True)
    gen_tab(out, 'xmlEscapeTabQuot', '\r"&<>', True)
    gen_tab(out, 'xmlEscapeTabAttr', '\t\n\r"&<>', True)

    out.write('#ifdef LIBXML_HTML_ENABLED\n\n')
    gen_tab(out, 'htmlEscapeTab', '&<>', False)
    gen_tab(out, 'htmlEscapeTabAttr', '"&<>', False)
    out.write('#endif /* LIBXML_HTML_ENABLED */\n')
