| <html>
|   <head>
|     <script>
|       src="../../resources/dump-as-markup.js"
|     "
"
|   <body>
|     <div>
|       "This test passes if it doesn't crash."
|     "
"
|     <ul>
|       contenteditable=""
|       "
    "
|       <li>
|         id="foo"
|       <li>
|         id="foo"
|         <#selection-caret>
|         <br>
|       "
"
|     "
"
|     <script>
|       "
window.getSelection().selectAllChildren(foo);
document.execCommand('insertParagraph');
"
|     "
"
