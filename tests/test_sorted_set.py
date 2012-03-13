import urllib

import test_mod_redis
import xml.etree.ElementTree as ET

class TestSortedSet(test_mod_redis.TestModRedis):

    def testSortedSetOperationsXml(self):

        setName = "testset%d" % (self.getNextCounterValue())

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(15):
            self.connection.request("PUT","/redis/%(setName)s/user%(user)d" % {"setName":setName,"user":i} ,"%s" % i,headers)            

            self.assertXmlResponse(self.connection.getresponse(),"integer","1")

        # Ensure there are 15 members of this set
        self.connection.request("GET","/redis/%(setName)s/count" % {"setName":setName})
        self.assertXmlResponse(self.connection.getresponse(),"integer","15")

        # Test range functionality
        self.connection.request("GET","/redis/%(setName)s/range/0/14" % {"setName":setName})
        document = self.responseToXml(self.connection.getresponse())

        allStringsFromResponse = document.findall('array/string')
        self.assertEqual(30,len(document.findall('array/string')),"Should have 30 string elements in the response")

        for i in range(15):
            userElement,scoreElement = allStringsFromResponse[0:2]
            self.assertEqual("user%d" % i,userElement.text)
            self.assertEqual("%d" % i,scoreElement.text)
            allStringsFromResponse = allStringsFromResponse[2:]

        # Test ZREM
        self.connection.request("DELETE","/redis/%(setName)s/user14" % {"setName":setName}) 
        self.assertXmlResponse(self.connection.getresponse(),"integer","1")

        # Ensure there are 14 members left
        self.connection.request("GET","/redis/%(setName)s/count" % {"setName":setName})
        self.assertXmlResponse(self.connection.getresponse(),"integer","14")

    def testSortedSetOperationsJson(self):
    
        setName = "testset%d" % (self.getNextCounterValue())

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(15):
            self.connection.request("PUT","/redis/%(setName)s/user%(user)d.json" % {"setName":setName,"user":i} ,"%s" % i,headers)            

            self.assertJsonResponse(self.connection.getresponse(),"integer","1")

        # Ensure there are 15 members of this set
        self.connection.request("GET","/redis/%(setName)s/count.json" % {"setName":setName})
        self.assertJsonResponse(self.connection.getresponse(),"integer","15")

        # Test range functionality
        self.connection.request("GET","/redis/%(setName)s/range/0/14.json" % {"setName":setName})
        data = self.responseToJson(self.connection.getresponse())
        allStringsFromResponse = data['array']
        self.assertEqual(30,len(allStringsFromResponse))

        for i in range(15):
            userElement,scoreElement = allStringsFromResponse[0:2]
            self.assertEqual("user%d" % i,userElement['string'])
            self.assertEqual("%d" % i,scoreElement['string'])
            allStringsFromResponse = allStringsFromResponse[2:]

        # Test ZREM
        self.connection.request("DELETE","/redis/%(setName)s/user14.json" % {"setName":setName}) 
        self.assertJsonResponse(self.connection.getresponse(),"integer","1")

        # Ensure there are 14 members left
        self.connection.request("GET","/redis/%(setName)s/count.json" % {"setName":setName})
        self.assertJsonResponse(self.connection.getresponse(),"integer","14")
