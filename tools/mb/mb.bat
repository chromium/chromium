@echo off
setlocal
:: This is required with cygwin only.
PATH=%~dp0;%PATH%
set PYTHONDONTWRITEBYTECODE=1
call python3 "%~dp0mb.py" %*
