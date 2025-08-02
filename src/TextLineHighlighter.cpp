/*******************************************************************************
 * FILENAME: TextLineHighlighter.cpp
 *
 * PROJECT:
 *    Whippy Term
 *
 * FILE DESCRIPTION:
 *    This has a display processor that highlights incoming messages.
 *    It is line based.
 *
 * COPYRIGHT:
 *    Copyright 01 Aug 2025 Paul Hutchinson.
 *
 *    This program is free software: you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation, either version 3 of the License, or (at your
 *    option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program. If not, see https://www.gnu.org/licenses/.
 *
 * CREATED BY:
 *    Paul Hutchinson (01 Aug 2025)
 *
 ******************************************************************************/

/*** HEADER FILES TO INCLUDE  ***/
#include "TextLineHighlighter.h"
#include "PluginSDK/Plugin.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

using namespace std;

/*** DEFINES                  ***/
#define REGISTER_PLUGIN_FUNCTION_PRIV_NAME      TextLineHighlighter // The name to append on the RegisterPlugin() function for built in version
#define NEEDED_MIN_API_VERSION                  0x02000000
//#define COLLECTED_BUFF_GROW_SIZE                1024
#define COLLECTED_BUFF_GROW_SIZE                5

/*** MACROS                   ***/

/*** TYPE DEFINITIONS         ***/
struct TextLineHighlighterData
{
    t_DataProMark *StartOfLineMarker;

    uint8_t *CollectedBytes;    // DEBUG PAUL: No longer used?
    int CollectedBytesBuffSize;    // DEBUG PAUL: No longer used?
    int NumOfCollectedBytes;    // DEBUG PAUL: No longer used?
    int NumOfCollectedChars;    // DEBUG PAUL: No longer used?

    bool UseSimpleHighlighters;
    string SimpleHighlighterStartsWith;
    string SimpleHighlighterEndsWith;
    string SimpleHighlighterContains;

    bool RegexRemoveEnabled;
    string RegexRemoveHighlighter1;
    string RegexRemoveHighlighter2;
    string RegexRemoveHighlighter3;
    string RegexRemoveHighlighter4;
    string RegexRemoveHighlighter5;

    bool RegexIncludeEnabled;
    string RegexIncludeHighlighter1;
    string RegexIncludeHighlighter2;
    string RegexIncludeHighlighter3;
    string RegexIncludeHighlighter4;
    string RegexIncludeHighlighter5;
};

struct TextLineHighlighter_SettingsWidgets
{
    t_WidgetSysHandle *HelpTabHandle;
    t_WidgetSysHandle *SimpleTabHandle;
    t_WidgetSysHandle *RegexTabHandle;

    struct PI_TextBox *HelpText;

    struct PI_Checkbox *UseSimpleHighlightering;
    struct PI_TextInput *SimpleHighlighterStartsWith;
    struct PI_TextInput *SimpleHighlighterEndsWith;
    struct PI_TextInput *SimpleHighlighterContains;
    struct PI_TextBox *SimpleHelpText;

    struct PI_Checkbox *UseRegexHighlightering;
    struct PI_GroupBox *RegexRemoveGroup;
    struct PI_Checkbox *RegexRemoveEnabled;
    struct PI_TextInput *RegexRemoveHighlighter1;
    struct PI_TextInput *RegexRemoveHighlighter2;
    struct PI_TextInput *RegexRemoveHighlighter3;
    struct PI_TextInput *RegexRemoveHighlighter4;
    struct PI_TextInput *RegexRemoveHighlighter5;
    struct PI_GroupBox *RegexIncludeGroup;
    struct PI_Checkbox *RegexIncludeEnabled;
    struct PI_TextInput *RegexIncludeHighlighter1;
    struct PI_TextInput *RegexIncludeHighlighter2;
    struct PI_TextInput *RegexIncludeHighlighter3;
    struct PI_TextInput *RegexIncludeHighlighter4;
    struct PI_TextInput *RegexIncludeHighlighter5;
};

/*** FUNCTION PROTOTYPES      ***/
const struct DataProcessorInfo *TextLineHighlighter_GetProcessorInfo(
        unsigned int *SizeOfInfo);
void TextLineHighlighter_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,PG_BOOL *Consumed);
static t_DataProSettingsWidgetsType *TextLineHighlighter_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings);
static void TextLineHighlighter_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData);
static void TextLineHighlighter_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings);
static void TextLineHighlighter_ApplySettings(t_DataProcessorHandleType *ConDataHandle,
        t_PIKVList *Settings);
static t_DataProcessorHandleType *TextLineHighlighter_AllocateData(void);
static void TextLineHighlighter_FreeData(t_DataProcessorHandleType *DataHandle);

/*** VARIABLE DEFINITIONS     ***/
struct DataProcessorAPI m_TextLineHighlighterCBs=
{
    TextLineHighlighter_AllocateData,
    TextLineHighlighter_FreeData,
    TextLineHighlighter_GetProcessorInfo,               // GetProcessorInfo
    NULL,                                               // ProcessKeyPress
    TextLineHighlighter_ProcessIncomingTextByte,        // ProcessIncomingTextByte
    NULL,                                               // ProcessIncomingBinaryByte
    /* V2 */
    NULL,                                   // ProcessOutGoingData
    TextLineHighlighter_AllocSettingsWidgets,
    TextLineHighlighter_FreeSettingsWidgets,
    TextLineHighlighter_SetSettingsFromWidgets,
    TextLineHighlighter_ApplySettings,
};


struct DataProcessorInfo m_TextLineHighlighter_Info=
{
    "Text Line Highlighter",
    "Highlights lines using regex's or a simple string matching.",
    "Highlights lines using regex's or a simple string matching.",
    e_DataProcessorType_Text,
    .TxtClass=e_TextDataProcessorClass_Other,
    e_BinaryDataProcessorMode_Hex
};

