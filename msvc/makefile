OUTDIR = WIN
PROJ = ChatTerm

#Directives from <win32.mak>
!IF "$(CPU)" != "i386"
!IF "$(CPU)" != "AMD64"
!ERROR  Must specify CPU environment variable ( CPU=i386, CPU=AMD64)
!ENDIF
!ENDIF

!IF "$(CPU)" == "AMD64"
OUTDIR=$(OUTDIR)_X64
!ENDIF

#Set DEBUG
!IF "$(NODEBUG)" == ""
OUTDIR=$(OUTDIR)_DEBUG
!ELSE
OUTDIR=$(OUTDIR)_RETAIL
!ENDIF

APPVER = "5.01"
NMAKE_WINVER = 0x0501

!IFDEF NODEBUG
cdebug = -Ox -DNDEBUG
!ELSE
cdebug = -Zi -Od -DDEBUG
!ENDIF

# declarations common to all compiler options
ccommon = -c -DCRTAPI1=_cdecl -DCRTAPI2=_cdecl -nologo -GS

!IF "$(CPU)" == "i386"
cflags = $(ccommon) -D_X86_=1  -DWIN32 -D_WIN32
!ELSEIF "$(CPU)" == "AMD64"
cflags = $(ccommon) -D_AMD64_=1 -DWIN64 -D_WIN64  -DWIN32 -D_WIN32 
!ENDIF

cflags = $(cflags) -W4 -D_WINNT -D_WIN32_WINNT=$(NMAKE_WINVER) -DNTDDI_VERSION=$(NMAKE_WINVER)0000

# for Windows applications that use the C Run-Time libraries
!IFDEF NODEBUG
cvarsmt = -D_MT -MT
!ELSE
cvarsmt = -D_MT -MTd
!ENDIF

!IFDEF NODEBUG
ldebug = /RELEASE
!ELSE
ldebug = /DEBUG /DEBUGTYPE:cv
!ENDIF

baselibs = kernel32.lib ws2_32.lib mswsock.lib advapi32.lib

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

EXTRA_LIBS = Rpcrt4.lib Msxml6.lib Msxml2.lib ole32.lib Oleaut32.lib Iphlpapi.lib shell32.lib
CL_PRE     = /DUNICODE=1

#----- If OUTDIR does not exist, then create directory
$(OUTDIR) :
    if not exist "$(OUTDIR)/$(NULL)" mkdir $(OUTDIR)

# Inference rule for updating the object files
{../src/}.cpp{$(OUTDIR)}.obj:
   cl $(CL_PRE) $(cdebug) $(cflags) $(cvarsmt) $(cvarsexe) -DCHATTERM_OS_WINDOWS /Fo"$(OUTDIR)\\" /Fd"$(OUTDIR)\\" $**

# Build rule for EXE
$(OUTDIR)\$(PROJ).exe: $(PROJ_OBJS)
    link $(ldebug) /INCREMENTAL:NO /NOLOGO /SUBSYSTEM:console,$(APPVER) $(baselibs) \
    $(PROJ_OBJS) $(EXTRA_LIBS) \
    -out:$(OUTDIR)\$(PROJ).exe $(MAPFILE)

# Rules for cleaning out those old files
clean:
        if exist $(OUTDIR)/$(NULL) rd /s /q $(OUTDIR)