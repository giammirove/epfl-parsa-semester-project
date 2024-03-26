## QEMU

### Building from source

- Configure and compile.
```bash
./configure --target-list=aarch64-softmmu
make -j 8
```
!!! Note: we want to compile qemu from source to steal `qemu/build/pc-bios/edk2-x86_64-code.fd`
so check that it has been generated

### Alpine
- Download the latest aarch64 image [here](https://alpinelinux.org/downloads).
- Create a qcow2 disk.
```bash
qemu-img create -f qcow2 alpine.qcow2 8G
```
- Proceed to create a UEFI firmware image for arm64 architecture using QEMU's provided tools.
```bash
truncate -s 64m efi.img
truncate -s 64m varstore.img
dd if=/usr/share/edk2/aarch64/QEMU_EFI.fd of=efi.img conv=notrunc
```
or using source compiled bios (I could make it working [see](#running-qemu-with-bios-option))
```bash
truncate -s 64m efi.img
truncate -s 64m varstore.img
dd if=$QEMU_SRC/build/pc-bios/edk2-x86_64-code.fd of=efi.img conv=notrunc
```
- Launch the installation
```bash
qemu-system-aarch64 -nographic -machine virt,gic-version=max -m 2G -cpu max -smp 2 \
-drive file=efi.img,if=pflash,format=raw \
-drive file=varstore.img,if=pflash,format=raw \
-drive file=alpine.qcow2,if=virtio,format=qcow2 \ 
-cdrom alpine.iso
```

Using self created alpine image:
```bash
qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \                 
        -bios $QEMU_SRC/build/pc-bios/edk2-aarch64-code.fd -serial telnet::4444,server -nographic \
        -drive if=none,file=alpine-prebaked.qcow2,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -device virtio-net-device,netdev=net0 \
        -netdev user,id=net0
```

### Busybox

#### Linux
```bash
make O=build_aarch64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig -j8
make O=build_aarch64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
```

#### Busybox
```bash
make O=build_aarch64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
```
Then select
```
Busybox Settings / Build Options / Build Busbox as a static binary (no shared libs) / yes
```
Build and install busybox
```bash
make O=build_aarch64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
make O=build_aarch64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- install -j8
```

#### Initramfs
Create an initramsfs
```bash
mkdir -p initramfs
cd initramfs
mkdir -p bin sbin etc proc sys usr/bin usr/sbin
cp -a ../busybox/build_aarch64/_install/* .
```
Write a simple init script
```bash
#!/bin/sh

mount -n -t tmpfs none /dev
mknod -m 622 /dev/console c 5 1
mknod -m 666 /dev/null c 1 3
mknod -m 666 /dev/zero c 1 5
mknod -m 666 /dev/ptmx c 5 2
mknod -m 666 /dev/tty c 5 0 # <-- mknod -m 444 /dev/random c 1 8
mknod -m 444 /dev/urandom c 1 9
chown root:tty /dev/{console,ptmx,tty}
mknod -m 666 /dev/ttyS0 c 4 64
mount -t proc none /proc
mount -t sysfs none /sys

cat <<!

Boot took $(cut -d' ' -f1 /proc/uptime) seconds

Welcome to giammi linux

!
setsid  cttyhack sh
exec /bin/sh 
```
Create the initramfs archive
```bash
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs_aarch64.cpio.gz
```

### Possible sources of virtual clock

- Instruction count
- QEMU Timers
- ARM64 register (PMCCNTR_EL0 or CNTPCT)
- Distributed algorithms

#### Instruction count

Instruction count seems to be the perfect candidate for being the virtual clock.
It is indipendent from the host and it updates only while the guest is running,
moreover instruction count is an integer, that means we can "split time" in a 
discrete manner and always know what happend after or before.
The major concern about instruction count is that from previous tests it appeard
to increment not using a single step, but its step increment depends on the number 
of vcpu we use (`-smp`).

From qemu documentation (see [here](https://www.qemu.org/docs/master/system/qemu-manpage.html))
```
-icount [shift=N|auto][,align=on|off][,sleep=on|off][,rr=record|replay,rrfile=filename[,rrsnapshot=snapshot]]

    Enable virtual instruction counter. The virtual cpu will execute one instruction every 2^N ns of virtual time. If auto is specified then the virtual cpu speed will be automatically adjusted to keep virtual time within a few seconds of real time.

    Note that while this option can give deterministic behavior, it does not provide cycle accurate emulation. Modern CPUs contain superscalar out of order cores with complex cache hierarchies. The number of instructions executed often has little or no correlation with actual performance.

    When the virtual cpu is sleeping, the virtual time will advance at default speed unless sleep=on is specified. With sleep=on, the virtual time will jump to the next timer deadline instantly whenever the virtual cpu goes to sleep mode and will not advance if no timer is enabled. This behavior gives deterministic execution times from the guest point of view. The default if icount is enabled is sleep=off. sleep=on cannot be used together with either shift=auto or align=on.
```

But how precise can we be in stopping the simulation at a specific instruction count? 
The official QEMU source code provides us with some tests, among them `tests/plugin/insn.c` seems
to be exactly what we need. It indeed count the number of instruction executed by QEMU triggering
a callback the moment before an instruction is executed (in theory).
That plugin works with or without `-icount` flag passed to QEMU.

| SMP | CORES | TARGET     | 1. QEMU ADD  | 1. MY ADD  | 1. FLAGS      | 2. QEMU ADD | 2. MY ADD  | 2. FLAGS                           |
|-----|-------|------------|--------------|------------|---------------|-------------|------------|------------------------------------|
| 1   | 1     | 1232203923 | 1232203922   | 1232203923 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 1   | 8     | 1232203923 | 1232203922   | 1232203923 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 1   | 16    | 1232203923 | 1232203922   | 1232203923 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 8   | 1     | 1232203923 | 1232203922   | 1232203924 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 8   | 8     | 1232203923 | 1232203922   | 1232203923 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 8   | 16    | 1232203923 | 1232203922   | 1232203925 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 16  | 1     | 1232203923 | 1232203922   | 1232203926 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 16  | 8     | 1232203923 | 1232203922   | 1232203924 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 16  | 16    | 1232203923 | 1232203923   | 1232203926 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 32  | 1     | 1232203923 | 1232203923   | 1232203926 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 32  | 8     | 1232203923 | 1232203923   | 1232203926 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 32  | 16    | 1232203923 | 1232203922   | 1232203925 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 64  | 1     | 1232203923 | 1232203881   | 1232203931 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 64  | 8     | 1232203923 | 1232203920   | 1232203927 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 64  | 16    | 1232203923 | 1232203908   | 1232203930 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 128 | 1     | 1232203923 | 1232203923   | 1232203936 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 128 | 8     | 1232203923 | 1232203926*  | 1232203940 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |
| 128 | 16    | 1232203923 | 1232203920   | 1232203930 | -rtc clock=vm | 1232203922  | 1232203923 | -icount shift=0,sleep=on,align=off |


Reason why using `icount` could not be a good idea:
https://github.com/zephyrproject-rtos/zephyr/issues/26868
```
@wentongwu
I think icount mode doesn’t work well on qemu_x86_64 multi core, because QEMU
can’t provide accurate timer on multi core platform. And QEMU can’t provide true physical concurrency.
```
Even though it is just a comment, it is to be considered while evaluating possibilities

#### Timers

In QEMU there are 4 timers (see [timers.h](https://github.com/qemu/qemu/blob/v4.2.0/include/qemu/timer.h#L16)):
- QEMU_CLOCK_REALTIME: Real time clock
- QEMU_CLOCK_VIRTUAL: virtual clock
- QEMU_CLOCK_HOST: host clock
- QEMU_CLOCK_VIRTUAL_RT: realtime clock used for icount warp

Activity while QEMU is suspended
- QEMU_CLOCK_REALTIME: active
- QEMU_CLOCK_VIRTUAL: not active
- QEMU_CLOCK_HOST: active
- QEMU_CLOCK_VIRTUAL_RT: not active (this a bit special)

Looking at the source code behind QEMU_CLOCK_VIRTUAL:
```c
/* qemu-timer.c */
int64_t qemu_clock_get_ns(QEMUClockType type)
{
    switch (type) {
    case QEMU_CLOCK_REALTIME:
        return get_clock();
    default:
    case QEMU_CLOCK_VIRTUAL:
        return cpus_get_virtual_clock();
    case QEMU_CLOCK_HOST:
        return REPLAY_CLOCK(REPLAY_CLOCK_HOST, get_clock_realtime());
    case QEMU_CLOCK_VIRTUAL_RT:
        return REPLAY_CLOCK(REPLAY_CLOCK_VIRTUAL_RT, cpu_get_clock());
    }
}
/* cpus.c */
int64_t cpus_get_virtual_clock(void)
{
    if (cpus_accel && cpus_accel->get_virtual_clock) {
        return cpus_accel->get_virtual_clock();
    }
    return cpu_get_clock();
}
/*
    get_virtual_clock() is only used when -icount flag is passed to qemu
    get_virtual_clock() and icount should holds the same value, see below
*/
/* tcg-accel-ops.c */
static void tcg_accel_ops_init(AccelOpsClass *ops)
{
    /* ... */
    /* -icount flag */
    if (icount_enabled()) {
      ops->handle_interrupt = icount_handle_interrupt;
      ops->get_virtual_clock = icount_get;
      ops->get_elapsed_ticks = icount_get;
    }
    /* ... */
}
/* cpu-timers.c */
int64_t cpu_get_clock(void)
{
    int64_t ti;
    unsigned start;

    do {
        start = seqlock_read_begin(&timers_state.vm_clock_seqlock);
        ti = cpu_get_clock_locked();
    } while (seqlock_read_retry(&timers_state.vm_clock_seqlock, start));

    return ti;
}
/* cpu-timers.c */
int64_t cpu_get_clock_locked(void)
{
    int64_t time;

    time = timers_state.cpu_clock_offset;
    if (timers_state.cpu_ticks_enabled) {
        time += get_clock();
    }

    return time;
}
/* timer.h */
static inline int64_t get_clock(void)
{
    if (use_rt_clock) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000LL + ts.tv_nsec;
    } else {
        /* XXX: using gettimeofday leads to problems if the date
           changes, so it should be avoided. */
        return get_clock_realtime();
    }
}
/* timer.h */
static inline int64_t get_clock_realtime(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000000LL + (tv.tv_usec * 1000);
}
/* 
*  Linux man 
*    The functions clock_gettime() and clock_settime() retrieve and set the time of the specified clock clk_id.
*    CLOCK_MONOTONIC: Clock that cannot be set and represents monotonic time since some unspecified starting point.
*/
```
Regarding `clock_gettime()` the man reports also that:
```
The processors in an SMP system do not start all at exactly the same time and therefore the timer registers are typically running at an offset. Some architectures include code that attempts to limit these offsets on bootup. However, the code cannot guarantee to accurately tune the offsets. Glibc contains no provisions to deal with these offsets (unlike the Linux Kernel). Typically these offsets are small and therefore the effects may be negligible in most cases.
```


Benchmarks:
Type: Poweroff after boot                                  
Clock: QEMU_CLOCK_VIRTUAL
| SMP | CORES | Period | Mean   | Median | Minimum | Maximum | Count | Outliers                                                                                                          |
|-----|-------|--------|--------|--------|---------|---------|-------|-------------------------------------------------------------------------------------------------------------------|
| 1   |   1   | 500    | 500.00 | 500    | 500     | 500     | 18    | /                                                                                                                 |
| 10* |   1   | 500    | 510.47 | 500    | 500     | 1372    | 176   | /                                                                                                                 |
| 10  |   1   | 500    | 500.12 | 500    | 500     | 501     | 32    | /                                                                                                                 |
| 50  |   1   | 500    | 500.31 | 500    | 500     | 503     | 98    | 503                                                                                                               |
| 50  |   1   | 500    | 500.83 | 500    | 500     | 532     | 127   | 503, 504, 507, 532                                                                                                |
| 100 |   1   | 500    | 502.71 | 503    | 500     | 647     | 567   | 508, 509, 510, 513, 514, 516, 517, 520, 524, 533, 647                                                             |
| 100 |   1   | 500    | 503.80 | 503    | 500     | 570     | 793   | 511, 513, 514, 515, 516, 517, 518, 519, 520, 523, 525, 526, 527, 530, 533, 539, 540, 543, 548, 552, 556, 559, 570 |

Type: Simple benchmark                               
Clock: QEMU_CLOCK_VIRTUAL
| SMP | CORES | Period | Mean   | Median | Minimum | Maximum | Count | Outliers                                                                                                          |
|-----|-------|--------|--------|--------|---------|---------|-------|-------------------------------------------------------------------------------------------------------------------|
| 1   |   1   | 500    | 500.05 | 500    | 500     | 501     | 243   | /                                                                                                                 |
| 16  |   4   | 500    | 500.07 | 500    | 500     | 504     | 2594  | /                                                                                                                 |
| 32  |   4   | 500    | 500.12 | 500    | 500     | 501     | 32    | /                                                                                                                 |

#### ARM64 Register (PMCCNTR_EL0 or CNTPCT)

[PMCCNTR_EL0](https://developer.arm.com/documentation/ddi0595/2021-03/AArch64-Registers/PMCCNTR-EL0--Performance-Monitors-Cycle-Count-Register) seems to rely on the host ticks
```c
/* helper.c */
    {
        .name = "PMCCNTR_EL0",
        .state = ARM_CP_STATE_AA64,
        .opc0 = 3,
        .opc1 = 3,
        .crn = 9,
        .crm = 13,
        .opc2 = 0,
        .access = PL0_RW,
        .accessfn = pmreg_access_ccntr,
        .fgt = FGT_PMCCNTR_EL0,
        .type = ARM_CP_IO,
        .fieldoffset = offsetof(CPUARMState, cp15.c15_ccnt),
        .readfn = pmccntr_read,
        .writefn = pmccntr_write,
        .raw_readfn = raw_read,
        .raw_writefn = raw_write,
    },
/* helper.c */
static uint64_t cycles_get_count(CPUARMState *env)
{
#ifndef CONFIG_USER_ONLY
  return muldiv64(qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL), ARM_CPU_FREQ,
                  NANOSECONDS_PER_SECOND);
#else
  return cpu_get_host_ticks();
#endif
}
/* helper.c */
static inline int64_t cpu_get_host_ticks(void)
{
    uint32_t low,high;
    int64_t val;
    asm volatile("rdtsc" : "=a" (low), "=d" (high));
    val = high;
    val <<= 32;
    val |= low;
    return val;
}
```

[CNTPCT](https://developer.arm.com/documentation/ddi0601/2023-12/External-Registers/CNTPCT--Counter-timer-Physical-Count) seems to rely on the QEMU_CLOCK_VIRTUAL, so it should have all its problems
```c
/* helper.c */
    {
        .name = "CNTPCT",
        .cp = 15,
        .crm = 14,
        .opc1 = 0,
        .access = PL0_R,
        .type = ARM_CP_64BIT | ARM_CP_NO_RAW | ARM_CP_IO,
        .accessfn = gt_pct_access,
        .readfn = gt_cnt_read,
        .resetfn = arm_cp_reset_ignore,
    },
/* helper.c */
static uint64_t gt_cnt_read(CPUARMState *env, const ARMCPRegInfo *ri)
{
  return gt_get_countervalue(env) - gt_phys_cnt_offset(env);
}
/* helper.c */
static uint64_t gt_get_countervalue(CPUARMState *env)
{
  ARMCPU *cpu = env_archcpu(env);

  return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) / gt_cntfrq_period_ns(cpu);
}
```
https://developer.arm.com/documentation/ddi0601/2023-12/External-Registers/CNTPCT--Counter-timer-Physical-Count

#### Distributed Algorithms

A Virtual Time System for Virtualization-Based Network Emulations and Simulations [[1](https://naoki-tanaka.com/pubs/ZNJT_JOS12.pdf)]
- running VEs instead of QEMU nodes
- emulation progress is controlled by a central application (Sim/Control)
- the virtual clock of a VE is advancing when it is executing on the CPU
- Sim/Control controls the network and deliver packets at the correct time
- it divides the execution in emulated cycles:
  - all VEs are blocked
  - Sim/Control collects all events made by VEs in the previous cycle
  - Sim/Control pushes all due events to VEs
  - Sim/Control decides which VEs can have timeslices in the current cycle

From that paper we see that we do not need to define a global common clock if we are able
to control the network and use a centralized server to dispatch packets.
Which clearly is an "easy" solution, but will make the central server the bottleneck of 
the whole system.
Moreover, since we have a functioning PDES in which we have barriers every T time units (quanta),
in that synchronization moment we can share all the information needed for the central server to 
compute every virtual clock and determine the correct order of the events.

Virtual Time and Global States of Distributed Systems [[2]](http://courses.csail.mit.edu/6.852/01/papers/VirtTime_GlobState.pdf)
- clock-vectors to model the concept of time and causality
- formally defined system of lattices (with proofs)
- definition of concept of time (based on 5 axioms)