static const struct PI_SystemAPI *m_TLF_SysAPI;
static const struct DPS_API *m_TLF_DPS;
static const struct PI_UIAPI *m_TLF_UIAPI;

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_RegisterPlugin
 *
 * SYNOPSIS:
 *    unsigned int TextLineHighlighter_RegisterPlugin(const struct PI_SystemAPI *SysAPI,
 *          unsigned int Version);
 *
 * PARAMETERS:
 *    SysAPI [I] -- The main API to WhippyTerm
 *    Version [I] -- What version of WhippyTerm is running.  This is used
 *                   to make sure we are compatible.  This is in the
 *                   Major<<24 | Minor<<16 | Rev<<8 | Patch format
 *
 * FUNCTION:
 *    This function registers this plugin with the system.
 *
 * RETURNS:
 *    0 if we support this version of WhippyTerm, and the minimum version
 *    we need if we are not.
 *
 * NOTES:
 *    This function is normally is called from the RegisterPlugin() when
 *    it is being used as a normal plugin.  As a std plugin it is called
 *    from RegisterStdPlugins() instead.
 *
 * SEE ALSO:
 *    
 ******************************************************************************/
/* This needs to be extern "C" because it is the main entry point for the
   plugin system */
extern "C"
{
    unsigned int REGISTER_PLUGIN_FUNCTION(const struct PI_SystemAPI *SysAPI,
            unsigned int Version)
    {
        if(Version<NEEDED_MIN_API_VERSION)
            return NEEDED_MIN_API_VERSION;

        m_TLF_SysAPI=SysAPI;
        m_TLF_DPS=SysAPI->GetAPI_DataProcessors();
        m_TLF_UIAPI=m_TLF_DPS->GetAPI_UI();

        /* If we are have the correct experimental API */
        if(SysAPI->GetExperimentalID()>0 &&
                SysAPI->GetExperimentalID()<1)
        {
            return 0xFFFFFFFF;
        }

        m_TLF_DPS->RegisterDataProcessor("TextLineHighlighter",
                &m_TextLineHighlighterCBs,sizeof(m_TextLineHighlighterCBs));

        return 0;
    }
}

static t_DataProcessorHandleType *TextLineHighlighter_AllocateData(void)
{
    struct TextLineHighlighterData *Data;

    try
    {
        Data=new struct TextLineHighlighterData;
        if(Data==NULL)
            return NULL;

        Data->CollectedBytes=(uint8_t *)malloc(COLLECTED_BUFF_GROW_SIZE);
        if(Data->CollectedBytes==NULL)
            throw(0);
        Data->CollectedBytesBuffSize=COLLECTED_BUFF_GROW_SIZE;
        Data->NumOfCollectedBytes=0;
        Data->NumOfCollectedChars=0;
        Data->StartOfLineMarker=NULL;
        Data->TestMark1=NULL;
    }
    catch(...)
    {
        if(Data!=NULL)
        {
            if(Data->CollectedBytes!=NULL)
                free(Data->CollectedBytes);

            delete Data;
        }
        return NULL;
    }

    return (t_DataProcessorHandleType *)Data;
}

static void TextLineHighlighter_FreeData(t_DataProcessorHandleType *DataHandle)
{
    struct TextLineHighlighterData *Data=(struct TextLineHighlighterData *)DataHandle;

    if(Data->StartOfLineMarker!=NULL)
        m_TLF_DPS->FreeMark(Data->StartOfLineMarker);
    if(Data->TestMark1!=NULL)
        m_TLF_DPS->FreeMark(Data->TestMark1);

    free(Data->CollectedBytes);

    delete Data;
}

/*******************************************************************************
 * NAME:
 *    TextLineHighlighter_GetProcessorInfo
 *
 * SYNOPSIS:
 *    const struct DataProcessorInfo *TextLineHighlighter_GetProcessorInfo(
 *              unsigned int *SizeOfInfo);
 *
 * PARAMETERS:
 *    SizeOfInfo [O] -- The size of 'struct DataProcessorInfo'.  This is used
 *                        for forward / backward compatibility.
 *
 * FUNCTION:
 *    This function gets info about the plugin.  'DataProcessorInfo' has
 *    the following fields:
 *          DisplayName -- The name we show the user
 *          Tip -- A tool tip (for when you hover the mouse over this plugin)
 *          Help -- A help string for this plugin.
 *          ProType -- The type of process.  Supported values:
 *                  e_DataProcessorType_Text -- This is a text processor.
 *                      This is the more clasic type of processor (like VT100).
 *                  e_DataProcessorType_Binary -- This is a binary processor.
 *                      These are processors for binary protocol.  This may
 *                      be something as simple as a hex dump.
 *          TxtClass -- This only applies to 'e_DataProcessorType_Text' type
 *              processors. This is what class of text processor is
 *              this.  Supported classes:
 *                      e_TextDataProcessorClass_Other -- This is a generic class
 *                          more than one of these processors can be active
 *                          at a time but no other requirements exist.
 *                      e_TextDataProcessorClass_CharEncoding -- This is a
 *                          class that converts the raw stream into some kind
 *                          of char encoding.  For example unicode is converted
 *                          from a number of bytes to chars in the system.
 *                      e_TextDataProcessorClass_TermEmulation -- This is a
 *                          type of terminal emulator.  An example of a
 *                          terminal emulator is VT100.
 *                      e_TextDataProcessorClass_Highlighter -- This is a processor
 *                          that highlights strings as they come in the input
 *                          stream.  For example a processor that underlines
 *                          URL's.
 *                      e_TextDataProcessorClass_Logger -- This is a processor
 *                          that saves the input.  It may save to a file or
 *                          send out a debugging service.  And example is
 *                          a processor that saves all the raw bytes to a file.
 * SEE ALSO:
 *    
 ******************************************************************************/
