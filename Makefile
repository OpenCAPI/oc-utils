#
# Copyright 2016 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
ifneq ($(CROSS_COMPILE),)
CC=$(CROSS_COMPILE)gcc
endif

ifneq ($(prefix),)
prefix=/usr/local
endif

HAS_GIT = $(shell git describe --tags > /dev/null 2>&1 && echo y || echo n)
VERSION=0.0.99-no-git

ifeq (${HAS_GIT},y)
	GIT_BRANCH=$(shell git describe --always --tags)
	VERSION:=$(GIT_BRANCH)
endif

CFLAGS=-Wall -Wno-unused-but-set-variable -Wno-unused-result -Wno-unused-variable -W -g -O2 -fno-stack-protector -I./include -DGIT_VERSION=\"$(VERSION)\"

ARCH_SUPPORTED:=$(shell echo -e "\n\#if !(defined(_ARCH_PPC64) && defined(_LITTLE_ENDIAN))"\
	"\n\#error \"This tool is only supported on ppc64le architecture\""\
	"\n\#endif" | ($(CC) $(CFLAGS) -E -o /dev/null - 2>&1 || exit 1))

ifneq ($(strip $(ARCH_SUPPORTED)),)
$(error Target not supported. Currently OpenCAPI utils is only supported on ppc64le)
endif

install_point=lib/oc-utils

TARGETS=oc-flash oc-reload

install_files = $(TARGETS) oc-utils-common.sh oc-flash-script.sh oc-reset.sh oc-reload.sh oc-list-cards.sh oc-devices

.PHONY: all 
all: $(TARGETS)

oc-flash: src/flsh_global_vars.c src/flsh_common_funcs.c src/flsh_main.c
	$(CC) $(CFLAGS) $^ -o $@
oc-reload: src/flsh_global_vars.c src/flsh_common_funcs.c src/img_reload.c
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: install
install: $(TARGETS)
	@chmod a+x oc-flash-*
	@mkdir -p $(prefix)/$(install_point)
	@cp $(install_files) $(prefix)/$(install_point)
	@ln -sf $(prefix)/$(install_point)/oc-flash-script.sh \
		$(prefix)/bin/oc-flash-script
	@ln -sf $(prefix)/$(install_point)/oc-reset.sh \
		$(prefix)/bin/oc-reset
	@ln -sf $(prefix)/$(install_point)/oc-reload.sh \
		$(prefix)/bin/oc-reload
	@ln -sf $(prefix)/$(install_point)/oc-list-cards.sh \
		$(prefix)/bin/oc-list-cards

.PHONY: uninstall
uninstall:
	@rm -rf $(prefix)/$(install_point)
	@rm -f $(prefix)/bin/oc-flash-script
	@rm -f $(prefix)/bin/oc-reset
	@rm -f $(prefix)/bin/oc-reload
	@rm -f $(prefix)/bin/oc-list-cards

.PHONY: clean
clean:
	@rm -rf $(TARGETS)

