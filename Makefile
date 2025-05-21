# vim : set noexpandtab shiftwidth=8 softtabstop=8 tabstop=8 :

.PHONY: windows android

build: android
run: windows windows_debug

all: android windows

windows_debug:
	@make -f Makefile.Windows debug

windows:
	@echo ***********************************************
	@echo WINDOWS WINDOWS WINDOWS WINDOWS WINDOWS WINDOWS
	@echo WINDOWS WINDOWS WINDOWS WINDOWS WINDOWS WINDOWS
	@echo ***********************************************
	@make -f Makefile.Windows

android:
	@echo ***********************************************
	@echo ANDROID ANDROID ANDROID ANDROID ANDROID ANDROID
	@echo ANDROID ANDROID ANDROID ANDROID ANDROID ANDROID
	@echo ***********************************************
	@make -f Makefile.Android