const struct DataProcessorInfo *TextLineHighlighter_GetProcessorInfo(
        unsigned int *SizeOfInfo)
{
    *SizeOfInfo=sizeof(struct DataProcessorInfo);
    return &m_TextLineHighlighter_Info;
}

/*******************************************************************************
 * NAME:
 *   ProcessIncomingTextByte
 *
 * SYNOPSIS:
 *   void ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
 *       const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
 *       PG_BOOL *Consumed);
 *
 * PARAMETERS:
 *   DataHandle [I] -- The data handle to work on.  This is your internal
 *                     data.
 *   RawByte [I] -- This is the byte that came in.
 *   ProcessedChar [I/O] -- This is a unicode char that has already been
 *                        processed by some of the other input filters.  You
 *                        can change this as you need.  It must remain only
 *                        one unicode char.
 *   CharLen [I/O] -- This number of bytes in 'ProcessedChar'
 *   Consumed [I/O] -- This tells the system (and other filters) if the
 *                     char has been used up and will not be added to the
 *                     screen.
 *
 * FUNCTION:
 *   This function is called for each byte that comes in if you are a
 *   'e_DataProcessorType_Text' type of processor.  You work on the
 *   'ProcessedChar' to change the byte as needed.
 *
 *   If you set 'Consumed' to true then the 'ProcessedChar' will not be added
 *   to the display (or passed to other processors).  If it is set to
 *   false then it will be added to the screen.
 *
 * RETURNS:
 *   NONE
 ******************************************************************************/
void TextLineHighlighter_ProcessIncomingTextByte(t_DataProcessorHandleType *DataHandle,
        const uint8_t RawByte,uint8_t *ProcessedChar,int *CharLen,
        PG_BOOL *Consumed)
{
    struct TextLineHighlighterData *Data=(struct TextLineHighlighterData *)DataHandle;

    /* If we haven't allocated a marker yet, then we need to (we can't
       allocate this in the AllocateData() because the m_TLF_DPS API doesn't
       work in that function) */
    if(Data->StartOfLineMarker==NULL)
    {
        Data->StartOfLineMarker=m_TLF_DPS->AllocateMark();
        if(Data->StartOfLineMarker==NULL)
            return;
    }
}

static void TextLineHighlighter_SimpleHighlighteringEventCB(const struct PICheckboxEvent *Event,void *UserData)
{
    struct TextLineHighlighter_SettingsWidgets *WData=(struct TextLineHighlighter_SettingsWidgets *)UserData;

    m_TLF_UIAPI->SetCheckboxChecked(WData->SimpleTabHandle,
            WData->UseSimpleHighlightering->Ctrl,true);
    m_TLF_UIAPI->SetCheckboxChecked(WData->RegexTabHandle,
            WData->UseRegexHighlightering->Ctrl,false);
}

static void TextLineHighlighter_RegexHighlighteringEventCB(const struct PICheckboxEvent *Event,void *UserData)
{
    struct TextLineHighlighter_SettingsWidgets *WData=(struct TextLineHighlighter_SettingsWidgets *)UserData;

    m_TLF_UIAPI->SetCheckboxChecked(WData->SimpleTabHandle,
            WData->UseSimpleHighlightering->Ctrl,false);
    m_TLF_UIAPI->SetCheckboxChecked(WData->RegexTabHandle,
            WData->UseRegexHighlightering->Ctrl,true);
}

