#! /bin/sh
export JAVA_HOME=`/usr/libexec/java_home -v 11`
mvn -Dmaven.test.skip=true install
stty_save=$(stty -g)
tput civis
stty -echo
stty cbreak
java -jar ./target/serial-1.0-SNAPSHOT-jar-with-dependencies.jar "$@"
stty $stty_save
tput cnorm
clear
