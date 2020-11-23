--TEST--
RarArchive::setAllowBroken has the desired effect
--SKIPIF--
<?php if(!extension_loaded("rar") || version_compare(phpversion(), '8.0') == -1) print "skip"; ?>
--FILE--
<?php

function retnull() { return null; }
$b = dirname(__FILE__) . "/multi_broken.part1.rar";

echo "* broken file; bad arguments\n";
$a = RarArchive::open($b, null, 'retnull');
try {
    $a->setAllowBroken();
    die("should have thrown exception.");
} catch (ArgumentCountError $e) {
    echo "\nOK, threw ArgumentCountError: " . $e->getMessage() . "\n";
}
try {
    rar_allow_broken_set($a);
    die("should have thrown exception.");
} catch (ArgumentCountError $e) {
    echo "\nOK, threw ArgumentCountError: " . $e->getMessage() . "\n";
}

echo "\n* broken file; do not allow broken (default)\n";
$a = RarArchive::open($b, null, 'retnull');
var_dump($a->getEntries());
var_dump(count($a));

echo "\n* broken file; do not allow broken (explicit)\n";
$a = RarArchive::open($b, null, 'retnull');
$a->setAllowBroken(false);
var_dump($a->getEntries());
var_dump(count($a));

echo "\n* broken file; allow broken\n";
$a = RarArchive::open($b, null, 'retnull');
$a->setAllowBroken(true);
foreach ($a->getEntries() as $e) {
	echo "$e\n";
}
var_dump(count($a));

echo "\n* broken file; allow broken; non OOP\n";
$a = RarArchive::open($b, null, 'retnull');
rar_allow_broken_set($a, true);
foreach ($a->getEntries() as $e) {
	echo "$e\n";
}
var_dump(count($a));

echo "\n";
echo "Done.\n";
--EXPECTF--
* broken file; bad arguments

OK, threw ArgumentCountError: RarArchive::setAllowBroken() expects exactly 1 argument, 0 given

OK, threw ArgumentCountError: rar_allow_broken_set() expects exactly 2 arguments, 1 given

* broken file; do not allow broken (default)

Warning: RarArchive::getEntries(): ERAR_EOPEN (file open error) in %s on line %d
bool(false)

Warning: %s(): ERAR_EOPEN (file open error) in %s on line %d
int(0)

* broken file; do not allow broken (explicit)

Warning: RarArchive::getEntries(): ERAR_EOPEN (file open error) in %s on line %d
bool(false)

Warning: %s(): ERAR_EOPEN (file open error) in %s on line %d
int(0)

* broken file; allow broken
RarEntry for file "file1.txt" (52b28202)
int(1)

* broken file; allow broken; non OOP
RarEntry for file "file1.txt" (52b28202)
int(1)

Done.
