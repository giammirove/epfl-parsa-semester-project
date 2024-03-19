qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \
        -bios /home/xgampx/Desktop/uni/epfl/semester/qflex/qemu/build/pc-bios/edk2-aarch64-code.fd \
        -drive if=none,file=alpine.qcow2,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -netdev tap,id=user1,ifname=tap1,script=no,downscript=no \
        -device virtio-net,netdev=user1,mac=e6:c8:ff:10:76:99 \
        -serial telnet::4445,server -nographic 

        # -netdev bridge,id=user1,br=br1 \

