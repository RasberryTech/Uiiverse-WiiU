
TARGET := UiiverseTest
BUILD := build

CXX := C:/devkitPro/devkitPPC/bin/powerpc-eabi-g++.exe

CXXFLAGS := -O2 -Wall -IC:/devkitPro/wut/include
OFILES := $(BUILD)/main.o

LIBS := -LC:/devkitPro/wut/lib -lwut
RPXSPECS := -specs=C:/devkitPro/wut/share/wut.specs

WUHBT := C:/devkitPro/tools/bin/wuhbtool
ELF2RPL := C:/devkitPro/tools/bin/elf2rpl.exe

all: $(TARGET).wuhb

$(BUILD):
	mkdir $(BUILD)

$(BUILD)/main.o: main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	@echo linking ... $(TARGET).elf
	$(CXX) $(OFILES) $(RPXSPECS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	$(ELF2RPL) $< $@
	@echo built ... $(TARGET).rpx

$(TARGET).wuhb: $(TARGET).rpx
	$(WUHBT) $< $@ --name="UiiverseTest" --short-name="UiiverseTest"
	@echo built ... $(TARGET).wuhb

clean:
	rm -rf $(BUILD)
	rm -f $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb
