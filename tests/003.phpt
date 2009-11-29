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
object(RarEntry)#%d (%d) {
  ["rarfile":"RarEntry":private]=>
  resource(%d) of type (Rar file)
  ["name":"RarEntry":private]=>
  string(30) "test file with whitespaces.txt"
  ["unpacked_size":"RarEntry":private]=>
  int(14)
  ["packed_size":"RarEntry":private]=>
  int(20)
  ["host_os":"RarEntry":private]=>
  int(3)
  ["file_time":"RarEntry":private]=>
  string(19) "2004-06-11 11:01:32"
  ["crc":"RarEntry":private]=>
  string(8) "21890dd9"
  ["attr":"RarEntry":private]=>
  int(33188)
  ["version":"RarEntry":private]=>
  int(29)
  ["method":"RarEntry":private]=>
  int(51)
  ["flags":"RarEntry":private]=>
  int(32800)
}
object(RarEntry)#%d (%d) {
  ["rarfile":"RarEntry":private]=>
  resource(%d) of type (Rar file)
  ["name":"RarEntry":private]=>
  string(5) "2.txt"
  ["unpacked_size":"RarEntry":private]=>
  int(5)
  ["packed_size":"RarEntry":private]=>
  int(16)
  ["host_os":"RarEntry":private]=>
  int(2)
  ["file_time":"RarEntry":private]=>
  string(19) "2004-06-11 10:07:26"
  ["crc":"RarEntry":private]=>
  string(8) "45a918de"
  ["attr":"RarEntry":private]=>
  int(32)
  ["version":"RarEntry":private]=>
  int(29)
  ["method":"RarEntry":private]=>
  int(53)
  ["flags":"RarEntry":private]=>
  int(37008)
}

Warning: rar_open(): Failed to open %s: ERAR_EOPEN (file open error) in %s on line %d

Warning: rar_entry_get() expects parameter 1 to be resource, boolean given in %s on line %d
NULL

Warning: rar_entry_get(): supplied resource is not a valid Rar file resource in %s on line %d
bool(false)
Done
