#include "ScriptMgr.h"
#include "Log.h"

namespace
{
class ExampleWorldScript final : public WorldScript
{
public:
    ExampleWorldScript() : WorldScript("mod_example_world") { }

    void OnStartup() override
    {
        TC_LOG_INFO("server.loading", "[mod-example] WorldScript::OnStartup registration verified");
    }
};
}

void Addmod_exampleScripts()
{
    new ExampleWorldScript();
}
