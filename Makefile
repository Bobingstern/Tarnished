# sp yoink



EXE_SUFFIX =
LDFLAGS = -fuse-ld=lld
SOURCES := $(wildcard src/*.cpp)
CXX := clang++


cat := $(if $(filter $(OS),Windows_NT),type,cat)
DEFAULT_NET := $(shell $(cat) network.txt)

ifndef EVALFILE
    EVALFILE = $(DEFAULT_NET).bin
    NO_EVALFILE_SET = true
endif

CXXFLAGS := -O3 -march=native -fno-finite-math-only -funroll-loops -flto -fuse-ld=lld -std=c++20 -DNDEBUG -pthread -DEVALFILE=\"$(EVALFILE)\"

ifdef NO_EVALFILE_SET
$(EVALFILE):
	$(info Downloading default network $(DEFAULT_NET).bin)
	curl -sOL https://github.com/Bobingstern/tarnished-nets/releases/download/$(DEFAULT_NET)/$(DEFAULT_NET).bin
download-net: $(EVALFILE)
endif

.DEFAULT_GOAL := native

ifndef EXE
    EXE = tarnished$(EXE_SUFFIX)
endif

$(EXE): $(EVALFILE) $(SOURCES) 
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SOURCES) -o $@

native: $(EXE)


