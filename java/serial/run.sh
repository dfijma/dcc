#! /bin/sh
export JAVA_HOME=`/usr/libexec/java_home -v 11`
stty_save=$(stty -g)
tput civis
stty -echo
stty cbreak
./build/install/serial/bin/serial "$@"
stty $stty_save
tput cnorm
clear
