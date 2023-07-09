# Source code for Linux 

## General instructions

- CD into the directory of the module/kernel driver's source code.
- To build: `make all`
- To install the module `make install`
- To clean the binaries and uinstall the module: `make clean` 


## echo

- Source: [src/echo/echo.c](src/echo/echo.c)
- Description: 
  - Takes a user-space string, write its to printk, echoes it back.
  - The driver is available under `/dev/echo`
- Goals:
    - To explore the kernel driver lifecyle.
    - To provide read/write primitives.
    - To make the driver available under `/dev` using the 
      Linux commands for that purpose.

### Sample Interactions

```
sudo bash -c "echo foobar > /dev/echo"
sudo bash -c "cat /dev/echo"
```

## echo with ioctl

- Source: [src/echo_ioctl/echo.c](src/echo_ioctl/echo.c)
- Description: 
  - Takes a user-space string, write its to printk, echoes it back.
  - Takes 'clear' and 'reverse' commands, which respectively clear and reverse
    the content sent to the driver and kept by it.
  - The driver is available under `/dev/echo`
- Goals:
    - To explore the kernel driver lifecyle.
    - To provide read/write primitives.
    - To explore ioctl support.
    - To make the driver available under `/dev` using the 
      Linux commands for that purpose.

### Sample Interactions

```
sudo bash -c "echo foobar > /dev/echo"
sudo ./echo_client reverse
sudo bash -c "cat /dev/echo"
raboof
sudo ./echo_client reverse
sudo bash -c "cat /dev/echo"

```

## dice

- Source: [src/dice/dice.c](src/dice/dice.c)
- Description: Kernel module that simulates dice rolls. The number of 
  dice to use may be specified by the user.
- The module makes a read-write proc entry available under `/proc/echo`.
- Goals:
    - To explore kernel module lifecycle.
    - To make a read-write proc entry available under procfs.
    - To explore the use of vmalloc.

### Sample Interactions

```
$ sudo bash -c "cat /proc/dice"
2,4
$ sudo bash -c "echo 6 > /proc/dice"
$ sudo bash -c "cat /proc/dice"
$ 3,4,4,6,1,4
```

## magic8ball

- Source: [src/magic8ball/magic8ball.c](src/magic8ball/magic8ball.c)
- Description: Kernel module that simulates displaying random good/bad fortune
  messages.
- The module makes a read-write proc entry available under `/proc/magic8ball`.
- Goals:
    - To explore kernel module lifecycle.
    - To make a read-write proc entry available under procfs.
    - To explore the joint use of kalloc/vmalloc.

### Sample Interactions

```
$ sudo bash -c "cat /proc/magic8ball"
Better not tell you now. My sources say no. 
$ sudo bash -c "echo 4 > /proc/magic8ball"
$ sudo bash -c "cat /proc/magic8ball"
Outlook not so good. Cannot predict now. Don't count on it. Very doubtful.
```
