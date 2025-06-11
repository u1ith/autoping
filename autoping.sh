#1/bin/bash

TIME=60
IPS=(
    8.8.8.8
    20.20.20.20
)
LOGFILE="$HOME/pinglog"

while true
do for ip in "${IPS[@]}"
    do if ! ping -c1 -s1 "$ip" > /dev/null
        then echo "[$(date)] $ip: failed" >> "$LOGFILE"
            notify-send "Ping Failed" "For $ip"
        fi
    done
    sleep "$TIME"
done
