import urllib
import random
import sys

import test_mod_redis
import xml.etree.ElementTree as ET

class TestPutGetDelete(test_mod_redis.TestModRedis):

    def testCRUDXml(self):
        testKey = "testcrudkey%d" % (self.getNextCounterValue())
        expectedValue = "expectedValue"

        # Make sure key is empty
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponseIsNil(self.connection.getresponse())

        # Populate the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT","/redis/%(testKey)s" % {"testKey":testKey},expectedValue,headers)
        self.assertXmlResponse(self.connection.getresponse(),"status","OK")

        # Read the key
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponse(self.connection.getresponse(),"string","expectedValue")

        # Delete the key
        self.connection.request("DELETE","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponse(self.connection.getresponse(),"integer","1")

        # Make sure key is empty
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponseIsNil(self.connection.getresponse())
        

        



