--TEST--
RarEntry::extract() method
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/linux_rar.rar'); 
$entry1 = rar_entry_get($rar_file1, 'test file with whitespaces.txt');

$entry1->extract(dirname(__FILE__));
$contents11 = file_get_contents(dirname(__FILE__).'/test file with whitespaces.txt');
echo $contents11."\n";

$entry1->extract(false,dirname(__FILE__).'/1.txt');
$contents12 = file_get_contents(dirname(__FILE__).'/1.txt');
echo $contents12."\n";

$rar_file2 = rar_open(dirname(__FILE__).'/latest_winrar.rar'); 
$entry2 = rar_entry_get($rar_file2, '2.txt');

$entry2->extract(dirname(__FILE__));
$contents21 = file_get_contents(dirname(__FILE__).'/2.txt');
echo $contents21."\n";

$entry2->extract(false,dirname(__FILE__).'/some.txt');
$contents22 = file_get_contents(dirname(__FILE__).'/some.txt');
echo $contents22."\n";

@unlink(dirname(__FILE__).'/test file with whitespaces.txt');
@unlink(dirname(__FILE__).'/1.txt');
@unlink(dirname(__FILE__).'/2.txt');
@unlink(dirname(__FILE__).'/some.txt');
?>
--EXPECTF--
blah-blah-blah
blah-blah-blah
22222
22222

