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
BUILD		:=	build
SOURCES		:=	src/main src/core/n64_audio src/core/n64_input src/core/n64_memory src/core/rsp src/core/r4300 src/core/r4300/ppc src/ui src/ui/libgui src/ui/fileBrowser src/platform/ps3 src/video/glN64
DATA		:=	data
SHADERS		:=	src/platform/ps3/shaders
INCLUDES	:= . $(SOURCES)
#LIBRARIES	:= -LJ:/PS3/PSDK3v2/MinGW/Lib

# ID del contenido para el paquete de PS3 (Requerido para pkg_package)
CONTENTID	:= UP0001-PS364GLN6_00-0000111122223333

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

CFLAGS		= -O3 -Wall -mcpu=cell -mtune=cell $(MACHDEP) $(INCLUDE) \
			-fno-exceptions -Wno-unused-parameter -pipe -DUSE_EXPANSION -D__BIG_ENDIAN__ -DNDEBUG -D_GLIBCXX_DEBUG=0 -U_GLIBCXX_DEBUG \
			-include ../src/main/winlnxdefs.h \
			-DPPC -D_BIG_ENDIAN -DPS3 -DPPC_DYNAREC -DUSE_RECOMP_CACHE -D__PSL1GHT__
	  
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
					$(sFILES:.s=.o) $(SFILES:.S=.o)

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
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg *.map
	@rm -f $(foreach dir,$(SOURCES),$(dir)/*.o) $(foreach dir,$(SOURCES),$(dir)/*.d)

#---------------------------------------------------------------------------------
run:
	ps3load $(OUTPUT).self
	
#---------------------------------------------------------------------------------
pkg:	$(BUILD) $(OUTPUT).pkg

$(OUTPUT).pkg: $(OUTPUT).self
	@echo "Generando PKG usando la carpeta 'pkg' de la raiz..."
	@mkdir -p $(CURDIR)/pkg/USRDIR
	@cp $< $(CURDIR)/pkg/USRDIR/EBOOT.BIN
	@$(PKG) --contentid $(CONTENTID) $(CURDIR)/pkg/ $@

#---------------------------------------------------------------------------------

npdrm: $(BUILD)
	@$(SELF_NPDRM) $(SCETOOL_FLAGS) --np-content-id=$(CONTENTID) --encrypt $(OUTPUT).elf EBOOT.BIN

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
