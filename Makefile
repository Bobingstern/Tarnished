# sp yoink
EXE_SUFFIX =
LDFLAGS = -fuse-ld=lld
SOURCES := $(wildcard src/*.cpp)
SOURCES += src/external/format.cpp
CXX := clang++

ARCH_LEVEL ?= native
ifeq ($(ARCH_LEVEL),native)
    ARCH := -march=native
else ifeq ($(ARCH_LEVEL),v1)
    ARCH := -march=x86-64
else ifeq ($(ARCH_LEVEL),v2)
    ARCH := -march=x86-64-v2
else ifeq ($(ARCH_LEVEL),v3)
    ARCH := -march=x86-64-v3
else ifeq ($(ARCH_LEVEL),v4)
    ARCH := -march=x86-64-v4
else
    $(error Invalid ARCH_LEVEL: $(ARCH_LEVEL). Use native, v1, v2, v3, or v4)
endif

cat := $(if $(filter $(OS),Windows_NT),type,cat)
DEFAULT_NET := $(shell $(cat) network.txt)

ifndef EVALFILE
    EVALFILE = $(DEFAULT_NET).bin
    NO_EVALFILE_SET = true
endif

ifeq ($(OS),Windows_NT)
    STACK_FLAGS := -Wl,/STACK:8388608
else
    STACK_FLAGS :=
endif

CXXFLAGS := -O3 $(ARCH) -fno-finite-math-only -funroll-loops -flto -fuse-ld=lld -std=c++20 -DNDEBUG -pthread -DEVALFILE=\"$(EVALFILE)\"

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
	$(CXX) $(CXXFLAGS) $(STACK_FLAGS) $(LDFLAGS) $(SOURCES) -o $@

native: $(EXE)

v1:
	$(MAKE) ARCH_LEVEL=v1

v2:
	$(MAKE) ARCH_LEVEL=v2

v3:
	$(MAKE) ARCH_LEVEL=v3

v4:
	$(MAKE) ARCH_LEVEL=v4