t_DataProSettingsWidgetsType *TextLineHighlighter_AllocSettingsWidgets(t_WidgetSysHandle *WidgetHandle,t_PIKVList *Settings)
{
    struct TextLineHighlighter_SettingsWidgets *WData;
    const char *UseSimpleHighlighteringStr;
    bool UseSimpleHighlightering;
    const char *SimpleHighlighter_StartingWith;
    const char *SimpleHighlighter_EndingWith;
    const char *SimpleHighlighter_Contains;
    const char *RegexHighlighter_RemoveEnabledStr;
    bool RegexHighlighter_RemoveEnabled;
    const char *RegexHighlighter_RemoveHighlighter1;
    const char *RegexHighlighter_RemoveHighlighter2;
    const char *RegexHighlighter_RemoveHighlighter3;
    const char *RegexHighlighter_RemoveHighlighter4;
    const char *RegexHighlighter_RemoveHighlighter5;
    const char *RegexHighlighter_IncludeEnabledStr;
    bool RegexHighlighter_IncludeEnabled;
    const char *RegexHighlighter_IncludeHighlighter1;
    const char *RegexHighlighter_IncludeHighlighter2;
    const char *RegexHighlighter_IncludeHighlighter3;
    const char *RegexHighlighter_IncludeHighlighter4;
    const char *RegexHighlighter_IncludeHighlighter5;

    try
    {
        WData=new TextLineHighlighter_SettingsWidgets;
        WData->HelpTabHandle=NULL;
        WData->SimpleTabHandle=NULL;
        WData->RegexTabHandle=NULL;
        WData->HelpText=NULL;
        WData->UseSimpleHighlightering=NULL;
        WData->SimpleHighlighterStartsWith=NULL;
        WData->SimpleHighlighterEndsWith=NULL;
        WData->SimpleHighlighterContains=NULL;
        WData->SimpleHelpText=NULL;
        WData->UseRegexHighlightering=NULL;
        WData->RegexRemoveGroup=NULL;
        WData->RegexRemoveEnabled=NULL;
        WData->RegexRemoveHighlighter1=NULL;
        WData->RegexRemoveHighlighter2=NULL;
        WData->RegexRemoveHighlighter3=NULL;
        WData->RegexRemoveHighlighter4=NULL;
        WData->RegexRemoveHighlighter5=NULL;
        WData->RegexIncludeGroup=NULL;
        WData->RegexIncludeEnabled=NULL;
        WData->RegexIncludeHighlighter1=NULL;
        WData->RegexIncludeHighlighter2=NULL;
        WData->RegexIncludeHighlighter3=NULL;
        WData->RegexIncludeHighlighter4=NULL;
        WData->RegexIncludeHighlighter5=NULL;

        m_TLF_DPS->SetCurrentSettingsTabName("Help");
        WData->HelpTabHandle=WidgetHandle;

        WData->HelpText=m_TLF_UIAPI->AddTextBox(WData->HelpTabHandle,NULL,
                "This plugin highlights lines out in the incoming stream.\n"
                "\n"
                "You can use a simple filter or more complex rxgex's\n"
                "\n"
                "You can only use simple or rxgex filtering not both at "
                "the same time.  When you enable simple it will disable "
                "rxgex and enabling rxgex will disable simple.");
        if(WData->HelpText==NULL)
            throw(0);

        WData->SimpleTabHandle=m_TLF_DPS->AddNewSettingsTab("Simple");
        if(WData->SimpleTabHandle==NULL)
            throw(0);
        WData->UseSimpleHighlightering=m_TLF_UIAPI->AddCheckbox(WData->
                SimpleTabHandle,"Use simple filtering",
                TextLineHighlighter_SimpleHighlighteringEventCB,(void *)WData);
        if(WData->UseSimpleHighlightering==NULL)
            throw(0);

        WData->SimpleHighlighterStartsWith=m_TLF_UIAPI->
                AddTextInput(WData->SimpleTabHandle,
                "Highlighter Lines that start with",NULL,NULL);
        if(WData->SimpleHighlighterStartsWith==NULL)
            throw(0);

        WData->SimpleHighlighterEndsWith=m_TLF_UIAPI->
                AddTextInput(WData->SimpleTabHandle,
                "Highlighter Lines that end with",NULL,NULL);
        if(WData->SimpleHighlighterEndsWith==NULL)
            throw(0);

        WData->SimpleHighlighterContains=m_TLF_UIAPI->
                AddTextInput(WData->SimpleTabHandle,
                "Highlighter Lines that contain",NULL,NULL);
        if(WData->SimpleHighlighterContains==NULL)
            throw(0);

        WData->SimpleHelpText=m_TLF_UIAPI->AddTextBox(WData->SimpleTabHandle,
                NULL,
                "All inputs are a list of words that will be matched, "
                "seperated by spaces.\n"
                "\n"
                "If you want to include spaces in your matched word put "
                "the string in quote's.\n"
                "\n"
                "If you want to include a quote in your matched word "
                "prefix it with a backslash (\\)\n"
                "\n"
                "If you want to include a backslash in your matched "
                "word use two backslashes (\\\\)");
        if(WData->SimpleHelpText==NULL)
            throw(0);

        WData->RegexTabHandle=m_TLF_DPS->AddNewSettingsTab("Regex");
        if(WData->RegexTabHandle==NULL)
            throw(0);
        WData->UseRegexHighlightering=m_TLF_UIAPI->
                AddCheckbox(WData->RegexTabHandle,
                "Use regex filtering",
                TextLineHighlighter_RegexHighlighteringEventCB,(void *)WData);
        if(WData->UseRegexHighlightering==NULL)
            throw(0);

        WData->RegexRemoveGroup=m_TLF_UIAPI->AddGroupBox(WData->RegexTabHandle,
                "Remove lines matching");
        if(WData->RegexRemoveGroup==NULL)
            throw(0);
        WData->RegexRemoveEnabled=m_TLF_UIAPI->AddCheckbox(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                "Enabled",NULL,NULL);
        if(WData->RegexRemoveEnabled==NULL)
            throw(0);
        WData->RegexRemoveHighlighter1=m_TLF_UIAPI->AddTextInput(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                "Highlighter 1",NULL,NULL);
        if(WData->RegexRemoveHighlighter1==NULL)
            throw(0);
        WData->RegexRemoveHighlighter2=m_TLF_UIAPI->AddTextInput(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                "Highlighter 2",NULL,NULL);
        if(WData->RegexRemoveHighlighter2==NULL)
            throw(0);
        WData->RegexRemoveHighlighter3=m_TLF_UIAPI->AddTextInput(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                "Highlighter 3",NULL,NULL);
        if(WData->RegexRemoveHighlighter3==NULL)
            throw(0);
        WData->RegexRemoveHighlighter4=m_TLF_UIAPI->AddTextInput(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                "Highlighter 4",NULL,NULL);
        if(WData->RegexRemoveHighlighter4==NULL)
            throw(0);
        WData->RegexRemoveHighlighter5=m_TLF_UIAPI->AddTextInput(WData->
                RegexRemoveGroup->GroupWidgetHandle,
                "Highlighter 5",NULL,NULL);
        if(WData->RegexRemoveHighlighter5==NULL)
            throw(0);

        WData->RegexIncludeGroup=m_TLF_UIAPI->AddGroupBox(WData->RegexTabHandle,
                "Only include lines matching");
        if(WData->RegexIncludeGroup==NULL)
            throw(0);
        WData->RegexIncludeEnabled=m_TLF_UIAPI->AddCheckbox(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                "Enabled",NULL,NULL);
        if(WData->RegexIncludeEnabled==NULL)
            throw(0);
        WData->RegexIncludeHighlighter1=m_TLF_UIAPI->AddTextInput(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                "Highlighter 1",NULL,NULL);
        if(WData->RegexIncludeHighlighter1==NULL)
            throw(0);
        WData->RegexIncludeHighlighter2=m_TLF_UIAPI->AddTextInput(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                "Highlighter 2",NULL,NULL);
        if(WData->RegexIncludeHighlighter2==NULL)
            throw(0);
        WData->RegexIncludeHighlighter3=m_TLF_UIAPI->AddTextInput(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                "Highlighter 3",NULL,NULL);
        if(WData->RegexIncludeHighlighter3==NULL)
            throw(0);
        WData->RegexIncludeHighlighter4=m_TLF_UIAPI->AddTextInput(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                "Highlighter 4",NULL,NULL);
        if(WData->RegexIncludeHighlighter4==NULL)
            throw(0);
        WData->RegexIncludeHighlighter5=m_TLF_UIAPI->AddTextInput(WData->
                RegexIncludeGroup->GroupWidgetHandle,
                "Highlighter 5",NULL,NULL);
        if(WData->RegexIncludeHighlighter5==NULL)
            throw(0);

        /* Set UI to settings values */
        UseSimpleHighlighteringStr=m_TLF_SysAPI->KVGetItem(Settings,"UseSimpleHighlightering");
        SimpleHighlighter_StartingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleHighlighter_StartingWith");
        SimpleHighlighter_EndingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleHighlighter_EndingWith");
        SimpleHighlighter_Contains=m_TLF_SysAPI->KVGetItem(Settings,"SimpleHighlighter_Contains");
        RegexHighlighter_RemoveEnabledStr=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveEnabled");
        RegexHighlighter_RemoveHighlighter1=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter1");
        RegexHighlighter_RemoveHighlighter2=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter2");
        RegexHighlighter_RemoveHighlighter3=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter3");
        RegexHighlighter_RemoveHighlighter4=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter4");
        RegexHighlighter_RemoveHighlighter5=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter5");
        RegexHighlighter_IncludeEnabledStr=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeEnabled");
        RegexHighlighter_IncludeHighlighter1=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter1");
        RegexHighlighter_IncludeHighlighter2=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter2");
        RegexHighlighter_IncludeHighlighter3=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter3");
        RegexHighlighter_IncludeHighlighter4=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter4");
        RegexHighlighter_IncludeHighlighter5=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter5");

        /* Set defaults */
        if(UseSimpleHighlighteringStr==NULL)
            UseSimpleHighlighteringStr="1";
        if(SimpleHighlighter_StartingWith==NULL)
            SimpleHighlighter_StartingWith="";
        if(SimpleHighlighter_EndingWith==NULL)
            SimpleHighlighter_EndingWith="";
        if(SimpleHighlighter_Contains==NULL)
            SimpleHighlighter_Contains="";
        if(RegexHighlighter_RemoveEnabledStr==NULL)
            RegexHighlighter_RemoveEnabledStr="0";
        if(RegexHighlighter_RemoveHighlighter1==NULL)
            RegexHighlighter_RemoveHighlighter1="";
        if(RegexHighlighter_RemoveHighlighter2==NULL)
            RegexHighlighter_RemoveHighlighter2="";
        if(RegexHighlighter_RemoveHighlighter3==NULL)
            RegexHighlighter_RemoveHighlighter3="";
        if(RegexHighlighter_RemoveHighlighter4==NULL)
            RegexHighlighter_RemoveHighlighter4="";
        if(RegexHighlighter_RemoveHighlighter5==NULL)
            RegexHighlighter_RemoveHighlighter5="";
        if(RegexHighlighter_IncludeEnabledStr==NULL)
            RegexHighlighter_IncludeEnabledStr="0";
        if(RegexHighlighter_IncludeHighlighter1==NULL)
            RegexHighlighter_IncludeHighlighter1="";
        if(RegexHighlighter_IncludeHighlighter2==NULL)
            RegexHighlighter_IncludeHighlighter2="";
        if(RegexHighlighter_IncludeHighlighter3==NULL)
            RegexHighlighter_IncludeHighlighter3="";
        if(RegexHighlighter_IncludeHighlighter4==NULL)
            RegexHighlighter_IncludeHighlighter4="";
        if(RegexHighlighter_IncludeHighlighter5==NULL)
            RegexHighlighter_IncludeHighlighter5="";

        UseSimpleHighlightering=atoi(UseSimpleHighlighteringStr);
        RegexHighlighter_RemoveEnabled=atoi(RegexHighlighter_RemoveEnabledStr);
        RegexHighlighter_IncludeEnabled=atoi(RegexHighlighter_IncludeEnabledStr);

        /* Set the widgets */
        m_TLF_UIAPI->SetCheckboxChecked(WData->SimpleTabHandle,
                WData->UseSimpleHighlightering->Ctrl,UseSimpleHighlightering);
        m_TLF_UIAPI->SetCheckboxChecked(WData->RegexTabHandle,
                WData->UseRegexHighlightering->Ctrl,!UseSimpleHighlightering);

        /** Simple **/
        m_TLF_UIAPI->SetTextInputText(WData->SimpleTabHandle,
                WData->SimpleHighlighterStartsWith->Ctrl,SimpleHighlighter_StartingWith);
        m_TLF_UIAPI->SetTextInputText(WData->SimpleTabHandle,
                WData->SimpleHighlighterEndsWith->Ctrl,SimpleHighlighter_EndingWith);
        m_TLF_UIAPI->SetTextInputText(WData->SimpleTabHandle,
                WData->SimpleHighlighterContains->Ctrl,SimpleHighlighter_Contains);

        /** Regex **/
        /*** Remove ***/
        m_TLF_UIAPI->SetCheckboxChecked(WData->RegexRemoveGroup->
                GroupWidgetHandle,WData->RegexRemoveEnabled->Ctrl,
                RegexHighlighter_RemoveEnabled);
        m_TLF_UIAPI->SetTextInputText(WData->RegexRemoveGroup->
                GroupWidgetHandle,WData->RegexRemoveHighlighter1->Ctrl,
                RegexHighlighter_RemoveHighlighter1);
        m_TLF_UIAPI->SetTextInputText(WData->RegexRemoveGroup->
                GroupWidgetHandle,WData->RegexRemoveHighlighter2->Ctrl,
                RegexHighlighter_RemoveHighlighter2);
        m_TLF_UIAPI->SetTextInputText(WData->RegexRemoveGroup->
                GroupWidgetHandle,WData->RegexRemoveHighlighter3->Ctrl,
                RegexHighlighter_RemoveHighlighter3);
        m_TLF_UIAPI->SetTextInputText(WData->RegexRemoveGroup->
                GroupWidgetHandle,WData->RegexRemoveHighlighter4->Ctrl,
                RegexHighlighter_RemoveHighlighter4);
        m_TLF_UIAPI->SetTextInputText(WData->RegexRemoveGroup->
                GroupWidgetHandle,WData->RegexRemoveHighlighter5->Ctrl,
                RegexHighlighter_RemoveHighlighter5);

        /*** Include ***/
        m_TLF_UIAPI->SetCheckboxChecked(WData->RegexIncludeGroup->
                GroupWidgetHandle,WData->RegexIncludeEnabled->Ctrl,
                RegexHighlighter_IncludeEnabled);
        m_TLF_UIAPI->SetTextInputText(WData->RegexIncludeGroup->
                GroupWidgetHandle,WData->RegexIncludeHighlighter1->Ctrl,
                RegexHighlighter_IncludeHighlighter1);
        m_TLF_UIAPI->SetTextInputText(WData->RegexIncludeGroup->
                GroupWidgetHandle,WData->RegexIncludeHighlighter2->Ctrl,
                RegexHighlighter_IncludeHighlighter2);
        m_TLF_UIAPI->SetTextInputText(WData->RegexIncludeGroup->
                GroupWidgetHandle,WData->RegexIncludeHighlighter3->Ctrl,
                RegexHighlighter_IncludeHighlighter3);
        m_TLF_UIAPI->SetTextInputText(WData->RegexIncludeGroup->
                GroupWidgetHandle,WData->RegexIncludeHighlighter4->Ctrl,
                RegexHighlighter_IncludeHighlighter4);
        m_TLF_UIAPI->SetTextInputText(WData->RegexIncludeGroup->
                GroupWidgetHandle,WData->RegexIncludeHighlighter5->Ctrl,
                RegexHighlighter_IncludeHighlighter5);
    }
    catch(...)
    {
        if(WData!=NULL)
        {
            TextLineHighlighter_FreeSettingsWidgets(
                    (t_DataProSettingsWidgetsType *)WData);
        }
        return NULL;
    }

    return (t_DataProSettingsWidgetsType *)WData;
}

