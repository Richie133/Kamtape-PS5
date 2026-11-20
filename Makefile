# KamTape PS5 - build entry points (Linux/WSL).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   make            the app folder dist/PPSA52005 and dist/PPSA52005.zip
#   make snapshots  PC pictures of the screens in build/snapshots
#   make test       the PC tests
#
# Based on ps5-native-app-boilerplate by BlackBearReloaded (GPL-3.0-or-later).

SHELL := /bin/bash
.DEFAULT_GOAL := app

-include .env

APP_DEFINITIONS ?=
APP_INCLUDE_PATHS ?=
APP_STATIC_ARCHIVES ?=
APP_IMPORT_STUBS ?=
APP_RUNTIME_MODULES ?=
PS5_OPENGL_PREFIX ?=

# The app always builds against ps5-opengl; user APP_* values append.
OPENGL_SDK := .deps/ps5-opengl/current
override APP_DEFINITIONS := $(strip GL_GLEXT_PROTOTYPES=1 $(APP_DEFINITIONS))
override APP_INCLUDE_PATHS := $(strip src $(OPENGL_SDK)/include $(APP_INCLUDE_PATHS))
override APP_STATIC_ARCHIVES := $(strip .deps/ps5-opengl/libps5opengl-group.a $(APP_STATIC_ARCHIVES))
override APP_IMPORT_STUBS := $(strip $(OPENGL_SDK)/lib/libSceAgc.so \
	$(OPENGL_SDK)/lib/libSceAgcDriver.so $(APP_IMPORT_STUBS))
PACBREW_PACKAGES ?=
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
BUILD_JOBS ?= $(shell nproc 2>/dev/null || echo 2)
USE_CCACHE ?= 1
export BUILD_JOBS USE_CCACHE
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_IMPORT_STUBS APP_RUNTIME_MODULES
export PS5_OPENGL_PREFIX
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh tools/build-host-tools.sh tools/ninja-build.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)

.PHONY: all app build deps doctor libc opengl snapshots test assets-check clean distclean help

all: app
build: app

doctor:
	@printf '%s\n' '==> [doctor] Checking the Linux/WSL host without changing it'
	@bash tools/doctor.sh

deps:
	@printf '%s\n' '==> [deps] Fetching declared native dependencies'
	@bash tools/setup-native-dependencies.sh
	@bash tools/prepare-opengl.sh

opengl:
	@printf '%s\n' '==> [opengl] Preparing the ps5-opengl SDK link group'
	@bash tools/prepare-opengl.sh

assets-check:
	@printf '%s\n' '==> [assets] Validating the launcher icon'
	@bash tools/validate-assets.sh

libc:
	@printf '%s\n' '==> [libc] Rebuilding and verifying the clean-room runtime'
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

app: $(RUNTIME) opengl
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder

snapshots:
	@printf '%s\n' '==> [snapshots] Running the app on the PC, pictures in build/snapshots'
	@bash tools/host-snapshots.sh

test:
	@printf '%s\n' '==> [test] PC tests'
	@bash tools/run-tests.sh

clean:
	@printf '%s\n' '==> [clean] Removing generated build outputs'
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)

distclean: clean
	@printf '%s\n' '==> [distclean] Removing downloaded dependency caches'
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make            Build dist/PPSA52005 (the app folder) and dist/PPSA52005.zip' \
	  'make snapshots  Run the app on the PC and write PNGs to build/snapshots' \
	  'make test       Build and run the PC tests' \
	  'make deps       Fetch the PS5 payload SDK and ps5-opengl into .deps/' \
	  'make doctor     Check required and optional Linux/WSL tools' \
	  'make libc       Force a deterministic runtime/libc.prx rebuild' \
	  'make clean      Remove build outputs' \
	  'make distclean  Also remove the .deps/ cache' \
	  'Build speed:    BUILD_JOBS defaults to all CPUs; USE_CCACHE=0 disables ccache'
