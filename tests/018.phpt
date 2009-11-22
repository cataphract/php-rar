--TEST--
rar_list()/rar_entry_get() with not first volume
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
$rar_file1 = rar_open(dirname(__FILE__).'/multi.part2.rar');
$entries = rar_list($rar_file1);
var_dump($entries);
$e = rar_entry_get($rar_file1, "file2.txt");
var_dump($e);
$e = rar_entry_get($rar_file1, "file3.txt");
var_dump($e);

echo "Done\n";
--EXPECTF--
array(1) {
  [0]=>
  object(RarEntry)#%d (%d) {
    ["rarfile"]=>
    resource(%d) of type (Rar file)
    ["name"]=>
    string(9) "file3.txt"
    ["unpacked_size"]=>
    int(18)
    ["packed_size"]=>
    int(27)
    ["host_os"]=>
    int(2)
    ["file_time"]=>
    string(19) "2009-11-18 23:52:36"
    ["crc"]=>
    string(8) "bcbce32e"
    ["attr"]=>
    int(32)
    ["version"]=>
    int(29)
    ["method"]=>
    int(51)
  }
}

Warning: rar_entry_get(): cannot find file "file2.txt" in Rar archive "%s. in %s on line %d
bool(false)
object(RarEntry)#%d (%d) {
  ["rarfile"]=>
  resource(%d) of type (Rar file)
  ["name"]=>
  string(9) "file3.txt"
  ["unpacked_size"]=>
  int(18)
  ["packed_size"]=>
  int(27)
  ["host_os"]=>
  int(2)
  ["file_time"]=>
  string(19) "2009-11-18 23:52:36"
  ["crc"]=>
  string(8) "bcbce32e"
  ["attr"]=>
  int(32)
  ["version"]=>
  int(29)
  ["method"]=>
  int(51)
}
Done
