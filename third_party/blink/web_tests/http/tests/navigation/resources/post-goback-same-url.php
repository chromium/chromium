<html>
<head>
<script>

function submitForm()
{
    document.getElementById('submit').click();
}

</script>
</head>
<body>

<?
IF ($_POST['textdata'] == "foo") {
    ECHO "You should not be seeing this text!";
}
?>

<form method="POST">
    <input type="text" name="textdata" value="foo">
    <input type="submit" id="submit">
</form>

</body>
</html>
