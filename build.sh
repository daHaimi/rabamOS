#!/usr/bin/env sh

GCC=arm-none-eabi-gcc
CPU=cortex-a7

$GCC -mcpu=$CPU -fpic -ffreestanding -c boot.S -o boot.o
$GCC -mcpu=$CPU -fpic -ffreestanding -std=gnu99 -c kernel.c -o kernel.o -O2 -Wall -Wextra
$GCC -T linker.ld -o myos.elf -ffreestanding -O2 -nostdlib boot.o kernel.o
