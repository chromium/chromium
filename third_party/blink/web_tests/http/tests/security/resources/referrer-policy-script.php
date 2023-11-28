var referrerHeader = "<?php echo ($_SERVER['HTTP_REFERER'] ?? null) ?>";
if (referrerHeader === "")
    scriptReferrer = "none";
else
    scriptReferrer = referrerHeader;