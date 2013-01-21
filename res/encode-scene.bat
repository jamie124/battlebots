@echo off

set encoder="%GAMEPLAY%\bin\windows\gameplay-encoder.exe"

%encoder% game.fbx ..\res\game.gpb

%encoder% -n -w 10000,4000,10000 -s 256,256 heightmap.raw

REM "%GAMEPLAY\bin\windows\gameplay-encoder.exe" -h heightmap.png game.fbx game.gpb

