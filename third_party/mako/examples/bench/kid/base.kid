<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:py="http://purl.org/kid/ns#">

  <p py:def="greeting(name)">
    Hello, ${name}!
  </p>

  <body py:match="item.tag == '{http://www.w3.org/1999/xhtml}body'" py:strip="">
    <div id="header">
      <h1>${title}</h1>
    </div>
    ${item}
    <div id="footer" />
  </body>
</html>
