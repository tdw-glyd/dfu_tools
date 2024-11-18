//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_tool
**
** DESCRIPTION: Command-line utility for exercising the DFU mode protocol, etc.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

// Base DFU protocol libraries
#include "dfu_proto_api.h"
#include "dfu_messages.h"

// Client-related libraries
#include "dfu_client.h"
#include "ethernet_sockets.h"
#include "async_timer.h"


#define DEBUG_INTERFACES_NAMES              (0)
#define KEYHIT_DELAY_MS                     (10000)

/*
** VERSION NUMBERS.
**
*/
#define MAJOR_VERSION                       (0)
#define MINOR_VERSION                       (1)
#define PATCH_VERSION                       (1)
#define DEFAULT_TRANSACTION_TIMEOUT_MS      (5000)


/*
HERE ARE SOME COMMAND-LINE ARGUMENT SETS THAT ARE HELPFUL FOR TESTING...

-m 753 -iface "\Device\NPF_{DADC3966-80F0-48A8-8F57-0188FDB7DB8D}" -dst 66:55:44:33:22:11 --type ethernet

-x <filename> -iface "\Device\NPF_{DADC3966-80F0-48A8-8F57-0188FDB7DB8D}" -dst 66:55:44:33:22:11 --type ethernet -i <image index> -a <image dest address> -e <encrypted?)

-x C:\Glydways\bl_demo\sample_app\generate\src\Clock_Ip_Cfg.c -iface "\Device\NPF_{DADC3966-80F0-48A8-8F57-0188FDB7DB8D}" -dst 66:55:44:33:22:11 --type ethernet -i 1 -a 0x00600000 -e 1

*/

/*
** Controls how the "help" functionality works.
** If the command-line contains a single "-h" or "--help",
** a variable will be set to HELP_TYPE_ALL. If there is a
** parameter to the "-h" or "--help", the variable will be
** set to "HELP_TYPE_SINGLE".  If no "-h" or "--help" is
** provided, the variable will be set to HELP_TYPE_NONE.
**
*/
typedef enum
{
    HELP_TYPE_NONE,
    HELP_TYPE_SINGLE,
    HELP_TYPE_ALL
}helpTypeEnum;


/*
** Function type and dispatcher table structure for command-line
** handlers.
**
*/
typedef bool (* cmdlineHandler)(int argc, char **argv, char *paramVal, dfuClientEnvStruct *dfuClient);
typedef void (* cmdlineHelpHandler)(char *argStr);

typedef struct
{
    char                *shortForm;
    char                *longForm;
    char                *shortHelp;
    cmdlineHelpHandler   helpHandler;
    cmdlineHandler       handler;
}cmdlineDispatchStruct;


/*
** Command-line handler prototypes for the main command table.
**
*/
static bool cmdlineHandlerMTU(int argc, char **argv, char *paramVal, dfuClientEnvStruct *dfuClient);
static void mtuHelpHandler(char *arg);
static bool cmdlineHandlerXferImage(int argc, char **argv, char *paramVal, dfuClientEnvStruct *dfuClient);

/*
** The main command-line dispatch table
**
*/
static cmdlineDispatchStruct cmdlineHandlers [] =
{
    {"-m", "--mtu", "Negotiate MTU with the target.", mtuHelpHandler, cmdlineHandlerMTU},
    {"-x", "--xfer", "Transfer an image to the target.", NULL, cmdlineHandlerXferImage},
    {NULL, NULL, NULL, NULL, NULL}
};


/*
** Internal support prototypes
**
*/
#if (DEBUG_INTERFACES_NAMES==1)
static int printInterfaces(void);
#endif
static int flag_srch(int argc, char **argv, const char *flag, int get_value, char **rtn);
static int dfuToolStrnicmp(const char *s1, const char *s2, size_t n);
static char * dfuToolStristr(const char *haystack, const char *needle);
static dfuClientEnvStruct * dfuToolInitLibForInterfaceType(int argc, char **argv);
static helpTypeEnum _getHelpType(int argc, char **argv, char **cmdForHelp);
static void _allCommandsHelp(void);
static char * _strip_quotes(char *str);
static uint32_t getFileSize(char *filename);
static void printApplicationBanner(void);
static char *_padStr(char *pStr, char padChar, int padLength);
static cmdlineHelpHandler _getHelpHandler(char *cmd);

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                          PUBLIC-FACING FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

