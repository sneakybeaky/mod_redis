import test_mod_redis
import httplib

class TestQueryString(test_mod_redis.TestModRedis):

    def setUp(self):
        super(TestQueryString,self).setUp()
        self.testSet = "testquerystring%d" % (self.getNextCounterValue())
        self.createTestSet()

    def createTestSet(self):
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(5):
            self.connection.request("PUT","/redis/%(setName)s/user%(user)d" % {"setName":self.testSet,"user":i} ,"%s" % i,headers)            
            self.assertXmlResponse(self.connection.getresponse(),{"integer":"1"})
        

    def tearDown(self):
        self.deleteRedisKey(self.testSet)

    def testQueryStringXml(self):

        # Read the key
        self.connection.request("GET","/redis/%(setName)s/rank?key=user2" % {"setName":self.testSet})
        self.assertXmlResponse(self.connection.getresponse(),{'integer':'2'})

    def testQueryStringJson(self):

        # Read the key
        self.connection.request("GET","/redis/%(setName)s/rank.json?key=user2" % {"setName":self.testSet})
        self.assertJsonResponse(self.connection.getresponse(),'integer','2')

