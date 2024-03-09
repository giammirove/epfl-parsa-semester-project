qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \
        -bios /home/giammi/qflex/qflex/src/qemu/build/pc-bios/edk2-aarch64-code.fd -serial telnet::4444,server -nographic \
        -drive if=none,file=alpine.qcow2,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        # -netdev bridge,id=hn1 -device virtio-net,netdev=hn1,mac=e6:c8:ff:09:76:99
        -netdev tap,id=hn1 -device virtio-net,netdev=hn1,ifname=tap0,script=no,downscript=no,mac=e6:c8:ff:09:76:99
        # -device virtio-net-device,netdev=net0 \
        # -netdev tap,id=net0,ifname=tap0,script=no,downscript=no

# $ sudo qemu-system-i386 -cdrom Core-current.iso -boot d -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no -device e1000,netdev=mynet0,mac=52:55:00:d1:55:01
