taskkill /F /IM beanstalkd.exe
beanstalkd.exe -l 0.0.0.0 -p 11300 -b c:/ > ./error.txt