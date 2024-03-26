### What we already know

- synchronization based on IPC has problems due to multi-core nodes
- we take for granted that the PDES algorithm described in [WWT](https://users.cs.duke.edu/~alvy/papers/p48-reinhardt.pdf) is correct
- we can exploit the TCP protocol to handle packet and their retransmission

### Preliminary steps

- network stability

### What we need to understand 

- how timers work in qemu
- how timers update in multi-core nodes (whether they advanced independetly from the numbers of cores or not)
- timers advance not in a fixed manner, therefore we need to think when we should stop the vm based on timer (the point is that, what should we do in case with pass the quanta?)

### What we need to devise

- a way to stop the qemu vm based on the quanta (see 4.1 of [WWT](https://users.cs.duke.edu/~alvy/papers/p48-reinhardt.pdf))

### Possible ideas

- virtual clock on distributed systems

### Raw Notes

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