void TextLineHighlighter_FreeSettingsWidgets(t_DataProSettingsWidgetsType *PrivData)
{
    struct TextLineHighlighter_SettingsWidgets *WData=(struct TextLineHighlighter_SettingsWidgets *)PrivData;

    if(WData->RegexIncludeHighlighter5!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter5);
    if(WData->RegexIncludeHighlighter4!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter4);
    if(WData->RegexIncludeHighlighter3!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter3);
    if(WData->RegexIncludeHighlighter2!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter2);
    if(WData->RegexIncludeHighlighter1!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter1);
    if(WData->RegexIncludeEnabled!=NULL)
        m_TLF_UIAPI->FreeCheckbox(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeEnabled);
    if(WData->RegexIncludeGroup!=NULL)
        m_TLF_UIAPI->FreeGroupBox(WData->RegexTabHandle,WData->RegexIncludeGroup);
    if(WData->RegexRemoveHighlighter5!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter5);
    if(WData->RegexRemoveHighlighter4!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter4);
    if(WData->RegexRemoveHighlighter3!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter3);
    if(WData->RegexRemoveHighlighter2!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter2);
    if(WData->RegexRemoveHighlighter1!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter1);
    if(WData->RegexRemoveEnabled!=NULL)
        m_TLF_UIAPI->FreeCheckbox(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveEnabled);
    if(WData->RegexRemoveGroup!=NULL)
        m_TLF_UIAPI->FreeGroupBox(WData->RegexTabHandle,WData->RegexRemoveGroup);
    if(WData->UseRegexHighlightering!=NULL)
        m_TLF_UIAPI->FreeCheckbox(WData->RegexTabHandle,WData->UseRegexHighlightering);

    if(WData->SimpleHelpText!=NULL)
        m_TLF_UIAPI->FreeTextBox(WData->SimpleTabHandle,WData->SimpleHelpText);
    if(WData->SimpleHighlighterContains!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->SimpleTabHandle,WData->SimpleHighlighterContains);
    if(WData->SimpleHighlighterEndsWith!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->SimpleTabHandle,WData->SimpleHighlighterEndsWith);
    if(WData->SimpleHighlighterStartsWith!=NULL)
        m_TLF_UIAPI->FreeTextInput(WData->SimpleTabHandle,WData->SimpleHighlighterStartsWith);
    if(WData->UseSimpleHighlightering!=NULL)
        m_TLF_UIAPI->FreeCheckbox(WData->SimpleTabHandle,WData->UseSimpleHighlightering);

    if(WData->HelpText!=NULL)
        m_TLF_UIAPI->FreeTextBox(WData->HelpTabHandle,WData->HelpText);

    delete WData;
}

