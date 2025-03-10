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
#include "path_utils.h"
#include "minIni.h"

#ifdef _WIN32
    #include <conio.h>
#else
    #include <sys/select.h>
    #include <termios.h>
    #include <unistd.h>
    #include <limits.h>
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
#include "file_kvp.h"
#include "dfu_client_crypto.h"
#include "path_utils.h"

#define APPLICATION_NAME                    ("Glydways Firmware Update Manager")

#define DFUTOOL_INI_FILENAME                ("dfutool.ini")
#define KEYHIT_DELAY_MS                     (5000)
#define DEFAULT_DEVICE_LISTEN_TIMEOUT_MS    (8000)

/*
** VERSION NUMBERS.
**
*/
#define MAJOR_VERSION                       (0)
#define MINOR_VERSION                       (5)
#define PATCH_VERSION                       (7)
#define DEFAULT_TRANSACTION_TIMEOUT_MS      (5000)

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
typedef bool (* cmdlineHandler)(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle);
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
static bool cmdlineHandlerVersion(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle);
static void versionHelpHandler(char *arg);

static bool cmdlineHandlerInstallImage(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle);
static void installImageHelpHandler(char *arg);

static bool cmdlineHandlerManifestInstall(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle);
static void installFromManifestHelpHandler(char *arg);

static bool cmdlineHandlerListDevices(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle);
static void listDevicesHelpHandler(char *arg);

static bool cmdlineHandlerInstallVehicle(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle);
static void installVehicleHelpHandler(char *arg);

/*
** The main command-line dispatch table
**
** "-i" : Install a core-image file to a target device TYPE/VARIANT.
** "-m" : Install core-images from a manifest, to a target TYPE/VARIANT
** "-v" : Install all released images to all devices in a vehicle, using
**        the vehicle manifest.
** "-d" : Display a list of devices currently in DFU mode and broadcasting.
**
*/
static cmdlineDispatchStruct cmdlineHandlers [] =
{
    {"-i", "--image", "Install a specified core-image file.\n                        Requires access to the encryption key.", installImageHelpHandler, cmdlineHandlerInstallImage},
    {"-m", "--manifest", "Installs images specified in the firmware manifest.", installFromManifestHelpHandler, cmdlineHandlerManifestInstall},
    {"-v", "--vehicle", "Install firmware on all vehicle boards.", installVehicleHelpHandler, cmdlineHandlerInstallVehicle},
    {"-d", "--devices", "Display list of devices in DFU mode", listDevicesHelpHandler, cmdlineHandlerListDevices},
    {"-ver", "--version", "Display the version of the application.", versionHelpHandler, cmdlineHandlerVersion},

    {NULL, NULL, NULL, NULL, NULL}
};

/*
** Where we keep our INI filename
**
*/
static char iniFilename[MAX_PATHUTILS_LEN];
static char scratch1[1024];
static char scratch2[1024];

/*
** Internal support prototypes
**
*/
static int flag_srch(int argc, char **argv, const char *flag, int get_value, char **rtn);
static bool initINI(void);
static dfuClientAPI* getClientAPIHandle(int argc, char **argv);
static interfaceTypeEnum getInterfaceType(int argc, char **argv);
static char* getDesiredArgumentValue(int argc,
                                     char **argv,
                                     char *desiredArg,
                                     char* iniSection,
                                     char* iniKey,
                                     char *dest,
                                     size_t destSize,
                                     bool shouldSave);

static helpTypeEnum _getHelpType(int argc, char **argv, char **cmdForHelp);
static void _allCommandsHelp(void);
static void printApplicationBanner(int argc, char** argv);
static char *getApplicationNameAndVersion(char* srcBuffer, size_t bufferSize);
static cmdlineHelpHandler _getHelpHandler(char *cmd);
static bool mainHelpHandler(int argc, char **argv);


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                          PUBLIC-FACING FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

