```
setenforce 0
mount -o remount,hidepid=0 /proc
echo 0 > /proc/sys/kernel/perf_event_paranoid
```