tcpp_register_module(
  ID mod-npc-teleport
  ENTRYPOINT Addmod_npc_teleportScripts
  SOURCES
    src/npc_teleport.cpp
    src/teleport_data.cpp)

# Keep source and execution character sets deterministic on non-UTF-8 Windows hosts.
if(MSVC)
  target_compile_options(workspace_mod-npc-teleport PRIVATE /utf-8)
endif()