///
/// @fn: main
///
/// @details Application entry point.  Initializes the software and then
///          examines command-line parameters to decide what the caller
///          wants to do.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
int main(int argc, char **argv)
{
    char*                    paramVal = NULL;
    ASYNC_TIMER_STRUCT       keyhitTimer;

    initINI();

    //
    // Display app banner. If the caller provided "-ver" or
    // "--version" on the command line, display basic info
    // and exit.
    //
    printApplicationBanner(argc, argv);

    // Client needed to pass something...
    if (argc > 0)
    {
        int                             index = 0;
        bool                            done = false;

        /*
        ** Checks to see if the caller wants help.  If not,
        ** try to handle standard command-line args.
        **
        */
        if (!mainHelpHandler(argc, argv))
        {
            dfuClientAPI*                   apiHandle = getClientAPIHandle(argc, argv);

            /*
            ** Look for parameters that are required for ALL uses
            ** these may come from the .INI if not provided on the
            ** command-line, but the MUST be available one of those
            ** sources
            **
            */
            if (apiHandle)
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
                        // Call the specified command
                        done = cmdlineHandlers[index].handler(argc, argv, paramVal, apiHandle);
                    }

                    ++index;
                }

                /*
                ** Pause for a few seconds and also allow the user to
                ** end now by a key press.
                **
                */
                FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
                printf("\r\n\r\n Press a key...");

                TIMER_Start(&keyhitTimer);
                do
                {
                } while ( (!_kbhit()) && (!TIMER_Finished(&keyhitTimer, KEYHIT_DELAY_MS)) );
                FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

                // Put the API handle back
                dfuClientAPIPut(apiHandle);
            }
        }
    }
    else
    {
        printf("\r\nNo command-line arguments provided!");
    }

    printf("\r\n\r\n");
    fflush(stdout);

    return 0;
}

///
/// @fn: cmdlineHandlerInstallImage
///
/// @details Sends and installs an image file to a target that
///          matches the TYPE and VARIANT from the file specidied.
///
///          "-i <image file name>"
///
///          Needs command-line values (or INI-saved values):
///
///          1. "-t <timeout in mS">   <<-- Optional
///          2. "-r <1 or true>"       <<-- Optional. If TRUE, reboots the
///                                         board after successful update.
///
///          "paramVal" must contain the name of the file to send
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static bool cmdlineHandlerInstallImage(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle)
{
    bool                    ret = true;

    if (
           (argc > 0) &&
           (argv) &&
           (paramVal) &&
           (strlen(paramVal) > 0) &&
           (apiHandle)
       )
    {
        char                timeoutStr[24];
        uint32_t            timeoutMS = DEFAULT_DEVICE_LISTEN_TIMEOUT_MS;
        apiErrorCodeEnum    err;
        char                shouldRebootStr[16];
        bool                shouldReboot = false;

        //
        // Check to see if the caller wants to reboot the
        // board after installation.
        // Do NOT save this to the INI file, since that
        // probably isn't something you want to happen
        // automatically without explicit instruction
        //
        if (getDesiredArgumentValue(argc,
                                    argv,
                                    "-r",
                                    "INSTALL_IMAGE",
                                    "should_reboot",
                                    shouldRebootStr,
                                    sizeof(shouldRebootStr),
                                    false))
        {
            if (
                  (dfuToolStricmp("true", shouldRebootStr)==0) ||
                  (dfuToolStricmp("1", shouldRebootStr)==0)
               )
            {
                shouldReboot = true;
            }
        }

        // See if there was a timeout value
        if (getDesiredArgumentValue(argc,
                                    argv,
                                    "-t",
                                    "INSTALL_IMAGE",
                                    "listen_timeout_ms",
                                    timeoutStr,
                                    sizeof(timeoutStr),
                                    true))
        {
            timeoutMS = strtoul(timeoutStr, NULL, 10);
        }

        /*
        ** See if the file name was an absolute path, or
        ** relative to where we are now.  If relative,
        ** we need to get the CWD and then add the filename
        **
        */
        if (!isAbsolutePath(paramVal))
        {
            snprintf(scratch1, sizeof(scratch1), "%s/%s", getCWD(scratch2, sizeof(scratch2)), paramVal);
        }
        else
        {
            snprintf(scratch1, sizeof(scratch1), "%s", paramVal);
        }

        // Call the library function to install the image
        err = dfuClientAPI_HL_InstallCoreImage(apiHandle,
                                               scratch1,
                                               timeoutMS,
                                               shouldReboot);
        if (err != API_ERR_NONE)
        {
            printf("\r\n Image Installation Failure: [%d]", err);
        }
    }

    return ret;
}

