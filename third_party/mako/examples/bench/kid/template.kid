<!DOCTYPE html
    PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:py="http://purl.org/kid/ns#"
      py:extends="'base.kid'"
      lang="en">
  <head>
    <title>${title}</title>
  </head>
  <body>
    <div>${greeting(user)}</div>
    <div>${greeting('me')}</div>
    <div>${greeting('world')}</div>
 
    <h2>Loop</h2>
    <ul py:if="items">
      <li py:for="idx, item in enumerate(items)" py:content="item"
          class="${idx + 1 == len(items) and 'last' or None}" />
    </ul>
  </body>
</html>
