SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .cpp .o

bin_PROGRAM = ChatTerm

OBJEXT = o

srcdir = ../src

chatterm_HEADERS = $(srcdir)/CHANNEL_INFO.h $(srcdir)/ChatTerminal.h $(srcdir)/Commands.h $(srcdir)/ConsoleIo.h \
	$(srcdir)/NetworkIo.h $(srcdir)/NixHelper.h $(srcdir)/ProcessorMsgX.h $(srcdir)/StrResources.h $(srcdir)/USER_INFO.h $(srcdir)/WinNixDefs.h \
	$(srcdir)/XmlHelper.h $(srcdir)/XmlHelper_libxml.h

chatterm_OBJECTS = CHANNEL_INFO.$(OBJEXT) \
	ChatTerminal.$(OBJEXT) Commands.$(OBJEXT) \
	ConsoleIo_curses.$(OBJEXT) Main.$(OBJEXT) NetworkIo.$(OBJEXT) \
	ProcessorMsgX.$(OBJEXT) StrResources.$(OBJEXT) \
	USER_INFO.$(OBJEXT) XmlHelper_libxml.$(OBJEXT)

LIBS = -ldl -lz -lm -lcrypto -luuid -lxml2 -lpthread -lncursesw

chatterm_LIBS =
chatterm_LDFLAGS =
chatterm_CPPFLAGS =
nodebug =

ifdef chatterm_LIBS
	LIBS += $(chatterm_LIBS)
endif

ifndef chatterm_CPPFLAGS
ifeq ($(nodebug),0)
	chatterm_CPPFLAGS = -D_DEBUG -O0 -g3
else
	chatterm_CPPFLAGS = -DNDEBUG -O3 -g0
endif
endif

all: $(bin_PROGRAM)

$(bin_PROGRAM): $(chatterm_OBJECTS)
	@rm -f $(bin_PROGRAM)
	$(CXX) $(LDFLAGS) $(chatterm_LDFLAGS) -o $(bin_PROGRAM) -s $(chatterm_OBJECTS) $(LIBS)

$(chatterm_OBJECTS): %.$(OBJEXT): $(srcdir)/%.cpp $(chatterm_HEADERS)
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $(chatterm_CPPFLAGS) $< -o $@

clean:
	-rm -f *.$(OBJEXT) $(bin_PROGRAM)
