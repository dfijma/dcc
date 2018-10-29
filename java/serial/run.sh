#! /bin/sh
stty_save=$(stty -g)
tput civis
stty -echo
stty cbreak
./build/install/serial/bin/serial
stty $stty_save
tput cnorm
