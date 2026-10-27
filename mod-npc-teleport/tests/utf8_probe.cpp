// Emit actual compiled literals as bytes, independently of console code pages.
#include "../src/teleport_data.h"
#include "../src/teleport_text.h"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    for (uint32 i = 0; i < TELEPORT_COUNT; ++i)
        std::cout << Tele[i].name << '\0';
    for (char const* text : {TeleportText::Confirmation, TeleportText::Combat,
        TeleportText::Unavailable, TeleportText::Money, TeleportText::Close})
        std::cout << text << '\0';
}
