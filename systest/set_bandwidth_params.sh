#!/bin/sh

set -e

DATADIR=data.systest

clean_data_dir() {
    rm -f  "$DATADIR"/log/log_*.log
    rm -f  "$DATADIR"/wallets.ldb*
    rm -f  "$DATADIR"/data.ldb*
    rm -f  "$DATADIR"/config-*.toml
    rm -rf "$DATADIR"/rocksdb/
}

msg() {
    :
    #echo "$@"
}

# the caller should set the env var VXLNETWORK_NODE_EXE to point to the vxlnetwork_node executable
# if VXLNETWORK_NODE_EXE is unset ot empty then "../../build/vxlnetwork_node" is used
VXLNETWORK_NODE_EXE=${VXLNETWORK_NODE_EXE:-../../build/vxlnetwork_node}

mkdir -p "$DATADIR/log"
clean_data_dir

# start vxlnetwork_node and store its pid so we can later send it
# the SIGHUP signal and so we can terminate it
msg start vxlnetwork_node
$VXLNETWORK_NODE_EXE --daemon --data_path "$DATADIR" >/dev/null &
pid=$!
msg pid=$pid

# wait for the node to start-up
sleep 2

# set bandwidth params 42 and 43 in the config file
cat > "$DATADIR/config-node.toml" <<EOF
[node]
bandwidth_limit = 42
bandwidth_limit_burst_ratio = 43
EOF

# send vxlnetwork_node the SIGHUP signal
kill -HUP $pid

# wait for the signal handler to kick in
sleep 2

# set another set of bandwidth params 44 and 45 in the config file
cat > "$DATADIR/config-node.toml" <<EOF
[node]
bandwidth_limit = 44
bandwidth_limit_burst_ratio = 45
EOF

# send vxlnetwork_node the SIGHUP signal
kill -HUP $pid

# wait for the signal handler to kick in
sleep 2

# terminate vxlnetwork node and wait for it to die
kill $pid
wait $pid

# the bandwidth params are logged in logger and we check for that logging below

# check that the first signal handler got run and the data is correct
grep -q "set_bandwidth_params(42, 43)" "$DATADIR"/log/log_*.log
rc1=$?
msg rc1=$rc1

# check that the second signal handler got run and the data is correct
grep -q "set_bandwidth_params(44, 45)" "$DATADIR"/log/log_*.log
rc2=$?
msg rc2=$rc2

if [ $rc1 -eq 0 -a $rc2 -eq 0 ]; then
    echo $0: PASSED
    exit 0
fi

exit 1
