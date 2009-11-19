--TEST--
RarEntry::getCrc() method in multi-volume archives (PECL bug #9470)
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/multi.part1.rar'); 
$list = rar_list($rar_file1);
var_dump($list);
echo "\n";

echo "Done\n";
?>
--EXPECTF--
array(3) {
  [0]=>
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
  [1]=>
  object(RarEntry)#%d (%d) {
    ["rarfile"]=>
    resource(%d) of type (Rar file)
    ["name"]=>
    string(9) "file2.txt"
    ["unpacked_size"]=>
    int(17704)
    ["packed_size"]=>
    int(13654)
    ["host_os"]=>
    int(2)
    ["file_time"]=>
    string(19) "2009-11-19 00:00:52"
    ["crc"]=>
    string(8) "f2c79881"
    ["attr"]=>
    int(32)
    ["version"]=>
    int(29)
    ["method"]=>
    int(51)
  }
  [2]=>
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

Done
