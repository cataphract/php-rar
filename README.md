PHP extension for reading RAR archives using the bundled UnRAR library.

This extension uses a modified version of the UnRAR library. The UnRAR library
is an official open-source library by RARLabs, an auto generated subset of the
RAR codebase. It is available from http://www.rarlab.com/rar_add.htm
Please note that it has a more restrictive license than the PHP bindings,
barring using it to re-create the RAR compression algorithm. See
unrar/LICENSE.txt for details.

Some modifications have been applied to the UnRAR library, mainly to allow
streaming extraction of files without using threads.

## Installation

### With PECL

```sh
pecl install rar
```

Then add `extension=rar` to your `php.ini`.

### With PIE

[PIE](https://github.com/php/pie) is the modern replacement for PECL, available from PHP 8.1+.

```sh
pie install rar
```

PIE automatically adds the extension to your `php.ini`.
