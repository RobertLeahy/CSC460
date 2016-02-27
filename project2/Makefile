SUFFIXES+=.mk


GCC:=avr-gcc


ifeq ($(RELEASE),YES)
	OPTIMIZATION:=-O2
else
	OPTIMIZATION:=-O0 -Wall -Wpedantic -Wextra -Werror -DDEBUG
endif


OPTS_SHARED:=-I ./include -std=gnu11 -mmcu=atmega2560 -DF_CPU=16000000L -DARDUINO=10607 -DARDUINO_AVR_MEGA2560 -DARDUINO_ARCH_AVR -ffunction-sections -fdata-sections


ifeq ($(OS),Windows_NT)
	MAKE_PARENT=mkdir.bat $(subst /,\,$(dir $(1)))
	RMDIR=rmdir.bat $(subst /,\,$(1))
	MKDIR=mkdir.bat $(subst /,\,$(1))
	MODULE_EXT:=.dll
	EXECUTABLE_EXT:=.exe
else
	MAKE_PARENT=mkdir -p $(dir $(1))
	RMDIR=rm -r -f $(1)
	MKDIR=mkdir $(1)
	OPTS_SHARED:=$(OPTS_SHARED) -fPIC
	MODULE_EXT:=.so
	EXECUTABLE_EXT:=
endif


GCC:=$(GCC) $(OPTS_SHARED) $(OPTIMIZATION)


.PHONY: all
all:


.PHONY: clean
clean:
	-@$(call RMDIR,obj)
	-@$(call RMDIR,bin)
	-@$(call RMDIR,makefiles)


bin:
	-@$(call MKDIR,bin)
	
	
NODEPS:=clean cleanall cleandeps
	
	
ifeq (0,$(words $(findstring $(MAKECMDGOALS),$(NODEPS))))

	ifeq ($(OS),Windows_NT)
		-include $(subst .c,.mk,$(subst src,makefiles,$(subst \,/,$(subst $(shell echo %CD%)\,,$(shell dir /b /s src\*.c)))))
	else
		-include $(patsubst src/%.c,makefiles/%.mk,$(shell find src/ -name "*.c"))
	endif

endif
	
	
obj/%.o:
	-@$(call MAKE_PARENT,$(patsubst obj/%.o,makefiles/%.mk,$@))
	$(GCC) -MM -MT "$@" $(patsubst obj/%.o,src/%.c,$@) -MF $(patsubst obj/%.o,makefiles/%.mk,$@)
	-@$(call MAKE_PARENT,$@)
	$(GCC) -c -o $@ $(patsubst obj/%.o,src/%.c,$@)

	
OBJS:=\
obj/kernel.o \
obj/main.o \
obj/os.o


all: bin/img.elf
bin/img.elf: \
$(OBJS) | \
bin
	$(GCC) -Wl,--gc-sections,--relax -o $@ $^


all: bin/img.eep
bin/img.eep: bin/img.elf | bin
	avr-objcopy -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 $^ $@


all: bin/img.hex
bin/img.hex: bin/img.elf | bin
	avr-objcopy -O ihex -R .eeprom $^ $@
