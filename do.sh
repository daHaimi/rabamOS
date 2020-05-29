#!/usr/bin/env bash

MKDIR="mkdir -p"
RM="rm -rf"
BIN_DIR=bin
BUILD_DIR=build
EMU=qemu-system-arm
GCC=arm-none-eabi-gcc
CPU=cortex-a7
SRC_DIR=src
COMMON_SRC=${SRC_DIR}/common
KER_SRC=${SRC_DIR}/kernel
KER_HEAD=include
DIRECTIVES=""
CFLAGS="-mcpu=${CPU} -fpic -ffreestanding ${DIRECTIVES}"
CSRCFLAGS="-O2 -Wall -Wextra"
LFLAGS="-ffreestanding -O2 -nostdlib"
IMG_NAME=kernel.img

if [[ "${1}" = "build" ]]; then
    ${MKDIR} ${BIN_DIR}
    for src in ${SRC_DIR}/*/*.S; do
        obj="$(basename ${src} .S).o"
        ${GCC} ${CFLAGS} -I${KER_SRC} -c ${src} -o ${BIN_DIR}/${obj}
    done
    for src in ${SRC_DIR}/*/*.c; do
        obj="$(basename ${src} .c).o"
        ${GCC} ${CFLAGS} -I${KER_SRC} -I${KER_HEAD} -std=gnu99 -c ${src} -o ${BIN_DIR}/${obj} ${CSRCFLAGS}
    done
    ${GCC} -T ${BUILD_DIR}/linker.ld -o ${BIN_DIR}/myos.elf ${LFLAGS} ${BIN_DIR}/*.o -o ${IMG_NAME}
fi

if [[ "${1}" = "run" ]]; then
    ${EMU} -m 256 -M raspi2 -serial stdio -kernel ${IMG_NAME}
fi

if [[ "${1}" = "clean" ]]; then
    ${RM} ${BIN_DIR}
    ${RM} ${IMG_NAME}
fi
