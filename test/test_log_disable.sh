#!/bin/bash

# test fails if any logging functions are exported
nm $1 | grep ' T ' | grep ' UDS_Log*'
test $? = 1