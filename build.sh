#!/bin/bash
echo "Building QRTOS..."

nasm -f bin boot/boot.asm -o boot/boot.bin
echo "[OK] Bootloader assembled"

nasm -f elf32 kernel/entry.asm -o kernel/entry.o
echo "[OK] Entry compiled"

gcc -m32 -ffreestanding -fno-pie -nostdlib -nostdinc \
    -fno-builtin -fno-stack-protector -Os -I. \
    -c kernel/kernel.c -o kernel/kernel.o
echo "[OK] Kernel compiled"

gcc -m32 -ffreestanding -fno-pie -nostdlib -nostdinc \
    -fno-builtin -fno-stack-protector -Os -I. \
    -c fs/fas32q.c -o fs/fas32q.o
echo "[OK] FAS32Q compiled"

ld -m elf_i386 -T kernel/linker.ld \
   -o kernel/kernel.bin \
   --oformat binary \
   kernel/entry.o kernel/kernel.o fs/fas32q.o
echo "[OK] Kernel linked"

echo "Kernel size:"
ls -la kernel/kernel.bin

dd if=/dev/zero of=qrtos.img bs=512 count=2880 2>/dev/null
dd if=boot/boot.bin of=qrtos.img conv=notrunc bs=512 seek=0 2>/dev/null
dd if=kernel/kernel.bin of=qrtos.img conv=notrunc bs=512 seek=1 2>/dev/null
echo "[OK] Disk image created"

echo ""
echo "Build complete!"
echo "Run: qemu-system-i386 -drive format=raw,file=qrtos.img,if=floppy -boot a"
