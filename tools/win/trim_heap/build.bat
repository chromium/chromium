@setlocal

where cl
if errorlevel 1 goto no_cl

pushd %~dp0
cl /nologo /Zi /GS- /EHsc trim_heap.cc /link /DEBUG /OPT:REF /OPT:ICF
popd

exit /b

:no_cl
@echo Run "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" amd64
@echo or equivalent to get the 64-bit compiler tools in the path.
exit /b
