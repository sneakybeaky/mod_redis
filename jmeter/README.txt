In this folder is the load test script for Apache JMeter. Install following the instructions at http://jmeter.apache.org/ and then add the plugins hosted at http://code.google.com/p/jmeter-plugins/.

Add the following configuration to Apache HTTPD :


	<Location ~ /redis/*>
		SetHandler redis
	</Location>

	<IfModule redis_module>
		RedisAlias ^ping$ "PING"
		RedisAlias ^([^/]+)$ "GET $1"
		RedisAlias ^([^/]+)$ "SET $1 %{DATA}" PUT
		RedisAlias ^([^/]+)$ "DEL $1" DELETE
		RedisAlias ^zadd$ "ZADD ${FORM:key} ${FORM:score} ${FORM:member}" POST
		RedisAlias ^zscore/([^/]+)$ "ZSCORE $1 ${QSA:member}"
		RedisAlias ^zrank/([^/]+)$ "ZRANK $1 ${QSA:member}"

	</IfModule>

Load the load test script into JMeter and you're ready to go.

Alternatively, you can use the loadtest.sh shell script to run jmeter in non-gui mode, as long as jmeter is in your path. 