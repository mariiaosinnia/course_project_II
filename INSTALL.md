# StudyRoom — Getting Started

> Synchronized music listening for you and your friends.

---

## Requirements

| Platform | Requirements |
|----------|-------------|
| macOS    | [Homebrew](https://brew.sh) |
| Linux    | `sudo` access (apt) |
| Windows  | Visual Studio 2022 with "Desktop development with C++", CMake |

---

## Install (one-time setup)

### macOS / Linux

```bash
chmod +x install.sh && ./install.sh
```

### Windows

Open PowerShell as Administrator, then:

```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
.\install.ps1
```

---

## Launch

### macOS / Linux

```bash
./studyroom
```

Or from anywhere after install:

```bash
studyroom
```

### Windows

```powershell
.\studyroom.ps1
```

Or from anywhere after install:

```powershell
studyroom
```

---

## Troubleshooting

**macOS: "cannot be opened because the developer cannot be verified"**  
Go to System Settings → Privacy & Security → click "Allow Anyway".

**Linux: no sound**  
Make sure PulseAudio or PipeWire is running: `pulseaudio --start`

**Windows: script won't run**  
Run this in PowerShell first:
```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
```
