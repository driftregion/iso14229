#!/bin/bash

# test fails if any exported symbols do not start with " UDS"
nm $1 | grep ' T ' | grep -v ' UDS'
test $? = 1