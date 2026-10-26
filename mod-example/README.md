# mod-example

Minimal static module for TCPP `b061f46ef86f84f713ea3956b2318383e0b71278`, workspace integration API 1.

The loader creates one `WorldScript`. Its existing `OnStartup()` hook logs
`[mod-example] WorldScript::OnStartup registration verified` once during normal
worldserver startup. It changes no gameplay and needs no database script bindings,
SQL, configuration files or additional dependencies.

Prepare and configure the managed core as described in the workspace
`docs/module-integration.md`. Discovery enables this module by default. Disable
with `-DTCPP_MODULE_MOD_EXAMPLE=OFF`, or all modules with `-DTCPP_MODULES=OFF`;
then rebuild and deploy. There is no runtime toggle.

The CMake file lists sources explicitly and calls `tcpp_register_module` with the
manifest identity and entrypoint. Do not call the entrypoint manually. The
generated workspace callback invokes the original TCPP loader first and this
entrypoint once, within ScriptMgr's existing static registration context.
