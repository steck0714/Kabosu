#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

APP_TITLE	:=	PPT2NX-AI
APP_AUTHOR	:=	mktm235
APP_VERSION	:=	1.0.0

# libtesla (https://github.com/WerWolv/libtesla): clone it here or set LIBTESLA
LIBTESLA	?=	libtesla

TARGET		:=	ppt2nx-ai
# RELEASE=1: release build, in build-release/ and out-release/
RELEASE		?=	1
ifeq ($(RELEASE),1)
BUILD		:=	build-release
OUT		:=	out-release
else
BUILD		:=	build
OUT		:=	out
endif
SOURCES		:=	source ../common
DATA		:=	data
INCLUDES	:=	source ../common $(LIBTESLA)/include

NO_ICON		:=	1

#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS	+=	$(INCLUDE) -D__SWITCH__ -DPPT2AI_RELEASE=$(RELEASE)
# keep the build path out of __FILE__
CFLAGS	+=	-fmacro-prefix-map=$(TOPDIR)/=
CXXFLAGS	:= $(CFLAGS) -fno-exceptions -std=c++20
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx
LIBDIRS	:= $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(if $(filter 1,$(RELEASE)),$(BUILD)/,)$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_SRC)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export NROFLAGS += --nacp=$(OUTPUT).nacp

.PHONY: $(BUILD) clean all dist

all: $(BUILD)

$(BUILD):
	@[ -d $(LIBTESLA)/include ] || { echo "libtesla not found at $(LIBTESLA)"; exit 1; }
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

# SD card layout: switch/.overlays/<target>.ovl
dist: all
	@rm -rf $(OUT)
	@mkdir -p $(OUT)/switch/.overlays
	@cp $(OUTPUT).ovl $(OUT)/switch/.overlays/$(TARGET).ovl
	@echo built $(OUT)/

clean:
	@echo clean ...
	@rm -fr build build-release out out-release $(TARGET).ovl $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

all	:	$(OUTPUT).ovl

$(OUTPUT).ovl	:	$(OUTPUT).elf $(OUTPUT).nacp
	@elf2nro $< $@ $(NROFLAGS)
	@echo "built ... $(notdir $(OUTPUT).ovl)"

$(OUTPUT).elf	:	$(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
