import unittest 
import httplib
import json
import xml.etree.ElementTree as ET


class TestModRedis(unittest.TestCase):

    servername = "localhost"
    port = "80"

    """

    A test class for the mod_redis apache module.

    """

    def setUp(self):

        """

        set up data used in the tests.

        setUp is called before each test function execution.

        """
        self.connection = httplib.HTTPConnection("%s:%s" % (self.servername,self.port) )

    def tearDown(self):
        self.connection.close()

    def responseToJson(self,response):
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)
        response = json.loads(response.read())
        return response

    def responseToXml(self,response):
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)
        document = ET.fromstring(response.read())
        return document

    def assertJsonResponseIsNil(self,response):
        self.assertJsonResponse(response,'nil','nil')

    def assertXmlResponseIsNil(self,response):
        document = self.responseToXml(response)
        response = document.findtext('nil')
        self.assertNotEqual(None,response,"Unable to find a status in the response")         
        self.assertTrue(len(response)==0,"nil element should be an empty string");

    def assertXmlResponse(self,response,expectedNamesAndValues):
        document = self.responseToXml(response)

        for (name,value) in expectedNamesAndValues.items():
            statusText = document.findtext(name)
            self.assertNotEqual(None,statusText,"Unable to find a field called '%s' in the response" % name)
            self.assertEqual(value,statusText,"Status should have been '%s', not '%s'" % (value,statusText) )


    def assertJsonResponse(self,response,elementName,expected):
        data = self.responseToJson(response)
        self.assertTrue(elementName in data,"Expected field '%s' not in response" % elementName)
        self.assertEqual(expected,data[elementName],"Result should have been %s, not %s" % (expected,data[elementName]))

    def getNextCounterValue(self):
        self.connection.request("GET","/redis/testcounter/next.json")

        data = self.responseToJson(self.connection.getresponse()) 
        self.assertTrue('integer' in data,"Expected field 'integer' not in response")
        return int(data['integer'])

    def deleteRedisKey(self,path):
        self.connection.request("DELETE",path)
        self.connection.close()

    