///
/// @fn: installImageHelpHandler
/// @details Displays help for individual firmware file installation,
///          without use of the manifest.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
static void installImageHelpHandler(char *arg)
{
    printf("\r\n");
    printf("\r\n    Installs a single image file to a target.");
    printf("\r\n    The target is determined by the board TYPE");
    printf("\r\n    and VARIANT found in the image metadata that");
    printf("\r\n    is attached to the image file.  The software");
    printf("\r\n    waits to hear from a matching device and if");
    printf("\r\n    it does hear from one, begins the transfer & ");
    printf("\r\n    installation transaction sequence.");

    printf("\r\n");
    return;
}

///
/// @fn: cmdlineHandlerManifestInstall
/// @details Installs all the files specified by the manifest,
///          whose path is provided with the command-line arguments.
///          Doesn't need the decryption key since it can determine
///          the device type from the manifest data.
///
///          This only installs the files for a SINGLE board type/variant.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
static bool cmdlineHandlerManifestInstall(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle)
{
    bool                    ret = true;

    return ret;
}

///
/// @fn: installFromManifestHelpHandler
/// @details Provides the help for board-level manifest installation.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
static void installFromManifestHelpHandler(char *arg)
{
    printf("\r\n");
    printf("\r\n    Installs any core images described in the manifest,");
    printf("\r\n    whose path is provided with the command-line arguments.");
    printf("\r\n    The device TYPE & VARIANT are specified by the manifest.");
    printf("\r\n    The program will wait to hear from devices that match.");
    printf("\r\n");
    printf("\r\n      Example: 'dfutool -m ./control_manifest.yaml'");

    printf("\r\n");
    return;
}


///
/// @fn: cmdlineHandlerListDevices
///
/// @details Listens for some period of time, displaying
///          device records that it hears being broadcast
///          on the interface.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static bool cmdlineHandlerListDevices(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle)
{
    bool                    ret = true;

    if (
           (argc > 0) &&
           (argv) &&
           (apiHandle)
       )
    {
        char                timeoutStr[24];
        uint32_t            timeoutMS = DEFAULT_DEVICE_LISTEN_TIMEOUT_MS*2;

        // See if there was a timeout value
        if (getDesiredArgumentValue(argc,
                                    argv,
                                    "-t",
                                    "DEVICE_LIST",
                                    "listen_timeout_ms",
                                    timeoutStr,
                                    sizeof(timeoutStr),
                                    true))
        {
            timeoutMS = strtoul(timeoutStr, NULL, 10);
        }

        if (timeoutMS > 0)
        {
            ASYNC_TIMER_STRUCT          timer;
            deviceInfoStruct*           deviceRecord;
            bool                        first = true;
            uint32_t                    index = 1;

            printf("\r\n Listening for DFU-mode devices...");
            TIMER_Start(&timer);
            do
            {
                if (first)
                {
                    deviceRecord = dfuClientAPI_LL_GetFirstDevice(apiHandle);
                    if (deviceRecord)
                    {
                        printf("\r                                         ");
                        first = false;
                    }
                }
                else
                {
                    deviceRecord = dfuClientAPI_LL_GetNextDevice(apiHandle);
                }

                if (deviceRecord)
                {
                    char                devAddrStr[64];

                    dfuClientAPIMacBytesToString(apiHandle,
                                                 deviceRecord->physicalID,
                                                 MAX_INTERFACE_MAC_LEN,
                                                 devAddrStr,
                                                 sizeof(devAddrStr));

                    printf("\r\n    ::: DEVICE (%2d) DESCRIPTION :::\r\n", index++);
                    printf("\r\n         Device MAC: %s", devAddrStr);
                    printf("\r\n        Device TYPE: %d", (int)deviceRecord->deviceType);
                    printf("\r\n     Device VARIANT: %d", (int)deviceRecord->deviceVariant);
                    printf("\r\n        Status Bits: 0x%02X", deviceRecord->statusBits);
                    printf("\r\n    Core Image Mask: 0x%02X", deviceRecord->coreImageMask);
                    printf("\r\n Bootloader Version: %d.%d.%d",
                           deviceRecord->blVersionMajor,
                           deviceRecord->blVersionMinor,
                           deviceRecord->blVersionPatch);
                    printf("\r\n       Last Update: %s", ctime(&deviceRecord->timestamp));
                    printf("\r\n");

                    // Refresh the timer
                    TIMER_Start(&timer);
                }
            } while (!TIMER_Finished(&timer, timeoutMS));
        }
    }

    return ret;
}

