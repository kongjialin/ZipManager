crontab -l > tmpcron
echo "*/5 * * * * /absolute/path/to/auto_clean.sh" >> tmpcron
echo "*/2 * * * * /absolute/path/to/monitor.sh" >> tmpcron
crontab tmpcron
rm tmpcron