int main(int argc, char **argv)
{
    char                    *paramVal = NULL;
    ASYNC_TIMER_STRUCT       keyhitTimer;

    printApplicationBanner();

    if (argc > 0)
    {
        int                             index = 0;
        bool                            done = false;
        char *                          dummy = NULL;
        char *                          destStr = NULL;
        helpTypeEnum                    helpType = HELP_TYPE_NONE;
        char *                          cmdForHelp = NULL;

    #if (DEBUG_INTERFACES_NAMES==1)
        printInterfaces();
    #endif

        /*
        ** Help Requested?  Either display all help or
        ** individual command help.
        **
        */
        helpType = _getHelpType(argc, argv, &cmdForHelp);
        if (helpType == HELP_TYPE_ALL)
        {
            _allCommandsHelp();
        }
        else if (helpType == HELP_TYPE_SINGLE)
        {
            if ( (cmdForHelp != NULL) && (strlen(cmdForHelp) > 0) )
            {
                printf("\r\n '%s' help:", cmdForHelp);
                cmdlineHelpHandler   handler = _getHelpHandler(cmdForHelp);
                if (handler)
                {
                    handler(cmdForHelp);
                }
                else
                {
                    printf("\r\n No help available.  Is this a valid command?");
                }
            }
        }
        else
        {
            /*
            ** Look for command-line handler (or help text)
            **
            */
            while ( (!done) && (cmdlineHandlers[index].handler != NULL) )
            {
                if (
                        (flag_srch(argc, argv, cmdlineHandlers[index].shortForm, 1, &paramVal)) ||
                        (flag_srch(argc, argv, cmdlineHandlers[index].longForm, 1, &paramVal))
                    )
                {
                    if (helpType == HELP_TYPE_NONE)
                    {
                        // Sets up the interface and other parts of the client, including default MTU.
                        dfuClientEnvStruct *            dfuClient = dfuToolInitLibForInterfaceType(argc, argv);

                        if (dfuClient)
                        {
                            // If there is a destination, ALWAYS try to negotiate the MTU
                            if (
                                (!flag_srch(argc, argv, "-m", 1, &dummy)) &&
                                (!flag_srch(argc, argv, "--mtu", 1, &dummy)) &&
                                (flag_srch(argc, argv, "-dst", 1, &destStr))
                            )
                            {
                                cmdlineHandlerMTU(argc, argv, paramVal, dfuClient);
                            }

                            // Open up a session
                            printf("\r\nOpening a session with the target...");

                            if (dfuClientTransaction_CMD_BEGIN_SESSION(dfuClient, DEFAULT_TRANSACTION_TIMEOUT_MS, destStr))
                            {
                                printf("Opened!");
                                printf("\r\n");

                                // Call the specified command
                                done = cmdlineHandlers[index].handler(argc, argv, paramVal, dfuClient);

                                // Close the session
                                printf("\r\n\r\nClosing session with target...");
                                if (dfuClientTransaction_CMD_END_SESSION(dfuClient, DEFAULT_TRANSACTION_TIMEOUT_MS, destStr))
                                {
                                    printf("Closed!");
                                }
                                else
                                {
                                    printf("Failed to close!");
                                }
                                fflush(stdout);
                                break;
                            }
                            else
                            {
                                printf("Failed to open session!");
                            }
                        }
                        else
                        {
                            printf("\r\nUnable to initialize the DFU protocol object.");
                        }
                    }
                }

                ++index;
            }
        }
    }
    else
    {
        printf("\r\nNo command-line arguments provided!");
    }

    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    printf("\r\n\r\nPress a key...");

    TIMER_Start(&keyhitTimer);
    do
    {
    } while ( (!_kbhit()) && (!TIMER_Finished(&keyhitTimer, KEYHIT_DELAY_MS)) );


    fflush(stdout);

    return 0;
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                       COMMAND-LINE DISPATCH HANDLERS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$


/*!
** FUNCTION: cmdlineHandlerSendImage
**
** DESCRIPTION: Transfer an image to the target.
**
** PARAMETERS:
**              - "-x" ("--xfer") <image file name>
**              - "-dst" ("--dest") <destination MAC ID>
**              - "-i" ("--index") <image index>
**              - "-a" ("--address") <storage address>
**              - "-e" ("--encrypted") <true or false>
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool cmdlineHandlerXferImage(int argc, char **argv, char *paramVal, dfuClientEnvStruct *dfuClient)
{
    bool                            ret = true;
    char *                          destStr = NULL;
    char *                          imageIndexStr = NULL;
    uint8_t                         imageIndex;
    char *                          imageAddressStr = NULL;
    uint32_t                        imageAddress;
    char *                          isEncryptedStr = NULL;
    bool                            isEncrypted;
    char *                          filenameStr = paramVal;  // argument to "-x" is the filename
    uint32_t                        imageSize;

    flag_srch(argc, argv, "-dst", 1, &destStr);
    flag_srch(argc, argv, "-i", 1, &imageIndexStr);
    flag_srch(argc, argv, "-a", 1, &imageAddressStr);
    flag_srch(argc, argv, "-e", 1, &isEncryptedStr);

    if (
            (destStr) &&
            (imageIndexStr) &&
            (imageAddressStr) &&
            (isEncryptedStr) &&
            (filenameStr)
       )
    {
        imageIndex = atoi(imageIndexStr);
        imageAddress = strtoul(imageAddressStr, NULL, 0);
        if (
               (strstr(isEncryptedStr, "true") != NULL) ||
               (strstr(isEncryptedStr, "True") != NULL) ||
               (strstr(isEncryptedStr, "TRUE") != NULL) ||
               (strstr(isEncryptedStr, "1") != NULL)
           )
        {
            isEncrypted = true;
        }

        imageSize = getFileSize(filenameStr);

        if (imageSize > 0)
        {
            FILE *                      handle;

            printf("\r\n Sending       : %s", filenameStr);
            printf("\r\n File size     : %u bytes", imageSize);
            printf("\r\n Image Index   : %d", imageIndex);
            printf("\r\n FLASH Address : 0x%08X", imageAddress);
            printf("\r\n Encrypted     : %s", isEncrypted ? "yes" : "no");
            fflush(stdout);

            // Open the file
            handle = fopen(filenameStr, "rb");
            if (handle)
            {
                uint8_t                         buffer[1500];
                size_t                          bytesRead = 0;
                uint32_t                        totalSent = 0;
                uint32_t                        totalTransactions = 0;

                /*
                ** First, get the transfer started
                **
                */
                if (dfuClientTransaction_CMD_BEGIN_RCV(dfuClient,
                                                       DEFAULT_TRANSACTION_TIMEOUT_MS,
                                                       destStr,
                                                       imageIndex,
                                                       imageSize,
                                                       imageAddress,
                                                       isEncrypted))
                {
                    printf("\n\n");
                    fflush(stdout);

                    /*
                    ** Send all of the image file data
                    **
                    */
                    while ((bytesRead = fread(buffer, 1, dfuClientGetInternalMTU(dfuClient)-1, handle))> 0)
                    {
                        printf("\r >> Transfer status: Sending [%4u] bytes...                     ", (uint32_t)bytesRead);
                        fflush(stdout);

                        if (!dfuClientTransaction_CMD_RCV_DATA(dfuClient,
                                                               DEFAULT_TRANSACTION_TIMEOUT_MS,
                                                               destStr,
                                                               buffer,
                                                               dfuClientGetInternalMTU(dfuClient)))
                        {
                            printf("\r Client rejected image WRITE operation!          ");
                            break;
                        }
                        else
                        {
                            totalSent += bytesRead;
                            ++totalTransactions;
                        }
                    } // while

                    printf("\r\n\r\n Sent [%u] bytes.  Total transactions: [%d]", totalSent, totalTransactions);

                    // Now send the "RCV_COMPLETE" transaction
                    if (!dfuClientTransaction_CMD_RCV_COMPLETE(dfuClient,
                                                               DEFAULT_TRANSACTION_TIMEOUT_MS,
                                                               destStr,
                                                               totalSent))
                    {
                        printf("\r\n Target did not accept RCV_COMPLETE command!");
                    }

                    fflush(stdout);
                }
                else
                {
                    printf("\r\n Target did not accept BEGIN_RCV command!");
                }

                fclose(handle);
            }
            else
            {
                printf("\r\n Failed to open [%s]!", filenameStr);
            }
        }
        else
        {
            printf("\r\n File size was ZERO!");
        }
    }
    else
    {
        printf("\r\n Invalid or missing parameters!");
    }

    fflush(stdout);

    return (ret);
}


