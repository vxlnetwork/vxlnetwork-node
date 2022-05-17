@echo off
set exit_code=0

echo "BUILD TYPE %BUILD_TYPE%"
echo "RUN %RUN%"

cmake .. ^
  -Ax64 ^
  %VXLNETWORK_TEST% ^
  %CI% ^
  %ROCKS_LIB% ^
  -DPORTABLE=1 ^
  -DQt5_DIR="c:\qt\5.13.1\msvc2017_64\lib\cmake\Qt5" ^
  -DVXLNETWORK_GUI=ON ^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
  -DACTIVE_NETWORK=vxlnetwork_%NETWORK_CFG%_network ^
  -DVXLNETWORK_SIMD_OPTIMIZATIONS=TRUE ^
  -Dgtest_force_shared_crt=on ^
  -DBoost_NO_SYSTEM_PATHS=TRUE ^
  -DBoost_NO_BOOST_CMAKE=TRUE ^
  -DVXLNETWORK_SHARED_BOOST=%VXLNETWORK_SHARED_BOOST%

set exit_code=%errorlevel%
if %exit_code% neq 0 goto exit

:exit
exit /B %exit_code%