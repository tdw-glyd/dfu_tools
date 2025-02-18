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

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#else
    #include <sys/select.h>
    #include <termios.h>
    #define GetStdHandle()
    #define STD_INPUT_HANDLE
    #define FlushConsoleInputBuffer(x)      tcflush(STDIN_FILENO, TCIFLUSH)

    int _kbhit(void)
    {
        struct timeval tv = {0L, 0L};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        return (select(1, &fds, NULL, NULL, &tv));
    }
#endif

// Base DFU protocol libraries
#include "dfu_proto_api.h"
#include "dfu_messages.h"

// Client-related libraries
#include "dfu_client.h"
#include "ethernet_sockets.h"
#include "async_timer.h"
#include "image_xfer.h"
#include "general_utils.h"
#include "sequence_ops.h"

#include "dfu_client_api.h"


// TEST %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// #include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
static void testOpenSSL(void);
uint8_t * encrypt_data_with_public_key(const char *pubkey_filename,
                                       void *valueToEncrypt,
                                       uint32_t encryptLength,
                                       bool shouldSave,
                                       const char *output_filename);
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#define DEBUG_INTERFACES_NAMES              (0)
#define KEYHIT_DELAY_MS                     (10000)
#define BLOCKS_PER_BUF                      (100)

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
static dfuClientEnvStruct * dfuToolInitLibForInterfaceType(int argc, char **argv);
static helpTypeEnum _getHelpType(int argc, char **argv, char **cmdForHelp);
static void _allCommandsHelp(void);
static void printApplicationBanner(void);
static cmdlineHelpHandler _getHelpHandler(char *cmd);
static bool mainHelpHandler(int argc, char **argv);

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                          PUBLIC-FACING FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

