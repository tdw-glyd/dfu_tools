##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=dfu_tools
ConfigurationName      :=Debug
WorkspaceConfiguration :=Debug
WorkspacePath          :=/mnt/c/Glydways/bl_tools/dfu_tools/dfu_tools
ProjectPath            :=/mnt/c/Glydways/bl_tools/dfu_tools
IntermediateDirectory  :=dfu_tools/build-$(WorkspaceConfiguration)/__
OutDir                 :=$(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=root
Date                   :=10/24/24
CodeLitePath           :=/root/.codelite
MakeDirCommand         :=mkdir -p
LinkerName             :=gcc
SharedObjectLinkerName :=gcc -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.o.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputDirectory        :=/mnt/c/Glydways/bl_tools/dfu_tools/dfu_tools/build-$(WorkspaceConfiguration)/bin
OutputFile             :=dfu_tools/build-$(WorkspaceConfiguration)/bin/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E 
ObjectsFileList        :=$(IntermediateDirectory)/ObjectsList.txt
PCHCompileFlags        :=
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_tools $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_tools/platform/include $(IncludeSwitch). $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_tools/platform/include $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_protocol/include $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_tools/include $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_tools/interfaces/Ethernet/include $(IncludeSwitch)/mnt/c/Glydways/bl_tools/dfu_tools/interfaces/CAN/include 
IncludePCH             := 
RcIncludePath          := 
Libs                   := 
ArLibs                 :=  
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overridden using an environment variable
##
AR       := ar rcus
CXX      := gcc
CC       := gcc
CXXFLAGS :=  -gdwarf-2 -O0 -Wall $(Preprocessors)
CFLAGS   :=  -gdwarf-2 -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_dfu_protocol_src_dfu_messages.c$(ObjectSuffix) $(IntermediateDirectory)/src_dfu_lib_support.c$(ObjectSuffix) $(IntermediateDirectory)/main.c$(ObjectSuffix) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_proto.c$(ObjectSuffix) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_platform_utils.c$(ObjectSuffix) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_client.c$(ObjectSuffix) $(IntermediateDirectory)/platform_src_async_timer.c$(ObjectSuffix) $(IntermediateDirectory)/interfaces_Ethernet_src_iface_enet.c$(ObjectSuffix) $(IntermediateDirectory)/interfaces_Ethernet_src_ethernet_sockets.c$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: MakeIntermediateDirs $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) "$(IntermediateDirectory)"
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@$(MakeDirCommand) "$(IntermediateDirectory)"
	@$(MakeDirCommand) "$(OutputDirectory)"

$(IntermediateDirectory)/.d:
	@$(MakeDirCommand) "$(IntermediateDirectory)"

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_dfu_protocol_src_dfu_messages.c$(ObjectSuffix): ../dfu_protocol/src/dfu_messages.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_protocol/src/dfu_messages.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_dfu_protocol_src_dfu_messages.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_dfu_protocol_src_dfu_messages.c$(PreprocessSuffix): ../dfu_protocol/src/dfu_messages.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_messages.c$(PreprocessSuffix) ../dfu_protocol/src/dfu_messages.c

$(IntermediateDirectory)/src_dfu_lib_support.c$(ObjectSuffix): src/dfu_lib_support.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_tools/src/dfu_lib_support.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_dfu_lib_support.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_dfu_lib_support.c$(PreprocessSuffix): src/dfu_lib_support.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_dfu_lib_support.c$(PreprocessSuffix) src/dfu_lib_support.c

$(IntermediateDirectory)/main.c$(ObjectSuffix): main.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_tools/main.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/main.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/main.c$(PreprocessSuffix): main.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/main.c$(PreprocessSuffix) main.c

$(IntermediateDirectory)/up_dfu_protocol_src_dfu_proto.c$(ObjectSuffix): ../dfu_protocol/src/dfu_proto.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_protocol/src/dfu_proto.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_dfu_protocol_src_dfu_proto.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_dfu_protocol_src_dfu_proto.c$(PreprocessSuffix): ../dfu_protocol/src/dfu_proto.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_proto.c$(PreprocessSuffix) ../dfu_protocol/src/dfu_proto.c

$(IntermediateDirectory)/up_dfu_protocol_src_dfu_platform_utils.c$(ObjectSuffix): ../dfu_protocol/src/dfu_platform_utils.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_protocol/src/dfu_platform_utils.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_dfu_protocol_src_dfu_platform_utils.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_dfu_protocol_src_dfu_platform_utils.c$(PreprocessSuffix): ../dfu_protocol/src/dfu_platform_utils.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_platform_utils.c$(PreprocessSuffix) ../dfu_protocol/src/dfu_platform_utils.c

$(IntermediateDirectory)/up_dfu_protocol_src_dfu_client.c$(ObjectSuffix): ../dfu_protocol/src/dfu_client.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_protocol/src/dfu_client.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_dfu_protocol_src_dfu_client.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_dfu_protocol_src_dfu_client.c$(PreprocessSuffix): ../dfu_protocol/src/dfu_client.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_dfu_protocol_src_dfu_client.c$(PreprocessSuffix) ../dfu_protocol/src/dfu_client.c

$(IntermediateDirectory)/platform_src_async_timer.c$(ObjectSuffix): platform/src/async_timer.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_tools/platform/src/async_timer.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/platform_src_async_timer.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/platform_src_async_timer.c$(PreprocessSuffix): platform/src/async_timer.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/platform_src_async_timer.c$(PreprocessSuffix) platform/src/async_timer.c

$(IntermediateDirectory)/interfaces_Ethernet_src_iface_enet.c$(ObjectSuffix): interfaces/Ethernet/src/iface_enet.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_tools/interfaces/Ethernet/src/iface_enet.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/interfaces_Ethernet_src_iface_enet.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/interfaces_Ethernet_src_iface_enet.c$(PreprocessSuffix): interfaces/Ethernet/src/iface_enet.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/interfaces_Ethernet_src_iface_enet.c$(PreprocessSuffix) interfaces/Ethernet/src/iface_enet.c

$(IntermediateDirectory)/interfaces_Ethernet_src_ethernet_sockets.c$(ObjectSuffix): interfaces/Ethernet/src/ethernet_sockets.c 
	$(CC) $(SourceSwitch) "/mnt/c/Glydways/bl_tools/dfu_tools/interfaces/Ethernet/src/ethernet_sockets.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/interfaces_Ethernet_src_ethernet_sockets.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/interfaces_Ethernet_src_ethernet_sockets.c$(PreprocessSuffix): interfaces/Ethernet/src/ethernet_sockets.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/interfaces_Ethernet_src_ethernet_sockets.c$(PreprocessSuffix) interfaces/Ethernet/src/ethernet_sockets.c

##
## Clean
##
clean:
	$(RM) -r $(IntermediateDirectory)


