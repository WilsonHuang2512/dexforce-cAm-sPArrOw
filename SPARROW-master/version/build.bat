@echo off
set PATH=%PATH%;C:/cygwin64/bin
cd

dos2unix "./build.sh"
bash "./build.sh"