int main(int argc, char **argv)
{
    char                    *paramVal = NULL;
    ASYNC_TIMER_STRUCT       keyhitTimer;
    uint8_t                  imageFlags;
    uint32_t                 imageAddress;
    uint32_t                 imageSize;
    uint8_t                  imageIndex;


    // TEST %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // testOpenSSL();
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    printApplicationBanner();

#define TEST_API            (1)

    // TEST ONLY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    dfuClientEnvStruct*             dfuClient = NULL;
#if (TEST_API==0)
    dfuClient = dfuClientInit(DFUCLIENT_INTERFACE_ETHERNET,  "\\Device\\NPF_{DADC3966-80F0-48A8-8F57-0188FDB7DB8D}");
#endif


#ifdef FHFHFHFH
dfuClient = dfuClientInit(DFUCLIENT_INTERFACE_ETHERNET,
                          "Ethernet 4");
    // TEST: Get image status

    imageIndex = 1;
    imageAddress = 0x00500000;
    dfuClientTransaction_CMD_IMAGE_STATUS(dfuClient,
                                     15000,
                                     "66:55:44:33:22:11",
                                     &imageIndex,
                                     imageAddress,
                                     &imageFlags,
                                     &imageSize);

#endif // FHFHFHFH

#define TEST_API            (1)
/*
dfuClientAPI*       api = dfuClientAPIGet(DFUCLIENT_INTERFACE_ETHERNET,
                                          "\\Device\\NPF_{DADC3966-80F0-48A8-8F57-0188FDB7DB8D}",
                                          "FOO");
*/

uint16_t mtu = 1024;
dfuClientAPI*       api = dfuClientAPIGet(DFUCLIENT_INTERFACE_ETHERNET,
                                          "Ethernet 4",
                                          "./public_key.pem",
                                          mtu);
if (api)
{
    deviceInfoStruct*       devRecord;

uint8_t    MAC[6] = {0x66,0x55,0x44,0x33,0x22,0x11};
//uint8_t    MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
for (;;)
{
    dfuClientAPI_LL_BeginSession(api, 1, 5, MAC, 6);
}

    ASYNC_TIMER_STRUCT    timer;
    TIMER_Start(&timer);

    while (!TIMER_Finished(&timer, 80000))
    {
        dfuClientAPI_LL_IdleDrive(api);
    }

    devRecord = dfuClientAPI_LL_GetFirstDevice(api);

    if (devRecord)
    {
        printf("\r\n ::: DEVICE LIST :::\r\n");
        do
        {
            printf("\r\n        Device Type: %02d", devRecord->deviceType);
            printf("\r\n     Device Variant: %02d", devRecord->deviceVariant);
            printf("\r\n Device Status Bits: 0x%02X", devRecord->statusBits);
            printf("\r\n    Core Image Mask: 0x%02X", devRecord->coreImageMask);
            printf("\r\n         BL Version: %d.%d.%d", devRecord->blVersionMajor, devRecord->blVersionMinor, devRecord->blVersionPatch);
            printf("\r\n         Time Stamp: %s", ctime(&devRecord->timestamp));

            devRecord = dfuClientAPI_LL_GetNextDevice(api);
            printf("\r\n\r\n");
        }while (devRecord != NULL);
    }

    dfuClientAPIPut(api);
}
#if (TEST_API==1)

#endif




#define TEST_DEV_LIST  (0)

#if (TEST_DEV_LIST==1)
ASYNC_TIMER_STRUCT timer;
TIMER_Start(&timer);
printf("\r\n Listening for devices in DFU mode...");
while (!TIMER_Finished(&timer, 3000))
{
    dfuClientDrive(dfuClient);
}
// return (0);
#endif

#define MAX_LOOPS       (10)
for (int i = 1; i <= MAX_LOOPS; i++)
{
    printf("\r\n\r\n *** INSTALL LOOP: [%5d] ***\r\n", i);

    macroSequenceInstallImage(
                              dfuClient,
                              1,
                              5,
                              //"FF:FF:FF:FF:FF:FF",
                              "66:55:44:33:22:11",
                              "c://public_key.pem",
                              //"C:/Glydways/bl_tools/image_builder/image_builder/sample_app.img",
                              &mtu,
                              "C:/Glydways/B2/embedded/sample_primary_app/debug_flash/sample_primary_app.img",
                              1,
                              0,
                              false,
                              1000
                              );

    sequenceEndSession(dfuClient, "66:55:44:33:22:11");
}
    printf("\r\n\r\n Exiting...");
    return (0);


    // serverRun(NULL, NULL, NULL, NULL, 0);
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    if (argc > 0)
    {
        int                             index = 0;
        bool                            done = false;
        char *                          dummy = NULL;
        char *                          destStr = NULL;
        char *                          devTypeStr = NULL;
        char *                          devVariantStr = NULL;

    #if (DEBUG_INTERFACES_NAMES==1)
        printInterfaces();
    #endif

        /*
        ** Checks to see if the caller wants help.  If not,
        ** try to handle standard command-line args.
        **
        */
        if (!mainHelpHandler(argc, argv))
        {
            flag_srch(argc, argv, "-devtype", 1, &devTypeStr);
            flag_srch(argc, argv, "-variant", 1, &devVariantStr);

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
                    if (flag_srch(argc, argv, "-dst", 1, &destStr))
                    {
                        // Sets up the interface and other parts of the client, including default MTU.
                        dfuClientEnvStruct *            dfuClient = dfuToolInitLibForInterfaceType(argc, argv);
                        uint8_t                         devType = 255;
                        uint8_t                         devVariant = 255;

                        if ( (devTypeStr) && (devVariantStr) )
                        {
                            devType = atoi(devTypeStr);
                            devVariant = atoi(devVariantStr);
                        }

                        if ( (dfuClient) && (devType < 255) && (devVariant < 255) )
                        {
                            uint32_t                    challengePW = 0;

                            // Open up a session
                            printf("\r\nOpening a session with the target...");

                            challengePW = dfuClientTransaction_CMD_BEGIN_SESSION(dfuClient,
                                                                                 devType,
                                                                                 devVariant,
                                                                                 DEFAULT_TRANSACTION_TIMEOUT_MS,
                                                                                 destStr);
                            if (challengePW != 0)
                            {
                                printf("Opened!");
                                printf("\r\n");

                                //
                                // If theGetStdHandlere is a destination, ALWAYS try to negotiate the MTU. If a session
                                // has been started, this message will be allowed by the target.
                                //
                                if (
                                    (!flag_srch(argc, argv, "-m", 1, &dummy)) &&
                                    (!flag_srch(argc, argv, "--mtu", 1, &dummy))
                                )
                                {
                                    cmdlineHandlerMTU(argc, argv, paramVal, dfuClient);
                                }

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
                    else
                    {
                        printf("\r\nNo destination device specified.");
                    }
                }

                ++index;
            }

            /*
            ** Pause for a few seconds and also allow the user to
            ** end now by a key press.
            **
            */
            FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
            printf("\r\n\r\nPress a key...");

            TIMER_Start(&keyhitTimer);
            do
            {
            } while ( (!_kbhit()) && (!TIMER_Finished(&keyhitTimer, KEYHIT_DELAY_MS)) );
        }
    }
    else
    {
        printf("\r\nNo command-line arguments provided!");
    }

    printf("\r\n");
    fflush(stdout);

    return 0;
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                       COMMAND-LINE DISPATCH HANDLERS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

static bool cmdlineHandlerXferImage(int argc, char **argv, char *filenameStr, dfuClientEnvStruct *dfuClient)
{
    bool                            ret = false;
    char *                          destStr = NULL;
    char *                          imageIndexStr = NULL;
    uint8_t                         imageIndex;
    char *                          imageAddressStr = NULL;
    uint32_t                        imageAddress;
    char *                          isEncryptedStr = NULL;
    bool                            isEncrypted = false;

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
        // Get the numeric image index
        imageIndex = atoi(imageIndexStr);

        // Get the numeric image address
        imageAddress = strtoul(imageAddressStr, NULL, 0);

        // Get the "isEncrypted" flag as a boolean
        if (
               (strstr(isEncryptedStr, "true") != NULL) ||
               (strstr(isEncryptedStr, "True") != NULL) ||
               (strstr(isEncryptedStr, "TRUE") != NULL) ||
               (strstr(isEncryptedStr, "1") != NULL)
           )
        {
            isEncrypted = true;
        }

        ret = xferImage(filenameStr,
                        destStr,
                        imageIndex,
                        imageAddress,
                        isEncrypted,
                        dfuClient);
    }

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
        printf("\r\n");
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
                *rtn = (char *)dfuToolStripQuotes(argv[i]);
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

        // ETHERNET ???
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
            else
            // CAN ???
            if (
                   (dfuToolStrnicmp(paramVal, "c", 3) == 0) ||
                   (dfuToolStristr(paramVal, "can") != NULL)
               )
            {
                // Get interface name
                if (flag_srch(argc, argv, "-iface", 1, &ifaceName))
                {
                    ret = dfuClientInit(DFUCLIENT_INTERFACE_CAN, ifaceName);
                }
            }
        }
    }

    return (ret);
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
        printf("\r\n  Negotiates the MTU with the target.");
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

    /*
    ** If no arguments provided, display the short-form "ALL"
    ** help.
    **
    */
    if (argc >= 2)
    {
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
    }
    else
    {
        printf("\r\n >> No command-line arguments provided!\r\n");
        ret = HELP_TYPE_ALL;
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

    printf("\r\n :: Available Primary Commands ::\r\n");
    while (cmdlineHandlers[index].handler != NULL)
    {
        sprintf(helpText, "\r\n '%s' ('%s')", cmdlineHandlers[index].shortForm, cmdlineHandlers[index].longForm);
        dfuToolPadStr(helpText, ' ', 20);
        strcat(helpText, ": ");
        strcat(helpText, cmdlineHandlers[index].shortHelp != NULL ? cmdlineHandlers[index].shortHelp : "No Help");
        printf("%s", helpText);
        ++index;
    }
    printf("\r\n\r\n");
    fflush(stdout);
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

static bool mainHelpHandler(int argc, char **argv)
{
    bool                            ret = false;
    helpTypeEnum                    helpType = HELP_TYPE_NONE;
    char *                          cmdForHelp = NULL;

    /*
    ** Help Requested?  Either display all help or
    ** individual command help.
    **
    */
    helpType = _getHelpType(argc, argv, &cmdForHelp);
    if (helpType == HELP_TYPE_ALL)
    {
        ret = true;
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
                printf("\r\n >> No help available.  Is this a valid command?");
            }

            ret = true;
        }
    }

    return (ret);
}


