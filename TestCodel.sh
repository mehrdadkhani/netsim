mm-delay $1 mm-link $2 $3 --uplink-queue=codel  --once --uplink-log=CODEL_LINK_LOG --downlink-queue=infinite --uplink-queue-args="packets=1000, target=$4,interval=$5, max_burst=1" ./start.sh

