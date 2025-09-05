# Idea and test code will be for once it's running.  Idea code is the mind ROM code ideas.

TYPE_CODE=./inc
PART_CODE=./src
MAKE_AREA=./target
CODE_LIST=$(shell find $(PART_CODE) -name "*.c")
MADE_FILE=flip
LOGS_MODE?=LOGS_FINE

all: flip

flip: tidy prep link test

link: prep
	aarch64-linux-android21-clang -fPIE -pie -O2 -I$(TYPE_CODE) $(CODE_LIST) -o $(MAKE_AREA)/$(MADE_FILE) -DLOGS_MODE=$(LOGS_MODE)

test: link
	adb push $(MAKE_AREA)/$(MADE_FILE) /data/local/tmp/$(MADE_FILE)
	adb shell "chmod 755 /data/local/tmp/$(MADE_FILE)"
	/usr/bin/env adb shell /data/local/tmp/$(MADE_FILE) 8080

prep: 
	mkdir -p $(MAKE_AREA)

tidy:
	rm -fr $(MAKE_AREA)

