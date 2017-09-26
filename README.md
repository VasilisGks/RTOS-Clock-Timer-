# RTOS-Clock-Timer- Code in usr/sample
<br>Tested in x86 Debian
<br>Download Prex RTOS (https://sourceforge.net/projects/prex/files/)
<br>Using gcc http://archive.debian.org/debian/pool/main/g/gcc-4.1/
<br>In prex-os directory run :  ./configure --target=x86-pc (if working on x86 linux with GNU gcc compiler)
<br>After downloading prexos floppy img download mtools .Type in terminal : sudo apt-get install mtools
<br>drive a: file="(path-to-img)/prex-0.8.0.i386-pc.img"
<br>mcopy -o prexos.a
<br> Install QEMU from apt repository
<br> Test :
<br> make clean 
<br> make all
<br> qemu-system-i386 –fda ./prex-0.8.0.i386-pc-img
