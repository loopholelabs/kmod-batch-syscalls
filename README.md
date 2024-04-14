<br/>
<div align="center">
  <a href="https://loopholelabs.io">
    <img src="https://cdn.loopholelabs.io/loopholelabs/LoopholeLabsLogo.svg" alt="Logo" height="30">
  </a>
  <h3 align="center">
    A Kernel Module that exposes the ability to run batch syscalls via 'ioctl'. 
  </h3>

[![License: GPL](https://img.shields.io/badge/License-GPL%20v3-brightgreen.svg)](https://www.gnu.org/licenses/gpl-3.0.en.html)
[![Discord](https://dcbadge.vercel.app/api/server/JYmFhtdPeu?style=flat)](https://loopholelabs.io/discord)
</div>

## Build and Load

Building the Kernel Module can be done using
```bash
make module
```

The `batch-syscalls.ko` file will automatically be generated. It can then
be loaded using `make load`. 

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

