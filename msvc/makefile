# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
# ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
# PARTICULAR PURPOSE.
#
# Copyright (c) Microsoft Corporation.  All Rights Reserved.
#
#
# Processor independent makefile for Platform SDK
#
# Target only i386

!if "$(CPU)" == "IA64"
!message Sorry. Building this sample for the IA64 platform is not currently supported.
!else
# Makefile for i386

# Target WinNT
TARGETOS=WINNT

# Nmake macros for building Win32 apps
!include <win32.mak>

PROJ = ChatTerm

all: $(OUTDIR) $(OUTDIR)\$(PROJ).exe

# Define project specific macros
# $(OUTDIR)\$(PROJ).obj
PROJ_OBJS  = $(OUTDIR)\Main.obj \
				$(OUTDIR)\ChatTerminal.obj \
				$(OUTDIR)\Commands.obj \
				$(OUTDIR)\ProcessorMsgX.obj \
				$(OUTDIR)\NetworkIo.obj \
				$(OUTDIR)\ConsoleIo_win.obj \
				$(OUTDIR)\XmlHelper_msxml.obj \
				$(OUTDIR)\USER_INFO.obj \
				$(OUTDIR)\CHANNEL_INFO.obj \
				$(OUTDIR)\StrResources.obj
BASE_OBJS  =
EXTRA_LIBS = Rpcrt4.lib Msxml2.lib ole32.lib Oleaut32.lib Iphlpapi.lib shell32.lib
GLOBAL_DEP =
RC_DEP     =
CL_PRE     = /DUNICODE=1

#----- If OUTDIR does not exist, then create directory
$(OUTDIR) :
    if not exist "$(OUTDIR)/$(NULL)" mkdir $(OUTDIR)

# Inference rule for updating the object files
{../src/}.cpp{$(OUTDIR)}.obj:
   $(cc) $(CL_PRE) $(cdebug) $(cflags) $(cvarsmt) $(cvarsexe) -DCHATTERM_OS_WINDOWS /Fo"$(OUTDIR)\\" /Fd"$(OUTDIR)\\" $**

# Build rule for resource file
$(OUTDIR)\$(PROJ).res: ChatTerminal.rc $(RC_DEP)
    $(rc) $(rcflags) $(rcvars) /fo $(OUTDIR)\$(PROJ).res ChatTerminal.rc

# Build rule for EXE
$(OUTDIR)\$(PROJ).exe: $(BASE_OBJS) $(PROJ_OBJS) $(OUTDIR)\$(PROJ).res
    $(link) $(ldebug) $(lflags) -subsystem:console $(conlibsmt) \
    $(BASE_OBJS) $(PROJ_OBJS) $(OUTDIR)\$(PROJ).res $(EXTRA_LIBS) \
    -out:$(OUTDIR)\$(PROJ).exe $(MAPFILE)

#$(conlflags) not the same as $(lflags) -subsystem:console,5.0

# Rules for cleaning out those old files
clean:
        $(CLEANUP)

!endif
