#mm-link trace10 trace10 --uplink-queue=droptail --downlink-queue=infinite --uplink-queue-args="packets=10000"
mm-delay 10 mm-link trace10 trace100 --uplink-queue=codel --downlink-queue=infinite --uplink-queue-args="packets=300, target=50, interval=100"
#mm-link trace10 trace10 --uplink-queue=infinite --downlink-queue=pie --downlink-queue-args="packets=300, qdelay_ref=50, max_burst=1" mm-delay 10