static void listDevicesHelpHandler(char *arg)
{
    printf("\r\n");
    printf("\r\n    Listens for DFU-mode broadcasts from any devices");
    printf("\r\n    on the network interface. Displays their data as");
    printf("\r\n    as each is discovered.");

    printf("\r\n");
    return;
}


static bool cmdlineHandlerInstallVehicle(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle)
{
    bool                ret = true;

    return ret;
}

static void installVehicleHelpHandler(char *arg)
{
    return;
}

///
/// @fn: cmdlineHandlerVersion
/// @details Dummy "-ver" and "--version" command handler.  The
///          "printApplicationBanner()" function actually handles
///          those arguments, but since the dispatch table needs an
///          entry in order to show help, we create these as placeholders.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
static bool cmdlineHandlerVersion(int argc, char **argv, char *paramVal, dfuClientAPI* apiHandle)
{
    return true;
}

///
/// @fn: versionHelpHandler
/// @details Displays the help for the "-ver" & "--version"
///          command.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
static void versionHelpHandler(char *arg)
{
    printf("\r\n");
    printf("\r\n    Displays the version of the program.");
    printf("\r\n");

    return;
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
    return 0;
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
static void printApplicationBanner(int argc, char** argv)
{
    char            msgStr[128];
    int             displayLastRun = 0;
    char*           paramVal;

    if (
           (
              (flag_srch(argc, argv, "-ver", true, &paramVal)) ||
              (flag_srch(argc, argv, "--version", true, &paramVal))
           ) &&
           (
              (!flag_srch(argc, argv, "-h", true, &paramVal)) &&
              (!flag_srch(argc, argv, "--help", true, &paramVal))
           )
       )
    {
        printf("\r\n%s\r\n\r\n", getApplicationNameAndVersion(scratch1, sizeof(scratch1)));
        exit(0);
    }

    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    // Display VERSION
    snprintf(scratch1,
             sizeof(scratch1),
             "\r\n:::                          Version %02d.%02d.%02d",
             MAJOR_VERSION,
             MINOR_VERSION,
             PATCH_VERSION);
    dfuToolPadStr(scratch1, ' ', 77);
    strcat(scratch1, ":::");
    printf(scratch1);

    // Display APP NAME
    snprintf(scratch1, sizeof(scratch1), "\r\n:::                   %s", APPLICATION_NAME);
    dfuToolPadStr(scratch1, ' ', 77);
    strcat(scratch1, ":::");
    printf(scratch1);

    // Copyright
    printf("\r\n::: Copyright (c) 2024, 2025, 2026 by Glydways, Inc. All Rights Reserved.  :::");

    // Display the last date/time we ran the app
    displayLastRun = ini_gets("SYSTEM",
                              "last_run",
                              "",
                              msgStr,
                              sizeof(msgStr),
                              iniFilename);
    if (displayLastRun)
    {
        dfuToolPadStr(msgStr, 0x20, 44);
        strcat(msgStr, ":::");
        printf("\r\n:::                  Last Run: %s", msgStr);
    }
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
    printf("\r\n::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    printf("\r\n");
    fflush(stdout);
}

///
/// @fn: getApplicationNameAndVersion
/// @details
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
static char *getApplicationNameAndVersion(char* srcBuffer, size_t bufferSize)
{
    char*               ret = NULL;

    if ( (srcBuffer) && (bufferSize > 0) )
    {
        snprintf(srcBuffer,
                 bufferSize,
                 "%s Version: %02d.%02d.%02d",
                 APPLICATION_NAME,
                 MAJOR_VERSION,
                 MINOR_VERSION,
                 PATCH_VERSION);
        ret = srcBuffer;
    }

    return ret;
}

///
/// @fn: getDatetimeString
///
/// @details Cross-platform routine to return a
///          quoted date-time string.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static char* getDatetimeString(char* buffer,
                               size_t bufferSize,
                               bool useUTC)
{
    if ( (!buffer) || (bufferSize < 26) )
    {
        return NULL;
    }

    time_t t = time(NULL);
    if (t == (time_t)-1)
    {
        // Call to "time()" failed
        return NULL;
    }

    struct tm timeData;

#ifdef _WIN32
    // Windows thread-safe versions
    int result = useUTC ? gmtime_s(&timeData, &t) : localtime_s(&timeData, &t);

    if (result != 0)
    {
        return NULL;
    }

    // Format the time using asctime_s (safer Windows version)
    if (asctime_s(buffer, bufferSize, &timeData) != 0)
    {
        return NULL;
    }
#else
    // POSIX systems
    struct tm*      timePtr = useUTC ? gmtime(&t) : localtime(&t);

    if (!timePtr)
    {
        return NULL;
    }

    // Make a copy to ensure thread safety
    memcpy(&timeData, timePtr, sizeof(struct tm));

    // Format using asctime and copy to buffer
    const char* asctimeStr = asctime(&timeData);
    if (!asctimeStr)
    {
        return NULL;
    }

    strncpy(buffer, asctimeStr, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
#endif

    // Remove trailing newline that asctime adds
    size_t len = strlen(buffer);
    if ( (len > 0) && (buffer[len-1] == '\n') )
    {
        buffer[len-1] = '\0';
        len--;
    }

    return buffer;
}

///
/// @fn: initINI
///
/// @details Determines the path to the INI file, then if it
///          doesn't exist, creates it.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static bool initINI(void)
{
    bool            ret = false;
    FILE*           handle = NULL;

    //
    // Set the path and name of the INI file for
    // use here and later on.
    //
    getDirectory(getExecutablePath(), iniFilename, sizeof(iniFilename));
    strncat(iniFilename, DFUTOOL_INI_FILENAME, sizeof(iniFilename)-1);

    handle = fopen(iniFilename, "a+");
    if (handle)
    {
        char            dateTimeStr[64];

        fclose(handle);

        // Get the date/time as a string
        if (getDatetimeString(dateTimeStr, sizeof(dateTimeStr), false))
        {
            if (ini_puts("SYSTEM", "last_run", dateTimeStr, iniFilename))
            {
                ret = true;
            }
        }
    }

    return ret;
}

///
/// @fn: getClientAPIHandle
///
/// @details If it can get all the parameters it needs from
///          either command-line arguments or the INI file,
///          this will initialize the client API library and
///          returns its address.
///
///          We need the following keys from somewhere:
///
///          1. "-e", "-c", or "-u" for the INTERFACE type
///          2. "-n" <value> for the INTERFACE *NAME*
///          3. "-rsa" <path/name> for the Public RSA challenge key
///          4. "-aes" <path/name> for the AES encryption/decryption key.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static dfuClientAPI* getClientAPIHandle(int argc, char **argv)
{
    dfuClientAPI*               ret = NULL;

    if ( (argc > 0) && (argv) )
    {
        interfaceTypeEnum       iface;

        // Get the interface
        iface = getInterfaceType(argc, argv);
        if (iface != INTERFACE_TYPE_NONE)
        {
            char        interfaceName[64];

            // Now get the interface name
            if (getDesiredArgumentValue(argc,
                                        argv,
                                        "-n",
                                        "SYSTEM",
                                        "interface_name",
                                        interfaceName,
                                        sizeof(interfaceName),
                                        true))
            {
                // Now get the RSA key path
                if (getDesiredArgumentValue(argc,
                                            argv,
                                            "-rsa",
                                            "SYSTEM",
                                            "rsa_keypath",
                                            scratch1,
                                            sizeof(scratch1),
                                            true))
                {
                    // Now get the RSA key path
                    if (getDesiredArgumentValue(argc,
                                                argv,
                                                "-aes",
                                                "SYSTEM",
                                                "aes_keypath",
                                                scratch2,
                                                sizeof(scratch2),
                                                true))
                    {
                        ret = dfuClientAPIGet(iface,
                                              interfaceName,
                                              scratch1,
                                              scratch2);
                    }
                }
            }
        }
    }

    return ret;
}

///
/// @fn: getInterfaceType
///
/// @details Looks for the desired interface TYPE, as one of the
///          following keys (with no value):
///
///          "-e": ETHERNET
///          "-c": CAN
///          "-u": UART
///
///           If not found on the command-line, look in the INI file
///           If not found there, return type NONE. If one was found
///           in the command-line, save it to the INI
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static interfaceTypeEnum getInterfaceType(int argc, char **argv)
{
    interfaceTypeEnum           ret = INTERFACE_TYPE_NONE;

    if ( (argc > 0) && (argv) )
    {
        char*                   paramVal = NULL;
        char                    interfaceKey[32];
        bool                    shouldSave = true;

        // Ethernet?
        if (flag_srch(argc, argv, "-e", 1, &paramVal))
        {
            snprintf(interfaceKey, sizeof(interfaceKey), "-e");
        }
        else
        // CAN?
        if (flag_srch(argc, argv, "-c", 1, &paramVal))
        {
            snprintf(interfaceKey, sizeof(interfaceKey), "-c");
        }
        else
        // UART?
        if (flag_srch(argc, argv, "-c", 1, &paramVal))
        {
            snprintf(interfaceKey, sizeof(interfaceKey), "-u");
        }
        else
        {
            shouldSave = false;

            // Look in the INI file
            if (!ini_gets("SYSTEM",
                          "interface_type",
                          "",
                          interfaceKey,
                          sizeof(interfaceKey),
                          iniFilename))
            {
                return ret;
            }
        }

        /*
        ** Decide which interface based on the key
        **
        */
        if (strncmp(interfaceKey, "-e", sizeof(interfaceKey))==0)
        {
            ret = INTERFACE_TYPE_ETHERNET;
        }
        else
        if (strncmp(interfaceKey, "-c", sizeof(interfaceKey))==0)
        {
            ret = INTERFACE_TYPE_CAN;
        }
        else
        if (strncmp(interfaceKey, "-u", sizeof(interfaceKey))==0)
        {
            ret = INTERFACE_TYPE_UART;
        }

        // Save the value to INI, if needed
        if (shouldSave)
        {
            ini_puts("SYSTEM", "interface_type", interfaceKey, iniFilename);
        }
    }

    return ret;
}

///
/// @fn: getDesiredArgumentValue
///
/// @details Given a string that specifies the key to look
///          for in the command-line arguments, this will
///          attempt to find it and return the string value
///          in the caller's buffer.  If it can't be found
///          in the command-line, looks in the .INI file
///          for a matching key name in the SECTION
///          specified in the arguments.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
static char* getDesiredArgumentValue(int argc,
                                     char **argv,
                                     char *desiredArg,
                                     char* iniSection,
                                     char* iniKey,
                                     char *dest,
                                     size_t destSize,
                                     bool shouldSave)
{
    char*                   ret = NULL;

    if (
           (argc > 0) &&
           (argv) &&
           (desiredArg) &&
           (iniSection) &&
           (iniKey) &&
           (dest) &&
           (destSize > 0)
       )
    {
        char*               paramVal = NULL;
        bool                mustSave = false;

        if (flag_srch(argc, argv, desiredArg, 1, &paramVal))
        {
            snprintf(dest, destSize, "%s", paramVal);
            mustSave = true;
            ret = dest;
        }
        else
        {
            // See if it's in the INI
            if (ini_gets(iniSection, iniKey, "", dest, destSize, iniFilename) > 0)
            {
                ret = dest;
            }
        }

        if ( (ret) && (shouldSave) && (mustSave) )
        {
            ini_puts(iniSection, iniKey, dest, iniFilename);
        }
    }

    return ret;
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

    printf("\r\n :: Available Commands ::\r\n");
    while (cmdlineHandlers[index].handler != NULL)
    {
        snprintf(helpText, sizeof(helpText), "\r\n '%s'", cmdlineHandlers[index].shortForm);
        dfuToolPadStr(helpText, ' ', 10);
        snprintf(helpText+strlen(helpText), sizeof(helpText), "(%s)", cmdlineHandlers[index].longForm);
        dfuToolPadStr(helpText, ' ', 24);
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

///
/// @fn: mainHelpHandler
///
/// @details Deals with help for all the commands.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
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
