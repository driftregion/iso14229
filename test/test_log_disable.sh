#!/bin/bash

# test fails if any logging functions are exported when the library is compiled with logging disabled.
nm $1 | grep ' T ' | grep ' UDS_Log*'
test $? = 1
