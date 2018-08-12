--TEST--
Redirection functions
--SKIPIF--
<?php
if(!extension_loaded("rar")) die("skip");
--FILE--
<?php
$m = array(
	RarEntry::FSREDIR_UNIXSYMLINK => 'FSREDIR_UNIXSYMLINK',
	RarEntry::FSREDIR_WINSYMLINK => 'FSREDIR_WINSYMLINK',
	RarEntry::FSREDIR_JUNCTION => 'FSREDIR_JUNCTION',
	RarEntry::FSREDIR_HARDLINK => 'FSREDIR_HARDLINK',
	RarEntry::FSREDIR_FILECOPY => 'FSREDIR_FILECOPY',
);
$a = rar_open(dirname(__FILE__) . '/rar5-links.rar');
$i = 0;
foreach ($a as $e) {
	if ($i++ != 0) echo "\n";
	echo "$i. ", $e->getName(), "\n";
	$type = $e->getRedirType();
	$type = $type ? $m[$type] : $type;
	echo "redir type: ", var_export($type, true), "\n";
	echo "redir to dir: ", var_export($e->isRedirectToDirectory(), true), "\n";
	echo "redir target: ", var_export($e->getRedirTarget(), true), "\n";
//	break;
}
echo "Done.\n";
--EXPECTF--
1. file1-hardlink.txt
redir type: NULL
redir to dir: NULL
redir target: NULL

2. file1.txt
redir type: 'FSREDIR_HARDLINK'
redir to dir: false
redir target: 'file1-hardlink.txt'

3. dir-link
redir type: 'FSREDIR_UNIXSYMLINK'
redir to dir: true
redir target: 'dir'

4. file1-link.txt
redir type: 'FSREDIR_UNIXSYMLINK'
redir to dir: false
redir target: 'file1.txt'

5. dir
redir type: NULL
redir to dir: NULL
redir target: NULL
Done.
