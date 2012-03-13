#!/bin/bash
#
# ./test.sh [RedisIPAddress] [RedisPortNumber]
#

function testGetXML {
	testXML 'GET' "$1" "$2" xpath "$3"
}

function testDelXML {
	testXML 'DELETE' "$1" "$2" xpath "$3"
}

function testPutXML {
	testXML 'PUT' "$1" "$3" xpath "$4" "$2"
}

function testPostXML {
	testXML 'POST' "$1" "$3" xpath "$4" "$2"
}

function testGet {
	testXML 'GET' "$1" "$2" grep
}

function testXML {
	
	if [ $lasterr -ne "0" ]; then
		return
	fi
	
	if [ "$1" == "POST" ]; then
		curlparms="--data \"$6\" "
	elif [ "$1" == "PUT" ]; then
		curlparms="--data \"$6\" --request PUT "
	elif [ "$1" == "DELETE" ]; then
		curlparms="--request DELETE "
	else
		curlparms=""
	fi
	
	curlcmd="curl -s $curlparms http://localhost:8081/redis/$2"
	eval $curlcmd  > "$testingdir/testout"
	
	xpathcmd="xpath -e"
	
	if [ "$4" == "xpath" ]; then
		result=`eval "cat $testingdir/testout | $xpathcmd $5 2> /dev/null"`
	else 
		result=`eval "cat $testingdir/testout | grep -e '$3' 2> /dev/null"`
		if [ "$result" ]; then
			result=$3
		fi
	fi
	
	if [ "$result" != "$3" ]; then
		echo '**FAIL**'
		echo "******** $4 expecting '$3' but was '$result'"
		echo "******** command: $curlcmd"
		echo "******** response:"
		cat "$testingdir/testout"
		echo
		lasterr=1
	else
		echo 'PASS'
	fi
}

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
redis_ping=`redis-cli -h ${redisipaddress} -p ${redisportnumber} PING | grep 'PONG'`

if [ -z $redis_ping ]; then 
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
          'RedisAlias ^([^/]+)/next$ "INCR $1" GET'
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
testGetXML 'ping' 'PONG' '/response/status/text\(\)'

#testing Z... commands, populate a sorted set 'test' with 15 items 
for i in {1..15}
do
	testPutXML "$testset/user$i" "$i" '1' 'count\(/response/integer\)'
done
testGetXML "$testset/count" '15' '/response/integer/text\(\)'

#testing ZRANGE
testGetXML "$testset/range/1/2" '4' 'count\(/response/array/string\)'
testGetXML "$testset/range/1/2" 'user2' '/response/array/string[1]/text\(\)'
testGetXML "$testset/range/1/2" '2' '/response/array/string[2]/text\(\)'
testGetXML "$testset/range/1/2" 'user3' '/response/array/string[3]/text\(\)'
testGetXML "$testset/range/1/2" '3' '/response/array/string[4]/text\(\)'
testGetXML "$testset/range/0/14" '30' 'count\(/response/array/string\)'

#testing ZREM
testDelXML "$testset/user15" '1' 'count\(/response/integer\)'
testGetXML "$testset/count" '14' '/response/integer/text\(\)'

#testing GET, PUT and DELETE
testPutXML "$testkey" 'bar' 'OK' '/response/status/text\(\)'
testGetXML "$testkey" 'bar' '/response/string/text\(\)'
testDelXML "$testkey" '1' '/response/integer/text\(\)'
testGetXML "$testkey" '' '/response/string/text\(\)'

#testing GET and PUT with Unicode characters
testPutXML "$testkey" 'Миф с' 'OK' '/response/status/text\(\)'
testGetXML "$testkey" 'Миф с' '/response/string/text\(\)'

#testing JSON response
testGet "$testkey.json" '{ "string" : "Миф с" }'
testGet "$testkey.json" 'Миф с'
testGet "$testset/range/1/1.json" '{ "array" : \[ { "string" : "user2" },{ "string" : "2" } \] }'

#testing JSONP response
testGet "$testkey.jsonp?callback=foo" 'foo({ "string" : "Миф с" });'

#testing ZREVRANGE with form parameters
testPostXML "$testset/revrange" "from=1&to=2" '4' 'count\(/response/array/string\)'

#testing POSTing form parameters
testPostXML "poster" "key=$testkey&value=hello%20world" 'OK' '/response/status/text\(\)'
testGetXML "$testkey" "hello world" '/response/string/text\(\)'

#testing query string arguments
testGetXML "$testset/rank?key=user2" "1" '/response/integer/text\(\)'
testGet "$testset/rank.jsonp?callback=foo\&key=user2" 'foo({ "integer" : "1" });'

#testing embedded quotes
testPutXML "nestedquotetest" "$testkey" 'OK' '/response/status/text\(\)'
testGetXML "$testkey" 'value with spaces' '/response/string/text\(\)'

#Clean up and stop our temporary apache instance 
echo "Cleaning up and stopping test httpd instance (8081)..."
for i in {1..15}
do
	curl -s --request DELETE "http://localhost/redis/$testset/user$i" > /dev/null	
done
curl -s --request DELETE "http://localhost/redis/$testkey" > /dev/null	
apachectl -f `pwd`"/$configfile" -k stop 2> /dev/null