/*!
** FUNCTION: cmdlineHandlerMTU
**
** DESCRIPTION: Get the target's MTU so we know how to manage future transactions.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool cmdlineHandlerMTU(int argc, char **argv, char *paramVal, dfuClientEnvStruct *dfuClient)
{
    bool                         ret = true;
    uint16_t                     retMTU;
    char *                       dest;

    if (flag_srch(argc, argv, "-dst", 1, &dest))
    {
        printf("\r\nNegotiating MTU. ");

        dfuClientSetDestination(dfuClient, dest);

        retMTU = dfuClientTransaction_CMD_NEGOTIATE_MTU(dfuClient,
                                                        DEFAULT_TRANSACTION_TIMEOUT_MS,
                                                        dest,
                                                        MAX_ETHERNET_MSG_LEN);

        dfuClientSetInternalMTU(dfuClient, retMTU);

        printf("Client MTU is: %d", retMTU);
    }

    return (ret);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
/*!
** FUNCTION: flag_srch(argc,argv,flag,get_value,&rtn )
**
** INPUT:   argc,argv   - Command line parameters
**      flag            - String (flag) to be found
**      get_value       - TRUE return pointer to next item
**                        FALSE no pointer returned
**      rtn             - Return pointer (NULL if not used or
**                        found)
**
** OUTPUT: TRUE if flag found
**       FALSE if not
**       Scan argument list for flag
**       If found then TRUE is returned and if get_value TRUE
**       return rtn pointing to string in argv[] following
**       the flag
**       NOTE only exact matches will be found
**
*/
static int flag_srch(int argc, char **argv, const char *flag, int get_value, char **rtn)
{
    int i;
    const char *ptr;

    /*
    ** Scan through the argv's and look for a matching flag
    **
    */
    for( i=0; i<argc; i++ )
    {
        ptr = argv[i];
        if( strcmp(ptr,flag) == 0 )
        {
            /*
            ** Match found, return pointer to the following
            ** (if requested or NULL if not)
            **
            */
            i++;
            if( get_value && i < argc )
            {
                *rtn = (char *)_strip_quotes(argv[i]);
            }
            else
                *rtn = NULL;
            return( 1 );
        }
    }

    // Failure exit here, so return to user with FALSE
    return( 0 );
}

