#!/bin/sh
ssh root@192.168.0.148 "socat - /dev/serial/by-id/usb-LMI_Stellaris_Evaluation_Board_070001C1-if01-port0,b115200,nonblock,raw,echo=0"
