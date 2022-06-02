<%page args="x, y=_('Page arg 1'), z=_('Page arg 2')"/>
<%!
import random
def gettext(message): return message
_ = gettext
def ungettext(s, p, c):
    if c == 1:
        return s
    return p
top = gettext('Begin')
%>
<%
   # TRANSLATOR: Hi there!
   hithere = _('Hi there!')

   # TRANSLATOR: you should not be seeing this in the .po
   rows = [[v for v in range(0,10)] for row in range(0,10)]

   hello = _('Hello')
%>
<div id="header">
  ${_('Welcome')}
</div>
<table>
    % for row in (hithere, hello, _('Yo')):
        ${makerow(row)}
    % endfor
    ${makerow(count=2)}
</table>


<div id="main">

## TRANSLATOR: Ensure so and
## so, thanks
  ${_('The')} fuzzy ${ungettext('bunny', 'bunnies', random.randint(1, 2))}
</div>

<div id="footer">
  ## TRANSLATOR: Good bye
  ${_('Goodbye')}
</div>

<%def name="makerow(row=_('Babel'), count=1)">
    <!-- ${ungettext('hella', 'hellas', count)} -->
    % for i in range(count):
      <tr>
      % for name in row:
          <td>${name}</td>\
      % endfor
      </tr>
    % endfor
</%def>

<%def name="comment()">
  <!-- ${caller.body()} -->
</%def>

<%block name="foo">
    ## TRANSLATOR: Ensure so and
    ## so, thanks
      ${_('The')} fuzzy ${ungettext('bunny', 'bunnies', random.randint(1, 2))}
</%block>

<%call expr="comment">
  P.S.
  ## TRANSLATOR: HTML comment
  ${_('Goodbye, really!')}
</%call>

<!-- ${_('P.S. byebye')} -->

<div id="end">
  <a href="#top">
    ## TRANSLATOR: you won't see this either

    ${_('Top')}
  </a>
</div>

<%def name="panel()">

${_(u'foo')} <%self:block_tpl title="#123", name="_('baz')" value="${_('hoho')}" something="hi'there" somethingelse='hi"there'>

${_(u'bar')}

</%self:block_tpl>

</%def>

## TRANSLATOR: <p> tag is ok?
<p>${_("Inside a p tag")}</p>

## TRANSLATOR: also this
<p>${even_with_other_code_first()} - ${_("Later in a p tag")}</p>

## TRANSLATOR: we still ignore comments too far from the string

<p>${_("No action at a distance.")}</p>

## TRANSLATOR: nothing to extract from these blocks

% if 1==1:
<p>One is one!</p>
% elif 1==2:
<p>One is two!</p>
% else:
<p>How much is one?</p>
% endif

% for i in range(10):
<p>${i} squared is ${i*i}</p>
% else:
<p>Done with squares!</p>
% endfor

% while random.randint(1,6) != 6:
<p>Not 6!</p>
% endwhile

## TRANSLATOR: for now, try/except blocks are ignored

% try:
<% 1/0 %>
% except:
<p>Failed!</p>
% endtry

## TRANSLATOR: this should not cause a parse error
${ 1 }
