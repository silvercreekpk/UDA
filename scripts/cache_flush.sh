#!/bin/sh

echo 3 > /proc/sys/vm/drop_caches
echo "Cache flushed on " $(hostname)
