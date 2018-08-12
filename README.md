PHP extension for reading RAR archives using the bundled UnRAR library.

This extension uses a modified version of the UnRAR library. The UnRAR library
is an official open-source library by RARLabs, an auto generated subset of the
RAR codebase. It is available from http://www.rarlab.com/rar_add.htm
Please note that it has a more restrictive license than the PHP bindings,
barring using it to re-create the RAR compression algorithm. See
unrar/LICENSE.txt for details.

Some modifications have been applied to the UnRAR library, mainly to allow
streaming extraction of files without using threads.

[![Build Status Appveyor](https://ci.appveyor.com/api/projects/status/cbgpepx6kyax2198/branch/master?svg=true)](https://ci.appveyor.com/project/cataphract/php-rar/branch/master)
[![Build Status Travis](https://travis-ci.org/cataphract/php-rar.svg?branch=master)](https://travis-ci.org/cataphract/php-rar)
[![codecov](https://codecov.io/gh/cataphract/php-rar/branch/master/graph/badge.svg)](https://codecov.io/gh/cataphract/php-rar)
