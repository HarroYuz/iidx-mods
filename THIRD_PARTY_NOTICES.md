# Third-party dependencies

The repository's existing [GPL-3.0 license](LICENSE) is retained for this project. The following separately licensed dependencies are fetched by CMake and statically linked into `2dx-no-fail.dll`:

| Project | Fixed source revision | License |
| --- | --- | --- |
| [SafetyHook](https://github.com/cursey/safetyhook) | `629558c64009a7291ba6ed5cfb49187086a27a47` | [Boost Software License 1.0](licenses/SafetyHook.txt) |
| [Zydis](https://github.com/zyantific/zydis) | `1ba75aeefae37094c7be8eba07ff81d4fe0f1f20` | [MIT](licenses/Zydis.txt) |
| [Zycore](https://github.com/zyantific/zycore-c) | Submodule revision pinned by the Zydis commit above | [MIT](licenses/Zycore.txt) |

These license texts are retained verbatim. Dependency source is retrieved from the fixed revisions above, not from local game directories. Required interface PNG/XML resources are kept with the relevant mod; optional art-generation drafts are excluded.

## IFS LayeredFS runtime dependency

The release includes the unmodified 64-bit `ifs_hook.dll` from [mon/ifs_layeredfs v3.7](https://github.com/mon/ifs_layeredfs/releases/tag/v3.7), official asset `ifs_layeredfs_3.7.zip`, member `64bit/ifs_hook.dll`. Its SHA-256 is `0543c6bcb8dd7aafe39dae4b82df0158c1c86ff740a0b078b1118aa923bdb24c`, identical to the locally tested copy.

Source: [v3.7 tree](https://github.com/mon/ifs_layeredfs/tree/v3.7). Upstream's [license](licenses/ifs_layeredfs.txt) specifies Apache-2.0 with additional rules preserving existing command-line argument names and requiring new arguments to start with `--layered-`. The [Apache-2.0 text](licenses/Apache-2.0.txt) and upstream modifications are retained; no DLL or argument modifications were made. This dependency is separately licensed and is not relicensed under this repository's GPL-3.0 license. All license texts accompany the release in `data_mods.zip`.

The game, GSM and FireNet binaries are not distributed in this repository or release. They are external runtime components and retain their respective rights and licenses.
