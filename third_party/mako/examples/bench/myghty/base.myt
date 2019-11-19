<!DOCTYPE html
    PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en">
<%args scope="request">
    title
</%args>

<& REQUEST:header &>

<body>
<div id="header">
  <h1><% title %></h1>
</div>

% m.call_next()

<div id="footer"></div>

</body>
</html>


<%method greeting>
<%args>
   name
</%args>
Hello, <% name | h %>
</%method>
