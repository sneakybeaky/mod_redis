import unittest 
import httplib
import json
import xml.etree.ElementTree as ET


class TestModRedis(unittest.TestCase):

    """

    A test class for the mod_redis apache module.

    """

    def setUp(self):

        """

        set up data used in the tests.

        setUp is called before each test function execution.

        """
        self.connection = httplib.HTTPConnection("localhost:8081")

    def tearDown(self):
        self.connection.close()

    def responseToXml(self,response):
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)
        document = ET.fromstring(response.read())
        return document        

    def assertXmlResponse(self,response,elementName,expected):
        document = self.responseToXml(response)

        statusText = document.findtext(elementName)
        self.assertNotEqual(None,statusText,"Unable to find a status in the response")
        self.assertEqual(expected,statusText,"Status should have been '%s', not '%s'" % (expected,statusText) )
        
    def assertJsonResponse(self,response,elementName,expected):
        self.assertEqual(response.status,httplib.OK,"Got non OK response - %s" % response.status)

        response = json.loads(response.read())
        self.assertTrue(elementName in response,"Expected field '%s' not in response" % elementName)
        self.assertEqual(expected,response[elementName],"Result should have been %s, not %s" % (expected,response[elementName]))
    


