call build

@echo off

:top
echo Trimming heaps at %time%
trim_heap %*
sleep 300
goto top
