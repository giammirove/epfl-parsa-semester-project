# qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \
#         -bios /home/giammi/qflex/qflex/src/qemu/build/pc-bios/edk2-aarch64-code.fd -serial telnet::4444,server -nographic \
#         -drive if=none,file=alpine.qcow2,id=hd0 \
#         -device virtio-blk-device,drive=hd0 \
#         -device virtio-net,netdev=hn1,ifname=tap0,script=no,downscript=no,mac=e6:c8:ff:09:76:99 \
#         -netdev tap,id=hn1 

qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \
        -bios /home/xgampx/Desktop/uni/epfl/semester/qflex/qemu/build/pc-bios/edk2-aarch64-code.fd \
        -drive if=none,file=alpine.qcow2,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -netdev tap,id=user0,ifname=tap0,script=no,downscript=no \
        -device virtio-net,netdev=user0,mac=e6:c8:ff:09:76:99 \
        -serial telnet::4444,server -nographic 

        # -netdev bridge,id=user0,br=br0 \
