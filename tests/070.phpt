--TEST--
URL stat PHP_STREAM_URL_STAT_QUIET does not leak memory
--FILE--
<?php

$file = "rar://" .
	dirname(__FILE__) . '/dirlink_unix.rar' .
	"#non_existant_file";

var_dump(is_dir($file));

echo "Done.\n";
--EXPECTF--
bool(false)
Done.
