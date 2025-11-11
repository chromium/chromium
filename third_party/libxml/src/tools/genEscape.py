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

### xmlEscapeContent

offset = [ None ] * 128
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

print('static const signed char xmlEscapeContent[] = {%s\n};\n' % r)

### xmlEscapeTab

escape = '\r&<>'
r = ''

for i in range(0x80):

    if chr(i) in escape:
        v = offset[i]
    elif i != 9 and i != 10 and i < 32:
        v = 0
    else:
        v = -1

    if i % 16 == 0: r += '\n    '
    else: r += ' '
    r += '%2d,' % v

print('static const signed char xmlEscapeTab[128] = {%s\n};\n' % r)

### xmlEscapeTabAttr

escape = '\t\n\r"&<>'
r = ''

for i in range(0x80):

    if chr(i) in escape:
        v = offset[i]
    elif i != 9 and i != 10 and i < 32:
        v = 0
    else:
        v = -1

    if i % 16 == 0: r += '\n    '
    else: r += ' '
    r += '%2d,' % v

print('static const char xmlEscapeTabAttr[128] = {%s\n};\n' % r)

