<?php
define('PHTRACE_PORT', 19229);

$sock = socket_create_listen(PHTRACE_PORT);

$descriptors = array(
    array('pipe', 'r'),
    array('pipe', 'w'),
    array('pipe', 'a'),
);

$cwd = getcwd();
$proc = proc_open("php -n -d extension=$cwd/modules/phtrace.so -r ';'", $descriptors, $pipes, $cwd);

$conn = socket_accept($sock);

$content = socket_read($conn, 1);
var_dump(bin2hex($content), unpack('C', $content));
$content = socket_read($conn, 4);
var_dump(bin2hex($content), unpack('L', $content));

$content = socket_read($conn, 16);
var_dump(bin2hex($content));

$content = socket_read($conn, 1);
var_dump(bin2hex($content), unpack('C', $content));
$content = socket_read($conn, 4);
var_dump(bin2hex($content), unpack('L', $content));

$content = socket_read($conn, 1);
var_dump(bin2hex($content));

proc_close($proc);
socket_close($conn);
socket_close($sock);
