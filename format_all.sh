#!/bin/bash

for file in framework/*.{cpp,hpp} framework/device/*.{cpp,hpp} platform/*.{hpp,cpp} platform/*/*.{hpp,cpp} samples/*/*.cpp
do
    echo "Formatting file: $file ..."
    clang-format -style=file -i $file
done
