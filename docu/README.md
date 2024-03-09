### QEMU

#### Building from source

- Configure and compile.
```
./configure --target-list=aarch64-softmmu
make -j 8
```
!!! Note: we want to compile qemu from source to steal `qemu/build/pc-bios/edk2-x86_64-code.fd`
so check that it has been generated

#### Running qemu with bios option
Tried to use prebaked debian image and ran using the qemu's bios compiled from
source (edk2-aarch64-code.fd)
```
qemu-system-aarch64 -machine virt -m 2G -cpu max -smp 1 \
-bios [path to QEMU build dir]/pc-bios/edk2-aarch64-code.fd \
-drive file=[pre-baked file],if=virtio,format=qcow2
```
but got this error:
```
BdsDxe: failed to load Boot0001 "UEFI Misc Device" from PciRoot(0x0)/Pci(0x2,0x0): Not Found

>>Start PXE over IPv4.
  PXE-E16: No valid offer received.
BdsDxe: failed to load Boot0002 "UEFI PXEv4 (MAC:525400123456)" from PciRoot(0x0)/Pci(0x1,0x0)/MAC(525400123456,0x1)/IPv4(0.0.0.0,0x0,DHCP,0.0.0.0,0.0.0.0,0.0.0.0): Not Found

>>Start PXE over IPv6.
  PXE-E16: No valid offer received.
BdsDxe: failed to load Boot0003 "UEFI PXEv6 (MAC:525400123456)" from PciRoot(0x0)/Pci(0x1,0x0)/MAC(525400123456,0x1)/IPv6(0000:0000:0000:0000:0000:0000:0000:0000,0x0,Static,0000:0000:0000:0000:0000:0000:0000:0000,0x40,0000:0000:0000:0000:0000:0000:0000:0000): Not Found
```

Using self created alpine image it works
```
qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \                 
        -bios $QEMU_SRC/build/pc-bios/edk2-aarch64-code.fd -serial telnet::4444,server -nographic \
        -drive if=none,file=alpine-prebaked.qcow2,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -device virtio-net-device,netdev=net0 \
        -netdev user,id=net0
```

#### Alpine
- Download the latest aarch64 image (here)[https://alpinelinux.org/downloads].
- Create a qcow2 disk.
```
qemu-img create -f qcow2 alpine.qcow2 8G
```
- Proceed to create a UEFI firmware image for arm64 architecture using QEMU's provided tools.
```
truncate -s 64m efi.img
truncate -s 64m varstore.img
dd if=/usr/share/edk2/aarch64/QEMU_EFI.fd of=efi.img conv=notrunc
```
or using source compiled bios (I could make it working [se](#running-qemu-with-bios-option))
```
truncate -s 64m efi.img
truncate -s 64m varstore.img
dd if=$QEMU_SRC/build/pc-bios/edk2-x86_64-code.fd of=efi.img conv=notrunc
```
- Launch the installation
```
qemu-system-aarch64 -nographic -machine virt,gic-version=max -m 2G -cpu max -smp 2 \
-drive file=efi.img,if=pflash,format=raw \
-drive file=varstore.img,if=pflash,format=raw \
-drive file=alpine.qcow2,if=virtio,format=qcow2 \ 
-cdrom alpine.iso
```

### GNS3

(GNS3)[https://docs.gns3.com/docs/] allows to use qemu images as devices (great!)

### NS3

(NS3 Tap Bridge)[https://www.nsnam.org/docs/release/3.10/doxygen/group___tap_bridge_model.html]

To setup the Tap Bridge:
```
sudo ip link add br0 type bridge
sudo ip addr add 10.0.0.1/24 dev br0
sudo ip link set br0 up
echo 'allow br0' | sudo tee -a /etc/qemu/bridge.conf
sudo ip tuntap add dev tap0 mode tap user $(whoami)
sudo ip addr add 10.0.0.100/24 dev tap0
sudo ip link set tap0 up
sudo ip link set tap0 promisc on
sudo ip link set tap0 master br0
sudo ip tuntap add dev tap1 mode tap user $(whoami)
sudo ip addr add 10.0.0.101/24 dev tap1
sudo ip link set tap1 up
sudo ip link set tap1 promisc on
sudo ip link set tap1 master br0
```

Start the QEMU with flags 
```
-netdev tap,id=hn1 -device virtio-net,netdev=hn1,ifname=tap0,script=no,downscript=no,mac=e6:c8:ff:09:76:99
```

Inside the VM 
```
ip addr add 10.0.0.101 dev eth0
```


### Useful links

https://insights.sei.cmu.edu/blog/how-to-use-docker-and-ns-3-to-create-realistic-network-simulations/
https://futurewei-cloud.github.io/ARM-Datacenter/qemu/network-aarch64-qemu-guests/


2: enp0s31f6: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether 64:00:6a:8b:32:3e brd ff:ff:ff:ff:ff:ff
    inet 128.178.116.194/24 brd 128.178.116.255 scope global dynamic noprefixroute enp0s31f6
       valid_lft 2222sec preferred_lft 2222sec
    inet6 fe80::f5b1:f0bb:633e:c170/64 scope link noprefixroute 
       valid_lft forever preferred_lft forever
3: wlp2s0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc noqueue state DOWN group default qlen 1000
    link/ether be:1a:c8:9e:c3:0b brd ff:ff:ff:ff:ff:ff permaddr e4:b3:18:08:9a:17
