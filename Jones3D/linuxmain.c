// Native (non-Windows) OpenJones3D entry point
#include "types.h"
#include "Indy3D.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[])
{
    // Rebuild the command line the game parses (e.g. -load <savegame path>), quoting arguments with spaces
    char aCmdLine[1024] = { 0 };
    size_t len = 0;
    for ( int i = 1; i < argc && len < sizeof(aCmdLine) - 1; ++i )
    {
        const char* pFormat = strchr(argv[i], ' ') ? "%s\"%s\"" : "%s%s";
        int n = snprintf(aCmdLine + len, sizeof(aCmdLine) - len, pFormat, i > 1 ? " " : "", argv[i]);
        if ( n < 0 ) {
            break;
        }
        len += (size_t)n;
    }

    Indy3D_InitProcess();
    return Indy3D_WinMain(NULL, NULL, aCmdLine, /*nShowCmd=*/1);
}
