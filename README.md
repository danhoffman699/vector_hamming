# Vectorized Hamming Codes

## Overview

Operates on 4KB memory pages.

Computes Hamming codes as stripes of bits by operating on a casted 128-bit array (vertical)

* cache friendly
* fast (effectively 128-bit SIMD vs lots of 1-bit operations)
* positions in the Hamming code are far apart in physical RAM, minimizing double or worse error cases
* native function output is storage efficicent, as we have bitwise storage precision

Compute 128 Hamming codes from the Hamming codes just generated, and redundantly store the second round of Hamming codes 3x in a RAID I manner (will vote on which bits are valid, but for now we just sanity check two of the three are equal).

## Module

This is being implemented as a block device in Linux, so with the help of swapping and control groups, it can be the primary memory holding device, at a slight speed cost. I am building and testing this against Linux 4.14.9 (it's what I run, but it should backport to 3.2 as well), but any modern system should be able to compile this and use it properly given they have the headers and source code installed.

Also, since I am working at a pretty low level, kernel panics and hanging might happen, so make sure you aren't doing anything important when you decide to load this.

I hope to expand this out, so that this can be applied to hard drive parititons as well, but since traditionally hardware modifications and filesystems don't mix well unless done properly (btrfs and RAID I), I am focusing on memory-only for now.

You can run a control group setup that seriously restricts raw RAM usage by running 
```
kernel_module/swap.sh
```

You need root permissions to do this, and you must copy `kernel_module/update.sh` to the root of your filesystem (locally this has been implemented as a cron job, so any new tasks are added over time).

## Numbers

All of these benchmarks are based off of a Core 2 Duo T7200, since that's my burner laptop for this. There are some basic optimizations that can be done to increase any of these numbers.

### Raw

* 8.2% storage overhead
* Can correct up to 128 sequential bits of corruption in the payload and in the error correcting data itself
* Checking throughput of 1950MB/s

NOTE: 1950MB/s isn't completely accurate, since the benchmarking process caches that data. However, this should be the case for simple copies as well, so the marginal addition shouldn't force it to enter the cache.

### Module

Module benchmarks were done without any protection, so this is the raw performance of the structures (32 depth binary tree with 8 HDD sectors per child, will probably change soon).

* 150MB/s write speed
* 300MB/s read speed

## Plans

### Device Mapper Integration

The next major plan is to store and check redundancy information on another block device via the device mapper. The size of the disk exposed to userspace is the size of the actual disk, with a set amount taken off for dedicated redundancy data. As the redundancy data is stored at the end, this can be used on a boot parition as well, since the MBR and partition scheme would line up properly.

On any fresh binding to a device, it resets all correction data and assumes everything on the disk currently is correct (if the filesystem were to have been mounted normally, no error correcting data would have been logged, and remounting with new data would bring up false positives). As the downtime as a whole is rather small, this is a somewhat safe assumption. 

Chunks of blocks are stored which are one-bit flags (redundantly stored, of course) that assert whether or not redundancy data is valid for a certain chunk of sectors (mapped as offsets). Should actually log any new information to disk with a timestamp, so I don't waste time computing data to only be overridden soon, and not destroy the flash by writing more data than practically needed (some flash controllers do dynamic mapping between sectors and actual locations on the chip, so they evenly wear all all the storage. If this doesn't have that, the life of the chip can go down pretty darn fast. IIRC most SD cards have this).

This also entails a more abstract mapping between two devices, instead of just traversing a tree in memory. This makes the following points easier to implement in a more generic way.

### Idle Memory Scrubber

Idle memory scrubber that goes around and corrects data. That's about it for this one.

### BadRAM/memmap patching

If I spare a bit of computing time for new allocations, I can sanity check that the allocated page isn't suffering from a hardware error by some simple memory test. If this does fail, I would be interested in freeing this block, reporting the address of the problem are to BadRAM/memmap, and see if it's possible for that to take effect live. If this test is done for every page, the BadRAM/memmap system should reconstruct itself relatively easily, assuming these spots aren't encountered during normal use. However, for retarded-levels of preparation, it shouldn't be too hard make a userspace application to append this information to the kernel command line via GRUB/LILO (pulling addresses from the sysfs interface).

This same logic can apply to an idle memory scrubber as well.

This can also be applied to normal block devices as well, but we would have to either resize the hard drive on the fly, or create and store a mapping on the disk for sectors as seen by the OS to sectors past the end of the hard drive. Restoring data on a reboot through this system doesn't sound like fun, unless we can ensure that there are no actual writes to the block device, and it has been mounted read only before being remounted with the new driver.

All in all this looks like work, but it's really needed and very useful.

## TODO

add sysfs to kernel module

allow for userspace setting of the max block device size through the sysfs

allow for multiple instance of /dev/hamming, right now there's only one created at module initialization

enable Hamming correction and detection to module

create a background memory scrubber for the structure

maybe drop down to ARM assembly to do some optimized 128-bit operations (NEON SIMD is the only instruction set on satellte that does 128-bits AFAIK), not a priority

more benchmarking

possibly add a second parity bit to Hamming code sets, converting to a SECDED code (single error correction, double error detection), but what are the odds that we would have fried already at that point?
