# sp yoink

ifndef EXE
    EXE = tarnished
endif

EXE_SUFFIX =
LDFLAGS = -fuse-ld=lld
ifeq ($(OS), Windows_NT)
	EXE_SUFFIX = .exe
endif

EVALFILE ?= network/latest.bin

SOURCES := src/*.cpp
CXX := clang++
CXXFLAGS := -O3 -march=native -fno-finite-math-only -funroll-loops -flto -fuse-ld=lld -std=c++20 -static -DNDEBUG -pthread

$(EXE)$(EXE_SUFFIX): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SOURCES) -o $(EXE)$(EXE_SUFFIX)