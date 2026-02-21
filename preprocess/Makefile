EXE_SUFFIX =
LDFLAGS = -fuse-ld=lld
SOURCES := permute.cpp ../src/parameters.cpp
CXX := clang++

ARCH_LEVEL ?= native
ifeq ($(ARCH_LEVEL),native)
	ARCH := -march=native
else ifeq ($(ARCH_LEVEL),v3)
	ARCH := -march=x86-64-v3 -static
else ifeq ($(ARCH_LEVEL),v4)
	ARCH := -march=x86-64-v4 -static
endif

CXXFLAGS := -O3 $(ARCH) -fno-finite-math-only -funroll-loops -flto -fuse-ld=lld -std=c++20 -DNDEBUG -pthread

ifndef EXE
	EXE = permute$(EXE_SUFFIX)
endif

$(EXE): $(SOURCES) 
	$(CXX) $(CXXFLAGS) $(STACK_FLAGS) $(LDFLAGS) $(SOURCES) -o $@

permute_net: $(EXE)
