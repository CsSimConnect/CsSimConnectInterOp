# Copilot Instructions

## Build and test

- Prerequisites:
  - Visual Studio 2022 / MSBuild with the `v143` toolset
  - A simulator-specific SDK root must be available for the target being built so the SimConnect headers and static libraries resolve
  - The native GoogleTest NuGet package restored under `packages\Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn.1.8.1.7\...`
- SDKs currently available in the maintainer environment:
  - FSX: `C:\FS\FSX SDK`
  - Prepar3D v4: `C:\FS\Prepar3Dv4_SDK_4.5.14.34698`
  - Prepar3D v5: `C:\FS\Prepar3Dv5_SDK_5.3.17.28160`
  - MSFS 2020: `H:\MSFS 2020 SDK`
  - MSFS 2024: `H:\MSFS 2024 SDK`
- When adding or updating build wiring for multiple simulators, target only the latest available release in each simulator family:
  - FSX is fixed and will not update further
  - use the latest available P3D major release SDK when that family is supported
  - use the current MSFS 2020 and MSFS 2024 SDKs
- Build the main DLL:
  - `msbuild CsSimConnectInterOp.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo`
- Build the release DLL for packaging:
  - `msbuild CsSimConnectInterOp.vcxproj /p:Configuration=Release /p:Platform=x64 /nologo`
- Build and pack the native MSFS packages:
  - `powershell -ExecutionPolicy Bypass -File .\build\Pack-NativePackages.ps1 -PackageVersion 0.2.0`
  - `powershell -ExecutionPolicy Bypass -File .\build\Pack-NativePackages.ps1 -Simulator MSFS2020 -PackageVersion 0.2.0`
- Build the test executable:
  - `msbuild CsSimConnectInterOpTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo`
- Build the whole solution:
  - `msbuild CsSimConnectInterOp.sln /m /p:Configuration=Debug /p:Platform=x64 /nologo`
  - At the moment, the solution build includes `CsSimConnectInterOpMock.vcxproj`, which fails on current VS2022 toolsets because `mock\CsSimConnectInterOpMock.cpp` still includes deprecated `<hash_map>`. For normal DLL/test work, build the individual projects above unless you are fixing the mock project.
- Run all tests:
  - `x64\Debug\CsSimConnectInterOpTests.exe`
- List tests:
  - `x64\Debug\CsSimConnectInterOpTests.exe --gtest_list_tests`
- Run a single test:
  - `x64\Debug\CsSimConnectInterOpTests.exe --gtest_filter=LogTests.TestLogging`
  - `x64\Debug\CsSimConnectInterOpTests.exe --gtest_filter=InterOpTests.TestConnect`
- Test-output layout is slightly non-obvious: the test project's intermediate files go under `x64\test-Debug\`, but the final executable is linked into `x64\Debug\` next to the DLL.
- `InterOpTests.TestConnect` is a live integration-style test against SimConnect and fails when no simulator connection is available.
- There is no separate lint target configured in the repository.

## High-level architecture

- This repository builds a thin native interop DLL that exposes a C ABI for managed callers while delegating almost all work to the SimConnect SDK.
- The intended long-term shape is one shared source base that survives across simulator families by compiling against simulator-specific SDKs instead of forking the implementation per simulator.
- Native NuGet packaging now lives in this repository as well: the repo is responsible for building simulator-specific native DLLs and emitting `CsSimConnect.Native.*` packages, while the managed `CsSimConnect` package can stay separate.
- The native NuGet packages are expected to be side-by-side safe: simulator-specific packages must keep their native DLLs in simulator-specific subfolders rather than flattening them into one shared output root.
- `src\CsSimConnectInterOp.h` is the public surface. It uses `extern "C"` export macros (`CS_SIMCONNECT_DLL_EXPORT_*`) and C-friendly signatures so the exported names stay unmangled for C# or other managed consumers.
- `src\CsSimConnectInterOp.cpp` is the main implementation file for the DLL. Nearly every exported API lives there and follows the same pattern: initialize logging, validate the handle when needed, optionally take the global `scMutex`, call the corresponding `SimConnect_*` function, and translate the result for the interop layer.
- The result translation is centralized in `fetchSendId(...)`. On successful SimConnect calls, wrappers try to return the last packet/send ID via `SimConnect_GetLastSentPacketID`; on direct failures they return the `HRESULT`.
- `src\Log.h` and `src\Logger.cpp` implement the in-repo logging subsystem used by the production DLL, the mock DLL, and the tests. Logging defaults to the root logger on stderr; both DLL implementations probe for `rakisLog2.properties`, but the config hook is currently commented out.
- `mock\CsSimConnectInterOpMock.cpp` mirrors the exported API with an in-memory simulator model. It tracks client handles, data definitions, client-data blocks, event/input groups, subscriptions, and queued `SIMCONNECT_RECV` messages so code can be exercised without a real simulator.
- `CsSimConnectInterOpTests.vcxproj` currently references `CsSimConnectInterOp.vcxproj`, not the mock project. That means the checked-in tests are linked against the real DLL and `TestConnect` is not mock-backed today.

## Key conventions

- Keep the interop layer thin. This is explicit in `README.md` and visible in the code: wrappers are expected to stay close to the underlying `SimConnect_*` APIs, adding logging, argument normalization, and return-value translation rather than new business logic.
- Maintain one cross-simulator API surface where possible. Prefer conditional compilation against simulator SDK differences over source forks so the same exported source files continue to build across FSX, P3D, MSFS 2020, and MSFS 2024.
- If a simulator SDK does not provide a given API call, keep the export present and provide a stub/mock implementation that throws an exception clearly telling the caller that the API is unavailable for that simulator.
- For MSFS native package releases, use `build\Pack-NativePackages.ps1` rather than editing the project to point at one SDK permanently. The script switches `MSFS_SDK` per build and produces separate `CsSimConnect.Native.MSFS2020` and `CsSimConnect.Native.MSFS2024` packages.
- Keep simulator-specific native package assets separated on disk. The current packaging flow places files under simulator-named subfolders and the packaged `.targets` preserve that relative layout when copying into consumer build/publish output.
- When you add or change an exported API, keep the public header and the DLL implementation in sync, and mirror the export in `mock\CsSimConnectInterOpMock.cpp` if testability matters for that API surface.
- Respect the existing guard/log/lock structure in wrappers:
  - `initLog();`
  - emit a trace/info log with the call details
  - reject `nullptr` handles with an error log and `FALSE`
  - serialize the actual SimConnect call with `std::scoped_lock<std::mutex> scLock(scMutex);`
- The header uses compile-time SDK detection (`SIMCONNECT_ENUM`, `IS_PREPAR3D`, `IS_MSFS2020`) to select overloads and signatures. Follow the existing `#if IS_PREPAR3D` splits instead of introducing separate runtime branching.
- The logger is shared by source inclusion, not by a separate library project: `src\Logger.cpp` is compiled directly into the DLL, the mock DLL, and the test executable.
- GoogleTest uses a handwritten `main` in `tests\TestMain.cpp`, so targeted runs should use standard GoogleTest flags like `--gtest_filter`.
- P3D SDKs are available locally for compatibility work, but there are no active developer licenses for P3D. Treat MSFS 2020 and MSFS 2024 as the actively maintained targets when choosing what to validate first.
