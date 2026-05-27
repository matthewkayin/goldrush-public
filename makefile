# Disable legacy SCCS/RCS suffix rules
%:: %,v
%:: RCS/%,v
%:: RCS/%
%:: s.%
%:: SCCS/s.%

# ------------------------------------------------------------------------------
# Project
# ------------------------------------------------------------------------------

TARGET          := gold
BUILD_DIR       := bin
OBJ_DIR         := obj
SRC_DIR         := src
LIB_DIR         := lib

CXX             := clang++
CXXSTD          := -std=c++17

INCLUDE_DIRS    := -Isrc -Ivendor
DEFINES         := -D_CRT_SECURE_NO_WARNINGS

CXXFLAGS        := $(CXXSTD)
LDFLAGS         := $(CXXSTD)

EXTENSION       :=
RC_FILES        :=

# ------------------------------------------------------------------------------
# Build Configuration
# ------------------------------------------------------------------------------

ifeq ($(RELEASE_VERSION),)
	CXXFLAGS += -O0 -g -Wall -Wextra -Wshadow
	LDFLAGS  += -g
else
	CXXFLAGS += -O2
	DEFINES  += -DRELEASE_VERSION=\"$(RELEASE_VERSION)\"
endif

ifeq ($(profiler),enabled)
	CXXFLAGS += -Ivendor/tracy
	DEFINES  += -DTRACY_ENABLE
endif

# ------------------------------------------------------------------------------
# Platform Detection
# ------------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)

	PLATFORM   := win64
	EXTENSION  := .exe
	LIB_DIR    := lib/win64

	# Recursive wildcard
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	SRC_FILES   := $(call rwildcard,$(SRC_DIR)/,*.cpp)
	RC_FILES    := $(SRC_DIR)/win_icon.rc
	DIRECTORY   := $(subst /,\,${CURDIR})
	DIRECTORIES := \$(SRC_DIR) $(subst $(DIRECTORY),,$(shell dir $(SRC_DIR) /S /AD /B | findstr /i $(SRC_DIR)))

	CXXFLAGS += \
		-Wno-missing-designated-field-initializers \
		-march=x86-64-v2

	LDFLAGS += \
		-L$(LIB_DIR) \
		-lSDL3 \
		-lSDL3_ttf \
		-lSDL3_mixer \
		-luser32 \
		-lws2_32 \
		-lwinmm \
		-lenet64 \
		-lsteam_api64 \
		-ldbghelp \
		-llua51

else

	UNAME_S := $(shell uname -s)

	SRC_FILES := $(shell find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.mm" \))
	DIRECTORIES := $(shell find src -type d)

	ifeq ($(UNAME_S),Darwin)

		PLATFORM := macos
		LIB_DIR  := lib/macos

		CXXFLAGS += \
			-Wno-deprecated-declarations \
			-mmacos-version-min=14.5

		LDFLAGS += \
			-L$(LIB_DIR) \
			-lenet \
			-lluajit \
			-lsteam_api \
			-F$(LIB_DIR) \
			-framework SDL3 \
			-framework SDL3_ttf \
			-framework SDL3_mixer

		ifeq ($(RELEASE_VERSION),)
			LDFLAGS += -rpath ../$(LIB_DIR)
		endif

	endif

	ifeq ($(UNAME_S),Linux)

		PLATFORM := linux
		LIB_DIR  := lib/linux64

		CXXFLAGS += -march=x86-64-v2

		LDFLAGS += \
			-lSDL3 \
			-lSDL3_ttf \
			-lSDL3_mixer \
			-ldl \
			-L$(LIB_DIR) \
			-Wl,-rpath='$$ORIGIN',--enable-new-dtags \
			-lenet \
			-lluajit \
			-lsteam_api

	endif

endif

# ------------------------------------------------------------------------------
# Source/Object Lists
# ------------------------------------------------------------------------------

OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)

ifeq ($(PLATFORM),win64)
	OBJ_FILES += $(RC_FILES:%=$(OBJ_DIR)/%.res)
endif

# ------------------------------------------------------------------------------
# Default Target
# ------------------------------------------------------------------------------

.PHONY: all
all: scaffold compile link

# ------------------------------------------------------------------------------
# Directory Setup
# ------------------------------------------------------------------------------

.PHONY: scaffold
scaffold:
	@echo Scaffolding...