/*!
** FUNCTION: _strip_quotes
**
** DESCRIPTION: Remove any double-quotes from the string passed.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static char * _strip_quotes(char *str)
{
    char *src = str, *dst = str;

    // Iterate through the string
    while (*src) {
        // Copy character if it's not a quote
        if (*src != '"') {
            *dst = *src;
            dst++;
        }
        src++;
    }

    // Null-terminate the result string
    *dst = '\0';

    return (str);
}

/*!
** FUNCTION: printInterfaces
**
** DESCRIPTION: Uses NPCAP to enumerate the interfaces.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
#if (DEBUG_INTERFACES_NAMES==1)
static int printInterfaces(void)
{
  pcap_if_t *alldevs;
  pcap_if_t *d;
  int i=0;
  char errbuf[PCAP_ERRBUF_SIZE];

  /* Retrieve the device list from the local machine */
  if (pcap_findalldevs(&alldevs, errbuf) == -1)
  {
      fprintf(stderr, "\rError enumerating all devices!");
      return (0);
  }

#ifdef JFJFJF
  if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING,
    NULL /* auth is not needed */,
    &alldevs, errbuf) == -1)
  {
    fprintf(stderr,
      "Error in pcap_findalldevs_ex: %s\n",
      errbuf);
    exit(1);
  }
