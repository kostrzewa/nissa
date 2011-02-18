# @ shell = /bin/bash
# @ job_name = job
# @ error  = $(job_name).$(jobid).err
# @ output = $(job_name).$(jobid).out
# @ environment = COPY_ALL
# @ notification = always
# @ notify_user = Francesco.Sanfilippo@roma1.infn.it
# @ wall_clock_limit = 01:00:00
# @ job_type         = BLUEGENE
# @ bg_size          = 512
# @ bg_connection    = TORUS
# @ queue
#

#!/bin/bash
set echo

echo
echo "====> JOB STARTED AT : " $(date)
echo

#setup everything
source ~/nissa_conf.sh

############ - 1 - Paths and names #########

#set base path for the analysis
base_analysis=[path_to_the_analysis]

#file with the list of configurations
conf_list_file=[conf_list_file]

#name of the analysis (eg: "2pts", or "Bk")
analysis_name=[name]

#Path to the configurations (which have to have named as 'conf.xxxx'
source_confs=[path_to_confs]

########### - 2 - Physical information ########

#volume
L=[L]
T=[T]

#noise type residual for the inverter and maximal number of iterations
stopping_residue=[resd]
num_max_iter=[num]

#kappa and masses
kappa=[kappac]
mu=([mu])

#separation between source and sink
tseparation=[t]

############## - 3 Setups ################

#now cd to the analysis
cd $base_analysis

#search the gauge configuration to analyse, and setup it
#exit if everything calculated
source $base_scripts/select_new_conf.sh

#if not present, generate the source position
if [ ! -f $base_conf/source_pos ]
then
    (
	echo -n $(($(bash $base_scripts/casual.sh)%$T))
	for((i=1;i<4;i++));do echo -n $(($(bash $base_scripts/casual.sh)%$L));done
    ) > $base_conf/source_pos
fi

################# main script #################

cd $base_conf

#prepare the input file

echo "\
L $L
T $T
NGaugeConf 1
GaugeConfPath Conf proton
Kappa $kappa
SourcePosition "$(cat source_pos)"
Mass $mu
Residue $stopping_residue
NiterMax $num_max_iter
TSeparation $tseparation
" > input

#shell the program
$MPI_TM_PREF $base_nissa/Appretto/projects/nucleons/nucleons input

######################### finalization ###########################

#prohibite future re-run of the same analysis on this conf
touch $base_conf/analysis_"$analysis_name"_completed

cd $base_analysis

llsubmit analysis.sh

echo
echo "====> JOB ENDED AT : " $(date)
echo

