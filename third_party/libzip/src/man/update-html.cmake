# expect variables IN, OUT, and DIR
EXECUTE_PROCESS(COMMAND mandoc -T html -Oman=%N.html,style=../nih-man.css ${DIR}/${IN}
  OUTPUT_VARIABLE HTML)
SET(LINKBASE "http://pubs.opengroup.org/onlinepubs/9699919799/functions/")
STRING(REGEX REPLACE "(<a class=\"Xr\" href=\")([^\"]*)(\">)" "\\1${LINKBASE}\\2\\3" HTML "${HTML}")
STRING(REGEX REPLACE "${LINKBASE}(libzip|zip)" "\\1" HTML "${HTML}")
STRING(REGEX REPLACE "NetBSD [0-9.]*" "NiH" HTML "${HTML}")
FILE(WRITE ${DIR}/${OUT}.new "${HTML}")
CONFIGURE_FILE(${DIR}/${OUT}.new ${DIR}/${OUT} COPYONLY)
FILE(REMOVE ${DIR}/${OUT}.new)


