# Licenses / Лицензии

BodySync itself is released under the MIT License (`LICENSE`, shipped as `LICENSE.txt`). It uses only publicly documented interfaces and the following third-party components. Their full license texts are in the `LICENSES\` folder next to the program.

| Component | Version | Used for | License | Text |
|---|---|---|---|---|
| OpenVR (`openvr.h`, `openvr_driver.h`, `openvr_api.dll`) | v2.12.14 | SteamVR client API and driver interface; `openvr_api.dll` is shipped unmodified next to `BodySync.exe` | BSD 3-Clause | `LICENSES\OpenVR-LICENSE.txt` |
| Dear ImGui | v1.91.9 | User interface (Win32 + DirectX 11 back ends) | MIT | `LICENSES\ImGui-LICENSE.txt` |
| nlohmann/json | v3.11.3 | JSON reading and writing | MIT | `LICENSES\nlohmann-json-LICENSE.txt` |
| Noto Sans (Regular, Bold) | as packaged by Debian fonts-noto-core (2020) | Interface font covering Latin, Latin Extended and Cyrillic (`fonts\NotoSans-Regular.ttf`, `NotoSans-Bold.ttf`, unmodified) | SIL Open Font License 1.1 | `LICENSES\NotoSans-OFL.txt` |
| GCC runtime (libgcc, libstdc++) | mingw-w64 GCC 13 | Statically linked into the executables | GPLv3 **with the GCC Runtime Library Exception** (no restrictions on the resulting program) | <https://www.gnu.org/licenses/gcc-exception-3.1.html> |
| mingw-w64 runtime | as packaged | C runtime and Windows API import libraries | ZPL 2.1 / public domain | <https://www.mingw-w64.org> |
| NSIS | 3.09 | Used only to build the installer; the stub is part of `BodySync-Setup.exe` | zlib/libpng-style license | <https://nsis.sourceforge.io/NSIS_License> |

Segoe UI, Tahoma or Arial are used only if the bundled Noto Sans cannot be loaded; they are Windows system fonts and are **not** redistributed.

The pinned dependency versions and SHA-256 checksums are in `scripts/fetch_deps.sh` and `deps.sha256`.
