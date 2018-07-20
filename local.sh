#!/bin/sh

while sleep 1; do 
    socat PTY,link=stellarisTty0,rawer,wait-slave EXEC:./remote.sh
done
