--TEST--
PECL bug #18449 (Extraction of uncompressed and encrypted files fails; stream variant)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--CLEAN--
<?php
	@unlink(dirname(__FILE__) . '/base.css');
	@unlink(dirname(__FILE__) . '/reset.css');
--FILE--
<?php

$rar = rar_open(dirname(__FILE__) . '/secret-none.rar', 'secret');
foreach ($rar as $rar_file) {
	var_dump(strlen(stream_get_contents($rar_file->getStream())));
}

echo "\nDone.\n";
--EXPECTF--
Warning: stream_get_contents(): The file size is supposed to be 2279 bytes, but we read more: 2288 bytes (corruption/wrong pwd) in %s on line %d
int(2288)

Warning: stream_get_contents(): The file size is supposed to be 1316 bytes, but we read more: 1328 bytes (corruption/wrong pwd) in %s on line %d
int(1328)

Done.
