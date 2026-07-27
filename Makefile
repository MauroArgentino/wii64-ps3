export PS3SDK=c:/PSDK3v2
export PATH := $(PS3SDK)/mingw/msys/1.0/bin:$(PS3SDK)/mingw/bin:$(PS3SDK)/ps3dev/bin:$(PS3SDK)/ps3dev/ppu/bin:$(PS3SDK)/ps3dev/spu/bin:$(PATH)
#export PSL1GHT=J:/PS3/PSDK3v2/psl1ght
#export PS3DEV=j:/PS3/PSDK3v2/ps3dev

#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include $(PSL1GHT)/ppu_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
#TARGET		:=	$(notdir $(CURDIR))
TARGET		:=	ps364_glN64
ifdef DEBUG
TARGET		:=	ps364_debug
BUILD		:=	build_debug
else
BUILD		:=	build
endif
SOURCES		:=	src/main src/core/n64_audio src/core/n64_input src/core/n64_memory src/core/rsp src/core/r4300 src/core/r4300/ppc src/ui src/ui/libgui src/ui/fileBrowser src/platform/ps3 src/video/glN64
DATA		:=	data
SHADERS		:=	src/platform/ps3/shaders
INCLUDES	:= . $(SOURCES)
#LIBRARIES	:= -LJ:/PS3/PSDK3v2/MinGW/Lib

# ID del contenido para el paquete de PS3 (Requerido para pkg_package)
CONTENTID	:= UP0001-WII64PS31_00-0000000000000000

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ifdef DEBUG
CFLAGS		= -O0 -g3 -Wall -mcpu=cell -mtune=cell $(MACHDEP) $(INCLUDE) \
			-fno-exceptions -Wno-unused-parameter -pipe -DUSE_EXPANSION -D__BIG_ENDIAN__ \
			-DDEBUG_POLYGONS -DSHOW_DEBUG \
			-include ../src/main/winlnxdefs.h \
			-DPPC -D_BIG_ENDIAN -DPS3 -DPPC_DYNAREC -DUSE_RECOMP_CACHE -D__PSL1GHT__
else
CFLAGS		= -O3 -Wall -mcpu=cell -mtune=cell $(MACHDEP) $(INCLUDE) \
			-fno-exceptions -Wno-unused-parameter -pipe -DUSE_EXPANSION -D__BIG_ENDIAN__ -DNDEBUG -D_GLIBCXX_DEBUG=0 -U_GLIBCXX_DEBUG \
			-include ../src/main/winlnxdefs.h \
			-DPPC -D_BIG_ENDIAN -DPS3 -DPPC_DYNAREC -DUSE_RECOMP_CACHE -D__PSL1GHT__
endif
	  
CXXFLAGS	=	$(CFLAGS) -fno-rtti -fno-exceptions -fpermissive

LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-laudio -lsimdmath -lrsx -lgcm_sys -lio -lsysmodule -lsysutil -lrt -llv2 -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# SPU program: compiled separately, embedded into PPU ELF via bin2s
#---------------------------------------------------------------------------------
SPU_DIR		:=	src/platform/ps3/spu_core
SPU_CC		:=	spu-gcc
SPU_LD		:=	spu-gcc
SPU_OBJCOPY	:=	spu-objcopy
SPU_AS		:=	ppu-as
SPU_BIN2S	:=	bin2s

SPU_CFLAGS	:=	-O2 -Wall -ffreestanding -nostdlib -I$(PSL1GHT)/spu/include
SPU_LDFLAGS	:=	-nostdlib -Ttext 0x0

