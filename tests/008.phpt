--TEST--
rar_entry_get() function
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/multi.part1.rar');
$entry = rar_entry_get($rar_file1, "file1.txt");
var_dump($entry);
echo "\n";

$rar_file2 = rar_open(dirname(__FILE__).'/nonexistent.rar'); 
$entry = rar_entry_get($rar_file2, "file1.txt");
var_dump($entry);
echo "\n";

echo "Done\n";
?>
--EXPECTF--
object(RarEntry)#%d (%d) {
  ["rarfile"]=>
  resource(%d) of type (Rar file)
  ["name"]=>
  string(9) "file1.txt"
  ["unpacked_size"]=>
  int(18)
  ["packed_size"]=>
  int(18)
  ["host_os"]=>
  int(2)
  ["file_time"]=>
  string(19) "2009-11-18 23:52:24"
  ["crc"]=>
  string(8) "52b28202"
  ["attr"]=>
  int(32)
  ["version"]=>
  int(29)
  ["method"]=>
  int(48)
}


Warning: rar_open(): Failed to open %s: ERAR_EOPEN (file open error) in %s on line %d

Warning: rar_entry_get() expects parameter 1 to be resource, boolean given in %s on line %d
NULL

Done
