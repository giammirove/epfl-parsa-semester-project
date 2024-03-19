

executing benchmarking on the networks (iperf or whatever)

mean rtt should be -+ 10%  to have a stable network
i can rely on tcp

flush the packet before arriving to qemu so that the network has to resend (TCP)
flush virtio-net buffers

how to stop the vm (timer tried but not good since based on IPC)

I should work at the level of qemu
so modify qemu to keep track of virtual time
not touching the simulated OS (that would violate any findings)

I can take for granted that the PDES is correct, we just need to make it works

