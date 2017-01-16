xterm -geometry 93x11+30+15 -e "iperf -c 100.64.0.1 -i 2 -t 10000 -N -P $1" &
xterm -geometry 93x11+90+15 -e "ping 100.64.0.1"&
xterm -geometry 233x21+50+85 -hold -e "cd ../rl-qm/rl-framework/; ./run.sh 100.64.0.4" &
xterm &
xterm
pkill xterm
