#!/bin/bash

VERSTRING=`cat VERSION.txt`
VERSION=$VERSTRING
export VERSION
echo $VERSION

cd doxygen
exec doxygen
