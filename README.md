# bettercpu (v1.0.1)
Adaptive real-time tuning of CPU frequency scaling, thermal limits, I/O schedulers, and memory pressure on Linux systems. Written by Christian (@pusheandoando)





## What it does
bettercpu runs as a background daemon that samples system state on a short interval and adjusts kernel-level tunables in response, then restores the original values on stop. It does not implement new algorithms; instead it applies well-documented Linux kernel mechanisms and operational practices, each described below alongside the source that documents the technique.





## How to use it
bettercpu is controlled entirely through a single command with subcommands. Some subcommands change system-level settings and require root privileges, so they must be run with `sudo`.

### General options
- `bettercpu -h` or `bettercpu --help`: Prints the list of available commands and a short description of each one.
- `bettercpu -v` or `bettercpu --version`: Prints the installed version of bettercpu.

### Starting the daemon
```
sudo bettercpu start
```

This starts the adaptive tuning daemon in the background. Before starting, bettercpu shows a confirmation prompt explaining that it is about to apply changes to your CPU, thermal, I/O, and memory settings, and that these changes take effect immediately. Type `y` and press Enter to continue, or `n` to cancel.

The first time it starts, bettercpu also registers itself as a systemd service so that tuning resumes automatically after a reboot. The original values of every setting it touches are saved beforehand, so nothing is lost.

### Stopping the daemon
```
sudo bettercpu stop
```

This stops the running daemon and restores every setting it changed back to its original value. It also removes the automatic startup registration created by `start`.

### Checking status
```
bettercpu status
```

Shows whether the daemon is currently running. This command does not require root privileges.

### Removing all generated files
```
sudo bettercpu clean
```

Deletes every file that bettercpu created on the system, such as saved state and its systemd service registration. The daemon must be stopped first with `sudo bettercpu stop`, otherwise this command will refuse to run.

### Managing running programs and processes
The `p` command lists the programs and processes currently running on the system, grouped by program name and sorted by CPU usage from highest to lowest.

```
sudo bettercpu p
```

Each program is shown with a temporary id in brackets, like `[0]`, `[1]`, and so on, followed by how many processes belong to it and its combined CPU usage. Underneath each program, its individual processes are listed in a small table, each with its own temporary id such as `p0`, `p1`, and so on, along with its name and CPU usage.

To act on a specific process instead of the whole program, combine the program id and the process id using the format `NpM`. For example, `0p1` refers to process `p1` that belongs to program `[0]`.

Additional options for `p`:

- `bettercpu p -h` or `bettercpu p --help`: Shows help specific to the `p` command.
- `sudo bettercpu p -l` or `sudo bettercpu p --list`: Lists all running programs and processes. This is the same as running `bettercpu p` with no options.
- `sudo bettercpu p -k <id>` or `sudo bettercpu p --kill <id>`: Terminates a program or process. Use a plain number like `0` to terminate an entire program and all of its processes, or use the `NpM` format like `0p1` to terminate only that specific process.
- `sudo bettercpu p -r <a:b>` or `sudo bettercpu p --range <a:b>`: Limits the listing to only the programs whose id falls between `a` and `b`, inclusive. For example, `sudo bettercpu p -r 0:4` shows only programs `[0]` through `[4]`.





## What it does (technical details)
### CPU frequency governor selection (schedutil)
On start, bettercpu sets the `schedutil` cpufreq governor on every online core. `schedutil` integrates directly with the CFS scheduler's Per-Entity Load Tracking (PELT) data instead of polling utilization on a fixed timer like `ondemand`, which lets it react to load changes with less lag.

- Linux Kernel Documentation, "CPU frequency and voltage scaling code in the Linux kernel": https://www.kernel.org/doc/Documentation/cpu-freq/governors.txt
- Linux Kernel Documentation, "Schedutil": https://docs.kernel.org/scheduler/schedutil.html
- Linux kernel commit introducing schedutil: https://github.com/torvalds/linux/commit/9bdcb44e391da5c41b98573bf0305a0e0b1c9569

### Energy Performance Preference (EPP) hints
Under load classification, bettercpu writes to `energy_performance_preference` in each core's cpufreq directory, shifting the hardware's internal P-state selection between a performance-favoring value and a power-saving value depending on measured load. This is the standard sysfs interface exposed by the `intel_pstate` driver (and mirrored by `amd_pstate`) when Hardware-Managed P-States (HWP) is enabled.

- Linux Kernel Documentation, "intel_pstate CPU Performance Scaling Driver": https://docs.kernel.org/admin-guide/pm/intel_pstate.html
- Linux Kernel Documentation, "CPU Performance Scaling": https://docs.kernel.org/admin-guide/pm/cpufreq.html
- Lenovo Press, "Using Processor Performance P-States with Linux on Intel-based ThinkSystem Servers": https://lenovopress.lenovo.com/lp1946-using-processor-performance-p-states-with-linux-on-intel-based-servers
- ArchWiki, "CPU frequency scaling": https://wiki.archlinux.org/title/CPU_frequency_scaling

