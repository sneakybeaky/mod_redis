#!/bin/sh

HTTP_SERVER=localhost
HTTP_PORT=80
NUMBER_OF_USERS=50
NUMBER_OF_LOOPS=1000
RAMP_UP_PERIOD=1


jmeter -n -t loadtest.jmx -l loadtest-results.jtl -j loadtest.log -Jhttp_server=${HTTP_SERVER} -Jhttp_port=${HTTP_PORT} -Jnumber_of_users=${NUMBER_OF_USERS} -Jnumber_of_loops=${NUMBER_OF_LOOPS} -Jramp_up_period=${RAMP_UP_PERIOD}