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

After loading the module the test program can be built and executed using the
following command.


```bash
make tests
```

The kernel module output can be retrieved using the `sudo dmesg` command or
`sudo dmesg -w` to keep watching the log output.

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
