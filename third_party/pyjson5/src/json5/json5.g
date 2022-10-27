grammar        = sp value:v sp end                    -> v

sp             = ws*

ws             = '\u0020' | eol | comment
               | '\u0009' | '\u000B' | '\u000C' | '\u00A0' | '\uFEFF'
               | ~~(anything:x ?( is_unicat(x, 'Zs') )) anything:x -> x

eol            = '\u000D' '\u000A' | '\u000D' | '\u000A'
               | '\u2028' | '\u2029'

comment        = '//' (~eol anything)*
               | '/*' (~'*/' anything)* '*/'

value          = 'null'                               -> 'None'
               | 'true'                               -> 'True'
               | 'false'                              -> 'False'
               | object:v                             -> ['object', v]
               | array:v                              -> ['array', v]
               | string:v                             -> ['string', v]
               | num_literal:v                        -> ['number', v]

object         = '{' sp member_list:v sp '}'          -> v
               | '{' sp '}'                           -> []

array          = '[' sp element_list:v sp ']'         -> v
               | '[' sp ']'                           -> []

string         = squote sqchar*:cs squote             -> join('', cs)
               | dquote dqchar*:cs dquote             -> join('', cs)

sqchar         = bslash esc_char:c                    -> c
               | bslash eol                           -> ''
               | ~bslash ~squote ~eol anything:c      -> c

dqchar         = bslash esc_char:c                    -> c
               | bslash eol                           -> ''
               | ~bslash ~dquote ~eol anything:c      -> c

bslash         = '\u005C'

squote         = '\u0027'

dquote         = '\u0022'

esc_char       = 'b'                                 -> '\u0008'
               | 'f'                                 -> '\u000C'
               | 'n'                                 -> '\u000A'
               | 'r'                                 -> '\u000D'
               | 't'                                 -> '\u0009'
               | 'v'                                 -> '\u000B'
               | squote                              -> '\u0027'
               | dquote                              -> '\u0022'
               | bslash                              -> '\u005C'
               | ~('x'|'u'|digit|eol) anything:c     -> c
               | '0' ~digit                          -> '\u0000'
               | hex_esc:c                           -> c
               | unicode_esc:c                       -> c

hex_esc        = 'x' hex:h1 hex:h2                   -> xtou(h1 + h2)

unicode_esc    = 'u' hex:a hex:b hex:c hex:d         -> xtou(a + b + c + d)

element_list   = value:v (sp ',' sp value)*:vs sp ','?   -> [v] + vs

member_list    = member:m (sp ',' sp member)*:ms sp ','? -> [m] + ms

member         = string:k sp ':' sp value:v          -> [k, v]
               | ident:k sp ':' sp value:v           -> [k, v]

ident          = id_start:hd id_continue*:tl         -> join('', [hd] + tl)

id_start       = ascii_id_start
               | other_id_start
               | bslash unicode_esc

ascii_id_start = 'a'..'z'
               | 'A'..'Z'
               | '$'
               | '_'

other_id_start = anything:x ?(is_unicat(x, 'Ll'))    -> x
               | anything:x ?(is_unicat(x, 'Lm'))    -> x
               | anything:x ?(is_unicat(x, 'Lo'))    -> x
               | anything:x ?(is_unicat(x, 'Lt'))    -> x
               | anything:x ?(is_unicat(x, 'Lu'))    -> x
               | anything:x ?(is_unicat(x, 'Nl'))    -> x

id_continue    = ascii_id_start
               | digit
               | other_id_start
               | anything:x ?(is_unicat(x, 'Mn'))    -> x
               | anything:x ?(is_unicat(x, 'Mc'))    -> x
               | anything:x ?(is_unicat(x, 'Nd'))    -> x
               | anything:x ?(is_unicat(x, 'Pc'))    -> x
               | bslash unicode_esc
               | '\u200C'
               | '\u200D'

num_literal    = '-' num_literal:n                   -> '-' + n
               | '+'? dec_literal:d ~id_start        -> d
               | hex_literal
               | 'Infinity'
               | 'NaN'

dec_literal    = dec_int_lit:d frac:f exp:e          -> d + f + e
               | dec_int_lit:d frac:f                -> d + f
               | dec_int_lit:d exp:e                 -> d + e
               | dec_int_lit:d                       -> d
               | frac:f exp:e                        -> f + e
               | frac:f                              -> f

dec_int_lit    = '0' ~digit                          -> '0'
               | nonzerodigit:d digit*:ds            -> d + join('', ds)

digit          = '0'..'9'

nonzerodigit   = '1'..'9'

hex_literal    = ('0x' | '0X') hex+:hs               -> '0x' + join('', hs)

hex            = 'a'..'f' | 'A'..'F' | digit

frac           = '.' digit*:ds                       -> '.' + join('', ds)

exp            = ('e' | 'E') ('+' | '-'):s digit*:ds -> 'e' + s + join('', ds)
               | ('e' | 'E') digit*:ds               -> 'e' + join('', ds)
