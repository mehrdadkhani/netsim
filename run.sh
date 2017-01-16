PropDelay=10
Bandwidth=trace12
NoFlow=5
Scheme=TestPIE
TargetDelay=10
Interval=20

#for Bandwidth in trace12 trace36 trace60 trace120 trace240 trace480 trace960
for Bandwidth in trace1 trace2 trace6
do
for PropDelay in 5 10 25 50 100
do
for TargetDelay in 5 10 20
do
for Scheme in TestPie TestCodel
do
iperf -s -w 800m &
sleep 1

./run_single.sh ${PropDelay} ${Bandwidth} ${NoFlow} ${Scheme} ${TargetDelay} ${Interval}

pkill iperf
sleep 1
pkill iperf
sleep 1

done

done

done

done
