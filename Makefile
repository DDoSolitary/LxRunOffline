CC := g++
CPPFLAGS := -std=c++17 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00
LDFLAGS := -municode
LDLIBS := -lntdll -lole32 -larchive -lboost_program_options-mt

PROJ := LxRunOffline
SRCS := $(wildcard $(PROJ)/*.cpp)
OBJS := $(patsubst $(PROJ)/%.cpp, $(PROJ)/%.o, $(SRCS)) $(PROJ)/manifest.o
TARGET := $(PROJ).exe

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

$(PROJ)/%.o: $(PROJ)/%.cpp
	$(CC) $(CPPFLAGS) -c -o $@ $<

$(PROJ)/manifest.o: $(PROJ)/manifest.rc $(PROJ)/app.manifest
	windres $(PROJ)/manifest.rc $(PROJ)/manifest.o

clean:
	rm -rf $(OBJS) $(TARGET)
