#
# File: makefile
#
# Description:
#   Makefile definitions for compiling and linking
#   List of object files
#   Build targets and rules
#   
#
# Author: Royce Muchmore
#
# Tools
#   Compiler: gcc, arm-unknown-linux-gnueabi-gcc
#   Linker: gcc, arm-unknown-linux-gnueabi-gcc
#   Debugger: gdb
#
# Leveraged Code:
#  https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
#
# Links:
#  https://www.gnu.org/software/make/manual/make.html#Reference
#  
#

#-include ../makefile.init

# StackOverflow
# if the first command line argument is "print"
ifeq ($(firstword $(MAKECMDGOALS)),print)

  # take the rest of the arguments as variable names
  VAR_NAMES := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))

  # turn them into do-nothing targets
  #$(eval $(VAR_NAMES):;@:))
  $(info $(VAR_NAMES))

  # then print them
  .PHONY: print
  print:
	@$(foreach var,$(VAR_NAMES),\
	echo '$(var) = $($(var))';)
endif

RM := rm -rf

#-include makefile.defs

#CROSS_COMPILE=
#CROSS_COMPILE=aarch64-buildroot-linux-uclibc-
#MY_CC:=$(CROSS_COMPILE)gcc
#MY_LD:=$(CROSS_COMPILE)gcc
MY_CC:=$(CC)
MY_LD:=$(CC)

MY_CC_OPTS= \
   -O0 \
   -g3 \
   -Wall \
   -c
# Remove during development
#   -Werror \
MY_LD_OPTS=

MY_VEHICLE_TARGET:=./vehicle/vehicle
MY_VEHICLE_DEFS=
MY_VEHICLE_INCLUDES=

MY_SCAN_TOOL_TARGET:=./scan_tool/scan_tool
MY_SCAN_TOOL_DEFS=
MY_SCAN_TOOL_INCLUDES=

# Sources are located in these folders
VPATH=

# Objects to build from sources
MY_VEHICLE_OBJS = \
./vehicle/vehicle.o

MY_VEHICLE_DEPS = $(MY_VEHICLE_OBJS:.o=.d)
MY_VEHICLE_SUS = $(MY_VEHICLE_OBJS:.o=.su)


MY_SCAN_TOOL_OBJS = \
./scan_tool/scan_tool.o

MY_SCAN_TOOL_DEPS = $(MY_SCAN_TOOL_OBJS:.o=.d)
MY_SCAN_TOOL_SUS = $(MY_SCAN_TOOL_OBJS:.o=.su)


# Compile sources to objects
./vehicle/%.o : %.c
	@echo 'Building file: $(@:%.o=%.c)'
	@echo 'Invoking: C Compiler'
	$(MY_CC) $(MY_VEHICLE_DEFS) $(MY_VEHICLE_INCLUDES) $(MY_CC_OPTS) -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

./scan_tool/%.o : %.c
	@echo 'Building file: $(@:%.o=%.c)'
	@echo 'Invoking: C Compiler'
	$(MY_CC) $(MY_SCAN_TOOL_DEFS) $(MY_SCAN_TOOL_INCLUDES) $(MY_CC_OPTS) -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
	

# All Target
all: $(MY_VEHICLE_TARGET) $(MY_SCAN_TOOL_TARGET)

default: $(MY_VEHICLE_TARGET) $(MY_SCAN_TOOL_TARGET)

# Link objects to image
$(MY_VEHICLE_TARGET): $(MY_VEHICLE_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Linker'
	$(MY_LD) $(MY_LD_OPTS)  -o "$(MY_VEHICLE_TARGET)" $(MY_VEHICLE_OBJS) $(MY_VEHICLE_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

$(MY_SCAN_TOOL_TARGET): $(MY_SCAN_TOOL_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Linker'
	$(MY_LD) $(MY_LD_OPTS)  -o "$(MY_SCAN_TOOL_TARGET)" $(MY_SCAN_TOOL_OBJS) $(MY_SCAN_TOOL_LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(MY_VEHICLE_OBJS) $(MY_VEHICLE_DEPS) $(MY_VEHICLE_SUS) $(MY_VEHICLE_TARGET)
	-$(RM) $(MY_SCAN_TOOL_OBJS) $(MY_SCAN_TOOL_DEPS) $(MY_SCAN_TOOL_SUS) $(MY_SCAN_TOOL_TARGET)
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-@echo ' '
	cp ./vehicle/vehicle ../../../rootfs_overlay_beaglebone/usr/bin
	cp ./scan_tool/scan_tool ../../../rootfs_overlay_beaglebone/usr/bin

.PHONY: all clean dependents post-build

