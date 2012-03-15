import test_mod_redis
import httplib

class TestEmbeddedQuotes(test_mod_redis.TestModRedis):

    def setUp(self):
        super(TestEmbeddedQuotes,self).setUp()
        self.testKey = "%d" % (self.getNextCounterValue())
        

    def tearDown(self):
        self.deleteRedisKey("redis/%s" % (self.testKey))

    def testEmbeddedQuotes(self):

        #Write the expected value into our key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT","/redis/nestedquotetest.json",self.testKey,headers)
        self.assertJsonResponse(self.connection.getresponse(),"status","OK")
            
        # Read the key
        self.connection.request("GET","/redis/%(testKey)s.json" % {"testKey":self.testKey})
        self.assertJsonResponse(self.connection.getresponse(),'string','value with spaces')