// Function to encrypt a uint32_t using a public key
uint8_t * encrypt_data_with_public_key(const char *pubkey_filename,
                                       void *valueToEncrypt,
                                       uint32_t encryptLength,
                                       bool shouldSave,
                                       const char *output_filename)
{
    uint8_t                 *ret = NULL;
    EVP_PKEY                *pkey = NULL;
    EVP_PKEY_CTX            *ctx = NULL;
    unsigned char           *encrypted_data = NULL;
    size_t                   encrypted_len = 0;

    // Initialize openSSL
    OPENSSL_init_crypto(0, NULL);
    printf("\r\nOpenSSL Version: %s", OpenSSL_version(OPENSSL_VERSION));

    // Read in the public key
    BIO * bio = BIO_new_file(pubkey_filename, "r");
    if (!bio)
    {
        printf("\r\nError creating BIO object!");
        goto cleanup;
    }

    // Read the public key into an EVP_PKEY structure
    pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    ERR_clear_error();

    // Create a context for encryption
    ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
    {
        fprintf(stderr, "Error: Failed to create EVP context: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // Initialize the context for encryption
    if (EVP_PKEY_encrypt_init(ctx) <= 0)
    {
        fprintf(stderr, "Error: Failed to initialize encryption: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // Determine the buffer length for the encrypted data
    if (EVP_PKEY_encrypt(ctx, NULL, &encrypted_len, (unsigned char *)valueToEncrypt, encryptLength) <= 0)
    {
        fprintf(stderr, "Error: Failed to determine encrypted length: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // Allocate memory for the encrypted data
    encrypted_data = malloc(encrypted_len);
    if (!encrypted_data)
    {
        fprintf(stderr, "Error: Failed to allocate memory for encrypted data.\n");
        goto cleanup;
    }

    // Perform the encryption
    if (EVP_PKEY_encrypt(ctx, encrypted_data, &encrypted_len, (unsigned char *)valueToEncrypt, encryptLength) <= 0)
    {
        fprintf(stderr, "Error: Encryption failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    //
    // Should we save the encrypted value?
    //
    if ( (shouldSave) && (output_filename != NULL) )
    {
        // Write the encrypted data to the output file
        FILE *output_file = fopen(output_filename, "wb");
        if (!output_file)
        {
            fprintf(stderr, "Error: Failed to open output file: %s\n", output_filename);
            goto cleanup;
        }
        fwrite(encrypted_data, 1, encrypted_len, output_file);
        fclose(output_file);
    }

    printf("\r\nEncryption successful.");


cleanup:
    if (bio) BIO_free(bio);
    if (pkey) EVP_PKEY_free(pkey);
    if (ctx) EVP_PKEY_CTX_free(ctx);
    if (encrypted_data)
    {
        if (shouldSave)
        {
            free(encrypted_data);
            printf(" Encrypted data written to: %s\n", output_filename);
        }
        else
            ret = encrypted_data;
    }

    EVP_cleanup();
    ERR_free_strings();

    return ret;
}

// Main function
static void testOpenSSL(void)
{
    const char      *pubkey_filename = "c:/public_key.pem"; // Path to the public key file
    const char      *output_filename = "c:/encrypted.bin"; // Output file to store encrypted data
    uint32_t         value = 123456789; // Value to encrypt

    printf("Encrypting value %u using public key from %s...\n", value, pubkey_filename);
    encrypt_data_with_public_key(pubkey_filename,
                                 &value,
                                 sizeof(value),
                                 true,
                                 output_filename);
    return;
}

