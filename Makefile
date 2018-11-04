CC := g++
CPPFLAGS := -std=c++17 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00
LDFLAGS := -municode
LDLIBS := -lntdll -lole32 -luuid -lshlwapi -larchive -lboost_program_options-mt -ltinyxml2

PROJ := LxRunOffline
SRCS := $(wildcard $(PROJ)/*.cpp)
OBJS := $(patsubst $(PROJ)/%.cpp, $(PROJ)/%.o, $(SRCS)) $(PROJ)/resources.o
TARGET := $(PROJ).exe

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

$(PROJ)/%.o: $(PROJ)/%.cpp
	$(CC) $(CPPFLAGS) -c -o $@ $<

$(PROJ)/resources.o: $(PROJ)/resources.rc $(PROJ)/app.manifest
	windres $(PROJ)/resources.rc $(PROJ)/resources.o

clean:
	rm -rf $(OBJS) $(TARGET)
