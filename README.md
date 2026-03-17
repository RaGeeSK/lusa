# LusaKey

LusaKey is a cross-platform password manager built with C++ and Qt Quick for Windows and Linux. The interface uses a warm, quiet visual language inspired by Claude-style product design: soft panels, neutral tones, calm spacing, and a focused two-pane workflow.

## Highlights

- Qt 6 desktop app with a C++ backend and QML interface
- Local encrypted vault protected by a master password
- AES-256-GCM encryption through OpenSSL
- PBKDF2-HMAC-SHA256 key derivation with a per-vault random salt
- Search, favorites, password strength hints, and password generation
- Cross-platform storage path based on `QStandardPaths::AppDataLocation`

## Requirements

- Qt 6.5 or newer
- CMake 3.21 or newer
- A C++20 compiler
- OpenSSL development libraries

## Build

### Windows

```powershell
cmake -S . -B build `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64/lib/cmake"
cmake --build build --config Release
```

### Linux

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.0/gcc_64/lib/cmake
cmake --build build --config Release
```

If Qt is already discoverable from your environment, `CMAKE_PREFIX_PATH` can be omitted.

## Run

- Windows: `.\build\Release\LusaKey.exe`
- Linux: `./build/LusaKey`

## Storage

The encrypted vault is stored inside the app data directory chosen by Qt:

- Windows: typically `%APPDATA%/LusaKey/vault.lusa`
- Linux: typically `~/.local/share/LusaKey/vault.lusa`

## Security Notes

- The master password is never written to disk.
- Vault contents are encrypted as a single JSON payload with AES-256-GCM.
- A 32-byte key is derived from the master password using PBKDF2-HMAC-SHA256 and a random 16-byte salt.
- The current implementation keeps the master password in process memory while the vault is unlocked so edits can be saved without constant prompts.
- This is a strong foundation for a personal desktop vault, but it has not been independently audited. If you plan to use it for sensitive production secrets, the next step should be a formal security review and optional OS keychain integration.

## Project Layout

```text
src/
  appcontroller.*        Application state and QML bridge
  vaultentry.*           Entry serialization helpers
  vaultlistmodel.*       Searchable list model for the UI
  vaultservice.*         Local encrypted vault persistence
  crypto/cryptoservice.* OpenSSL-backed encryption helpers
qml/
  Main.qml               Complete Qt Quick user interface
```
