<?php
define('PHTRACE_PORT', 19229);

$sock = socket_create_listen(PHTRACE_PORT);

$descriptors = array(
    array('pipe', 'r'),
    array('pipe', 'w'),
    array('pipe', 'a'),
);

$cwd = getcwd();
$proc = proc_open("php -n -d extension=$cwd/modules/phtrace.so", $descriptors, $pipes, $cwd);

$conn = socket_accept($sock);
$content = socket_read($conn, 1<<14);

proc_close($proc);
socket_close($conn);
socket_close($sock);

$expected = <<<EOE
{"some_json_field":"some_json_value"}

EOE;

exit($expected == $content ? 0 : 1);
