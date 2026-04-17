# CsSimConnect native package

This package contains the native `CsSimConnectInterOp.dll` build for one simulator target.

Install only the package that matches the simulator family you want to run against:

- `CsSimConnect.Native.MSFS2020`
- `CsSimConnect.Native.MSFS2024`

The package places the native DLL under a simulator-specific subfolder inside `runtimes\win-x64\native\` and includes MSBuild targets that preserve that subfolder when copying the native assets to the consuming project's output directory.
