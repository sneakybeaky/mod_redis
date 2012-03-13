import urllib
import test_mod_redis


class TestPutGetDelete(test_mod_redis.TestModRedis):

    def testCRUDXml(self):
        testKey = "testcrudkey%d" % (self.getNextCounterValue())
        expectedValue = "expectedValue"        
        expectedValue2 = "expectedValue2"

        # Make sure key is empty
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponseIsNil(self.connection.getresponse())

        # Populate the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT","/redis/%(testKey)s" % {"testKey":testKey},expectedValue,headers)
        self.assertXmlResponse(self.connection.getresponse(),"status","OK")

        # Read the key
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponse(self.connection.getresponse(),"string",expectedValue)

        # Update the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT","/redis/%(testKey)s" % {"testKey":testKey},expectedValue2,headers)
        self.assertXmlResponse(self.connection.getresponse(),"status","OK")

        # Read the key
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponse(self.connection.getresponse(),"string",expectedValue2)

        # Delete the key
        self.connection.request("DELETE","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponse(self.connection.getresponse(),"integer","1")

        # Make sure key is empty
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponseIsNil(self.connection.getresponse())
        

        



