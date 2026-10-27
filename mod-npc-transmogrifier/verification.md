# Verification record

Executed on Windows on 2026-10-27 against TCPP
`b061f46ef86f84f713ea3956b2318383e0b71278`.

| Verification | Result |
| --- | --- |
| Module integration suite | 25 tests passed, no skips. |
| Workspace SQL updater suite | 40 tests passed, no skips. |
| Module suite | 4 tests passed, no skips; includes 58 C++ behavioral checks compiled with MSVC. |
| Actual TCPP build | `tools/build.ps1 -Configuration RelWithDebInfo`, tools ON, modules ON: build and installation succeeded. |
| Static entrypoint | `dumpbin /linkermember:1` finds `Addmod_npc_transmogrifierScripts` in the module library. Generated aggregate loader calls it. |
| Installed executable | Both script names and module configuration log string are present in installed worldserver PE. Entrypoint name is present in linked worldserver PDB; installed and build executable SHA-256 hashes match. |
| Runtime registration/database binding | Not tested: worldserver was not started. |
| NPC interaction | Not tested in game. |
| Actual appearance and observer synchronization | Not tested in game. |
| Relog/restart persistence | Source path investigated; not tested against a running server/database. |
| Migration execution/MySQL schema acceptance | Not executed; offline catalog/parser/schema checks only. |

Installed executable size: 55,495,680 bytes. SHA-256:
`7edb4311f3184beb15063cbf93967cb6d7a2f4f0d6cea283a12cac1279b21fbe`.

Workspace-local logs (ignored build output):

- `.build/transmog-build.log`
- `.build/transmog-integration-tests.log`
- `.build/transmog-sql-tests.log`
- `.build/transmog-module-tests.log`
- `.build/transmog-linkage.json`

Initial sandbox execution could not read Git submodule metadata or create usable
compiler-test temporary files. The successful build/checks were rerun with access
to those existing local tools and repositories. No core checkout, reference
repository, submodule revision, live database, installed user configuration,
or existing unrelated module was modified. The module index README was updated.
No commit or push was performed. Root status reports the dirty modules submodule;
modules status reports its README and new `mod-npc-transmogrifier/` directory.

Installation and outstanding runtime acceptance are described in [README](README.md).