void TextLineHighlighter_SetSettingsFromWidgets(t_DataProSettingsWidgetsType *PrivData,t_PIKVList *Settings)
{
    struct TextLineHighlighter_SettingsWidgets *WData=(struct TextLineHighlighter_SettingsWidgets *)PrivData;
    const char *UseSimpleHighlighteringStr;
    string SimpleHighlighter_StartingWith;
    string SimpleHighlighter_EndingWith;
    string SimpleHighlighter_Contains;
    const char *RegexHighlighter_RemoveEnabledStr;
    string RegexHighlighter_RemoveHighlighter1;
    string RegexHighlighter_RemoveHighlighter2;
    string RegexHighlighter_RemoveHighlighter3;
    string RegexHighlighter_RemoveHighlighter4;
    string RegexHighlighter_RemoveHighlighter5;
    const char *RegexHighlighter_IncludeEnabledStr;
    string RegexHighlighter_IncludeHighlighter1;
    string RegexHighlighter_IncludeHighlighter2;
    string RegexHighlighter_IncludeHighlighter3;
    string RegexHighlighter_IncludeHighlighter4;
    string RegexHighlighter_IncludeHighlighter5;

    if(m_TLF_UIAPI->IsCheckboxChecked(WData->SimpleTabHandle,WData->UseSimpleHighlightering->Ctrl))
        UseSimpleHighlighteringStr="1";
    else
        UseSimpleHighlighteringStr="0";

    /** Simple **/
    SimpleHighlighter_StartingWith=m_TLF_UIAPI->GetTextInputText(WData->SimpleTabHandle,WData->SimpleHighlighterStartsWith->Ctrl);
    SimpleHighlighter_EndingWith=m_TLF_UIAPI->GetTextInputText(WData->SimpleTabHandle,WData->SimpleHighlighterEndsWith->Ctrl);
    SimpleHighlighter_Contains=m_TLF_UIAPI->GetTextInputText(WData->SimpleTabHandle,WData->SimpleHighlighterContains->Ctrl);

    /** Regex **/
    /*** Remove ***/
    if(m_TLF_UIAPI->IsCheckboxChecked(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveEnabled->Ctrl))
        RegexHighlighter_RemoveEnabledStr="1";
    else
        RegexHighlighter_RemoveEnabledStr="0";
    RegexHighlighter_RemoveHighlighter1=m_TLF_UIAPI->GetTextInputText(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter1->Ctrl);
    RegexHighlighter_RemoveHighlighter2=m_TLF_UIAPI->GetTextInputText(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter2->Ctrl);
    RegexHighlighter_RemoveHighlighter3=m_TLF_UIAPI->GetTextInputText(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter3->Ctrl);
    RegexHighlighter_RemoveHighlighter4=m_TLF_UIAPI->GetTextInputText(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter4->Ctrl);
    RegexHighlighter_RemoveHighlighter5=m_TLF_UIAPI->GetTextInputText(WData->RegexRemoveGroup->GroupWidgetHandle,WData->RegexRemoveHighlighter5->Ctrl);

    /*** Include ***/
    if(m_TLF_UIAPI->IsCheckboxChecked(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeEnabled->Ctrl))
        RegexHighlighter_IncludeEnabledStr="1";
    else
        RegexHighlighter_IncludeEnabledStr="0";
    RegexHighlighter_IncludeHighlighter1=m_TLF_UIAPI->GetTextInputText(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter1->Ctrl);
    RegexHighlighter_IncludeHighlighter2=m_TLF_UIAPI->GetTextInputText(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter2->Ctrl);
    RegexHighlighter_IncludeHighlighter3=m_TLF_UIAPI->GetTextInputText(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter3->Ctrl);
    RegexHighlighter_IncludeHighlighter4=m_TLF_UIAPI->GetTextInputText(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter4->Ctrl);
    RegexHighlighter_IncludeHighlighter5=m_TLF_UIAPI->GetTextInputText(WData->RegexIncludeGroup->GroupWidgetHandle,WData->RegexIncludeHighlighter5->Ctrl);

    m_TLF_SysAPI->KVAddItem(Settings,"UseSimpleHighlightering",UseSimpleHighlighteringStr);
    m_TLF_SysAPI->KVAddItem(Settings,"SimpleHighlighter_StartingWith",SimpleHighlighter_StartingWith.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"SimpleHighlighter_EndingWith",SimpleHighlighter_EndingWith.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"SimpleHighlighter_Contains",SimpleHighlighter_Contains.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_RemoveEnabled",RegexHighlighter_RemoveEnabledStr);
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_RemoveHighlighter1",RegexHighlighter_RemoveHighlighter1.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_RemoveHighlighter2",RegexHighlighter_RemoveHighlighter2.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_RemoveHighlighter3",RegexHighlighter_RemoveHighlighter3.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_RemoveHighlighter4",RegexHighlighter_RemoveHighlighter4.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_RemoveHighlighter5",RegexHighlighter_RemoveHighlighter5.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_IncludeEnabled",RegexHighlighter_IncludeEnabledStr);
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_IncludeHighlighter1",RegexHighlighter_IncludeHighlighter1.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_IncludeHighlighter2",RegexHighlighter_IncludeHighlighter2.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_IncludeHighlighter3",RegexHighlighter_IncludeHighlighter3.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_IncludeHighlighter4",RegexHighlighter_IncludeHighlighter4.c_str());
    m_TLF_SysAPI->KVAddItem(Settings,"RegexHighlighter_IncludeHighlighter5",RegexHighlighter_IncludeHighlighter5.c_str());
}

