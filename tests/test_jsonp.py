import test_mod_redis
import httplib
import json
import re

class TestJsonp(test_mod_redis.TestModRedis):

    def setUp(self):
        super(TestJsonp,self).setUp()
        self.testKey = "/redis/testjsonpkey%d" % (self.getNextCounterValue())

    def tearDown(self):
        self.deleteRedisKey(self.testKey)

    def testJsonp(self):

        expectedValue = 'blah'        

        # Populate the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT",self.testKey,expectedValue,headers)
        self.assertXmlResponse(self.connection.getresponse(),"status","OK")

        # Read the key
        self.connection.request("GET","%(testKey)s.jsonp?callback=foo" % {"testKey":self.testKey})
        response = self.connection.getresponse()
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)

        body = response.read()
        m=re.match("foo\((?P<json>\{.*\})\)",body)

        self.assertTrue(m,"Response '%s' not valid JSONP" % body)

        data=json.loads(m.group('json'))
        self.assertEqual(expectedValue,data['string'],"Result should have been %s, not %s" % (expectedValue,data['string']))
        
