--TEST--
Stream wrapper basic test
--FILE--
<?php
$stream = fopen("rar://" .
	dirname(__FILE__) . '/latest_winrar.rar' .
	"#1.txt", "r");
var_dump(stream_get_contents($stream));

echo "Done.\n";
--EXPECTF--
string(5) "11111"
Done.
