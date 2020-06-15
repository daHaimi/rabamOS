#!/usr/bin/env bash

# Commands
ECHO="echo"
CP="cp -R"
MKDIR="mkdir -p"
RM="rm -rf"
CHMOD="chmod"
KILL="kill"
MD5SUM="md5sum"
UNZIP="unzip"
WGET="wget"
OBJCOPY="arm-none-eabi-objcopy"
EMU="/usr/local/bin/qemu-system-arm"
VNC="vinagre"
GCC="arm-none-eabi-gcc"
XZCAT="xzcat"
# Files and folders
BIN_DIR="bin"
DIST_DIR="dist"
BUILD_DIR="build"
SRC_DIR="src"
COMMON_SRC="${SRC_DIR}/common"
KER_SRC="${SRC_DIR}/kernel"
KER_HEAD="include"
IMG_NAME="kernel"
# Arguments and directives
CPU=arm1176jzf-s
DIRECTIVES=" -D MODEL_1"
CFLAGS="-mcpu=${CPU} -fpic -ffreestanding ${DIRECTIVES}"
CSRCFLAGS="-O2 -Wall -Wextra"
LFLAGS="-ffreestanding -O2 -nostdlib"
# PiLFS image
IMAGE_FILE="pilfs-base-rpi1-20160824.img.xz"
IMAGE_REPO="https://gitlab.com/gusco/pilfs-images/-/raw/master/"

if [[ "${1}" == "build" ]]; then
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

if [[ "${1}" == "run" ]]; then
  ${EMU} -m 256 -cpu arm1176 -M versatilepb -kernel ${IMG_NAME}.elf -vnc :5 &
  export RABAMOS_PID="$!" &&
    ${VNC} localhost:5905 && ${KILL} ${RABAMOS_PID}
fi

if [[ "${1}" == "clean" ]]; then
  ${RM} ${BIN_DIR}
  ${RM} ${IMG_NAME}.{img,elf}
fi

if [[ "${1}" == "prepare" ]]; then
  if [[ ! $EUID -eq 0 ]]; then
    ${ECHO} "Disk operations must be run as root. Please run again with sudo or as root."
    exit 1
  fi
  if [[ ${2} == "" || ! -e ${2} ]]; then
    ${ECHO} "Param must be a valid device e.g. /dev/sdb"
    exit 2
  fi
  # If mounted: unmount
  if grep -qs "${2}" /proc/mounts; then
    for dev in "${2}"{0,1,2,3,4,5,6,7,8,9}; do
      if [[ -e ${dev} ]] && ! umount "${dev}"; then
        ${ECHO} "Unable to unmount device ${dev}. Close every process using it and retry."
        exit 3
      fi
    done
  fi
  if [[ ! -f "${DIST_DIR}/${IMAGE_FILE}" ]]; then
    ${MKDIR} ${DIST_DIR} && ${WGET} --directory-prefix=${DIST_DIR} "${IMAGE_REPO}${IMAGE_FILE}"
  fi
  ${ECHO} "Downloading base image completed. Now flashing device ${2}"
  ${XZCAT} "${DIST_DIR}/${IMAGE_FILE}" | dd bs=4M of="${2}"
  ${CHMOD} 0777 ${DIST_DIR} -R
fi

if [[ "${1}" == "deploy" ]]; then
  if [[ -z "${2}" ]]; then
    ${ECHO} "You need to pass the mount path to a SD card boot image."
    ${ECHO} "There are following mounts available:"
    for dir in "/media/${USER}/"*; do
      ${ECHO} "- $(basename -- "${dir}")"
    done
    exit 4
  fi
  export BOOT_DIR="/media/${USER}/${2}"
  ${CP} ${IMG_NAME}.img "${BOOT_DIR}/"
  (cd "${BOOT_DIR}/" && ${MD5SUM} ${IMG_NAME}.img >${IMG_NAME}.img.md5)
  sync
fi
