#!/bin/bash
# whitespace for testing
SONAR_URL=https://sonarcloud.io

if echo $OSTYPE | grep -i darwin ; then
	BUILD_OS=macosx
	BUILD_ARCH=x86
else
	BUILD_OS=linux
	BUILD_ARCH=x86-64
fi

SCANNER_DIR=sonar-scanner-cli-4.6.2.2472-${BUILD_OS}
SCANNER_ZIP=${SCANNER_DIR}.zip

WRAPPER_DIR=build-wrapper-${BUILD_OS}-x86
WRAPPER_ZIP=${WRAPPER_DIR}.zip

rm -rf $SCANNER_ZIP $SCANNER_DIR $WRAPPER_DIR $WRAPPER_ZIP

curl -O ${SONAR_URL}/static/cpp/$WRAPPER_ZIP
unzip build-wrapper-${BUILD_OS}-x86.zip

curl -O https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/$SCANNER_ZIP
unzip $SCANNER_ZIP


WRAPPER="./build-wrapper-${BUILD_OS}-x86/build-wrapper-${BUILD_OS}-${BUILD_ARCH} --out-dir build_wrapper_output_directory"
SCANNER="./sonar-scanner-4.6.2.2472-${BUILD_OS}/bin/sonar-scanner"

rm -f sdkconfig
$WRAPPER idf.py reconfigure clean build

RET=$?
if [ $RET -ne 0 ] ; then
  echo Build failed!
  exit $RET
fi

if [ ! -e build/espcam.bin ] ; then
	echo Build failed or didn\'t generate binary
	exit 1
fi

$SCANNER -Dproject.settings=sonar-project.properties -Dsonar.login=${SONAR_AUTHTOKEN}
