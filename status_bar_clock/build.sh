#!/usr/bin/env bash
gcc -o status_bar_clock.so status_bar_clock.c $(yed --print-cflags) $(yed --print-ldflags)