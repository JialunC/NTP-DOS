# NTP-DOS

Note: Despite spoofed the NTP requests are reaching the NTP server, somehow I am not seeing
responses being sent back.

Maybe should figure out where this is being blocked by: https://serverfault.com/questions/816393/disabling-rp-filter-on-one-interface
## Start NTP server manually in ntp container
```
apt-get update && apt-get install -y ntp
```
Follow the prompt to set up the server

In the host machine, run the following command to update ntp.conf
```
docker cp ./ntp.conf ntp:/etc
```

And restart the ntp server by:
```
kill $(lsof -t -i:123)
```

The run:
```
ntpd start
```

## Debug from Victim
```
ntpdate -q 172.18.0.3
```

## Debug from NTP server
```
tcpdump udp port 123 -vv -X
```

## Run attack from Attack Server
```
docker exec -ti attacker sh
```

```
./ntpreflec 172.18.0.2 172.18.0.3 5 20
```