#endif // JFJFJF

  /* Print the list */
  for(d= alldevs; d != NULL; d= d->next)
  {
    printf("%d. NAME: %s", ++i, d->name);
    if (d->description)
      printf(" DESC: %s\n", d->description);
    else
      printf(" (No description available)\n");
  }

  if (i == 0)
  {
    printf("\nNo interfaces found! Make sure Npcap is installed.\n");
    return (0);
  }

  /* We don't need any more the device list. Free it */
  pcap_freealldevs(alldevs);

  return (1);
}
#endif

/*!
** FUNCTION: dfuToolInitLibForInterfaceType
**
** DESCRIPTION: Looks for the "-t" (or "--type") to be either
**              "ethernet", "CAN" or "UART".  Also looks for
**              the interface name and if both are provided,
**              attempts to initialize the DFU client library
**              and return the environment handle.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static dfuClientEnvStruct * dfuToolInitLibForInterfaceType(int argc, char **argv)
{
    dfuClientEnvStruct *             ret = NULL;

    if ( (argc > 1) && (argv) )
    {
        char *      paramVal;
        char *      ifaceName;

        if (
               (flag_srch(argc, argv, "-t", 1, &paramVal)) ||
               (flag_srch(argc, argv, "--type", 1, &paramVal))
           )
        {
            if (
                   (dfuToolStrnicmp(paramVal, "e", 3) == 0) ||
                   (dfuToolStristr(paramVal, "eth") != NULL)
               )
            {
                // Get interface name
                if (flag_srch(argc, argv, "-iface", 1, &ifaceName))
                {
                    ret = dfuClientInit(DFUCLIENT_INTERFACE_ETHERNET, ifaceName);
                }
            }
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuToolStrnicmp
**
** DESCRIPTION: Case-insensitive string comparison
**
** PARAMETERS:
**
** RETURNS: 0 if a match.  Non-zero otherwise
**
** COMMENTS:
**
*/
static int dfuToolStrnicmp(const char *s1, const char *s2, size_t n)
{
    uint8_t                 c1;
    uint8_t                 c2;

    if ( (s1) && (s2) )
    {
        while (n-- != 0 && (*s1 || *s2))
        {
            c1 = *(const unsigned char *)s1++;
            if ('a' <= c1 && c1 <= 'z')
                c1 += ('A' - 'a');
            c2 = *(const unsigned char *)s2++;
            if ('a' <= c2 && c2 <= 'z')
                c2 += ('A' - 'a');
            if (c1 != c2)
                return c1 - c2;
        }
    }

    return 0;
}

