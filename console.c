/*
 * Copyright (c) 2004-2023 Mark Burkley.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 *  Implement a command line console for executing the TI99/4A environment.  By
 *  default, running ti994a enters console mode where commands can be given, the
 *  ROM can be stepped through, registers examined, etc.  If a script is given
 *  as a parameter, it is used for input instead of the comamnd line.  While
 *  running, pressing  CTRL-C will return to the command line.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
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

static char *fileToRead;
static bool ti994aQuitFlag;
static int instPerInterrupt;

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

static void sigHandler (int signo)
{
    int i;
    int n;
    char **str;

    void *bt[10];

    switch (signo)
    {
    case SIGQUIT:
    case SIGTERM:
        printf ("quit seen\n");
        ti994aQuitFlag = true;
        break;

    /*  If CTRL-C then stop running and return to console */
    case SIGINT:
        printf("Set run flag to zero\n");
        extern bool ti994aRunFlag;
        ti994aRunFlag = false;
        break;

    case SIGSEGV:
        n = backtrace (bt, 10);
        str = backtrace_symbols (bt, n);

        if (str)
        {
            for (i = 0; i < n; i++)
            {
                printf ("%s\n", str[i]);
            }
        }
        else
            printf ("failed to get a backtrace\n");

        exit (1);
        break;

    default:
        printf ("uknown sig %d\n", signo);
        exit (1);
        break;
    }
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
    ti994aRun (instPerInterrupt);

    return true;
}

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

bool consoleCtrlC (int argc, char *argv[])
{
    if (signal(SIGINT, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register INT handler\n");
        exit (1);
    }
    return true;
}

bool consoleInsPerSec (int argc, char *argv[])
{
    int count;

    if (argc < 2 || !parseValue (argv[1], &count))
        return false;

    if (count < 50)
        return false;

    instPerInterrupt = count / 50;

    printf ("Running at %d instructions per second (%d per VDP interrupt)\n",
            count, instPerInterrupt);

    return true;
}

struct _commands
{
    char *cmd;
    int paramCount;
    bool (*func)(int argc, char *argv[]);
    char *usage;
    char *help;
}
commands[] =
{
    { "break", 2, consoleBreak, "break [ add <addr> | list | remove <addr> ]",
            "Add, list or remove breakpoints from addresses in ROM" },
    { "watch", 2, consoleWatch, "watch [ add <addr> | list | remove <addr> ]",
            "Add, list or remove a watched memory location" },
    { "condition", 2, consoleCondition, "condition [ add <addr> <cond> <value> | list | remove <addr> ]",
            "Add, list or remove a conditional break on a memory location. "
            "<cond> can be EQ, NE, or CH for equal, not equal and change "
            "respectively" },
    { "show", 2, consoleShow, "show [ cpu | pad | padgpl | (mem|grom|vdp) <addr> [ <size> ]]",
            "Show various things.  Show cpu shows CPU registers, internal and "
            "workspace.  Show pad dumps the scratchpad memory.  Show padgpl shows "
            "the scratchpad memory with GPL annotations.  Show mem, grom and vdp "
            "reads a number of bytes from CPU memory, GROM memory and VDP memory "
            "respectively" },
    { "@", 2, consoleReadInput, "@ <file>",
            "Reads input commands from a file" },
    { "go", 1, consoleGo, "go",
            "Begin running from the current program counter" },
    { "boot", 1, consoleBoot, "boot",
            "Boot the CPU.  Initialise WP, PC and ST registers" },
    { "unassemble", 1, consoleUnassemble, "unassemble [ covered ]",
            "Begin disassembly of each instruction as it is executed.  If the "
            "optional \"covered\" keyword is given, then only disassemble an "
            "instruction the first time it is executed." },
    { "level", 2, consoleLevel, "level <dbg-level>",
            "Set the debug level.  See trace.h for description of levels" },
    { "quit", 1, consoleQuit, "quit", "Exit the program" },
    { "video", 1, consoleVideo, "video", "Enable video output" },
    { "sound", 1, consoleSound, "sound", "Enable audio output" },
    { "comments", 2, consoleComments, "comments <file>",
            "Load disassembly comments from a file" },
    { "load", 2, consoleLoadRom, "load <file> <addr> <length>",
            "Load a ROM binary file to the specified CPU memory address" },
    { "grom", 2, consoleLoadGrom, "grom <file>",
            "Load a GROM binary file to the specified GROM memory address" },
    { "keyboard", 2, consoleKeyboard, "keyboard <file>",
            "Begin reading key events from the specified device file" },
    { "ctrlc", 1, consoleCtrlC, "ctrlc",
            "Capture Ctrl-C and return to console for input" },
    { "inspersec", 2, consoleInsPerSec, "inspersec <count>",
            "Specify how many instructions per second to run (minimum 50)" }
};

#define NCOMMAND (sizeof (commands) / sizeof (struct _commands))

/*  Break a line into word delimited by white space.  Destructive and maintains
 *  pointers to within line.  Ensure memory is not freed until all processing of
 *  args is finished.
 */
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
    int argc;
    char *argv[100];

    if (fp)
    {
        /*  If fgets returns NULL, assume EOF or other condition to be handled
         *  by caller
         */
        if (fgets (line, sizeof line, fp) == NULL)
            return;

        if (line[strlen(line)-1] == '\n')
            line[strlen(line)-1] = 0;
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

    if (fp == NULL && argc == 0)
    {
        cpuExecute (cpuFetch());
        cpuShowStatus ();
        gromShowStatus ();
        return;
    }

    if (strlen (line) < 1 || argv[0][0] == '#')
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
            printf ("%s : %s\n\n", commands[i].usage, commands[i].help);

        return;
    }

    printf ("Unknown command '%s', type HELP for a list\n", argv[0]);
}

int main (int argc, char *argv[])
{
    FILE *fp = NULL;
    FILE *fpSave = NULL;

    if (argc > 1)
        fileToRead = argv[1];
    else
        fileToRead = "config.txt";

    /* Configure readline to auto-complete paths when the tab key is hit.
     */
    rl_bind_key('\t', rl_complete);

    /* Enable history
     */
    using_history();

    /*  Trap signals.  Mainly to allow control to be handed back to the CLI
     *  when CTRL-C is pressed and also to generate a backtrace if a segfault
     *  occurs.
     */
    if (signal(SIGSEGV, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register SEGV handler\n");
        exit (1);
    }

    if (signal(SIGQUIT, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register QUIT handler\n");
        exit (1);
    }

    if (signal(SIGTERM, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register TERM handler\n");
        exit (1);
    }

    ti994aInit ();

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


