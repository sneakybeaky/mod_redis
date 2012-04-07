#!/bin/bash
#
# ./test.sh [RedisIPAddress] [RedisPortNumber]
#


setRedisAlias() {	
	sedcwd=`pwd | sed "s/\\\//\\\\\\\\\//g"`

	#modify the portnumber, loglevel, error and access log locations
	cat $httpd_root/$config | 
		sed 's/Listen [0-9]*/Listen 8081/' | 
		sed 's/LogLevel .*/LogLevel debug/' | 
		sed "s/ErrorLog.*/ErrorLog $sedcwd\/$testingdir\/error.log/" | 
		sed "s/CustomLog .* /CustomLog $sedcwd\/$testingdir\/access.log /" > "$configfile"
	
	echo  >> "$configfile"
	echo PidFile `pwd`"/$testingdir/httpd.pid" >> "$configfile"
	
	if [[ $httpd_ver == 2.2.* ]]; then
		echo LockFile `pwd`"/$testingdir/accept.lock" >> "$configfile"
	fi
	
	#add the redis module handler and configuration
	echo "<Location ~ /redis/*>" >> "$configfile"
	echo "SetHandler redis" >> "$configfile"
	echo "</Location>" >> "$configfile"
	echo "" >> "$configfile"
	echo "<IfModule redis_module>" >> "$configfile"
	echo "RedisIPAddress $redisipaddress"
	echo "RedisPort $redisportnumber"
	declare -a arg1=("${!1}")
	for cmd in "${arg1[@]}"
	do
		echo "RedisAlias $cmd"  >> "$configfile"
	done
	echo "</IfModule>" >> "$configfile"
	echo "" >> "$configfile"
}

#cleanup after previous test
rm testing/error.log
rm testing/access.log

#used as a switch to stop after the first error
lasterr=0

#setup some defaults
mkdir -p testing
testingdir=testing
configfile="$testingdir/httpd.conf"
testset="testset$RANDOM"
testkey="testkey$RANDOM"
redisipaddress=$1
redisportnumber=$2

if [ -z $redisipaddress ]; then
	redisipaddress=127.0.0.1
fi

if [ -z $redisportnumber ]; then
	redisportnumber=6379
fi

#determine the location of apache and it's config file
config=`apachectl -V 2> /dev/null | grep SERVER_CONFIG_FILE | sed 's/.*\"\(.*\)\"/\1/'`
httpd_root=`apachectl -V 2> /dev/null | grep HTTPD_ROOT | sed 's/.*\"\(.*\)\"/\1/'`
httpd_ver=`apachectl -V 2> /dev/null | grep 'Server version:' | sed -E 's/.*Apache\/([0-9.]+).*/\1/'`

#ensure that mod_redis has been installed
redis_module=`cat $httpd_root/$config | grep -E '^LoadModule redis_module' | wc -l`
if [ $redis_module != 1 ]; then 
	echo '**FAIL**'
	echo "******** LoadModule statement is missing from '$httpd_root/$config'"
	echo "******** Please compile and install the mod_redis before executing the tests"
	exit
fi

#ensure REDIS is running locally
echo "Testing connection to REDIS on $redisipaddress:$redisportnumber..."
python scripts/ping_redis.py -s ${redisipaddress} -p ${redisportnumber}
OUT=$?

if [ $OUT -eq 1 ]; then 
	echo '**FAIL**'
	echo "******** Unable to determine if REDIS is running"
	echo "******** Please start REDIS server and re-run tests"
	exit
fi

#define the RedisAlias commands that should be added in a NEW section in
#the httpd.conf file that the script has found. The conf file is copied to
#a new location before being modified so no changes are made to the source
#file
echo "Configuring httpd and restarting on port 8081..."
aliases=( '^ping$ PING'
		  '^nestedquotetest$ "SET %{DATA} \"value with spaces\"" PUT'
     	  '^poster$ "SET ${FORM:key} ${FORM:value}" POST'
          '^([^/]+)/next$ "INCR $1" GET'
     	  '^([^/]+)/([^/]+)$ "ZADD $1 %{DATA} $2" PUT'
     	  '^([^/]+)/([^/]+)$ "ZREM $1 $2" DELETE'
     	  '^([^/]+)/count$ "ZCARD $1"'
     	  '^([^/]+)$ "GET $1"'
     	  '^([^/]+)$ "SET $1 %{DATA}" PUT'
     	  '^([^/]+)$ "DEL $1" DELETE'
     	  '^([^/]+)/range/([^/]+)/([^/]+)$ "ZRANGE $1 $2 $3 WITHSCORES"'
     	  '^([^/]+)/revrange$ "ZREVRANGE $1 ${FORM:from} ${FORM:to} WITHSCORES" POST'
     	  '^([^/]+)/rank$ "ZRANK $1 ${QSA:key}"'
     	  )
setRedisAlias aliases[@]
apachectl -f `pwd`"/$configfile" -k start	

#we have a new instance of Apache running, start the tests
echo "Running tests..."

(cd tests ;python all_tests.py)

apachectl -f `pwd`"/$configfile" -k stop 2> /dev/null


