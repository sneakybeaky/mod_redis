import urllib
import random
import sys

import test_mod_redis
import xml.etree.ElementTree as ET

class TestPutGetDelete(test_mod_redis.TestModRedis):

    def testCRUDXml(self):
        testKey = "testcrudkey%d" % (self.getNextCounterValue())

        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponseIsNil(self.connection.getresponse())

        



