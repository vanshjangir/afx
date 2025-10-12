#!/bin/bash
SERVER=127.0.0.1
PORT=8080
DATA=$(head -c 512 </dev/urandom | base64 | head -c 512)

for i in {1..10000}; do
  (
    echo "$DATA" | nc $SERVER $PORT > /dev/null
  ) &
done

wait
echo "Done"
