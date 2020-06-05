# rabamOS - Raspberry Pi Bare Metal OS

Who else has some of the "old" _Raspberry Pi_ boards of the first generation floating around?
As they only have a single core processor and at max 256MB of RAM, they are not good for running 
a current version of any OS in a stable and performant way.

## History and Ideation
As I did some projects with ESP8266 boards up to a level where I used up all GPIOs and was 
struggling with the performance barrier, I was thinking about switching to the _Raspberry Pi_
for a lot more power and memory. But I was not able to find a good base to start from without
having to use a full raspbian in the first place.

So I started off with the [tutorial of @jsandler18](https://jsandler18.github.io/) to build
a custom kernel and placed it on a SD card with [PiLFS](https://intestinate.com/pilfs/), as
it was the most minimal system I could find.

I replaced the Makefile based build with the more convenient (IMHO), bash-based `do.sh`.

## Preparation
Most packages are available in the Ubuntu repositories. Unfortunately, `qemu-system-arm` is 
available in a very old version, so I could not get it running. So I downloaded the most 
current version from https://download.qemu.org/.

As VNC viewer, I chose `vinagre`, but you are free to choose any other. Just don't forget 
to change it in `do.sh`.
 
Installation on ubuntu/mint: compiler and VM
> `sudo apt install gcc-arm-none-eabi vinagre unzip wget`

# Prepared drivers and base image
SD Card based on PiLFS: https://gitlab.com/gusco/pilfs-images/

## Building

The build process is done using do.sh

### Building the kernel
> `./do.sh build`
### Running the kernel on qemu
> `./do.sh run`
### Prepare the SD card with PiLFS
> `./do.sh prepare`
### Clean all object and image files
> `./do.sh clean`
