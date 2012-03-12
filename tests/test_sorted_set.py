import urllib
import random
import sys

import test_mod_redis

class TestSortedSet(test_mod_redis.TestModRedis):

    def testAddFifteenItemsXML(self):
    
        randomValue = random.randint(0,sys.maxint)

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(15):
            self.connection.request("PUT","/redis/testset%(randomValue)d/user%(user)d" % {"randomValue":randomValue,"user":i} ,"%s" % i,headers)            

            self.assertXmlResponse(self.connection.getresponse(),"integer","1")

        
        self.connection.request("GET","/redis/testset%(randomValue)d/count" % {"randomValue":randomValue})
        self.assertXmlResponse(self.connection.getresponse(),"integer","15")

    def testAddFifteenItemsJson(self):
    
        randomValue = random.randint(0,sys.maxint)

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(15):
            self.connection.request("PUT","/redis/testset%(randomValue)d/user%(user)d.json" % {"randomValue":randomValue,"user":i} ,"%s" % i,headers)            

            self.assertJsonResponse(self.connection.getresponse(),"integer","1")

        
        self.connection.request("GET","/redis/testset%(randomValue)d/count.json" % {"randomValue":randomValue})
        self.assertJsonResponse(self.connection.getresponse(),"integer","15")
