import urllib
import random
import sys

import test_mod_redis

class TestSortedSet(test_mod_redis.TestModRedis):

    def testPingAsXML(self):
        self.connection.request("GET","/redis/ping")

        self.assertXmlResponse(self.connection.getresponse(),{"status":"PONG"})

            	
    def testPingAsJSON(self):
        self.connection.request("GET","/redis/ping.json")
        self.assertJsonResponse(self.connection.getresponse(),"status","PONG")
