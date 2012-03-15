import test_mod_redis
import httplib
import urllib



class TestPostForm(test_mod_redis.TestModRedis):

    def setUp(self):
        super(TestPostForm,self).setUp()
        self.testKey = "testformpost%d" % (self.getNextCounterValue())

    def tearDown(self):
        self.deleteRedisKey(self.testKey)

    def testPostFormParams(self):

        expectedValue = 'helloworld'        

        # Populate the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        params = urllib.urlencode({'key':self.testKey, 'value': expectedValue})
        self.connection.request("POST","/redis/poster",params,headers)
        self.assertXmlResponse(self.connection.getresponse(),{"status":"OK"})

        # Read the key
        self.connection.request("GET","/redis/%s" % (self.testKey))
        self.assertXmlResponse(self.connection.getresponse(),{"string":expectedValue})

