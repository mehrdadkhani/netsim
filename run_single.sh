INTF_PATH=../rl-qm/mahimahiSocketInterfaceSingleLoop/
PropDelay=$1
Bandwidth=$2
NoFlow=$3
Scheme=$4
TargetDelay=$5
Interval=$6
./${Scheme}.sh ${PropDelay} ${Bandwidth} ${NoFlow} ${TargetDelay} ${Interval}
TAG="${Scheme}_${PropDelay}_${Bandwidth}_${NoFlow}_${TargetDelay}_${Interval}"
cp ${INTF_PATH}log.txt log/${TAG}
echo ${TAG}



