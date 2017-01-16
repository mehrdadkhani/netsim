#mm-link trace10 trace10 --uplink-queue=droptail --downlink-queue=infinite --uplink-queue-args="packets=10000"
#mm-link trace10 trace10 --uplink-queue=infinite --downlink-queue=pie --downlink-queue-args="packets=300, qdelay_ref=50, max_burst=1" mm-delay 10
###########################################################
# Constant:
#mm-delay 10 mm-link trace10 trace100 --uplink-queue=pie --downlink-queue=infinite --uplink-queue-args="packets=300, qdelay_ref=10, max_burst=1"
###########################################################
# Variable:
mm-delay 10 mm-link traces/Verizon-LTE-short.up trace100  --uplink-log=RL_LOG_VERIZON_UP --uplink-queue=pie --downlink-queue=infinite --uplink-queue-args="packets=300, qdelay_ref=10, max_burst=1"
