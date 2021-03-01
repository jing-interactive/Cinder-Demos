cd %~dp0\vc2019
set proj
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" %proj% /p:Configuration=Release /p:Platform=x64 /v:minimal /m
"D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" %proj% /p:Configuration=Release /p:Platform=x64 /v:minimal /m
REM msbuild %proj% /p:Configuration=Debug_Shared /p:Platform=x64 /v:minimal /m
REM msbuild %proj% /p:Configuration=Release_Shared /p:Platform=x64 /v:minimal /m
cd ..