SPU_TARGET	:=	spu_core
SPU_ELF		:=	$(SPU_DIR)/$(SPU_TARGET).elf
SPU_BIN		:=	$(SPU_DIR)/$(SPU_TARGET).bin
SPU_EMBED_OBJ	:=	$(SPU_DIR)/$(SPU_TARGET)_elf.o

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(foreach dir,$(SHADERS),$(CURDIR)/$(dir))
export VPATH	:=	$(VPATH) $(CURDIR)/src/platform/ps3
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
VCGFILES	:=	$(foreach dir,$(SHADERS),$(notdir $(wildcard $(dir)/*.vcg)))
FCGFILES	:=	$(foreach dir,$(SHADERS),$(notdir $(wildcard $(dir)/*.fcg)))

VPOFILES	:=	$(VCGFILES:.vcg=.vpo)
FPOFILES	:=	$(FCGFILES:.fcg=.fpo)

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(addsuffix .o,$(VPOFILES)) \
					$(addsuffix .o,$(FPOFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o) \
					$(notdir $(SPU_EMBED_OBJ))

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/../$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(LIBPSL1GHT_INC) \
					-I$(PORTLIBS)/include \
					-I$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(LIBPSL1GHT_LIB) \
					-L$(PORTLIBS)/lib

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD): spu_build
	@[ -d $@ ] || mkdir -p $@
	@cp -f $(SPU_EMBED_OBJ) $@/$(notdir $(SPU_EMBED_OBJ))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
spu_build:
	@if [ ! -f "$(SPU_EMBED_OBJ)" ]; then \
		echo "Compiling SPU program..."; \
		$(SPU_CC) $(SPU_CFLAGS) -c $(SPU_DIR)/spu_main.c -o $(SPU_DIR)/spu_main.o; \
		$(SPU_LD) $(SPU_LDFLAGS) -L$(PSL1GHT)/spu/lib $(SPU_DIR)/spu_main.o -lsputhread -o $(SPU_ELF); \
		echo "Embedding SPU ELF into PPU object via bin2s..."; \
		$(SPU_BIN2S) -a 64 $(SPU_ELF) | $(SPU_AS) -o $(SPU_EMBED_OBJ); \
		echo "SPU build complete"; \
	else \
		echo "SPU pre-built artifacts found, skipping build"; \
	fi

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) build build_debug ps364_glN64.elf ps364_glN64.self ps364_debug.elf ps364_debug.self *.pkg *.map
	@rm -f $(foreach dir,$(SOURCES),$(dir)/*.o) $(foreach dir,$(SOURCES),$(dir)/*.d)
	@echo "Cleaning SPU artifacts..."
	@rm -f $(SPU_DIR)/*.o $(SPU_DIR)/*.elf $(SPU_DIR)/*.bin $(SPU_EMBED_OBJ)

#---------------------------------------------------------------------------------
run:
	ps3load $(OUTPUT).self
	
#---------------------------------------------------------------------------------
pkg:	$(BUILD) $(OUTPUT).pkg

$(OUTPUT).pkg: $(OUTPUT).elf
	@echo "Generando PKG con EBOOT.BIN NPDRM..."
	@mkdir -p $(CURDIR)/pkg/USRDIR
	@$(SELF_NPDRM) $(SCETOOL_FLAGS) --np-content-id=$(CONTENTID) --encrypt $< $(CURDIR)/pkg/USRDIR/EBOOT.BIN
	@C:/PSDK3v2/mingw/Python27/python.exe C:/PSDK3v2/ps3dev/bin/pkg.py --contentid $(CONTENTID) "$(CURDIR)/pkg/" $@

#---------------------------------------------------------------------------------

npdrm: $(BUILD)
	@$(SELF_NPDRM) $(SCETOOL_FLAGS) --np-content-id=$(CONTENTID) --encrypt $(OUTPUT).elf EBOOT.BIN

#---------------------------------------------------------------------------------
# Debug build: make dbg
# Produces ps364_debug.self with -O0 -g3 -DDEBUG_POLYGONS
#---------------------------------------------------------------------------------
dbg:
	@echo "=== Building DEBUG version ==="
	$(MAKE) DEBUG=1

#---------------------------------------------------------------------------------
# Release build: make rel (same as default make)
# Produces ps364_glN64.self with -O3 -DNDEBUG
#---------------------------------------------------------------------------------
rel:
	@echo "=== Building RELEASE version ==="
	$(MAKE)

#---------------------------------------------------------------------------------
# Build both debug and release
#---------------------------------------------------------------------------------
all: rel dbg

#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
else

export BUILDDIR	:=	$(CURDIR)

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin extension
#---------------------------------------------------------------------------------
%.z64.o	:	%.z64
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
%.vpo.o	:	%.vpo
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.fpo.o	:	%.fpo
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
