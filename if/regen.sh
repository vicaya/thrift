#!/bin/sh
cd "`dirname $0`"
../compiler/cpp/thrift -cpp reflection_limited.thrift
cp gen-cpp/reflection_limited_types.h   ../lib/cpp/src/
cp gen-cpp/reflection_limited_types.cpp ../lib/cpp/src/