#!/bin/bash
# Daemon for monitoring the status of programs 'Convert_only' and 'Upload_only'.
# This shell script is add to crontab and is executed each minute to ensure the
# programs are keep running.
UPLOAD_CONFIG_NUM=10
convert_num()
{
    num=`ps -ef | grep Convert_only | grep -v grep | wc -l`
    return $num
}

upload_num()
{
    num=`ps -ef | grep "Upload_only $1" | grep -v grep | wc -l`
  	return $num
}

convert_num
number=$?
if [ $number -eq 0 ]
then
    nohup /absolute/path/to/Convert_only > /dev/null 2>&1 &
fi

i=1
while [ $i -le $UPLOAD_CONFIG_NUM ]
do
    upload_num $i
    number=$?
    if [ $number -eq 0 ]
    then
        nohup /absolute/path/to/Upload_only $i > /dev/null 2>&1 &
    fi
    i=`expr $i + 1`
done

