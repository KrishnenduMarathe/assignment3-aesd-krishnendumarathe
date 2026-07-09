#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v6.18.38
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

# CUSTOM: Setup path from /bin/bash in docker for /bin/sh
PATH=$PATH:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/arm-cross-compiler/install/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin

mkdir -p ${OUTDIR}
CURR_PATH=$(pwd)

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE mrproper
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE defconfig
    make -j$(nproc) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE all
    make -j$(nproc) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE modules
    make -j$(nproc) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE dtbs
fi

echo "Adding the Image in outdir"
cp arch/$ARCH/boot/Image $OUTDIR

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var usr/bin usr/lib usr/sbin var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=$OUTDIR/rootfs install

echo "Library dependencies"
cd "$OUTDIR/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
CROSSCHAIN_LOC=$(dirname $(dirname $(which aarch64-none-linux-gnu-gcc)))
cp $CROSSCHAIN_LOC/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 lib
cp $CROSSCHAIN_LOC/aarch64-none-linux-gnu/libc/lib64/libm.so.6 lib64
cp $CROSSCHAIN_LOC/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 lib64
cp $CROSSCHAIN_LOC/aarch64-none-linux-gnu/libc/lib64/libc.so.6 lib64

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Compiling writer utility using make make CROSS_COMPILE=$CROSS_COMPILE"
cd $CURR_PATH

if [ -f writer.o || -f writer ]; then
    make clean
fi
make CROSS_COMPILE=$CROSS_COMPILE

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer $OUTDIR/rootfs/home
cp finder.sh $OUTDIR/rootfs/home
cp finder-test.sh $OUTDIR/rootfs/home

mkdir $OUTDIR/rootfs/home/conf
cp conf/assignment.txt $OUTDIR/rootfs/home/conf
cp conf/username.txt $OUTDIR/rootfs/home/conf
cp autorun-qemu.sh $OUTDIR/rootfs/home

# TODO: Chown the root directory
cd $OUTDIR/rootfs
find . | cpio -H newc -ov --owner root:root > $OUTDIR/initramfs.cpio

# TODO: Create initramfs.cpio.gz
cd $OUTDIR
gzip -f initramfs.cpio
