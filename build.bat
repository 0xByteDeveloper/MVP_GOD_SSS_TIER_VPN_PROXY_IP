@echo off
rem MVP_GOD_SSS_TIER — Windows derleme (MSVC 2019+ x64 Native Tools)
rem SIFIR DIS BAGIMLILIK: WinHTTP + SChannel + WinSock — HTML arayuz exe'ye gomulur
rem (src\web_embedded.h repoda hazir gelir; web/ degisirse scripts\embed_web.py
rem  calistirilip yeniden uretilir — python gerekir).
where cl >nul 2>&1
if errorlevel 1 (
  echo [HATA] cl bulunamadi — "x64 Native Tools Command Prompt" icinde calistirin
  echo        ya da hazir bin\MVP_GOD_SSS_TIER.exe'yi kullanin
  exit /b 1
)
if not exist bin mkdir bin

echo [1/2] MVP_GOD_SSS_TIER.exe (motor + gomulu WebGUI)...
cl /nologo /std:c++17 /O2 /EHsc /W3 ^
  src\main.cpp src\core\util.cpp src\core\log.cpp ^
  src\net\net.cpp src\net\httpclient.cpp src\net\tls.cpp ^
  src\providers\hola.cpp src\providers\more.cpp ^
  src\engine\router.cpp src\engine\instance.cpp ^
  src\engine\proclist.cpp src\engine\monitor.cpp ^
  src\httpserver.cpp src\api.cpp ^
  /I src /Fe:bin\MVP_GOD_SSS_TIER.exe ^
  ws2_32.lib winhttp.lib secur32.lib crypt32.lib iphlpapi.lib advapi32.lib
if errorlevel 1 goto :err

echo [2/2] godtier_hook.dll (surec kanca DLL)...
cl /nologo /std:c++17 /O2 /EHsc /W3 /LD ^
  hook\hook_windows.cpp ^
  /I src /Fe:bin\godtier_hook.dll ws2_32.lib user32.lib
if errorlevel 1 goto :err

echo.
echo OK  → bin\MVP_GOD_SSS_TIER.exe + bin\godtier_hook.dll
echo Calistir (tek exe — web klasoru gerekmez, HTML icinde):
echo   cd /d %cd% && bin\MVP_GOD_SSS_TIER.exe serve 7787
echo   WebGUI: http://127.0.0.1:7787/
goto :eof

:err
echo [HATA] derleme basarisiz
exit /b 1
