#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "ti994a.h"
#include "grom.h"
#include "cpu.h"
#include "break.h"
#include "vdp.h"
#include "trace.h"
#include "break.h"
#include "watch.h"
#include "cond.h"
#include "unasm.h"
#include "cover.h"
#include "kbd.h"
#include "sound.h"

/*  Safely parses a value from a string.  Value can be hex, decimal, octal, etc.
 *  Returns true if parsed successfully.
 */
static bool parseValue (char *s, int *result)
{
    int base = 0;

    /*  If TI hex notation (>) is used or if x on its own is used then force
     *  base 16.  Otherwise let glibc figure it out.
     */
    if (s[0] == '>' || s[0] == 'x')
    {
        s++;
        base = 16;
    }

    /*  Set errno to zero first and check it afterwards.  Slight safer than checking the
     *  return value
     */
    errno = 0;
    unsigned long conversion = strtoul (s, NULL, base);
    if (errno != 0)
        return false;

    *result = (int) conversion;
    return true;
}

bool consoleBreak (int argc, char *argv[])
{
    if (!strncmp (argv[1], "list", strlen(argv[1])))
        breakPointList ();
    else
    {
        int addr;

        if (argc < 3 || !parseValue (argv[2], &addr))
            return false;

        if (!strncmp (argv[1], "add", strlen(argv[1])))
            breakPointAdd (addr);
        else if (!strncmp (argv[1], "remove", strlen(argv[1])))
            breakPointRemove (addr);
        #if 0
        else if (!strncmp (argv[1], "condition", strlen(argv[1])))
            breakPointCondition (addr);
        #endif
        else
            return false;
    }

    return true;
}

bool consoleWatch (int argc, char *argv[])
{
    if (!strncmp (argv[1], "list", strlen(argv[1])))
        watchList ();
    else
    {
        int addr;

        if (argc < 3 || !parseValue (argv[2], &addr))
            return false;

        if (!strncmp (argv[1], "add", strlen(argv[1])))
            watchAdd (addr);
        else if (!strncmp (argv[1], "remove", strlen(argv[1])))
            watchRemove (addr);
        else
            return false;
    }

    return true;
}

bool consoleCondition (int argc, char *argv[])
{
    if (!strncmp (argv[1], "list", strlen(argv[1])))
        conditionList ();
    else
    {
        int addr;

        if (argc < 3 || !parseValue (argv[2], &addr))
            return false;

        if (!strncmp (argv[1], "add", strlen(argv[1])))
        {
            int value;

            if (argc < 5 || !parseValue (argv[4], &value))
                return false;

            conditionAdd (addr, argv[3], value);
        }
        else if (!strncmp (argv[1], "remove", strlen(argv[1])))
            conditionRemove (addr);
        else
            return false;
    }

    return true;
}

bool consoleShow (int argc, char *argv[])
{
    bool memory = false;
    bool grom = false;
    bool vdp = false;

    if (!strncmp (argv[1], "cpu", strlen(argv[1])))
        cpuShowStatus();
    else if (!strncmp (argv[1], "pad", strlen(argv[1])))
        ti994aShowScratchPad (false);
    else if (!strncmp (argv[1], "padgpl", strlen(argv[1])))
        ti994aShowScratchPad (true);
    else if (!strncmp (argv[1], "mem", strlen(argv[1])))
        memory = true;
    else if (!strncmp (argv[1], "grom", strlen(argv[1])))
    {
        memory = true;
        grom = true;
    }
    else if (!strncmp (argv[1], "vdp", strlen(argv[1])))
    {
        memory = true;
        vdp = true;
    }
    else
        return false;

    if (memory)
    {
        int addr;
        int bytes = 1;
        WORD data;

        if (argc < 3 || !parseValue (argv[2], &addr))
            return false;

        if (argc > 3 && !parseValue (argv[3], &bytes))
            return false;

        if (grom)
        {
            printf ("GROM ");
            data = gromRead (addr, bytes); // TODO method to access actual
        }
        else if (vdp)
        {
            printf ("VDP ");
            data = vdpRead (addr, bytes); // TODO method to access actual
        }
        else
        {
            data = memRead (addr, bytes);
        }

        if (bytes == 1)
            printf ("%04X = %02X\n", addr, data);
        else
            printf ("%04X = %04X\n", addr, data);
    }

    return true;
}

bool consoleGo (int argc, char *argv[])
{
    printf ("Running\n");
    // cpuBoot ();
         ti994aRun ();
    // run = 1;

    return true;
}

static char *fileToRead;

bool consoleReadInput (int argc, char *argv[])
{
    fileToRead = argv[1];
    printf ("Reading from file %s\n", fileToRead);

    return true;
}

bool consoleBoot (int argc, char *argv[])
{
    cpuBoot ();

    return true;
}

bool consoleUnassemble (int argc, char *argv[])
{
    unasmRunTimeHookAdd();

    if (argc == 2)
    {
        if (!strncmp (argv[1], "covered", strlen(argv[1])))
        {
            outputCovered = 1;
            mprintf (0, "Uncovered code only will be unassembled\n");
        }
        else
            return false;
    }

    return true;
}

bool consoleComments (int argc, char *argv[])
{
    printf ("Reading code comments from '%s'\n", argv[1]);
    unasmReadText (argv[1]);

    return true;
}

bool consoleLevel (int argc, char *argv[])
{
    int newLevel;

    if (!parseValue (argv[1], &newLevel))
        return false;

    outputLevel = newLevel;
    printf ("Level set to %x\n", outputLevel);

    return true;
}

