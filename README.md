# phtrace

The phtrace extension provides a way to record the trace of a php script
execution. It records function call timing which allows to build flame graphs.
It uses [RDTSCP](http://www.felixcloutier.com/x86/RDTSCP.html) for microtimig.

This extension is experimental. Please do not use it in production.

### Installation

    phpize
    ./configure
    make
    make install
