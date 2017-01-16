#<<<<<<< HEAD
xterm -geometry 93x31+300+150 -e "iperf -c 100.64.0.1 -t 60000 -i 5 -N -P 10" &
xterm -geometry 233x31+500+650 -hold -e "cd ../../rl-qm/rl-framework/; ./run.sh 100.64.0.4" &
#=======
#xterm -geometry 93x31+300+150 -e "iperf -c 100.64.0.1 -i 2 -t 10000 -N" &
#xterm -geometry 93x31+200+150 -e "ping 100.64.0.1"&
#xterm -geometry 233x31+500+650 -hold -e "cd ../rl-qm/mahimahiSocketInterfaceSingleLoop/; python RLMahimahiInterface_beta.py 100.64.0.4" &
#xterm &
#>>>>>>> aa18dc10445ce79c181e883442ba0db72881f0ce
xterm
pkill xterm