### Thermal-aware frequency capping
bettercpu polls `/sys/class/thermal/thermal_zone*/temp` and reduces `scaling_max_freq` incrementally when a zone crosses a passive trip threshold, restoring the original ceiling once temperatures recover below a hysteresis point. This mirrors the passive-cooling behavior of the kernel's built-in `step_wise` thermal governor, which throttles cooling devices such as cpufreq incrementally rather than in a single step.

- Linux Kernel Documentation, "Generic Thermal Sysfs driver How To": https://docs.kernel.org/driver-api/thermal/sysfs-api.html
- Linux Kernel Documentation, "Power allocator governor tunables": https://docs.kernel.org/driver-api/thermal/power_allocator.html

### Memory pressure monitoring via PSI
bettercpu reads `/proc/pressure/memory` and classifies pressure using the `avg10` field, following the Pressure Stall Information (PSI) interface introduced by Johannes Weiner and merged into Linux 4.20. PSI reports lost wall-clock time due to resource contention, which is a more direct saturation signal than the traditional load average.

- Linux Kernel Documentation, "PSI - Pressure Stall Information": https://docs.kernel.org/accounting/psi.html
- Meta Engineering, "Open-sourcing oomd, a new approach to handling OOMs": https://engineering.fb.com/2018/07/19/production-engineering/oomd/

### Userspace-triggered graceful termination under memory pressure
When memory pressure crosses the critical threshold, bettercpu identifies the largest RSS consumer via `/proc/[pid]/status` and sends it `SIGTERM` before the kernel's own OOM killer would engage. This follows the same rationale as Facebook's `oomd`: acting in userspace, ahead of the kernel OOM killer, avoids the extended livelock that can occur while the kernel is still reclaiming pages.

- facebookincubator/oomd README: https://github.com/facebookincubator/oomd/blob/main/README.md
- Meta Engineering, "Open-sourcing oomd, a new approach to handling OOMs": https://engineering.fb.com/2018/07/19/production-engineering/oomd/

### VFS cache pressure and dirty page ratio tuning
Under rising (non-critical) memory pressure, bettercpu increases `vm.vfs_cache_pressure` and reduces `vm.dirty_ratio`, causing the kernel to reclaim inode and dentry caches more aggressively and to start synchronous writeback sooner. Both sysctls and their effects are documented directly by the kernel.

- Linux Kernel Documentation, "Documentation for /proc/sys/vm/": https://docs.kernel.org/admin-guide/sysctl/vm.html

### I/O scheduler selection by device class
On start, and continuously while under moderate-or-higher load, bettercpu inspects each block device's rotational flag and sets `none` for NVMe devices, `mq-deadline` for HDDs and SATA SSDs. This matches Red Hat's documented defaults: NVMe devices already reorder and queue requests internally, so an additional software scheduler mostly adds overhead, while rotational and SATA-attached devices benefit from the request merging and read-priority behavior that `mq-deadline` provides.

- Red Hat Enterprise Linux 9 Documentation, "Setting the disk scheduler": https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/9/html/monitoring_and_managing_system_status_and_performance/setting-the-disk-scheduler_monitoring-and-managing-system-status-and-performance
- Red Hat Customer Portal, "Suggested I/O scheduler": https://access.redhat.com/solutions/5427

### NVMe latency target tuning
When an NVMe device is under the `none` scheduler and has fewer than a small number of requests in flight, bettercpu writes to the device's `read_lat_nsec` and `write_lat_nsec` queue attributes to hint an expected completion latency to the underlying I/O scheduling logic. This uses the same sysfs attributes documented for the NVMe block layer's latency-based hints.

- Red Hat Enterprise Linux 8 Documentation, "Setting the disk scheduler": https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/8/html/managing_storage_devices/setting-the-disk-scheduler_managing-storage-devices

### CPU topology and hybrid core detection
bettercpu reads `/sys/devices/system/cpu/cpu*/topology/` to determine core and package IDs, SMT sibling relationships, and per-core capacity, which lets it distinguish physical cores from SMT siblings and detect hybrid (performance/efficiency) core layouts before applying per-core policy. This follows the standard CPU topology sysfs layout exposed by the Linux kernel for all multi-core and hybrid systems.

- Linux Kernel Documentation, "CPU Performance Scaling": https://docs.kernel.org/admin-guide/pm/cpufreq.html

### State backup and restoration
Before applying any tuning, bettercpu snapshots the pre-existing governor, frequency limits, EPP value, swappiness, cache pressure, dirty ratios, and I/O scheduler for every core and device, and writes that snapshot to `/var/lib/bettercpu/state_backup.dat`. On stop, it reads the snapshot back and restores every value, so the daemon never leaves the system in a modified state after exiting. This is a standard operational safeguard for any tool that mutates live kernel tunables.