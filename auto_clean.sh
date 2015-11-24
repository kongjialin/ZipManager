#!/bin/bash
# NOTE: The '*' at the end of the variables below are needed!
UNZ_OUT="/usr/trans/code/path/unz_out/*"
SUCCESS="/usr/trans/code/path/success/*"
BACKUP="/usr/trans/code/path/backup/*"
FAIL="/usr/trans/code/path/fail/*"
find $UNZ_OUT -maxdepth 0 -type d -cmin +5 -exec rm -rf {} \;
find $SUCCESS -maxdepth 0 -type f -cmin +5 -exec rm -rf {} \;
find $BACKUP -maxdepth 0 -type f -cmin +5 -exec rm -rf {} \;
find $FAIL -maxdepth 1 -type f -ctime +7 | xargs rm -rf
