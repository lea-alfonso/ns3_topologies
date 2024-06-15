#!/bin/bash

for uesPerGnb in 10 20
do
    for useUdp in true false
    do
        for direction in DL UL
        do
            for trafficTypeConf in {0..7}
            do 
                echo "Running with parameters:"
                echo "trafficTypeConf: ${trafficTypeConf}"
                echo "direction: ${direction}"
                echo "useUdp: ${useUdp}"
                echo "uesPerGnb: ${uesPerGnb}"
                ./ns3 run "contrib/nr/examples/topology_1_3.cc --outputDir=./contrib/nr/examples/experiments/initial_states --simTag=first --bottleNeckDelay=0ms --simTimeMs=60400 --direction=${direction} --trafficTypeConf=${trafficTypeConf} --useUdp=${useUdp} --uesPerGnb=${uesPerGnb}"
            done
        done
    done
done