bool consoleQuit (int argc, char *argv[])
{
    exit (0);

    return true;
}

bool consoleVideo (int argc, char *argv[])
{
    vdpInitGraphics();

    return true;
}

bool consoleSound (int argc, char *argv[])
{
    soundInit();

    return true;
}

bool consoleLoadRom (int argc, char *argv[])
{
    int addr;
    int length;

    if (argc < 4 || !parseValue (argv[2], &addr) || !parseValue (argv[3], &length))
        return false;

    ti994aMemLoad (argv[1], addr, length);
    return true;
}

bool consoleLoadGrom (int argc, char *argv[])
{
    int addr;
    int length;

    if (argc < 4 || !parseValue (argv[2], &addr) || !parseValue (argv[3], &length))
        return false;

    gromLoad (argv[1], addr, length);
    return true;
}

bool consoleKeyboard (int argc, char *argv[])
{
    if (argc < 2)
        return false;

    kbdOpen (argv[1]);
    return true;
}

struct _commands
{
    char *cmd;
    int paramCount;
    bool (*func)(int argc, char *argv[]);
    char *help;
}
commands[] =
{
    { "break", 2, consoleBreak, "break [ add <addr> | list | remove <addr> | condition <addr> ]\n" },
    { "watch", 2, consoleWatch, "watch [ add <addr> | list | remove <addr> ]\n" },
    { "condition", 2, consoleCondition, "condition [ add <addr> | list | remove <addr> ]\n" },
    { "show", 2, consoleShow, "show [ cpu | pad | padgpl | (mem|grom|vdp) <addr> [ <size> ]]\n" },
    { "@", 2, consoleReadInput, "@ <file>\n" },
    { "go", 1, consoleGo, "go\n" },
    { "boot", 1, consoleBoot, "boot\n" },
    { "unassemble", 1, consoleUnassemble, "unassemble [ covered ]\n" },
    { "level", 2, consoleLevel, "level <dbg-level>\n" },
    { "quit", 1, consoleQuit, "quit\n" },
    { "video", 1, consoleVideo, "video\n" },
    { "sound", 1, consoleSound, "sound\n" },
    { "comments", 2, consoleComments, "comments <file>\n" },
    { "load", 2, consoleLoadRom, "load <file>\n" },
    { "grom", 2, consoleLoadGrom, "grom <file>\n" },
    { "keyboard", 2, consoleKeyboard, "keyboard <file>\n" }
};

#define NCOMMAND (sizeof (commands) / sizeof (struct _commands))

static int parse (char *line, char *argv[])
{
    char *pos;
    int argc = 0;

    while ((pos = strtok (line, " \n\t")) != NULL)
    {
        argv[argc++] = pos;
        line = NULL;
    }

    return argc;
}

static void input (FILE *fp)
{
    char line[1024];
    // BOOL run = 0;
    int argc;
    char *argv[100];

    if (fp)
    {
        /*  If fgets returns NULL, assume EOF or other condition to be handled
         *  by caller
         */
        if (fgets (line, sizeof line, fp) == NULL)
            return;
    }
    else
    {
        /*  No file open, assume console input.  Use readline library, but copy
         *  result to local storage and free result immediately so we don't
         *  forget later.
         */
        char* input = readline("TI-99/4A > ");

        if (!input)
            return;

        /* Add input to readline history. */
        add_history(input);

        if (strlen (input) > sizeof line - 1)
            input[sizeof line - 1] = 0;

        strcpy (line, input);
        free(input);
    }

    argc = parse (line, argv);

    if (argc == 0)
    {
        // tms9900.ram.covered[tms9900.pc>>1] = 1;
        cpuExecute (cpuFetch());
        cpuShowStatus ();
        gromShowStatus ();
        return;
    }

    if (argv[0][0] == '#')
        return;

    for (int i = 0; i < NCOMMAND; i++)
    {
        if (!strncmp (argv[0], commands[i].cmd, strlen (argv[0])))
        {
            if (!commands[i].func (argc, argv))
                printf (commands[i].help);

            return;
        }
    }

    if (!strcmp (argv[0], "?") || !strncmp (argv[0], "help", strlen(argv[0])))
    {
        int i;

        for (i = 0; i < NCOMMAND; i++)
            printf (commands[i].help);

        return;
    }

    printf ("Unknown command, type HELP for a list\n");

    // if (run)
    //     ti994aRun ();
}

int main (int argc, char *argv[])
{
    FILE *fp = NULL;
    FILE *fpSave = NULL;

    if (argc > 1)
        fileToRead = argv[1];

    // Configure readline to auto-complete paths when the tab key is hit.
    rl_bind_key('\t', rl_complete);

    // Enable history
    using_history();

    #if 0
    {
        if ((fp = fopen (argv[1], "r")) == NULL)
        {
            printf ("Can't open '%s'\n", argv[1]);
        }
    }
    #endif

    ti994aInit ();

    extern int ti994aQuitFlag;

    while (!ti994aQuitFlag)
    {
        if (fileToRead != NULL)
        {
            fpSave = fp;
            if ((fp = fopen (fileToRead, "r")) == NULL)
            {
                fprintf (stderr, "Failed to open %s\n", fileToRead);
                fp = fpSave;
            }
            fileToRead = NULL;
        }

        input (fp);

        if (fp && feof (fp))
        {
            fclose (fp);
            fp = fpSave;
            fpSave = NULL;
        }
    }

    ti994aClose ();

    return 0;
}


