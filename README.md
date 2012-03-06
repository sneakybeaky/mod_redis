mod_redis
=========

This Apache module uses a rule-based engine (based on regular expression parser) to map URLs to 
REDIS commands on the fly. It supports an unlimited number of rules and can match on the full URL 
and the request method (GET, POST, PUT or DELETE) to provide a very flexible option for defining 
a RESTful interface to REDIS.

By default the module returns an XML representation of the resulting REDIS response. For example
configuring a RedisAlias to support the PING command (`RedisAlias ^ping$ "PING"`), the response 
would be:

    <?xml version="1.0" encoding="UTF-8"?>
    <response>
    <status>PONG</status></response>

This behavior can be altered by appending .json to the url, for example:

    { "status" : "PONG" }

Alternatively, appending .jsonp and adding a query parameter ‘callback’ respond with:

    callback({ "status" : "PONG" });


Installation
------------

1. Download Hiredis, a minimalistic C client library for the Redis database

    `curl https://github.com/antirez/hiredis/zipball/master -L -o hiredis.zip`

2. Unzip the downloaded file

    `unzip hiredis.zip`

3. Extract the contents of the zip file (hiredis.zip) and define the base directory. In the 
example below, the last part of the embedded directory has been obscured so please remember to
use the directory created when unzipping the file

    `HIREDIS_HOME=`pwd`/antirez-hiredis-XXXXXXX`

4. Compile the hiredis library

    `pushd $HIREDIS_HOME ; make ; popd`

5. Compile mod_redis

    `apxs -c -I $HIREDIS_HOME $HIREDIS_HOME/libhiredis.a mod_redis.c`

6. Install the module into the currently installed Apache instance

    `sudo apxs -i -a mod_redis.la`

7. Run the tests. This will create a custom httpd.conf and start a new Apache instance on port 
8081 for the purposes of testing. Temp files for this test instance can be found in the ./testing 
directory. By default, the test script will attempt to connect to local REDIS server on the 
standard REDIS port (6379), this can be modified with command line parameters

    `./test.sh [RedisIPAddress] [RedisPort]`


RedisAlias Directive
--------------------

**Description**: Defines a rule to match a URL to a REDIS command  
**Syntax**:      RedisAlias TestString RedisCommandPattern [Method]  

Rules are matched based on the order in which they are defined. Depending on the expression, care
should be taken not to define less specific expressions before more specific ones. Consider a
configuraton defining two directives in the following order, in the following example the second
expression would never be matched because the first expression would evaluate successfully first.

    RedisAlias ^([^/]+)$ "GET $1"
    RedisAlias ^ping$    "PONG"
    
**TestString**   

A regular expression defining the elements in the URL to match. This excludes the
protocol, hostname, portnumber, module location and query string. Consider an local installation
of Apache with mod_redis handler configured for the location `/redis/*`, in the following 
contrived example, the element of the URL that can be matched against is in square brackets

    http://localhost/redis/[Matchable].json?callback=alert
    
Round brackets can used used to create a backreference for part of the matching string which can be 
later referenced in the RedisCommandPattern. This is essential to extract elements of the URL for 
use in the REDIS command itself.

    ^([^/]+)/count$

This example matches any characters appearing before the first matching forward slash, followed by 
a forward slash, the word count and then the end of the string. The round brackets create a 
backreference, capturing the characters before the forward slash so that it can be used in the 
RedisCommandPattern, i.e. `"ZCARD $1"`. Using this example the URL `<host>/<handler>/myset/count` 
would result in the REDIS command `ZCARD myset`. 

**RedisCommandsPattern**  

The pattern used to create a REDIS command to be executed. The format of 
this command must confirm to the REDIS command syntax. Additionally the text can contain the 
following:

* TestString backreferences: These backreferences take the form $N (0<=N<=9) which provide to the 
grouped parts (in parentheses) of the TestString.

* Request variables: These are variables from the current request being processed

    %{DATA}         The body content of a PUT or POST request  
    ${FORM:field}   The value of a form field taken from PUT or POST request. The request must have
               include the Content-Type header as application/x-www-form-urlencoded  
    ${QSA:field}   The value of a query string argument taken from the request URL

**Method**  

The HTTP method of the given request, GET, PUT, POST or DELETE. When a method is not
specified, GET is assumed as the default. The Method parameter can be used to increase the 
specificity of the directive. In the following example, the **TestString** is identical, however 
the **Method** determines which directive would successfully match.

    RedisAlias ^([^/]+)$ "GET $1"
    RedisAlias ^([^/]+)$ "SET $1 %{DATA}" PUT
    RedisAlias ^([^/]+)$ "DEL $1" DELETE
    

**Examples**  

To define alias to add, remove and count the number of entries in a sorted set, you would add the 
following directives:

    RedisAlias ^([^/]+)/([^/]+)$ "ZADD $1 %{DATA} $2" PUT  
    RedisAlias ^([^/]+)/([^/]+)$ "ZREM $1 $2" DELETE  
    RedisAlias ^([^/]+)/count$ "ZCARD $1"  
    
To define an alias to retrive a list of members from a sorted set, you could add the following
directive:

    RedisAlias ^([^/]+)/range/([^/]+)/([^/]+)$ "ZRANGE $1 $2 $3 WITHSCORES"

To define an alias to create a new member to a sorted set from a POSTed form you could add the 
following directive:

    RedisAlias ^zadd$ "ZADD ${FORM:key} ${FORM:score} ${FORM:member}" POST
    
To define an alias to determine the rank of a key in a sorted set based on a query parameter, you
could add the following directive:

    RedisAlias ^([^/]+)/rank$ "ZRANK $1 ${QSA:key}"


RedisIPAddress Directive
------------------------

**Description**: Defines the IP address of the REDIS instance to connect to  
**Syntax**:      RedisIPAddress IPAddress  

The default IPAddress is 127.0.0.1


RedisPort Directive
-------------------

**Description**: Defines the port number of the REDIS instance to connect to  
**Syntax**:      RedisPort PortNumber  

The default PortNumber is 6379


Example httpd.conf
------------------

    <Location ~ /redis/*>
    SetHandler redis
    </Location>

    <IfModule redis_module>
    RedisIPAddress 127.0.0.1
    RedisPort 6379
    RedisAlias ^ping$ "PING"
    </IfModule>