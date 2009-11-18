--TEST--
rar_entry_get() function
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/linux_rar.rar'); 
$entry1 = rar_entry_get($rar_file1, 'test file with whitespaces.txt');
var_dump($entry1);

$rar_file2 = rar_open(dirname(__FILE__).'/latest_winrar.rar'); 
$entry2 = rar_entry_get($rar_file2, '2.txt');
var_dump($entry2);

$rar_file3 = rar_open(dirname(__FILE__).'/no_such_file.rar'); 
$entry3 = rar_entry_get($rar_file3, '2.txt');
var_dump($entry3);

$fp = fopen(__FILE__, "r");
var_dump(rar_entry_get($fp, '2.txt'));

echo "Done\n";
?>
--EXPECTF--
object(RarEntry)#%d (10) {
  ["rarfile"]=>
  resource(%d) of type (Rar file)
  ["name"]=>
  string(30) "test file with whitespaces.txt"
  ["unpacked_size"]=>
  int(14)
  ["packed_size"]=>
  int(20)
  ["host_os"]=>
  int(3)
  ["file_time"]=>
  string(19) "2004-06-11 11:01:32"
  ["crc"]=>
  string(8) "21890dd9"
  ["attr"]=>
  int(33188)
  ["version"]=>
  int(29)
  ["method"]=>
  int(51)
}
object(RarEntry)#%d (10) {
  ["rarfile"]=>
  resource(%d) of type (Rar file)
  ["name"]=>
  string(5) "2.txt"
  ["unpacked_size"]=>
  int(5)
  ["packed_size"]=>
  int(16)
  ["host_os"]=>
  int(2)
  ["file_time"]=>
  string(19) "2004-06-11 10:07:26"
  ["crc"]=>
  string(8) "45a918de"
  ["attr"]=>
  int(32)
  ["version"]=>
  int(29)
  ["method"]=>
  int(53)
}

Warning: rar_open(): Failed to open %s: ERAR_EOPEN (file open error) in %s on line %d

Warning: rar_entry_get() expects parameter 1 to be resource, boolean given in %s on line %d
NULL

Warning: rar_entry_get(): supplied resource is not a valid Rar file resource in %s on line %d
bool(false)
Done
