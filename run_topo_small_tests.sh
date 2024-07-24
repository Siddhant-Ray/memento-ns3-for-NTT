#!/bin/bash

# Original author: Siddhant Ray

## First in arguments is the topology
## Second specifies different congestion for different receivers (default is 0)
## Third specifies the seed for the random number generator (change for multiple runs)

## Current setup generates fine-tuning data with 2 bottlenecks, $2 is the second bottleneck rate (!=0)
## To generate pre-training data with only one bottleneck, replace --prefix with 
## --prefix=results/small_test_no_disturbance_with_message_ids$3 and pass $2 as 0

#    // Network topology 1
#    //
#    //                                  disturbance1
#    //                                       |
#    // 3x n_apps(senders) --- switchA --- switchB --- receiver1
#    // 

# If running inside the VSCode's environment to run Docker containers: Replace ./docker-run.sh waf with just waf 

# queue_size=10
bandwidth=100
nseed=0
tcpcc="dctcp"
# loop over queue sizes 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
# Make queue size 1, 10, 100 and 1000
q_array=(1 10 100 1000)
for queue_size in "${q_array[@]}"
do 
    # increment seed by 1
    nseed=$((nseed+1))
    echo "Running with seed $nseed and queue size $queue_size"
    mkdir -p results_test
    ./docker-run.sh waf --run "trafficgen_small_tests
                        --topo=$1
                        --apps=20
                        --apprate=1Mbps
                        --startwindow=50
                        --queuesize=$queue_size"p"
                        --linkrate=$bandwidth"Mbps"
                        --linkdelay=5ms
                        --w1=1
                        --w2=1
                        --w3=1
                        --cc=ns3::TcpDctcp
                        --congestion1=$2Mbps
                        --prefix=results_test/small_test_no_disturbance_with_message_ids1
                        --useL4s=True
                        --ceThreshold=1
                        --useECT0=False
                        --seed=$nseed"

    mkdir -p results_test/1Mbps_60senders_inf_rst_dctcp_1_$bandwidth"mbps_qs_"$queue_size
    mv results_test/*.csv results_test/1Mbps_60senders_inf_rst_dctcp_1_$bandwidth"mbps_qs_"$queue_size

    python visualise.py --apprate 1 --n_senders 60 --rst True --seed 1 --tcpcc dctcp --infinitedata True --bw $bandwidth --qs $queue_size 
done