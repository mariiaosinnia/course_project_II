# course_project_II

## Build and run

### Windows (Visual Studio / MSVC)

There are two ways to provide Boost for the Windows build.

Option 1: use vcpkg.

```powershell
$env:VCPKG_ROOT = 'C:\path\to\vcpkg'
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

If vcpkg is not integrated yet:

```powershell
& "C:\path\to\vcpkg\vcpkg.exe" integrate install
& "C:\path\to\vcpkg\vcpkg.exe" install boost-headers portaudio
```

The project uses FetchContent for Opus when it is not installed system-wide, so installing Opus via vcpkg is optional. If you prefer to manage all dependencies through vcpkg, you may also install `opus`.

Option 2: use a manually installed Boost package and point CMake to it.

```powershell
$env:BOOST_ROOT = 'C:\local\boost_1_91_0'
$env:Boost_INCLUDE_DIR = 'C:\local\boost_1_91_0\include'
$env:Boost_LIBRARY_DIR = 'C:\local\boost_1_91_0\lib64-msvc-14.4'
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

If you prefer not to set environment variables, pass paths directly to CMake:

```powershell
cmake --preset windows-msvc -DBoost_INCLUDE_DIR="C:\local\boost_1_91_0\include" -DBoost_LIBRARY_DIR="C:\local\boost_1_91_0\lib64-msvc-14.4"
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