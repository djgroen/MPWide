INCLUDE_DIR       = 
LIBRARY_DIR       = 
LDFLAGS         = -L.
LDLIBS = -lMPW -lpthread
CXXFLAGS    = -O3 -Wall -fPIC
TARGET_ARCH =  #-arch i386
INSTALL_PREFIX    = .

Test_objects = Test.o
Amuse_objects = amuse/AmuseAgent.o
TestRestart_objects = TestRestart.o
dg_objects   = DataGather.o
fw_objects = Forwarder.o
wcp_objects =  mpw-cp.o

# OS X
#SO_EXT = dylib
#SHARED_LINK_FLAGS = -dynamiclib

# Linux
SO_EXT = so
SHARED_LINK_FLAGS = -shared

all : MPWTest MPWTestRestart MPWDataGather MPWForwarder MPWFileCopy libMPW.a libMPW.$(SO_EXT)

install: libMPW.a libMPW.$(SO_EXT) MPWForwarder
	mkdir -p $(INSTALL_PREFIX)/lib
	mkdir -p $(INSTALL_PREFIX)/bin
	mkdir -p $(INSTALL_PREFIX)/include
	cp libMPW.$(SO_EXT)* $(INSTALL_PREFIX)/lib/
	cp libMPW.a  $(INSTALL_PREFIX)/lib/
	cp MPWForwarder $(INSTALL_PREFIX)/bin/
	cp MPWide.h $(INSTALL_PREFIX)/include/

libMPW.a: MPWide.o Socket.o
	$(AR) $(ARFLAGS) $@ $^

libMPW.$(SO_EXT): MPWide.o Socket.o
	$(CXX) $(CXXFLAGS) $(SHARED_LINK_FLAGS) $(TARGET_ARCH) -dynamiclib -o $@ $^
#    ld -shared -soname libMPW.so.1 -o libMPW.so.1.0 -lc MPWide.o Socket.o

LINK_EXE = $(CXX) $(LDFLAGS) $(TARGET_ARCH) $< $(LOADLIBES) $(LDLIBS) -o $@

MPWTest: $(Test_objects) libMPW.a
	$(LINK_EXE)

MPWAmuseAgent: $(Amuse_objects) libMPW.a
	$(LINK_EXE)

MPWTestRestart: $(TestRestart_objects) libMPW.a
	$(LINK_EXE)

MPWForwarder: $(fw_objects) libMPW.a
	$(LINK_EXE)

MPWDataGather: $(dg_objects) libMPW.a
	$(LINK_EXE)

MPWFileCopy: $(wcp_objects) libMPW.a
	$(LINK_EXE)

Test: Test.cpp
TestRestart: TestRestart.cpp
Forwarder: Forwarder.cpp

clean:
	rm -f *.o MPWTest MPWTestRestart MPWDataGather MPWForwarder MPWAmuseAgent MPWFileCopy libMPW.a libMPW.$(SO_EXT)* bin lib include
