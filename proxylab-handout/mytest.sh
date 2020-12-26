#!/bin/bash

HOME_DIR=`pwd`
PROXY_DIR="./.proxy"
NOPROXY_DIR="./.noproxy"
TIMEOUT=5
MAX_RAND=63000
PORT_START=1024
PORT_MAX=65000
MAX_PORT_TRIES=10

#
# download_proxy - download a file from the origin server via the proxy
# usage: download_proxy <testdir> <filename> <origin_url> <proxy_url>
#
function download_proxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --proxy $4 --output $2 $3
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# download_noproxy - download a file directly from the origin server
# usage: download_noproxy <testdir> <filename> <origin_url>
#
function download_noproxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --output $2 $3 
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

tiny_port=26666
proxy_port=8080

cd tiny
./tiny ${tiny_port} &> /dev/null  &
tiny_pid=$!
cd ..
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

file=home.html

echo download_proxy
download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
echo download_noproxy
download_noproxy $NOPROXY_DIR ${file} "http://localhost:${tiny_port}/${file}"

echo "   Comparing the two files"
diff -q ${PROXY_DIR}/${file} ${NOPROXY_DIR}/${file} # &> /dev/null

kill $tiny_pid
kill $proxy_pid