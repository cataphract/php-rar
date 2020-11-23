--TEST--
Iterate by object RarArchive
--SKIPIF--
<?php
if (!extension_loaded("rar") || version_compare(phpversion(), '8.0') == -1) die("skip");
--FILE--
<?php

$rar = rar_open(dirname(__FILE__) . "/linux_rar.rar");

foreach ($rar as $key => $value) {
    var_dump($key, $value);
}
echo "Done\n";
--EXPECTF--
int(0)
object(RarEntry)#3 (15) {
  ["rarfile":"RarEntry":private]=>
  object(RarArchive)#1 (0) {
  }
  ["position":"RarEntry":private]=>
  int(0)
  ["name":"RarEntry":private]=>
  string(9) "plain.txt"
  ["unpacked_size":"RarEntry":private]=>
  int(15)
  ["packed_size":"RarEntry":private]=>
  int(25)
  ["host_os":"RarEntry":private]=>
  int(3)
  ["file_time":"RarEntry":private]=>
  string(19) "2004-06-11 11:01:24"
  ["crc":"RarEntry":private]=>
  string(8) "7728b6fe"
  ["attr":"RarEntry":private]=>
  int(33188)
  ["version":"RarEntry":private]=>
  int(29)
  ["method":"RarEntry":private]=>
  int(51)
  ["flags":"RarEntry":private]=>
  int(0)
  ["redir_type":"RarEntry":private]=>
  int(0)
  ["redir_to_directory":"RarEntry":private]=>
  NULL
  ["redir_target":"RarEntry":private]=>
  NULL
}
int(1)
object(RarEntry)#4 (15) {
  ["rarfile":"RarEntry":private]=>
  object(RarArchive)#1 (0) {
  }
  ["position":"RarEntry":private]=>
  int(1)
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
  int(0)
  ["redir_type":"RarEntry":private]=>
  int(0)
  ["redir_to_directory":"RarEntry":private]=>
  NULL
  ["redir_target":"RarEntry":private]=>
  NULL
}
Done
