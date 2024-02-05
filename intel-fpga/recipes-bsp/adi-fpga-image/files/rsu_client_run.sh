#!/bin/bash

#ADI RSU application: run in target for Remote System Update to the application slot in Qspi flash.

#Syntax:         rsu_client_run.sh [rpd_file_name [rsu_slot]]
#Usage example:  rsu_client_run.sh ./application2.rpd  1

rsu_client_run_v=0.1

echo -e "\nADI RSU Client v${rsu_client_v}.\n"

if [ "$1" ]; then
    echo -e "1st argment is rpd_file_name: $1"
    rpd_file_name=$1
else
    echo -e "Error: The first argument for rpd_file_name is not provided. Exited!\n"
     exit 1
fi

if [ "$2" ]; then
    echo -e "2nd argment is rsu_slot: $2"
    rsu_slot=$2
else
    echo -e "Error: The second argument for rsu_slot number is not provided. Exited!\n"
    exit 1
fi

#must be on(gaining) root access right
if [ $EUID != 0 ]; then
    sudo "$0" "$@"
    exit $?
fi

if [ ! -f "$rpd_file_name" ]; then
    echo "Error: $rpd_file_name is not foud!"
    exit 1
fi

if [[ $rsu_slot =~ ^[0-9]+$ ]];then
    if [[ $((rsu_slot)) > $((2)) ]] ;then 
        echo "Error: rsu_slot $rsu_slot is out of range[0,1,2]!"
        exit 1
    fi
else
    echo "Error: rsu_slot $rsu_slot is non a valid number."
    exit 1
fi

#rsu_client in /usr/bin folder
if [ ! -f /usr/bin/rsu_client ]; then
    echo "Error: rsu_client app not found, aborted!"
    exit 1
fi

#verify segmented JIC programmed to the Qflash already by checking slot count
rsu_slot_verify=$(rsu_client --count)
if [[ $rsu_slot_verify == *"ERROR"* ]]
 then
    echo "Error: Segmented JIC content was not programmed, or it is corrupted in Qflash!"
    exit 1
fi

#verify firstly to see if it is a duplicated/repeated/already update
rsu_pre_verify=$(rsu_client --verify $rpd_file_name --slot $rsu_slot)
if [[ $rsu_pre_verify == *"ERROR"* ]]
 then
    echo "Info: rsu_slot $rsu_slot's content is different from $rpd_file_name. It will be erased and updated..."
else
    echo "Info: rsu_slot $rsu_slot's content is the already same as $rpd_file_name. Done!"
    exit 0
fi

echo -e "Erasing rsu_slot:${rsu_slot}..."
rsu_client --erase $rsu_slot

echo -e "Programming rpd file:${rpd_file_name} to rsu_slot:${rsu_slot}..."
rsu_client --add $rpd_file_name --slot $rsu_slot

echo -e "Verifying rpd file:${rpd_file_name} against rsu_slot:${rsu_slot} just programmed..."
rsu_client --verify $rpd_file_name  --slot $rsu_slot

#check the highest priority 1 to confirm successfully.
rsu_status=$(rsu_client --list $rsu_slot)
echo -e "rsu_status:\n $rsu_status"
regexPri='[0-9]+'
priority_val=255
if [[ $rsu_status =~ (PRIORITY: )($regexPri) ]]	
 then
    priority_val="${BASH_REMATCH[2]}"
    echo -e "Priority is ${priority_val} for rsu_slot:${rsu_slot}!\n"
else
    echo -e "Error: Getting priority value for rsu_slot:${rsu_slot} failed!\n"  
    exit 1
fi

if [[ $((priority_val)) != $((1)) ]] ;then 
    echo "Error: rsu_slot: $rsu_slot is not updated, for priority_val = $priority_val!"
    exit 1
else
    echo -e "Success: rsu_slot: $rsu_slot is updated!\n"
fi

echo -e "Please power cycle(disconnect the power completely from) the board to make the update effective.\n"

exit 0
