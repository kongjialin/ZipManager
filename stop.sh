ps -ef | grep Convert_only | grep -v grep | awk '{print $2}' | xargs kill
ps -ef | grep Upload_only | grep -v grep | awk '{print $2}' | xargs kill
