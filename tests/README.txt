To run all the mod_redis tests simply do

	python all_tests.py

You can supply parameters to the script to modify the configuration. To see what is possible do

	python all_tests.py -h
	

The tests assume the Apache httpd server has mod_redis installed with the following config :

<IfModule redis_module>
	RedisAlias ^ping$ PING
	RedisAlias ^nestedquotetest$ "SET %{DATA} \"value with spaces\"" PUT
	RedisAlias ^poster$ "SET ${FORM:key} ${FORM:value}" POST
	RedisAlias ^([^/]+)/next$ "INCR $1" GET
	RedisAlias ^([^/]+)/([^/]+)$ "ZADD $1 %{DATA} $2" PUT
	RedisAlias ^([^/]+)/([^/]+)$ "ZREM $1 $2" DELETE
	RedisAlias ^([^/]+)/count$ "ZCARD $1"
	RedisAlias ^([^/]+)$ "GET $1"
	RedisAlias ^([^/]+)$ "SET $1 %{DATA}" PUT
	RedisAlias ^([^/]+)$ "DEL $1" DELETE
	RedisAlias ^([^/]+)/range/([^/]+)/([^/]+)$ "ZRANGE $1 $2 $3 WITHSCORES"
	RedisAlias ^([^/]+)/revrange$ "ZREVRANGE $1 ${FORM:from} ${FORM:to} WITHSCORES" POST
	RedisAlias ^([^/]+)/rank$ "ZRANK $1 ${QSA:key}"
</IfModule>

The tests try and clean up after themselves, but there may be some data left in redis after a run.