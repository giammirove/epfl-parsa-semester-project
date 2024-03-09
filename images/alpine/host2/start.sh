qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \
        -bios /home/giammi/qflex/qflex/src/qemu/build/pc-bios/edk2-aarch64-code.fd -serial telnet::4445,server -nographic \
        -drive if=none,file=alpine.qcow2,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        # -netdev bridge,id=hn2 -device virtio-net,netdev=hn2,mac=e6:c8:ff:09:76:9c
        -netdev tap,id=hn2 -device virtio-net,netdev=hn2,ifname=tap1,script=no,downscript=no,mac=e6:c8:ff:09:76:9c
        # -device e1000,netdev=net0 \
        # -netdev tap,id=net0,ifname=tap1,script=no,downscript=no

