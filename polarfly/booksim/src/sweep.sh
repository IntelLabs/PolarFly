#!/bin/bash           

#we choose which topologies to simulate
DF=0
MMS=1

#parameters

logFilesPath="output_min_bitrev"

mkdir ${logFilesPath}

injection=( 0.01 0.1 0.2 0.25 0.3 0.35 0.4 0.45 0.5 0.55 0.6 0.65 0.7 0.75 0.8 )
routing_fun=( min )
buff_size=( 64 )
scenario=( bitrev )

pathToDF="examples/dragonflyconfig"
pathToMMS="mms/mmsconfig"
pathToMMSStructure="mms/MMS.29/MMS.29.15.bsconf"
pathToMMSRoutingTable="mms/MMS.29/MMS.29.15.route"

commonContents="packet_size  = 1;
num_vcs      = 2;
vc_allocator = separable_input_first; 
sw_allocator = separable_input_first;

alloc_iters   = 1;
sample_period = 2000;

wait_for_tail_credit = 0;
use_read_write       = 0;

credit_delay   = 2;
routing_delay  = 0;
vc_alloc_delay = 1;
sw_alloc_delay = 1;
st_final_delay = 1;

input_speedup     = 1;
output_speedup    = 1;
internal_speedup  = 2.0;

warmup_periods = 3;
sim_count      = 1;

priority = none;
injection_rate_uses_flits=1;"

for i in "${routing_fun[@]}"
do
	for k in "${buff_size[@]}"
	do
		for b in "${scenario[@]}"
		do
			for j in "${injection[@]}"
			do
				echo ">>>>>>>>>>>>>>>> Simulation for injection rate ${j} and routing function ${i} and scenario ${b} and buff size ${k}"

				if [ "${DF}" == "1" ]; then
					echo ">>>>>>> Dragon Fly... "

					echo "topology = dragonflynew;" > $pathToDF
					echo "k = 7;" 			>> $pathToDF
					echo "n = 1;" 			>> $pathToDF
					echo "injection_rate = ${j};" 	>> $pathToDF
					echo "traffic = ${b};"		>> $pathToDF

					if [ "${b}" == "shuffle" ]; then
						echo "use_size_power_of_two = yes;" >> $pathToDF
					else
						echo "use_size_power_of_two = no;" >> $pathToDF
					fi

					echo "routing_function = ${i};" >> $pathToDF
					echo "vc_buf_size = ${k};"	>> $pathToDF

					echo "${commonContents}" 	>> $pathToDF

					./booksim ./${pathToDF}		> ./${logFilesPath}"/DF.output_scen="${b}"_inj="${j}"_fun="${i}"_buff="${k}".log"
				fi

				if [ "${MMS}" == "1" ]; then
					echo ">>>>>>> MMS... "

					echo "topology = anynet;" 			> $pathToMMS
					echo "k = 15;" 					>> $pathToMMS
					echo "injection_rate = ${j};"		 	>> $pathToMMS
					echo "traffic = ${b};"				>> $pathToMMS

					if [ "${b}" == "shuffle" ]; then
						echo "use_size_power_of_two = yes;" 	>> $pathToMMS
					else
						echo "use_size_power_of_two = no;"	>> $pathToMMS
					fi

					echo "routing_function = ${i};"			>> $pathToMMS
					echo "vc_buf_size = ${k};"			>> $pathToMMS

					echo "network_file = ${pathToMMSStructure};"	>> $pathToMMS

					echo "load_routing_file = no;"			>> $pathToMMS
					echo "routing_table_file = ${pathToMMSRoutingTable};"	>> $pathToMMS

					echo "${commonContents}" 			>> $pathToMMS

					./booksim ./${pathToMMS}			> ./${logFilesPath}"/MMS.output_scen="${b}"_inj="${j}"_fun="${i}"_buff="${k}".log"
				fi
			done
		done
    	done
done




