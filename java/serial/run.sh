#! /bin/sh
export JAVA_HOME=`/usr/libexec/java_home -v 11`
mvn -Dmaven.test.skip=true install
stty_save=$(stty -g)
tput civis
stty -echo
stty cbreak
stty $stty_save
java -jar ./target/serial-1.0-SNAPSHOT-jar-with-dependencies.jar "$@"
tput cnorm
# clear
