#!/bin/bash

CMAKE_DIR="cmake-build-debug"
CMAKE_OPTS="-DCMAKE_BUILD_TYPE=Debug"

args=`getopt hrc $*`
RETVAL=$?
if [ $RETVAL -ne 0 ]; then
	echo "invalig arguments, build.sh -h for help"
	exit
fi

for i ; do
       case "$i"
       in
               -h)
                       echo "build.sh [-c]"
                       echo "  -c clean the build directory before building"
                       echo "  -r build as release"
                       exit 1;;
               -c)
                       CLEAN=1
                       shift;;
               -r)
			echo "Building release version"
			CMAKE_DIR="cmake-build-release"
			CMAKE_OPTS="-DCMAKE_BUILD_TYPE=Release"
			;;
               --)
                       shift; break;;
       esac
done

if [ ! -z "$CLEAN" ]; then
	echo "Removing build directory $CMAKE_DIR"
	rm -rf $CMAKE_DIR
fi

if [ ! -d "$CMAKE_DIR" ]; then
	echo "Creating build directory $CMAKE_DIR"
	mkdir $CMAKE_DIR
fi

if [ -z "$IDF_PATH" ] || [ ! -f "${IDF_PATH}/tools/cmake/project.cmake" ]; then
	echo "Missing or invalid IDF_PATH environment variable"
	exit 1
fi

IDFPY=`which idf.py`

if [ -z "$IDFPY" ]; then
	echo "idf.py not in the path, did you run . ./export.sh?"
	exit
fi
 
idf.py -B ${CMAKE_DIR} $CMAKE_OPTS build