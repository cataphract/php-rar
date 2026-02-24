PHP extension for reading RAR archives using the bundled UnRAR library.

This extension uses a modified version of the UnRAR library. The UnRAR library
is an official open-source library by RARLabs, an auto generated subset of the
RAR codebase. It is available from http://www.rarlab.com/rar_add.htm
Please note that it has a more restrictive license than the PHP bindings,
barring using it to re-create the RAR compression algorithm. See
unrar/LICENSE.txt for details.

Some modifications have been applied to the UnRAR library, mainly to allow
streaming extraction of files without using threads.

| Version | Status                       |
|---------|------------------------------|
| master  | unmaintened :x:              |
| v4.x    | maintened :white_check_mark: |

Maintained PHP Versions compatibility:

| PHP Version | Status                 |
|-------------|------------------------|
| 5.x         | no :x:                 |
| 7.x         | no :x:                 |
| 8.0         | yes :white_check_mark: |
| 8.1         | yes :white_check_mark: |
| 8.2         | yes :white_check_mark: |
| 8.3         | yes :white_check_mark: |
| 8.4         | yes :white_check_mark: |
| 8.5         | yes :white_check_mark: |

Installation system support:

| Platform | Status                 |
|----------|------------------------|
| PECL     | no  :x:                |
| PIE      | yes :white_check_mark: |

To install the extension, use PIE (PHP Installer Extension) with a command like:

```bash
pie install php-win-ext/rar
```
