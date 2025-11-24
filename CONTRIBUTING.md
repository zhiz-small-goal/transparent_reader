# Contributing to TransparentMdReader

Thanks for considering contributing!

## How to contribute
- Bug reports / feature requests: open an issue with复现步骤、预期结果、实际结果。
- Code changes: fork & PR，保持改动聚焦，附带简短说明和测试要点。

## Development quickstart
- Requirements: Qt 6.x (Widgets, WebEngine, WebChannel), CMake 3.20+, C++17 toolchain.
- Configure (example VS 2022 x64):
  ```
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Debug
  ```
- Run: execute `build/.../TransparentMdReader.exe`.
- Resources: keep `resources/web` and `resources/icons/app.ico` alongside the binary.

## Coding guidelines
- C++17, avoid C++20+ features.
- Qt6 best practices; keep Qt-related code under `src/app/`; web resources in `resources/web/`.
- Prefer relative paths; avoid hard-coded absolute paths.
- Keep changes minimal and behavior-preserving unless feature work is intended.

## Testing
- Before submitting, describe manual test steps or add automated checks where applicable.

## License
- By contributing, you agree your contributions are licensed under the MIT License.
