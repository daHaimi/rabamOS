# rabamOS - Raspberry Pi Bare Metal OS

based on https://jsandler18.github.io/

## Preparation

Installation on ubuntu: compiler and VM
> `sudo apt install qemu-system-arm gcc-arm-none-eabi`

# Prepared drivers and base image
SD Card based on PiLFS: 
https://gitlab.com/gusco/pilfs-images/

## Building

The build process is done using do.sh

> `./do.sh build`
> `./do.sh run`
> `./do.sh clean`
