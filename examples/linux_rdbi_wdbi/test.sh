#!/bin/bash

SERVER=$1
CLIENT=$2

CANLOG=$TEST_UNDECLARED_OUTPUTS_DIR/CAN.log
(candump -L vcan0 > $CANLOG )&
pid_candump=$!

( timeout 4 $SERVER )&
pid_server=$!
echo "Server pid: $pid_server"
sleep 0.2
( timeout 3 $CLIENT )&
pid_client=$!

wait $pid_client
status_client=$?
echo "client exited with status $status_client"

kill $pid_server
wait $pid_server
status_server=$?
echo "server exited with status $status_server"

kill $pid_candump
echo "full CAN log:"
cat $CANLOG

exit $status_client
