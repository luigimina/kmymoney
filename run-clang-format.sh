#!/bin/bash

find kmymoney tools \( -name "*.cpp" -or -name "*.h"  -or -name "*.c"  -or -name "*.cc" \) -exec clang-format-11 -i {} \;
