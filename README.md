# ![safecerthax logo](https://safecerthax.rocks/img/cover.png)
safecerthax is a remote ARM9 & ARM 11 kernel exploit for the Original Nintendo 3DS (O3DS-only) SAFE_FIRM.
For more information on safecerthax, please visit the [safecerthax website](https://safecerthax.rocks).

*Note:* this repository only contains the code for the safecerthax 3DS binaries, please check out the [safecerthax-server](https://github.com/MrNbaYoh/safecerthax-server) repository for the server-side code and  the [safecerthax-proxy](https://github.com/MrNbaYoh/safecerthax-proxy) repository for a proxy version.

## Affected versions
| Model | Firmware | Version | Vulnerable? |
| --- | --- | --- | --- |
| O3DS/2DS | SAFE_FIRM | all | ✓ |
| O3DS/2DS | NATIVE_FIRM | 1.0.0-4.4.0 | ✓ |
| O3DS/2DS | NATIVE_FIRM | >=5.0.0 | ✗ |
| N3DS/N2DS | all | all | ✗ |  

## How does it work?
In this section, we only provide summarized information on how safecerthax works, if you are looking for more details, please take a look at the exploit code and check out the write-up on the [website](https://safecerthax.rocks).


safecerthax is based on [SSLoth](https://github.com/MrNbaYoh/3ds-ssloth), a SSL/TLS certificate verification bypass exploit for the 3DS SSL system module. While SSLoth has been patched on NATIVE_FIRM with firmware update 11.14, it is still exploitable on SAFE_FIRM.


### Direct remote ARM9 kernel code execution
safecerthax uses SSLoth to spoof official NUS servers that handle firmware updates. The custom NUS server fakes the necessity to install a new title for a system update. When installing a new title, the system updater asks the NUS servers for the title's certificate chain. These certificates are passed to Process9 (ARM9) with the `PXIAM:ImportCertificates` command.

However, before firmware version 5.0.0 (NATIVE_FIRM), the `PXIAM:ImportCertificates` command was vulnerable to a heap buffer overflow making it possible to get ARM9 kernel code execution. This flaw was patched on NATIVE_FIRM and N3DS/N2DS SAFE_FIRM but the fix was never backported to O3DS/2DS SAFE_FIRM.

With the custom NUS server, we trigger this vulnerability and get remote ARM9 kernel code execution on O3DS/2DS SAFE_FIRM.

### ARM9 -> ARM11 kernel takeover
The previous flaw gives us ARM9 kernel code execution. Though, we still need to take over the ARM11 kernel from the ARM9 side.

The Process9 code responsible for PXI communications was known not to check the size of incoming commands before firmware version 5.0.0 (NATIVE_FIRM). Actually, the ARM11 PXI system module suffered from the same flaw. While the ARM11 flaw is useless in classic `ARM11 -> ARM9` chains, it is particularly useful when you need to do an `ARM9 -> ARM11` takeover.

The buffer for the `PXIMC:Shutdown` command is located on the stack of the ARM11 PXI system module. Thus, we can trigger a stack buffer overflow by sending a large enough response from the ARM9 side. This is enough to get ARM11 userland ROPchain execution in the PXI system module.

Once we get userland ROPchain execution on the ARM11 side, we can then jump to code pages mapped with all permissions from the ARM9 side (by modifying the L1 tables). This gives ARM11 kernel code execution.

## How to build?

### Requirements
- git
- gcc
- make
- DevkitARM

### Building

First, clone this repository:
```
git clone git@github.com:MrNbaYoh/safecerthax.git
```

safecerthax uses the [kernelhaxcode_3ds](https://github.com/tuxsh/kernelhaxcode_3ds) submodule, run the following commands to get it:
```
git submodule init
git submodule update
```

To build the `safecerthax.bin` & `kernelhaxcode_3ds.bin` binaries run:
```
make && make -C kernelhaxcode_3ds
```

## Thanks
- **yellows8** ([github](https://github.com/yellows8), [twitter](https://twitter.com/ylws8)): `PXIAM:ImportCertificates` vulnerability discovery ([3dbrew](https://www.3dbrew.org/wiki/3DS_System_Flaws#Process9)), PXI command buffer overflow vulnerability discovery ([3dbrew](https://www.3dbrew.org/wiki/3DS_System_Flaws#Process9))
- **plutoo** ([github](https://github.com/plutooo), [twitter](https://twitter.com/qlutoo)): PXI command buffer overflow vulnerability discovery ([3dbrew](https://www.3dbrew.org/wiki/3DS_System_Flaws#Process9))
- **TuxSH** ([github](https://github.com/tuxsh), [twitter](https://twitter.com/TuxSH)): general help,  kernelhaxcode_3ds ([github](https://github.com/tuxsh/kernelhaxcode_3ds))
