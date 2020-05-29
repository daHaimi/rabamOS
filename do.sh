#!/usr/bin/env bash

MKDIR="mkdir -p"
RM="rm -rf"
BIN_DIR=bin
EMU=qemu-system-arm
GCC=arm-none-eabi-gcc
CPU=cortex-a7

if [[ "${1}" = "build" ]]; then
    ${MKDIR} ${BIN_DIR}
    ${GCC} -mcpu=${CPU} -fpic -ffreestanding -c boot.S -o ${BIN_DIR}/boot.o
    ${GCC} -mcpu=${CPU} -fpic -ffreestanding -std=gnu99 -c kernel.c -o ${BIN_DIR}/kernel.o -O2 -Wall -Wextra
    ${GCC} -T linker.ld -o ${BIN_DIR}/myos.elf -ffreestanding -O2 -nostdlib ${BIN_DIR}/boot.o ${BIN_DIR}/kernel.o
fi

if [[ "${1}" = "run" ]]; then
    ${EMU} -m 256 -M raspi2 -serial stdio -kernel ${BIN_DIR}/myos.elf
fi

if [[ "${1}" = "clean" ]]; then
    ${RM} ${BIN_DIR}
fi
