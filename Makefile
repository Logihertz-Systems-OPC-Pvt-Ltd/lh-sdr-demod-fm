# ==============================================================================
# Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
# Module:        Makefile
# Role:          Zero-Dependency C++17 Build & Test Harness
# Organization:  Logihertz Systems (OPC) Private Limited
# Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
# Contact:       ops@logihertz.com / security@logihertz.com
# License:       Apache-2.0
# ==============================================================================

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS ?= -lm

# SIMD auto-detection
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    SIMD_FLAGS ?= -mavx2
else ifneq ($(filter arm% aarch64,$(UNAME_M)),)
    SIMD_FLAGS ?= -march=armv8-a+simd
else
    SIMD_FLAGS ?=
endif

SRCS := src/iq_conditioner.cpp \
        src/fm_demodulator.cpp \
        src/wav_writer.cpp \
        src/json_parser.cpp \
        src/sdr_auditor.cpp \
        src/exporters.cpp

OBJS := $(SRCS:src/%.cpp=build/%.o)

.PHONY: all test clean

all: bin/lh-sdr-fm

build:
	mkdir -p build

bin:
	mkdir -p bin

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) $(SIMD_FLAGS) -c $< -o $@

build/main.o: src/main.cpp | build
	$(CXX) $(CXXFLAGS) $(SIMD_FLAGS) -c $< -o $@

bin/lh-sdr-fm: $(OBJS) build/main.o | bin
	$(CXX) $(CXXFLAGS) $(SIMD_FLAGS) -o $@ $^ $(LDFLAGS)

build/test_runner.o: tests/test_runner.cpp | build
	$(CXX) $(CXXFLAGS) $(SIMD_FLAGS) -c $< -o $@

tests/test_runner: $(OBJS) build/test_runner.o
	$(CXX) $(CXXFLAGS) $(SIMD_FLAGS) -o $@ $^ $(LDFLAGS)

test: tests/test_runner
	./tests/test_runner

clean:
	rm -rf build bin/lh-sdr-fm tests/test_runner dist *.wav *.iq