/*!
** FUNCTION: dfuToolStristr
**
** DESCRIPTION: Does a case-insensitve search for "needle" in
**              the "haystack".  If found, returns the address
**              where it was found. If not found, returns NULL
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static char * dfuToolStristr(const char *haystack, const char *needle)
{
    int c = tolower((unsigned char)*needle);
    if (c == '\0')
        return (char *)haystack;

    for (; *haystack; haystack++)
    {
        if (tolower((unsigned char)*haystack) == c)
        {
            size_t              i;

            for (i = 0;;)
            {
                if (needle[++i] == '\0')
                    return (char *)haystack;

                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                    break;
            }
        }
    }

    return NULL;
}

/*!
** FUNCTION: getFileSize
**
** DESCRIPTION: Returns the length of the named file.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static uint32_t getFileSize(char *filename)
{
    uint32_t             ret = 0;

    if ((filename) && (strlen(filename)) )
    {
        FILE *                  handle = fopen(filename, "r+b");

        if (handle)
        {
            fseek(handle, 0, SEEK_END);
            ret = (uint32_t)ftell(handle);
            fclose(handle);
        }
    }

    return (ret);
}

/*!
** FUNCTION: _padStr
**
** DESCRIPTION: Pad a string to the desired length using the
**              specified character as the padding.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static char *_padStr(char *pStr, char padChar, int padLength)
{
    int             i;
    char           *pLocal;
    int             localLen;

    if ( (pStr) && (strlen(pStr) < (size_t)padLength) )
    {
        pLocal = pStr + strlen(pStr);
        localLen = (padLength - strlen(pStr));
        for (i = 0; i < localLen; i++)
        {
            *pLocal = padChar;
            pLocal++;
            *pLocal = 0x00;
        }

        return (pStr);
    }

    return (NULL);
}

/*!
** FUNCTION: printApplicationBanner
**
** DESCRIPTION: Display the application banner (version, copyright, etc.)
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static void printApplicationBanner(void)
{
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
    printf("\r\n:::              Glydways DFU Test Tool Version: %02d.%02d.%02d                  :::", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    printf("\r\n::: Copyright (c) 2024, 2025, 2026 by Glydways, Inc. All Rights Reserved.  :::");
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    printf("\r\n");
    fflush(stdout);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                          Command-Line Help Handlers
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: mtuHelpHandler
**
** DESCRIPTION: Detailed help for the MTU command
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static void mtuHelpHandler(char *arg)
{
    if (arg)
    {
        printf("\r\n Negotiate the MTU with the target.");
        printf("\r\n");
    }

    return;
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                           HELP Support Functions
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: _getHelpType
**
** DESCRIPTION: Checks the command-line arguments to see if the caller wants
**              help and retuns whether it's for a single command, or ALL
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static helpTypeEnum _getHelpType(int argc, char **argv, char **cmdForHelp)
{
    helpTypeEnum                ret = HELP_TYPE_NONE;
    char *                      paramVal = NULL;

    if (
            (flag_srch(argc, argv, "-h", 1, &paramVal)) ||
            (flag_srch(argc, argv, "--help", 1, &paramVal))
        )
    {
        if ( (paramVal != NULL) && (strlen(paramVal) > 0) )
        {
            *cmdForHelp = paramVal;
            ret = HELP_TYPE_SINGLE;
        }
        else
        {
            printf("\r\n >> Primary Command Help <<\r\n");
            ret = HELP_TYPE_ALL;
        }
    }

    return (ret);
}


/*!
** FUNCTION: _allCommandsHelp
**
** DESCRIPTION: Displays a list of all the commands and their "short help"
**              text.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static void _allCommandsHelp(void)
{
    int                     index = 0;
    char                    helpText[128];

    while (cmdlineHandlers[index].handler != NULL)
    {
        sprintf(helpText, "\r\n '%s' ('%s')", cmdlineHandlers[index].shortForm, cmdlineHandlers[index].longForm);
        _padStr(helpText, ' ', 20);
        strcat(helpText, ": ");
        strcat(helpText, cmdlineHandlers[index].shortHelp != NULL ? cmdlineHandlers[index].shortHelp : "No Help");
        printf(helpText);
        ++index;
    }
}

/*!
** FUNCTION: _getHelpHandler
**
** DESCRIPTION: Looks up the command and if there is a detailed command
**              line help handler, return it.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static cmdlineHelpHandler _getHelpHandler(char *cmd)
{
    cmdlineHelpHandler                  ret = NULL;

    if ( (cmd) && (strlen(cmd) > 0) )
    {
        int                     index = 0;

        while (cmdlineHandlers[index].handler != NULL)
        {
            if (
                   (strcmp(cmdlineHandlers[index].shortForm, cmd) == 0) ||
                   (strcmp(cmdlineHandlers[index].longForm, cmd) == 0)
               )
            {
                return (cmdlineHandlers[index].helpHandler);
            }
            ++index;
        }
    }

    return (ret);
}
