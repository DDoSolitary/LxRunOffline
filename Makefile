CC := g++
CPPFLAGS := -std=c++17 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00
LDFLAGS := -municode -static -static-libgcc -static-libstdc++
LDLIBS := -lntdll -lole32 -luuid -larchive -lboost_program_options-mt -ltinyxml2 -lexpat -lbz2 -llz4 -liconv -llzma -lz -lnettle -lzstd -lbcrypt

PROJ := LxRunOffline
SRCS := $(filter-out $(PROJ)/stdafx.cpp, $(wildcard $(PROJ)/*.cpp))
OBJS := $(patsubst $(PROJ)/%.cpp, $(PROJ)/%.o, $(SRCS)) $(PROJ)/resources.o
TARGET := $(PROJ).exe
OUTPUT := $(OBJS) $(PROJ)/stdafx.h.gch $(TARGET)

VERSION := $(shell git describe --tags | cut -c 2-)
FILE_VERSION := $(shell echo $(VERSION) | grep -o "^[^-]*" | tr . ,),0

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(PROJ)/%.o: $(PROJ)/%.cpp $(PROJ)/stdafx.h.gch
	$(CC) $(CPPFLAGS) -DLXRUNOFFLINE_VERSION='"'v$(VERSION)'"' -c -o $@ $<

$(PROJ)/stdafx.h.gch: $(PROJ)/stdafx.cpp $(PROJ)/stdafx.h
	$(CC) $(CPPFLAGS) -x c++-header -o $@ $<

$(PROJ)/resources.o: $(PROJ)/resources.rc $(PROJ)/app.manifest
	windres -DLXRUNOFFLINE_FILE_VERSION=$(FILE_VERSION) -DLXRUNOFFLINE_FILE_VERSION_STR='\"'$(VERSION)'\"' $< $@

clean:
	rm -rf $(OUTPUT)
