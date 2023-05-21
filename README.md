# Source code for Linux 

## General instructions

- CD into the directory of the module/kernel driver's source code.
- To build: `make all`
- To install the module `make install`
- To clean the binaries and uinstall the module: `make clean` 


## echo

Source: [src/echo/echo.c](src/echo/echo.c)
Description: takes a user-space string, write its to printk, echoes it back.




```