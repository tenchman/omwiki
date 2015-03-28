#!/bin/sh
echo "Useful for dev only. kill didiwiki daemon. ctr-c to quit"
read a
kill -15 `pidof didiwiki`