static void TextLineHighlighter_ApplySettings(t_DataProcessorHandleType *DataHandle,
        t_PIKVList *Settings)
{
    struct TextLineHighlighterData *Data=(struct TextLineHighlighterData *)DataHandle;
    const char *UseSimpleHighlighteringStr;
    bool UseSimpleHighlightering;
    const char *SimpleHighlighter_StartingWith;
    const char *SimpleHighlighter_EndingWith;
    const char *SimpleHighlighter_Contains;
    const char *RegexHighlighter_RemoveEnabledStr;
    bool RegexHighlighter_RemoveEnabled;
    const char *RegexHighlighter_RemoveHighlighter1;
    const char *RegexHighlighter_RemoveHighlighter2;
    const char *RegexHighlighter_RemoveHighlighter3;
    const char *RegexHighlighter_RemoveHighlighter4;
    const char *RegexHighlighter_RemoveHighlighter5;
    const char *RegexHighlighter_IncludeEnabledStr;
    bool RegexHighlighter_IncludeEnabled;
    const char *RegexHighlighter_IncludeHighlighter1;
    const char *RegexHighlighter_IncludeHighlighter2;
    const char *RegexHighlighter_IncludeHighlighter3;
    const char *RegexHighlighter_IncludeHighlighter4;
    const char *RegexHighlighter_IncludeHighlighter5;

    UseSimpleHighlighteringStr=m_TLF_SysAPI->KVGetItem(Settings,"UseSimpleHighlightering");
    SimpleHighlighter_StartingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleHighlighter_StartingWith");
    SimpleHighlighter_EndingWith=m_TLF_SysAPI->KVGetItem(Settings,"SimpleHighlighter_EndingWith");
    SimpleHighlighter_Contains=m_TLF_SysAPI->KVGetItem(Settings,"SimpleHighlighter_Contains");
    RegexHighlighter_RemoveEnabledStr=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveEnabled");
    RegexHighlighter_RemoveHighlighter1=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter1");
    RegexHighlighter_RemoveHighlighter2=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter2");
    RegexHighlighter_RemoveHighlighter3=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter3");
    RegexHighlighter_RemoveHighlighter4=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter4");
    RegexHighlighter_RemoveHighlighter5=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_RemoveHighlighter5");
    RegexHighlighter_IncludeEnabledStr=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeEnabled");
    RegexHighlighter_IncludeHighlighter1=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter1");
    RegexHighlighter_IncludeHighlighter2=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter2");
    RegexHighlighter_IncludeHighlighter3=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter3");
    RegexHighlighter_IncludeHighlighter4=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter4");
    RegexHighlighter_IncludeHighlighter5=m_TLF_SysAPI->KVGetItem(Settings,"RegexHighlighter_IncludeHighlighter5");

    /* Set defaults */
    if(UseSimpleHighlighteringStr==NULL)
        UseSimpleHighlighteringStr="1";
    if(SimpleHighlighter_StartingWith==NULL)
        SimpleHighlighter_StartingWith="";
    if(SimpleHighlighter_EndingWith==NULL)
        SimpleHighlighter_EndingWith="";
    if(SimpleHighlighter_Contains==NULL)
        SimpleHighlighter_Contains="";
    if(RegexHighlighter_RemoveEnabledStr==NULL)
        RegexHighlighter_RemoveEnabledStr="0";
    if(RegexHighlighter_RemoveHighlighter1==NULL)
        RegexHighlighter_RemoveHighlighter1="";
    if(RegexHighlighter_RemoveHighlighter2==NULL)
        RegexHighlighter_RemoveHighlighter2="";
    if(RegexHighlighter_RemoveHighlighter3==NULL)
        RegexHighlighter_RemoveHighlighter3="";
    if(RegexHighlighter_RemoveHighlighter4==NULL)
        RegexHighlighter_RemoveHighlighter4="";
    if(RegexHighlighter_RemoveHighlighter5==NULL)
        RegexHighlighter_RemoveHighlighter5="";
    if(RegexHighlighter_IncludeEnabledStr==NULL)
        RegexHighlighter_IncludeEnabledStr="0";
    if(RegexHighlighter_IncludeHighlighter1==NULL)
        RegexHighlighter_IncludeHighlighter1="";
    if(RegexHighlighter_IncludeHighlighter2==NULL)
        RegexHighlighter_IncludeHighlighter2="";
    if(RegexHighlighter_IncludeHighlighter3==NULL)
        RegexHighlighter_IncludeHighlighter3="";
    if(RegexHighlighter_IncludeHighlighter4==NULL)
        RegexHighlighter_IncludeHighlighter4="";
    if(RegexHighlighter_IncludeHighlighter5==NULL)
        RegexHighlighter_IncludeHighlighter5="";

    UseSimpleHighlightering=atoi(UseSimpleHighlighteringStr);
    RegexHighlighter_RemoveEnabled=atoi(RegexHighlighter_RemoveEnabledStr);
    RegexHighlighter_IncludeEnabled=atoi(RegexHighlighter_IncludeEnabledStr);

    Data->UseSimpleHighlighters=UseSimpleHighlightering;
    Data->SimpleHighlighterStartsWith=SimpleHighlighter_StartingWith;
    Data->SimpleHighlighterEndsWith=SimpleHighlighter_EndingWith;
    Data->SimpleHighlighterContains=SimpleHighlighter_Contains;

    Data->RegexRemoveEnabled=RegexHighlighter_RemoveEnabled;
    Data->RegexRemoveHighlighter1=RegexHighlighter_RemoveHighlighter1;
    Data->RegexRemoveHighlighter2=RegexHighlighter_RemoveHighlighter2;
    Data->RegexRemoveHighlighter3=RegexHighlighter_RemoveHighlighter3;
    Data->RegexRemoveHighlighter4=RegexHighlighter_RemoveHighlighter4;
    Data->RegexRemoveHighlighter5=RegexHighlighter_RemoveHighlighter5;

    Data->RegexIncludeEnabled=RegexHighlighter_IncludeEnabled;
    Data->RegexIncludeHighlighter1=RegexHighlighter_IncludeHighlighter1;
    Data->RegexIncludeHighlighter2=RegexHighlighter_IncludeHighlighter2;
    Data->RegexIncludeHighlighter3=RegexHighlighter_IncludeHighlighter3;
    Data->RegexIncludeHighlighter4=RegexHighlighter_IncludeHighlighter4;
    Data->RegexIncludeHighlighter5=RegexHighlighter_IncludeHighlighter5;
}
