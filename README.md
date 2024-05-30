<br/>
<div align="center">
  <a href="https://loopholelabs.io">
    <img src="https://cdn.loopholelabs.io/loopholelabs/LoopholeLabsLogo.svg" alt="Logo" height="30">
  </a>
  <h3 align="center">
    A Kernel Module that allows mapping multiple segments of memory over a base memory space.
  </h3>

[![License: GPL](https://img.shields.io/badge/License-GPL%20v3-brightgreen.svg)](https://www.gnu.org/licenses/gpl-3.0.en.html)
[![Discord](https://dcbadge.vercel.app/api/server/JYmFhtdPeu?style=flat)](https://loopholelabs.io/discord)
</div>

## Build and Load

The kernel module requires Linux kernel v6.5+. Building the Kernel Module can
be done using the following command.

```bash
make module
```

The `batch-syscalls.ko` file will automatically be generated. It can then
be loaded using `sudo make load` and unloaded using `sudo make unload`.

It is possible to use the `LOG_LEVEL` variable to change the kernel module log
level on build.

```bash
make module LOG_LEVEL=2
```

The following log levels are available.

* `LOG_LEVEL=1`: `INFO` (default).
* `LOG_LEVEL=2`: `DEBUG`.
* `LOG_LEVEL=3`: `TRACE`.

## Testing and Example

`tests/page_fault/page_fault.c` contains a sample userspace C program that
illustrates how the module can be used.

The test program maps randomized test files into memory. These files can be
created using the helper Go code that can be executed with the following
command (requires Go to be installed).

```bash
make test-generate
```

After loading the module and generating the test data, the test program can be
built and executed using the following command.

```bash
make tests
```

The kernel module output can be retrieved using the `sudo dmesg` command or
`sudo dmesg -w` to keep watching the log output.

## Known Issues

### Unsupported CPU architectures

The only CPU architecture currently supported is `x86-64`.

### Loading data from a FUSE file system

If the test files are stored in a FUSE file system, the kernel module may panic
with the following error.

```
[  937.417750] kernel BUG at mm/truncate.c:667!
[  937.418572] invalid opcode: 0000 [#1] PREEMPT SMP PTI
[  937.418927] CPU: 1 PID: 859139 Comm: page_fault_mult Tainted: G           OE      6.5.13-debug-v1 #6
[  937.419296] Hardware name: VMware, Inc. VMware Virtual Platform/440BX Desktop Reference Platform, BIOS 6.00 05/28/2020
[  937.419712] RIP: 0010:invalidate_inode_pages2_range+0x265/0x4d0
[  937.420160] Code: 85 c0 0f 8e 12 01 00 00 4c 89 ff e8 a5 9c 04 00 49 8b 07 a9 00 00 01 00 0f 84 c6 fe ff ff 41 8b 47 58 85 c0 0f 8e 01 01 00 00 <0f> 0b 4d 3b 67 18 0f 85 d2 fe ff ff be c0 0c 00 00 4c 89 ff e8 c2
[  937.421253] RSP: 0018:ffff99f3c1bf7ab8 EFLAGS: 00010246
[  937.421857] RAX: 0000000000000000 RBX: 0000000000000000 RCX: 0000000000000000
[  937.422444] RDX: 0000000000000000 RSI: 0000000000000000 RDI: 0000000000000000
[  937.423032] RBP: ffff99f3c1bf7c08 R08: 0000000000000000 R09: 0000000000000000
[  937.423669] R10: 0000000000000000 R11: 0000000000000000 R12: ffff8951cd68aef8
[  937.424284] R13: 0000000000000001 R14: 0000000000000000 R15: ffffed5884f9b040
[  937.424907] FS:  00007cfa2a7ec640(0000) GS:ffff8954efc80000(0000) knlGS:0000000000000000
[  937.425572] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
[  937.426239] CR2: 000072bd02ffd658 CR3: 0000000116ff0002 CR4: 00000000003706e0
[  937.427030] Call Trace:
[  937.427747]  <TASK>
[  937.428474]  ? show_regs+0x72/0x90
[  937.429195]  ? die+0x38/0xb0
[  937.429918]  ? do_trap+0xe3/0x100
[  937.430687]  ? do_error_trap+0x75/0xb0
[  937.431426]  ? invalidate_inode_pages2_range+0x265/0x4d0
[  937.432190]  ? exc_invalid_op+0x53/0x80
[  937.432962]  ? invalidate_inode_pages2_range+0x265/0x4d0
[  937.433791]  ? asm_exc_invalid_op+0x1b/0x20
[  937.434633]  ? invalidate_inode_pages2_range+0x265/0x4d0
[  937.435458]  ? invalidate_inode_pages2_range+0x24b/0x4d0
[  937.436277]  ? fuse_put_request+0x9d/0x110
[  937.437106]  invalidate_inode_pages2+0x17/0x30
[  937.437722]  fuse_open_common+0x1cc/0x220
[  937.438313]  ? __pfx_fuse_open+0x10/0x10
[  937.438909]  fuse_open+0x10/0x20
[  937.439508]  do_dentry_open+0x187/0x590
[  937.440140]  vfs_open+0x33/0x50
[  937.440755]  path_openat+0xaed/0x10a0
[  937.441379]  ? asm_sysvec_call_function_single+0x1b/0x20
[  937.442034]  do_filp_open+0xb2/0x160
[  937.442525]  ? __pfx_autoremove_wake_function+0x10/0x10
[  937.443040]  ? alloc_fd+0xad/0x1a0
[  937.443547]  do_sys_openat2+0xa1/0xd0
[  937.444068]  __x64_sys_openat+0x55/0xa0
[  937.444585]  x64_sys_call+0xee8/0x2570
[  937.445104]  do_syscall_64+0x56/0x90
[  937.445631]  entry_SYSCALL_64_after_hwframe+0x73/0xdd
[  937.446167] RIP: 0033:0x7cfac49145b4
[  937.446715] Code: 24 20 eb 8f 66 90 44 89 54 24 0c e8 56 c4 f7 ff 44 8b 54 24 0c 44 89 e2 48 89 ee 41 89 c0 bf 9c ff ff ff b8 01 01 00 00 0f 05 <48> 3d 00 f0 ff ff 77 34 44 89 c7 89 44 24 0c e8 98 c4 f7 ff 8b 44
[  937.447783] RSP: 002b:00007cfa2a7ebd90 EFLAGS: 00000293 ORIG_RAX: 0000000000000101
[  937.448308] RAX: ffffffffffffffda RBX: 00007cfa2a7ec640 RCX: 00007cfac49145b4
[  937.448812] RDX: 0000000000000000 RSI: 000063bc9c8be020 RDI: 00000000ffffff9c
[  937.449316] RBP: 000063bc9c8be020 R08: 0000000000000000 R09: 000000007fffffff
[  937.449817] R10: 0000000000000000 R11: 0000000000000293 R12: 0000000000000000
[  937.450307] R13: 0000000000000000 R14: 00007cfac48947d0 R15: 00007ffd47198540
[  937.450799]  </TASK>
[  937.451256] Modules linked in: batch_syscalls(OE) vsock_loopback(E) vmw_vsock_virtio_transport_common(E) vmw_vsock_vmci_transport(E) vsock(E) binfmt_misc(E) intel_rapl_msr(E) intel_rapl_common(E) vmw_balloon(E) rapl(E) joydev(E) input_leds(E) serio_raw(E) vmw_vmci(E) mac_hid(E) sch_fq_codel(E) dm_multipath(E) scsi_dh_rdac(E) scsi_dh_emc(E) scsi_dh_alua(E) msr(E) efi_pstore(E) ip_tables(E) x_tables(E) autofs4(E) btrfs(E) blake2b_generic(E) raid10(E) raid456(E) async_raid6_recov(E) async_memcpy(E) async_pq(E) async_xor(E) async_tx(E) xor(E) raid6_pq(E) libcrc32c(E) raid1(E) raid0(E) multipath(E) linear(E) hid_generic(E) crct10dif_pclmul(E) crc32_pclmul(E) polyval_clmulni(E) polyval_generic(E) ghash_clmulni_intel(E) sha256_ssse3(E) sha1_ssse3(E) aesni_intel(E) crypto_simd(E) cryptd(E) usbhid(E) hid(E) vmwgfx(E) drm_ttm_helper(E) psmouse(E) ttm(E) drm_kms_helper(E) vmxnet3(E) mptspi(E) drm(E) ahci(E) mptscsih(E) libahci(E) mptbase(E) scsi_transport_spi(E) i2c_piix4(E) pata_acpi(E)
[  937.454514] ---[ end trace 0000000000000000 ]---
[  937.454974] RIP: 0010:invalidate_inode_pages2_range+0x265/0x4d0
[  937.455442] Code: 85 c0 0f 8e 12 01 00 00 4c 89 ff e8 a5 9c 04 00 49 8b 07 a9 00 00 01 00 0f 84 c6 fe ff ff 41 8b 47 58 85 c0 0f 8e 01 01 00 00 <0f> 0b 4d 3b 67 18 0f 85 d2 fe ff ff be c0 0c 00 00 4c 89 ff e8 c2
[  937.456403] RSP: 0018:ffff99f3c1bf7ab8 EFLAGS: 00010246
[  937.456897] RAX: 0000000000000000 RBX: 0000000000000000 RCX: 0000000000000000
[  937.457354] RDX: 0000000000000000 RSI: 0000000000000000 RDI: 0000000000000000
[  937.457893] RBP: ffff99f3c1bf7c08 R08: 0000000000000000 R09: 0000000000000000
[  937.458363] R10: 0000000000000000 R11: 0000000000000000 R12: ffff8951cd68aef8
[  937.458813] R13: 0000000000000001 R14: 0000000000000000 R15: ffffed5884f9b040
[  937.459255] FS:  00007cfa2a7ec640(0000) GS:ffff8954efc80000(0000) knlGS:0000000000000000
[  937.459694] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
[  937.460133] CR2: 000072bd02ffd658 CR3: 0000000116ff0002 CR4: 00000000003706e0
```

To avoid this issue, run the tests from a non-FUSE file system.

## Contributing

Bug reports and pull requests are welcome on GitHub at [https://github.com/loopholelabs/kmod-batch-syscalls][gitrepo]. For more
contribution information check
out [the contribution guide](https://github.com/loopholelabs/kmod-batch-syscalls/blob/master/CONTRIBUTING.md).

## License

This Kernel Module is available as open source under the terms of
the [GPL v3 License](https://www.gnu.org/licenses/gpl-3.0.en.html).

## Code of Conduct

Everyone interacting in this project’s codebases, issue trackers, chat rooms and mailing lists is expected to follow the [CNCF Code of Conduct](https://github.com/cncf/foundation/blob/master/code-of-conduct.md).

## Project Managed By:

[![https://loopholelabs.io][loopholelabs]](https://loopholelabs.io)

[gitrepo]: https://github.com/loopholelabs/kmod-batch-syscalls
[loopholelabs]: https://cdn.loopholelabs.io/loopholelabs/LoopholeLabsLogo.svg
[loophomepage]: https://loopholelabs.io
