#
# Automatically generated file. Don't edit
#
_CONFIG_MK_=1
ARCH=x86
PLATFORM=pc
PROFILE=pc
_GNUC_=1
CC=gcc-4.1
CPP=cpp
AS=as
LD=ld
AR=ar
OBJCOPY=objcopy
OBJDUMP=objdump
STRIP=strip
GCCFLAGS+= -fno-stack-protector
GCCFLAGS+= -march=i386 -O2
GCCFLAGS+= -mpreferred-stack-boundary=2
CONFIG_LOADER_TEXT=0x00004000
CONFIG_KERNEL_TEXT=0x80010000
CONFIG_BOOTIMG_BASE=0x80100000
CONFIG_SYSPAGE_BASE=0x80000000
CONFIG_HZ=1000
CONFIG_TIME_SLICE=50
CONFIG_OPEN_MAX=16
CONFIG_BUF_CACHE=32
CONFIG_FS_THREADS=4
CONFIG_I386=y
CONFIG_MMU=y
CONFIG_CACHE=y
CONFIG_BOOTDISK=y
CONFIG_POSIX=n
CONFIG_CMDBOX=n
CONFIG_KD=y
CONFIG_DIAG_BOCHS=n
CONFIG_FIFOFS=y
CONFIG_DEVFS=y
CONFIG_RAMFS=y
CONFIG_ARFS=y
CONFIG_FATFS=y
CONFIG_PM=y
CONFIG_PM_PERFORMANCE=y
CONFIG_DVS_EMULATION=y
CONFIG_PM=y
CONFIG_I8237=y
CONFIG_CONS=y
CONFIG_WSCONS=y
CONFIG_PCKBD=y
CONFIG_VGA=y
CONFIG_CPUFREQ=y
CONFIG_EST=y
CONFIG_RTC=y
CONFIG_MC146818=y
CONFIG_NULL=y
CONFIG_ZERO=y
CONFIG_RAMDISK=y
CONFIG_FDD=y
CONFIG_NS16550_BASE=0x3f8
CONFIG_NS16550_IRQ=4
CONFIG_MC146818_BASE=0x70
CONFIG_CMD_CAT=n
CONFIG_CMD_CLEAR=n
CONFIG_CMD_CP=n
CONFIG_CMD_DATE=n
CONFIG_CMD_DMESG=n
CONFIG_CMD_ECHO=n
CONFIG_CMD_FREE=n
CONFIG_CMD_HEAD=n
CONFIG_CMD_HOSTNAME=n
CONFIG_CMD_KILL=n
CONFIG_CMD_LS=n
CONFIG_CMD_MKDIR=n
CONFIG_CMD_MORE=n
CONFIG_CMD_MV=n
CONFIG_CMD_NICE=n
CONFIG_CMD_PRINTENV=n
CONFIG_CMD_PS=n
CONFIG_CMD_PWD=n
CONFIG_CMD_RM=n
CONFIG_CMD_RMDIR=n
CONFIG_CMD_SH=n
CONFIG_CMD_SLEEP=n
CONFIG_CMD_SYNC=n
CONFIG_CMD_TOUCH=n
CONFIG_CMD_UNAME=n
CONFIG_CMD_DISKUTIL=n
CONFIG_CMD_INSTALL=n
CONFIG_CMD_PMCTRL=n
CONFIG_CMD_KTRACE=n
CONFIG_CMD_LOCK=n
CONFIG_CMD_DEBUG=n