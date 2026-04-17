# CsSimConnectInterOp - A Layer between the SimConnect Library and Managed Code

The `CsSimConnectInterOp.dll` library is a C++ layer between the (static) SimConnect library and Managed Code.
The intent is to keep it as thin as possible, but provide useful logging and translation functions as needed.

## Building the InterOp DLL

This project requires Visual Studio 2022 (Community edition) to be installed.

The main DLL can be built directly with MSBuild:

```powershell
msbuild CsSimConnectInterOp.vcxproj /p:Configuration=Release /p:Platform=x64 /nologo
```

The project uses the `MSFS_SDK` environment variable to resolve the active SimConnect SDK root.

## Packing native NuGet packages

This repository can produce simulator-specific native NuGet packages for the actively maintained MSFS targets:

- `CsSimConnect.Native.MSFS2020`
- `CsSimConnect.Native.MSFS2024`

Run the packaging script from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build\Pack-NativePackages.ps1 -PackageVersion 0.2.0
```

By default the script:

- builds `CsSimConnectInterOp.vcxproj` in `Release|x64`
- builds once against `H:\MSFS 2020 SDK`
- builds once against `H:\MSFS 2024 SDK`
- writes build artifacts under `artifacts\build\...`
- writes NuGet packages under `artifacts\packages\...`
- stores native assets in simulator-specific subfolders so `CsSimConnect.Native.MSFS2020` and `CsSimConnect.Native.MSFS2024` can be installed side by side

You can package only one simulator with `-Simulator MSFS2020` or `-Simulator MSFS2024`.

To publish the created packages after packing, pass `-Push` and provide a NuGet API key through `-ApiKey` or the `NUGET_API_KEY` environment variable.

## Function interfaces and DLL exports

A DLL in Windows is essentially an executable with a customized entry point defined in the
[`dllmain.cpp`](src/dllmain.cpp) module. This entry point (called "`dllmain`") differs from the usual "`main()`"
function in that it receives information on the sharing mode under which it was invoked, which can involve
a new process or a new thread, and wether it is an "attach" or "detach" event. Once attached, the client
process (or thread) can call the library's exported functions by name or number.

The easiest way to match exported functions is by name, but then the DLL must use easily recognizable names,
which isn't always the case for modern languages. When a language supports overloading of functions, or is
Object Oriented, the compiler will "mangle" the full name. Mangled names work well when the client is written
in the same language, but in our case the client is in C#. An simple alternative is to use the entry number, which
can be fixed by including a "`.DEF`" file when building the DLL. The approach chosen here is to force the compiler
_not_ to mangle the names, by delaring them as "`extern "C"`", because C does not support overloading.

## Matching errors with Requests

Because some errors won't be known until the simulator has processed the request, they are generally reported
asynchronously, using a "`SIMCONNECT_RECV_EXCEPTION`" message. This message includes a "`dwSendID`" field,
which has no relation to all the other Ids used. To help matching requests with exceptions, the InterOp layer
will perform a `SimConnect_GetLastSentPacketID()` call. The return value of all calls witll therefore be:

* Less than zero to return a direct error value,
* Zero to indicate an error that has no error code associated, which typically means an invalid or null handle,
* One to indicate a success that has no `PacketSendID` associated with it, or
* A `PacketSendID` value if higher than one.
