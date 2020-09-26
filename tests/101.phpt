--TEST--
Supports version 5 RAR files
--SKIPIF--
<?php if(!extension_loaded("rar")) die("skip");
if (isset($_ENV['APPVEYOR'])) die("skip failing on appveyor");
--FILE--
<?php
RarException::setUsingExceptions(true);
$file = dirname(__FILE__) . '/rar5_multi.part1.rar';
$rar = RarArchive::open($file);
$entry = $rar->getEntry('usr' . DIRECTORY_SEPARATOR . 'bin' .
  DIRECTORY_SEPARATOR . 'text2image');
var_dump($entry);
$stream = $entry->getStream('passw0rd');
$contents = stream_get_contents($stream);
echo "(unpacked) MD5: ", md5($contents), "\n";
echo "Done.\n";
?>
--EXPECTF--
object(RarEntry)#%d (%d) {
  ["rarfile%sprivate%s=>
  object(RarArchive)#%d (%d) {
  }
  ["position%sprivate%s=>
  int(0)
  ["name%sprivate%s=>
  string(18) "usr%sbin%stext2image"
  ["unpacked_size%sprivate%s=>
  int(147528)
  ["packed_size%sprivate%s=>
  int(57104)
  ["host_os%sprivate%s=>
  int(3)
  ["file_time%sprivate%s=>
  string(19) "%s"
  ["crc%sprivate%s=>
  string(8) "83c9a6b7"
  ["attr%sprivate%s=>
  int(33261)
  ["version%sprivate%s=>
  int(50)
  ["method%sprivate%s=>
  int(51)
  ["flags%sprivate%s=>
  int(5)
  ["redir_type%sprivate%s=>
  int(0)
  ["redir_to_directory%sprivate%s=>
  NULL
  ["redir_target%sprivate%s=>
  NULL
}
(unpacked) MD5: c07ce36ec260848f47fe8ac1408f938f
Done.
