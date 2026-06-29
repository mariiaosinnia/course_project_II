# course_project_II

## Build and run

### Windows (Visual Studio / MSVC)

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

Run the binaries:

```powershell
./build/windows-msvc/server/Debug/server.exe
./build/windows-msvc/client/Debug/client.exe
```

### Linux (GCC or Clang)

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-build
```

### WSL

```bash
sudo apt update
sudo apt install -y ninja-build build-essential git cmake libboost-all-dev pkg-config
rm -rf build/linux-gcc
cmake --preset linux-gcc
cmake --build --preset linux-gcc-build
```

Or with Clang:

```bash
cmake --preset linux-clang
cmake --build --preset linux-clang-build
```

Run the binaries:

```bash
./build/linux-gcc/server/server
./build/linux-gcc/client/client
```

### macOS (Clang)

```bash
cmake --preset macos-clang
cmake --build --preset macos-clang-build
```

Run the binaries:

```bash
./build/macos-clang/server/server
./build/macos-clang/client/client
```

### Notes

- If CMake cannot find a generator, install Ninja or use the appropriate native generator for your platform.
- The project uses FetchContent to download missing dependencies such as Opus and PortAudio when they are not available from the system.
- The build output is written under the out/build directory.