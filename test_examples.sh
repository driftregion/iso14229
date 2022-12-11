#!/bin/bash

ERR=0

./server vcan0 & 
server_pid=$!

./client vcan0 &
client_pid=$!

sleep 1 &
timeout_pid=$!

while ps -p $client_pid > /dev/null && ps -p $timeout_pid > /dev/null
do
    sleep 1
done

if ps -p  $client_pid > /dev/null
then
    kill $client_pid
    ERR=-1
else
    wait $client_pid
    client_exit_code=$?
    if [ $client_exit_code -ne 0 ] 
    then
        echo "client exited with error code " $?
        ERR=1
    fi
fi

if ps -p  $server_pid > /dev/null
then
    echo "server didn't exit"
    kill $server_pid
    ERR=-1
else
    wait $server_pid
    server_exit_code=$?
    if [ $server_exit_code -ne 0 ] 
    then
        echo "server exited with error code " $?
        ERR=1
    fi
fi

wait
exit $ERR
