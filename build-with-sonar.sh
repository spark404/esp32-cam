#!/bin/bash

SONAR_URL=https://sonarcloud.io

if echo $OSTYPE | grep -i darwin ; then
	WRAPPER_OS=macosx
else
	WRAPPER_OS=linux
fi

SCANNER_DIR=sonar-scanner-cli-4.6.2.2472-${WRAPPER_OS}
SCANNER_ZIP=${SCANNER_DIR}.zip

rm -rf build-wrapper-${WRAPPER_OS}-x86.zip build-wrapper-${WRAPPER_OS}-x86
curl -O ${SONAR_URL}/static/cpp/build-wrapper-${WRAPPER_OS}-x86.zip
unzip build-wrapper-${WRAPPER_OS}-x86.zip

rm -rf $SCANNER_ZIP $SCANNER_DIR
curl -O https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/$SCANNER_ZIP
unzip $SCANNER_ZIP

WRAPPER="./build-wrapper-${WRAPPER_OS}-x86/build-wrapper-${WRAPPER_OS}-x86 --out-dir build_wrapper_output_directory"
SCANNER="./sonar-scanner-4.6.2.2472-${WRAPPER_OS}/bin/sonar-scanner"

rm -f sdkconfig
$WRAPPER idf.py reconfigure clean build

$SCANNER -Dproject.settings=sonar-project.properties -Dsonar.login=$(<~/sonar.authtoken)
