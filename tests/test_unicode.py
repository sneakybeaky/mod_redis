# -*- coding: utf-8 -*-

import urllib
import test_mod_redis
import httplib
import xml.etree.ElementTree as ET
import json

class TestUnicode(test_mod_redis.TestModRedis):

    def testUnicodeGetAndPutXml(self):
        testKey = "testunicodekey%d" % (self.getNextCounterValue())
        expectedValue = u'abcdéМиф'.encode('utf-8')        

        # Populate the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT","/redis/%(testKey)s" % {"testKey":testKey},expectedValue,headers)
        self.assertXmlResponse(self.connection.getresponse(),{"status":"OK"})

        # Read the key
        self.connection.request("GET","/redis/%(testKey)s" % {"testKey":testKey})
        response = self.connection.getresponse()
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)
        document = ET.fromstring(response.read())
        statusText = document.findtext('string').encode('utf-8')
        self.assertNotEqual(None,statusText,"Unable to find a status in the response")
        self.assertEqual(expectedValue,statusText)


        # Delete the key
        self.connection.request("DELETE","/redis/%(testKey)s" % {"testKey":testKey})
        self.assertXmlResponse(self.connection.getresponse(),{"integer":"1"})


    def testUnicodeGetAndPutJson(self):
        testKey = "testunicodekey%d" % (self.getNextCounterValue())
        expectedValue = u'abcdéМиф'.encode('utf-8')        

        # Populate the key
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        self.connection.request("PUT","/redis/%(testKey)s.json" % {"testKey":testKey},expectedValue,headers)
        self.assertJsonResponse(self.connection.getresponse(),"status","OK")

        # Read the key
        self.connection.request("GET","/redis/%(testKey)s.json" % {"testKey":testKey})
        response = self.connection.getresponse()
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)
        data = json.loads(response.read())

        self.assertTrue('string' in data,"Expected field 'string' not in response")
        self.assertEqual(expectedValue,data['string'].encode('utf-8'))

        # Delete the key
        self.connection.request("DELETE","/redis/%(testKey)s.json" % {"testKey":testKey})
        self.assertJsonResponse(self.connection.getresponse(),"integer","1")