ifeq ($(PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
endif
	@echo Done.

# ------------------------------------------------------------------------------
# Compile
# ------------------------------------------------------------------------------

# This phony just prints messages before compilation
.PHONY: compile
compile:
	@echo Compiler flags: $(CXXFLAGS)
	@echo Defines:       $(DEFINES)
	@echo Compiling...

# Compile cpp to object
$(OBJ_DIR)/%.cpp.o: %.cpp
	@echo   $<...
	@$(CXX) $< $(CXXFLAGS) -c -o $@ $(DEFINES) $(INCLUDE_DIRS)

# Compile Windows resource scripts
ifeq ($(PLATFORM),win64)
$(OBJ_DIR)/%.rc.res: %.rc
	@rc -fo $@ $^
endif

# ------------------------------------------------------------------------------
# Link
# ------------------------------------------------------------------------------

.PHONY: link
link: scaffold $(OBJ_FILES)
	@echo Linker flags $(LINKER_FLAGS)
	@echo Linking $(ASSEMBLY)...
ifeq ($(PLATFORM),win64)
	@$(CXX) $(OBJ_FILES) -o $(BUILD_DIR)\$(TARGET)$(EXTENSION) $(LDFLAGS)
else
	@$(CXX) $(OBJ_FILES) -o $(BUILD_DIR)/$(TARGET)$(EXTENSION) $(LDFLAGS)
endif

# ------------------------------------------------------------------------------
# Clean
# ------------------------------------------------------------------------------

.PHONY: clean
clean:
	@echo Cleaning...

ifeq ($(PLATFORM),win64)
	@if exist $(BUILD_DIR)\$(TARGET)$(EXTENSION) del $(BUILD_DIR)\$(TARGET)$(EXTENSION)
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
else
	@rm -rf $(OBJ_DIR)
	@rm -f $(BUILD_DIR)/$(TARGET)$(EXTENSION)
endif

ifeq ($(PLATFORM),macos)
	@rm -rf $(BUILD_DIR)/Gold\ Rush.app
endif

# ------------------------------------------------------------------------------
# Bundle
# ------------------------------------------------------------------------------

.PHONY: bundle
bundle:

ifeq ($(PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\sprite
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y /s .\res\sprite $(BUILD_DIR)\sprite
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\shader
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y /s .\res\shader $(BUILD_DIR)\shader
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\font
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y /s .\res\font $(BUILD_DIR)\font
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\sfx
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y /s .\res\sfx $(BUILD_DIR)\sfx
	-@setlocal enableextensions enabledelayedexpansion && cd $(BUILD_DIR) && tar.exe -acvf goldrush_windows.zip gold.exe *.dll *.lib font sfx shader sprite
endif

ifeq ($(PLATFORM),macos)
	@./appify.sh -s $(BUILD_DIR)/gold -i icon.icns
	@mv $(ASSEMBLY).app $(BUILD_DIR)/Gold\ Rush.app
	@cp -a ./res/ $(BUILD_DIR)/Gold\ Rush.app/Contents/Resources/
	@cp -a ./lib/macos/ $(BUILD_DIR)/Gold\ Rush.app/Contents/MacOS/
	@mkdir $(BUILD_DIR)/Gold\ Rush.app/Contents/Frameworks
	@cp -r ./lib/macos/*.framework $(BUILD_DIR)/Gold\ Rush.app/Contents/Frameworks/
	@install_name_tool -add_rpath @executable_path/../Frameworks $(BUILD_DIR)/Gold\ Rush.app/Contents/MacOS/gold
	@cd $(BUILD_DIR) && zip -vr ./goldrush_macos.zip ./Gold\ Rush.app/
endif

ifeq ($(PLATFORM),linux)
	@cp -a ./res/* $(BUILD_DIR)/
	@cp -a ./lib/linux64/* $(BUILD_DIR)/
	@tar -czvf goldrush_linux.tar.gz -C $(BUILD_DIR) .
	@mv goldrush_linux.tar.gz $(BUILD_DIR)/goldrush_linux.tar.gz
endif

# ------------------------------------------------------------------------------
# Utility Targets
# ------------------------------------------------------------------------------

.PHONY: libcopy
libcopy:
ifeq ($(PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && xcopy $(LIB_DIR) $(BUILD_DIR)
endif
ifeq ($(PLATFORM),macos)
	-@cp $(LIB_DIR)/*.a $(BUILD_DIR)/
	-@cp $(LIB_DIR)/*.dylib $(BUILD_DIR)/
endif

.PHONY: luadoc
luadoc:
ifeq ($(PLATFORM),win64)
	@cd $(BUILD_DIR) && $(TARGET)$(EXTENSION) --lua-doc
else
	@cd $(BUILD_DIR) && ./$(TARGET)$(EXTENSION) --lua-doc
endif

.PHONY: resource-pack
resource-pack:
ifeq ($(PLATFORM),win64)
	@cd $(BUILD_DIR) && $(TARGET)$(EXTENSION) --resource-pack
else
	@cd $(BUILD_DIR) && ./$(TARGET)$(EXTENSION) --resource-pack
endif

.PHONY: road-data
road-data:
ifeq ($(PLATFORM),win64)
	@cd $(BUILD_DIR) && $(TARGET)$(EXTENSION) --road-data
else
	@cd $(BUILD_DIR) && ./$(TARGET)$(EXTENSION) --road-data
endif

.PHONY: scenario-export
scenario-export:
ifeq ($(PLATFORM),win64)
	@cd $(BUILD_DIR) && $(TARGET)$(EXTENSION) --scenario-export
else
	@cd $(BUILD_DIR) && ./$(TARGET)$(EXTENSION) --scenario-export
endif
