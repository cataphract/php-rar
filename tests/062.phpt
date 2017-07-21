--TEST--
RAR directory stream stat
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--ENV--
TZ=Europe/Lisbon
--FILE--
<?php
umask(0);

echo "Root:\n";

$u = "rar://" .
	dirname(__FILE__) . '/dirs_and_extra_headers.rar';

print_r(array_slice(fstat(opendir($u)), 13));

echo "\nSub-root directory:\n";

$u = "rar://" .
	dirname(__FILE__) . '/dirs_and_extra_headers.rar#%EF%AC%B0';

$r = array_slice(fstat(opendir($u)), 13);
if (PHP_OS == 'WINNT') {
	// we don't give the correct values on windows
	$r['atime'] = 1272938643;
	$r['mtime'] = 1272938643;
	$r['ctime'] = 1272813170;
}
print_r($r);

echo "Done.\n";
--EXPECTF--
Root:
Array
(
    [dev] => 0
    [ino] => 0
    [mode] => 16895
    [nlink] => 1
    [uid] => 0
    [gid] => 0
    [rdev] => 0
    [size] => 0
    [atime] => 0
    [mtime] => 312768000
    [ctime] => 0
    [blksize] => %s
    [blocks] => %s
)

Sub-root directory:
Array
(
    [dev] => 0
    [ino] => 0
    [mode] => 16895
    [nlink] => 1
    [uid] => 0
    [gid] => 0
    [rdev] => 0
    [size] => 0
    [atime] => 1272938643
    [mtime] => 1272938643
    [ctime] => 1272813170
    [blksize] => %s
    [blocks] => %s
)
Done.
