tcpp_register_module(
  ID mod-npc-transmogrifier
  ENTRYPOINT Addmod_npc_transmogrifierScripts
  SOURCES src/npc_transmogrifier.cpp)
if(MSVC)
  target_compile_options(workspace_mod-npc-transmogrifier PRIVATE /utf-8)
endif()
