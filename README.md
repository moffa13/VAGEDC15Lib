# VAGEDC15Lib

Shared C++ library for working with **Bosch EDC15 ECUs (VAG group)**.

This is a library only — it doesn't produce any executable. It provides the common building blocks used by all the VAGEDC15 tools:

- **VAGEDC15Logger** — live data logging
- **VAGEDC15Patcher** — binary patching
- **VAGEDC15Checksum** — checksum calculation and correction
- **EDC15Multimap** — multimap app (Qt)

## Contents

| Module | Description |
|---|---|
| `EDC15/` | `EDC15File`: loading, reading, writing and pattern searching in EDC15 flash dumps |
| `Core/` | Command interpreter, configuration handling and general utilities |
| `Logging/` | Logger shared by all tools |

## Requirements

- CMake
- A C++ compiler with C++20 support

## Usage

The easiest way to use the library is through CMake `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
    VAGEDC15Lib
    GIT_REPOSITORY https://github.com/moffa13/VAGEDC15Lib.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(VAGEDC15Lib)

target_link_libraries(your_target PRIVATE vagedc15core)
```

Then include the headers you need:

```cpp
#include "EDC15/EDC15File.h"
#include "Logging/Logger.h"
```

### Local development

To work on the library and an application at the same time, point `FetchContent` to your local clone instead of downloading it:

```
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_VAGEDC15LIB=/path/to/VAGEDC15Lib
```

## Disclaimer

Always keep a backup of the original flash before writing a modified file to the ECU. Use at your own risk.