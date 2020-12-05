echo Running appveyor.bat
echo on

CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"

IF NOT EXIST "C:\projects\php-sdk" (
	wget -nv http://windows.php.net/downloads/php-sdk/php-sdk-binary-tools-20110915.zip
	7z x -y php-sdk-binary-tools-20110915.zip -oC:\projects\php-sdk
)
IF NOT EXIST "C:\projects\php-src\Release_TS\php7ts.lib" (
	git clone --depth=1 --branch=PHP-7.1 https://github.com/php/php-src C:\projects\php-src
	wget -nv http://windows.php.net/downloads/php-sdk/deps-7.1-vc14-x86.7z
	7z x -y deps-7.1-vc14-x86.7z -oC:\projects\php-src
	CALL C:\projects\php-sdk\bin\phpsdk_setvars.bat
	cd C:\projects\php-src
	CALL buildconf.bat
	CALL configure.bat --disable-all --enable-cli --with-config-file-scan-dir=C:\projects\extension\bin\modules.d --with-prefix=%APPVEYOR_BUILD_FOLDER%\bin --with-php-build=deps
	nmake
) ELSE (
	echo php7ts.lib already exists
	cd C:\projects\php-src
	CALL C:\projects\php-sdk\bin\phpsdk_setvars.bat
)

CALL buildconf.bat --force --add-modules-dir=%APPVEYOR_BUILD_FOLDER%\..
CALL configure.bat --disable-all --enable-cli --enable-rar=shared --with-config-file-scan-dir=C:\projects\extension\bin\modules.d --with-prefix=%APPVEYOR_BUILD_FOLDER%\bin --with-php-build=deps
nmake || exit /b
rmdir Release_TS\php-rar /S /Q
del /S /Q "Release_TS\*.sbr"

copy %APPVEYOR_BUILD_FOLDER%\run-tests8.php C:\projects\php-src\run-tests.php
