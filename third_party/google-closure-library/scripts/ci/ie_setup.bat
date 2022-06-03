:: Copyright 2018 The Closure Library Authors. All Rights Reserved.
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::      http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS-IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.

:: Disable "Stop running this script?" alert dialog.
reg add "HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\Styles" ^
    /v "MaxScriptStatements" /t REG_DWORD /d 0xffffffff /f

:: Disable Java update dialogs.
reg add ^
    "HKEY_LOCAL_MACHINE\Software\JavaSoft\Java Update\Policy" ^
reg add ^
    "HKEY_LOCAL_MACHINE\Software\JavaSoft\Java Update\Policy" ^
    /v "EnableJavaUpdate" /t REG_DWORD /d 0 /f
reg add ^
    "HKEY_CURRENT_USER\Software\AppDataLow\Software\JavaSoft\DeploymentProperties" ^
    /v "deployment.expiration.check.enabled" /t REG_SZ /d false /f
reg add ^
    "HKEY_CURRENT_USER\Software\AppDataLow\Software\JavaSoft\DeploymentProperties" ^
    /v "deployment.expiration.decision.10.55.2" /t REG_SZ /d block /f
reg add ^
    "HKEY_CURRENT_USER\Software\AppDataLow\Software\JavaSoft\DeploymentProperties" ^
    /v "deployment.expiration.decision.suppression.10.55.2" /t REG_SZ /d true /f
reg add ^
    "HKEY_CURRENT_USER\Software\AppDataLow\Software\JavaSoft\DeploymentProperties" ^
    /v "deployment.expiration.decision.timestamp.10.55.2" /t REG_SZ /d 1413180896 /f
reg add ^
    "HKEY_CURRENT_USER\Software\Microsoft\Active Setup\Declined Install On Demand IEv5" ^
    /v "{08B0e5c0-4FCB-11CF-AAA5-00401C608501}" /t REG_SZ /d 1 /f   /v "EnableJavaUpdate" /t REG_DWORD /d 0 /f

:: Disable Windows crash dialogs.
reg add ^
    "HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\Windows Error Reporting" ^
    /v "DontShowUI" /t REG_DWORD /d 1 /f
reg add ^
    "HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\Windows Error Reporting" ^
    /v "ForceQueue" /t REG_DWORD /d 1 /f

:: Disable JavaScript error dialogs.
reg add "HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\Main" ^
    /v "Error Dlg Displayed On Every Error" /t REG_SZ /d no /f

:: Disable script error dialogs for IE8 & IE9.
reg add "HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\Main" ^
    /v "Disable Script Debugger" /t REG_SZ /d yes /f
reg add "HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer\Main" ^
    /v "DisableScriptDebuggerIE" /t REG_SZ /d yes /f

:: Disable cross-origin XHRs. Sauce enables this in IE by default.
@ECHO OFF
REG ADD "HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings\Zones\3" /v 1406 /t REG_DWORD /d 3 /f

setlocal

set "URL=https://raw.githubusercontent.com/google/closure-library/master/scripts/ci/CloseAdobeDialog.exe"
set "SaveAs=C:\Users\Administrator\Desktop\CloseAdobeDialog.exe"
powershell "Import-Module BitsTransfer; Start-BitsTransfer '%URL%' '%SaveAs%'"

start C:\Users\Administrator\Desktop\CloseAdobeDialog.exe
exit
