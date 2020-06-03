# kernelhaxcode_3ds

Code meant to be used as a submodule that, given a kernel exploit, sets up the rest of the chain and ultimately executes an Arm9 payload in a clean environment.

## Usage

See [universal-otherapp](https://github.com/TuxSH/universal-otherapp) for an example.

## Technical details

We need to hook the firmlaunch procedure to eventually run safehax (1.0-11.2) or safehax (11.3) are leveraged to gain Arm9 code execution (see the `arm11` subfolder for that).

Our method aims to leave little to no room for undefined behavior.

* Prior to running this code, a kernel exploit writes to the TTBR1 L1 page tables to map the code, AXIWRAM and a few I/O register blocks with the broadest permissions
* It is fine to directly jump into the code (after cleaning and invalidating the data cache):
    * the kernel has never accessed the virtual address range `0x40000000` + a few GBs, meaning no stale TLB entries
    * the kernel has never executed code at the physical address the code is contained (linear heap is at the other end of the memregion, ie. most likely at least a few dozen MBs) either, meaning no other instruction cache line has the same (physical) tag (instruction cache indices are virtual)
* We patch the final firmlaunch stub supposed to relinquish control to the new firmware; it runs uncached, so this is fine
* We modify the SVC table to inject our own function and re-escalate privileges. This is fine because the data cache is Physically Indexed, Physically Tagged (PIPT)
* We then swap the interrupt top-half handler for SGI 6 (KernelSetState SGI) with our own. This allows us to hook the firmlaunch process without patching any code at all
* Firmlaunch finally happens and control is passed to our `arm11` subfolder

## Credits

* @zoogie: testing and debugging on exotic firmware versions
* @fincs: LazyPixie exploitation ideas which influenced a lot of things in this framework
