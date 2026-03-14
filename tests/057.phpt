--TEST--
RAR file stream stat consistency with url stat
--FILE--
<?php
$u = "rar://" .
	dirname(__FILE__) . '/latest_winrar.rar' .
	"#1.txt";
$stream = fopen($u, "r");
$fs = (fstat($stream));

$us = stat($u);

var_dump($fs == $us);

echo "Done.\n";
--EXPECTF--
bool(true)
Done.
