
### GNS3

[GNS3](https://docs.gns3.com/docs/) allows to use qemu images as devices (great!)

### NS3

[NS3 Tap Bridge](https://www.nsnam.org/docs/release/3.10/doxygen/group___tap_bridge_model.html)

To setup the Tap Bridge (`scripts/create_tap.sh`,`scripts/destroy_tap.sh`):
```
sudo ip link add br0 type bridge
sudo ip link add br1 type bridge
sudo ip tuntap add mode tap tap0
sudo ip tuntap add mode tap tap1
sudo ip addr add 0.0.0.0 dev tap0
sudo ip addr add 0.0.0.0 dev tap1
sudo ip link set dev tap0 master br0
sudo ip link set dev tap1 master br1
sudo ip link set tap0 promisc on up
sudo ip link set tap1 promisc on up

# taps for ns3 
sudo ip link add br0 type bridge
sudo ip link add br1 type bridge
sudo ip tuntap add mode tap tap0-ns
sudo ip tuntap add mode tap tap1-ns
sudo ip addr add 0.0.0.0 dev tap0-ns
sudo ip addr add 0.0.0.0 dev tap1-ns
sudo ip link set dev tap0-ns master br0
sudo ip link set dev tap1-ns master br1
sudo ip link set tap0-ns promisc on up
sudo ip link set tap1-ns promisc on up

sudo ip link set dev br0 up
sudo ip link set dev br1 up
echo 'allow br0' | sudo tee -a /etc/qemu/bridge.conf
echo 'allow br1' | sudo tee -a /etc/qemu/bridge.conf
```
```
sudo ip link del tap0
sudo ip link del tap1
sudo ip link del br0 
sudo ip link del br1
```

Start the QEMU with flags 
```
-device virtio-net,netdev=hn0 \
-netdev tap,id=hn0,ifname=tap0,script=no,downscript=no,,mac=e6:c8:ff:00:76:99
```

Inside the VM 
```
ip addr add 10.0.0.1/24 dev eth0
ip addr add 10.0.0.2/24 dev eth0
```

To run ns3
```
./ns3 run "tap-csma-virtual-machine"
```
!!! The python version seems not to work (tested on two devices, both running arch linux)

### Network Benchmarks

#### Configuration #1

```
  VM0(10.0.0.1)     VM1(10.0.0.2)
   |                 |
  tap0              tap1
   |                 |
  br0               br1
   |                 |
tap0-ns           tap1-ns
   |                 |
|----------NS3----------|
```

```
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate
[  5]   0.00-10.05  sec   296 MBytes   247 Mbits/sec                  receiver
-----------------------------------------------------------
```
```
# on host 10.0.0.1, 120 seconds
hping3 --traceroute -V -1 10.0.0.2
# ... 
--- 10.0.0.2 hping statistic ---
677 packets tramitted, 677 packets received, 0% packet loss
round-trip min/avg/max = 1.6/5.8/9.7 ms
```
```
ping -w 120 10.0.0.2
# ... 
--- 10.0.0.2 ping statistics ---
120 packets transmitted, 120 packets received, 0% packet loss
round-trip min/avg/max = 1.487/2.611/4.363 ms
```

#### Configuration #2

```
  VM0(10.0.0.1)     VM1(10.0.0.2)
   |                 |
  tap0              tap1
   |                 |
|-------virtbr0--------|
```

!!! tap0 and tap1 are not manually generated but automatic created by qemu when
using `-netdev bridge,br=virtbr0`

```
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate
[  5]   0.00-10.02  sec  2.29 GBytes  1.96 Gbits/sec                  receiver
-----------------------------------------------------------
```
```
ping -w 120 10.0.0.2
# ... 
--- 10.0.0.2 ping statistics ---
120 packets transmitted, 120 packets received, 0% packet loss
round-trip min/avg/max = 1.620/2.350/3.219 ms
```
```
hping3 --traceroute -V -1 10.0.0.2
# ... 
--- 10.0.0.2 hping statistic ---
410 packets tramitted, 410 packets received, 0% packet loss
round-trip min/avg/max = 1.3/5.2/9.1 ms
```

!!! using iproute2 tools suite we can achieve some of the features of NS3, such
as network delay and packet loss
```
# 40 ms +- 10 ms
sudo tc qdisc add dev tap0 root netem delay 40ms 10ms
# packet loss
tc qdisc add dev tap0 root netem loss 0.1%
```

#### Considerations

Based on [Microsoft](https://docs.microsoft.com/en-us/skypeforbusiness/optimizing-your-network/media-quality-and-network-connectivity-performance) the target RTT is < 100ms, 
therefore both tested configurations grant a stable network in terms of RTT.
Even if configuration #1 has a lower bitrate (247 Mbits/sec) compared to configuration #2 (1.96 Gbits/sec),
it allows the programmer to have more control on the network, permitting to easily change parameters and topology.
It has to be taken in account that the guest device is aarch64 while the host is x86_64, that in some extent contributes
to slow down the network. Indeed even ping on the loopback address takes more than expected :
```
# ping on alpine device aarch64
ping -w 120 127.0.0.1
# ... 
--- 127.0.0.1 ping statistics ---
120 packets transmitted, 120 packets received, 0% packet loss
round-trip min/avg/max = 0.410/0.744/1.340 ms
```
```
# ping on debian device aarch64
ping -w 120 127.0.0.1
# ... 
--- 127.0.0.1 ping statistics ---
120 packets transmitted, 120 received, 0% packet loss, time 119301ms
rtt min/avg/max/mdev = 0.377/0.643/1.344/0.185 ms
```

So I tried to manually introduce a delay in the network using NS3:
```
ping -w 120 10.0.0.2
# ... 
--- 10.0.0.2 ping statistics ---
120 packets transmitted, 120 packets received, 0% packet loss
round-trip min/avg/max = 22.517/23.840/31.827 ms
```
```
hping3 -V -1 10.0.0.2
# ... 
--- 10.0.0.2 hping statistic ---
120 packets tramitted, 120 packets received, 0% packet loss
round-trip min/avg/max = 23.3/26.5/30.6 ms
```
!!! the max value in almost all the tests is given by the first ping
As we can see introducing a fixed delay minimum and average rrt are closer.

### Timing

https://access.redhat.com/documentation/it-it/red_hat_enterprise_linux/7/html/virtualization_deployment_and_administration_guide/chap-kvm_guest_timing_management

http://courses.csail.mit.edu/6.852/01/papers/VirtTime_GlobState.pdf

https://naoki-tanaka.com/pubs/ZNJT_JOS12.pdf

file:///home/xgampx/Downloads/A_Comparison_of_Two_Approaches_to_Parallel_Simulat.pdf

file:///home/xgampx/Downloads/concurrency00.pdf

http://cobweb.cs.uga.edu/~maria/pads/papers/p404-jefferson.pdf


### Useful links

https://insights.sei.cmu.edu/blog/how-to-use-docker-and-ns-3-to-create-realistic-network-simulations/
https://futurewei-cloud.github.io/ARM-Datacenter/qemu/network-aarch64-qemu-guests/
