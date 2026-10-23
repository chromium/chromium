<?php
$cors_arg = strtolower($_GET['cors'] ?? null);
if ($cors_arg != 'false') {
  if ($cors_arg == '' || $cors_arg == 'true') {
    header('Access-Control-Allow-Origin: http://127.0.0.1:8000');
  } else {
    header('Access-Control-Allow-Origin: ' . $cors_arg);
  }
}

$max_age = $_GET['max-age'] ?? null;
if (!(empty($max_age))) {
  header('Cache-Control: max-age=' . $max_age);
}

if (strtolower($_GET['credentials'] ?? '') == 'true') {
  header('Access-Control-Allow-Credentials: true');
}

$custom_header_arg = strtolower($_GET['custom'] ?? '');
if (!(empty($custom_header_arg))) {
  header('Access-Control-Allow-Headers: ' . $custom_header_arg);
}

header('Content-Type: application/javascript');

$delay = $_GET['delay'] ?? null;
if ($delay)
  usleep(1000 * $delay);

if (isset($_SERVER['HTTP_ORIGIN']) && isset($_GET['value_cors'])) {
  $value = $_GET['value_cors'];
} else {
  $value = $_GET['value'] ?? null;
}

$cookie = $_GET['cookie'] ?? null;

if ($value || $cookie) {
  if ($cookie) {
    $value = $_COOKIE[$cookie];
  }

  $result_var = 'result';
  if (isset($_GET['resultvar']) && !empty($_GET['resultvar'])) {
    $result_var = $_GET['resultvar'];
  }

  echo $result_var . " = \"" . $value . "\";";
} else if (strtolower($_GET['fail'] ?? null) == 'true') {
  echo "throw({toString: function(){ return 'SomeError' }});";
} else {
  echo "alert('script ran.');";
}
?>
