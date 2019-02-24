CC := g++
CPPFLAGS := -std=c++17 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00
LDFLAGS := -municode
LDLIBS := -lntdll -lole32 -luuid -larchive -lboost_program_options-mt -ltinyxml2

PROJ := LxRunOffline
SRCS := $(wildcard $(PROJ)/*.cpp)
OBJS := $(patsubst $(PROJ)/%.cpp, $(PROJ)/%.o, $(SRCS)) $(PROJ)/resources.o
TARGET := $(PROJ).exe
OUTPUT := $(OBJS) $(PROJ)/stdafx.h.gch $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS)

$(PROJ)/%.o: $(PROJ)/%.cpp $(PROJ)/stdafx.h.gch
	$(CC) $(CPPFLAGS) -c -o $@ $<

$(PROJ)/stdafx.h.gch: $(PROJ)/stdafx.h
	$(CC) $(CPPFLAGS) -o $@ $<

$(PROJ)/resources.o: $(PROJ)/resources.rc $(PROJ)/app.manifest
	windres $< $@

clean:
	rm -rf $(OUTPUT)
