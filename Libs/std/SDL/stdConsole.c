// stdConsole for non-Windows builds: output goes to the terminal (stdout), text attributes become ANSI colors
#include <std/Win95/stdConsole.h>

#include <stdio.h>
#include <unistd.h>

static WORD stdConsole_attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

void stdConsole_InstallHooks(void)
{
}

int J3DAPI stdConsole_Startup(const char* pTitleText, int attributes, int bShowMinimized)
{
    J3D_UNUSED(bShowMinimized);
    if ( pTitleText && isatty(STDOUT_FILENO) ) {
        printf("\033]0;%s\007", pTitleText); // terminal title
    }

    stdConsole_SetAttributes((WORD)attributes);
    return 1;
}

void stdConsole_Shutdown(void)
{
    if ( isatty(STDOUT_FILENO) ) {
        fputs("\033[0m", stdout);
    }
    fflush(stdout);
}

int J3DAPI stdConsole_SetAttributes(WORD wAttributes)
{
    stdConsole_attributes = wAttributes;
    return 1;
}

int J3DAPI stdConsole_SetConsoleTextAttribute(WORD wAttributes)
{
    return stdConsole_SetAttributes(wAttributes);
}

int stdConsole_InitOutputConsole(void)
{
    return 1;
}

int J3DAPI stdConsole_WriteConsole(const char* pText, uint32_t textAttribute)
{
    if ( !pText ) {
        return 0;
    }

    if ( isatty(STDOUT_FILENO) )
    {
        // Console colors are BGR bits, ANSI colors RGB bits
        uint32_t ansi = ((textAttribute & FOREGROUND_RED) ? 1 : 0) | ((textAttribute & FOREGROUND_GREEN) ? 2 : 0) | ((textAttribute & FOREGROUND_BLUE) ? 4 : 0);
        printf("\033[%d;%dm%s\033[0m", (textAttribute & FOREGROUND_INTENSITY) ? 1 : 0, 30 + ansi, pText);
    }
    else
    {
        fputs(pText, stdout);
    }

    fflush(stdout);
    return 1;
}
