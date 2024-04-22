qemu-system-arm \
    -m 1024 \
    -smp 4 \
    -M vexpress-a9 \
    -kernel zImage \
    -initrd rootfs.cpio \
    -append "console=ttyAMA0" \
    -dtb vexpress-v2p-ca9.dtb \
    -net nic,model=lan9118 \
    -net user,hostfwd=tcp::1337-:1337 \
    -monitor /dev/null \
    -nographic

