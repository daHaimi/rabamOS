#!/usr/bin/env bash

MKDIR="mkdir -p"
RM="rm -rf"
ECHO="echo"
CP="cp -R"
OBJCOPY="arm-none-eabi-objcopy"
MD5SUM=md5sum
UNZIP="unzip"
BIN_DIR=bin
BIN_FW="${BIN_DIR}/firmware"
BUILD_DIR=build
EMU=/usr/local/bin/qemu-system-arm
VNC=vinagre
GCC=arm-none-eabi-gcc
WGET=wget
CPU=arm1176jzf-s
SRC_DIR=src
COMMON_SRC=${SRC_DIR}/common
KER_SRC=${SRC_DIR}/kernel
KER_HEAD=include
DIRECTIVES=" -D MODEL_1"
CFLAGS="-mcpu=${CPU} -fpic -ffreestanding ${DIRECTIVES}"
CSRCFLAGS="-O2 -Wall -Wextra"
LFLAGS="-ffreestanding -O2 -nostdlib"
IMG_NAME="kernel"

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
    ${GCC} -T ${BUILD_DIR}/linker.ld ${LFLAGS} ${BIN_DIR}/*.o -o ${IMG_NAME}.elf
    ${OBJCOPY} -O binary -S ${IMG_NAME}.elf ${IMG_NAME}.img
fi

if [[ "${1}" = "run" ]]; then
    ${EMU} -m 512 -M raspi1 -serial stdio -kernel ${IMG_NAME}.elf -vnc :5 & ${VNC} localhost:5905 && killall ${EMU}
fi

if [[ "${1}" = "clean" ]]; then
    ${RM} ${BIN_DIR}
    ${RM} ${IMG_NAME}.{img,elf}
fi

if [[ "${1}" = "deploy" ]]; then
    export BOOT_DIR="/media/${USER}/${2}"
    ${CP} ${IMG_NAME}.img "${BOOT_DIR}/"
    ( cd "${BOOT_DIR}/" && ${MD5SUM} ${IMG_NAME}.img > ${IMG_NAME}.img.md5 )
    sync
fi
