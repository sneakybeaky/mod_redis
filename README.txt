Apache Module mod_redis

Summary
=======

This module uses a rule-based engine (based on regular expression parser) to map URLs to REDIS 
commands on the fly. It supports an unlimited number of rules and can match on the full URL and
the request method (GET, POST, PUT or DELETE) to provide a very flexible option for defining a 
RESTful interface to redis.

By default the module returns an XML representation of the resulting REDIS response. For example
configuring a RedisAlias to support the PING command (RedisAlias ^ping$ "PING"), the response 
would be:-

<?xml version="1.0" encoding="UTF-8"?>
<response>
<status>PONG</status></response>

This behavior can be altered by appending .json to the url, for example:

{ "status" : "PONG" }

Alternatively, appending .jsonp and adding a query parameter ‘callback’ respond with:

callback({ "status" : "PONG" });


Installation
============

1. Download Hiredis, a minimalistic C client library for the Redis database

curl https://github.com/antirez/hiredis/zipball/master -L -o hiredis.zip

2. Unzip the downloaded file

unzip hiredis.zip

3. Extract the contents of the zip file (hiredis.zip) and define the base directory. In the 
example below, the last part of the embedded directory has been obscured so please remember to
use the directory created when unzipping the file

HIREDIS_HOME=`pwd`/antirez-hiredis-XXXXXXX

Compile the hiredis library

pushd $HIREDIS_HOME ; make ; popd

4. Compile mod_redis

apxs -c -I $HIREDIS_HOME $HIREDIS_HOME/libhiredis.a mod_redis.c

5. Install the module into the currently installed Apache instance

sudo apxs -i -a mod_redis.la

6. Run the tests. This will create a custom httpd.conf and start a new Apache instance on port 
8081 for the purposes of testing. Temp files for this test instance can be found in the ./testing 
directory. By default, the test script will attempt to connect to local REDIS server on the 
standard REDIS port (6379), this can be modified with command line parameters

./test.sh [RedisIPAddress] [RedisPort]


RedisAlias Directive
====================

Description: Defines a rule to match a URL to a REDIS command
Syntax:      RedisAlias TestString RedisCommandPattern [Method]

TestString is a regular expression string defining the rule to match. Rules are matched based on
the order in which they are defined. Expression groups (captured in parentheses) can be referenced
in the RedisCommandPattern.

RedisCommandsPattern is the pattern used to create a REDIS command to be executed. The format of 
this command must confirm to the REDIS command syntax. Additionally the text can contain the 
following:

* TestString backreferences: These backreferences take the form $N (0<=N<=9) which provide to the 
grouped parts (in parentheses) of the TestString.

* Request variables: These are variables from the current request being processed

{DATA}         The body content of a PUT or POST request
{FORM:field}   The value of a form field taken from PUT or POST request. The request must have
               include the Content-Type header as application/x-www-form-urlencoded

* Method: The HTTP method of the given request, GET, PUT, POST or DELETE. When a method is not
specified, GET is assumed as the default

Examples

To define aliases which will map a single URL path element to the REDIS commands for KEY 
manipulation (specifically GET, SET and DELete) you would add the following directives:

RedisAlias ^([^/]+)$ "GET $1"
RedisAlias ^([^/]+)$ "SET $1 ${DATA}" PUT
RedisAlias ^([^/]+)$ "DEL $1" DELETE

To define an alias to create a new member to a sorted set from a POSTed form you could add the 
following directive:

RedisAlias ^zadd$ "ZADD ${FORM:key} ${FORM:score} ${FORM:member}" POST


RedisIPAddress Directive
========================

Description: Defines the IP address of the REDIS instance to connect to
Syntax:      RedisIPAddress IPAddress

The default IPAddress is 127.0.0.1


RedisPort Directive
===================

Description: Defines the port number of the REDIS instance to connect to
Syntax:      RedisPort PortNumber

The default PortNumber is 6379


Example httpd.conf
==================

<Location ~ /redis/*>
SetHandler redis
</Location>

<IfModule redis_module>
RedisAlias ^ping$ "PING"
</IfModule>

