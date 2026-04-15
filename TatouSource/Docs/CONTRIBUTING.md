# Contributing to FITD

Thank you for your interest in contributing to Free In The Dark! This guide covers everything you need to get started.

---

## Getting Started

1. **Fork** the repository and clone your fork with submodules:

   ```bash
   git clone --recurse-submodules https://github.com/<you>/FITD.git
   ```

2. **Build** the project — see [BUILDING.md](BUILDING.md) for platform-specific instructions.

3. **Obtain game data** — you need original game files from Steam or GOG (AITD1 is the easiest to start with).

4. **Read the architecture guide** — [ARCHITECTURE.md](ARCHITECTURE.md) gives a full overview of the code modules and data flow.

---

## Development Workflow

1. Create a feature branch from `main`:

   ```bash
   git checkout -b feature/my-change
   ```

2. Make your changes, keeping commits small and focused.

3. Build and run the game to verify nothing is broken.

4. Push your branch and open a **Pull Request** against `main`.

---

## Coding Standards

### Language & Compiler

- **C++17** (`CMAKE_CXX_STANDARD 17` is enforced by CMake).
- The project must build cleanly with MSVC (VS2022+), GCC, and Clang.

### Formatting

The project includes an `.editorconfig` file. Key rules:

| Setting | Value |
|---------|-------|
| Indent style | Spaces |
| Indent size | 4 |
| Tab width | 4 |

Please configure your editor to respect `.editorconfig` — most modern editors (VS, VS Code, CLion, etc.) support it out of the box.

### Naming Conventions

The codebase is a reverse-engineered reimplementation, so naming is a mix of original French identifiers and English additions. Follow these guidelines for **new** code:

- **Functions**: `camelCase` (e.g., `loadRemasterConfig`, `updateController`)
- **Types / Structs / Classes**: `PascalCase` (e.g., `RemasterConfig`, `PostProcessing`)
- **Global variables**: prefix with `g_` (e.g., `g_gameId`, `g_controllerState`)
- **Constants / Enums**: `ALL_CAPS` or `PascalCase` enum values (follow surrounding context)
- **File names**: `camelCase.cpp` / `camelCase.h` for new files

For code that directly reimplements original engine functions, **keep the original naming** (e.g., `AffObjet`, `GereDec`, `LoadEtage`) to maintain traceability to the reverse-engineered source.

### Header Guards

Use `#pragma once` for new headers, or traditional include guards matching the existing style:

```cpp
#ifndef _MY_HEADER_H_
#define _MY_HEADER_H_
// ...
#endif
```

### Comments

- Add comments to explain *why*, not *what*.
- Use the existing copyright/author header block style for new files:

  ```cpp
  ///////////////////////////////////////////////////////////////////////////////
  // Alone In The Dark Re-Haunted
  // <description>
  ///////////////////////////////////////////////////////////////////////////////
  ```

### Error Handling

- Prefer returning error codes or `bool` over throwing exceptions (the existing codebase does not use exceptions).
- Use `nullptr` checks when dealing with resource pointers from HQR/PAK loading.

---

## Areas Where Help Is Needed

| Area | Description |
|------|-------------|
| **AITD2 / AITD3 completability** | These games have many unimplemented Life macros and missing features |
| **Graphics correctness** | Polygon rendering, palette handling, and depth-masking issues |
| **macOS support** | Build works but is largely untested at runtime |
| **Time Gate: Knight's Chase** | Very early; most engine extensions are unimplemented |
| **Testing** | Any playtesting and bug reports are valuable |

---

## Reporting Bugs

When filing an issue, please include:

1. Which game you were playing (AITD1, AITD2, AITD3, Jack, Time Gate)
2. Where in the game the issue occurs (floor, room, or scene description)
3. Expected vs. actual behaviour
4. Build configuration (OS, compiler, Debug/Release)
5. Screenshots or short video if it's a visual issue

---

## License

By contributing, you agree that your contributions will be licensed under the **GNU General Public License v2** — the same license as the rest of the project.
