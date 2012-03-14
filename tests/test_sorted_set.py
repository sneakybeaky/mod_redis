import urllib

import test_mod_redis
import xml.etree.ElementTree as ET

class TestSortedSet(test_mod_redis.TestModRedis):

    def setUp(self):
        super(TestSortedSet,self).setUp()
        self.setName = "testset%d" % (self.getNextCounterValue())

    def tearDown(self):
        self.deleteRedisKey(self.setName)

    def testZrevrangeFromFormXml(self):

        self.addFifteenUsers()

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        params = urllib.urlencode({'from': 1, 'to': 2})
        self.connection.request("POST","/redis/%(setName)s/revrange" % {"setName":self.setName},params,headers)

        document = self.responseToXml(self.connection.getresponse())
        stringElements = document.findall('array/string')
        
        foundstrings = set()
        for element in stringElements:
            foundstrings.add(element.text)

        self.assertEqual(foundstrings,set(['user13','13','user12','12']))

    def testZrevrangeFromFormJson(self):

        self.addFifteenUsers()

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        params = urllib.urlencode({'from': 1, 'to': 2})
        self.connection.request("POST","/redis/%(setName)s/revrange.json" % {"setName":self.setName},params,headers)

        data = self.responseToJson(self.connection.getresponse())
        allStringsFromResponse = data['array']
 
        foundstrings = set()
        for nameAndValue in allStringsFromResponse:
            foundstrings.add(nameAndValue['string'])

        self.assertEqual(foundstrings,set(['user13','13','user12','12']))



    def addFifteenUsers(self):    
        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(15):
            self.connection.request("PUT","/redis/%(setName)s/user%(user)d" % {"setName":self.setName,"user":i} ,"%s" % i,headers)            
            self.assertXmlResponse(self.connection.getresponse(),{"integer":"1"})

    def testSortedSetOperationsXml(self):

        self.addFifteenUsers()
        # Ensure there are 15 members of this set
        self.connection.request("GET","/redis/%(setName)s/count" % {"setName":self.setName})
        self.assertXmlResponse(self.connection.getresponse(),{"integer":"15"})

        # Test range functionality
        self.connection.request("GET","/redis/%(setName)s/range/0/14" % {"setName":self.setName})
        document = self.responseToXml(self.connection.getresponse())

        allStringsFromResponse = document.findall('array/string')
        self.assertEqual(30,len(document.findall('array/string')),"Should have 30 string elements in the response")

        for i in range(15):
            userElement,scoreElement = allStringsFromResponse[0:2]
            self.assertEqual("user%d" % i,userElement.text)
            self.assertEqual("%d" % i,scoreElement.text)
            allStringsFromResponse = allStringsFromResponse[2:]

        # Test ZREM
        self.connection.request("DELETE","/redis/%(setName)s/user14" % {"setName":self.setName}) 
        self.assertXmlResponse(self.connection.getresponse(),{"integer":"1"})

        # Ensure there are 14 members left
        self.connection.request("GET","/redis/%(setName)s/count" % {"setName":self.setName})
        self.assertXmlResponse(self.connection.getresponse(),{"integer":"14"})

    def testSortedSetOperationsJson(self):    

        headers = {"Content-type": "application/x-www-form-urlencoded"}
        
        for i in range(15):
            self.connection.request("PUT","/redis/%(setName)s/user%(user)d.json" % {"setName":self.setName,"user":i} ,"%s" % i,headers)            

            self.assertJsonResponse(self.connection.getresponse(),"integer","1")

        # Ensure there are 15 members of this set
        self.connection.request("GET","/redis/%(setName)s/count.json" % {"setName":self.setName})
        self.assertJsonResponse(self.connection.getresponse(),"integer","15")

        # Test range functionality
        self.connection.request("GET","/redis/%(setName)s/range/0/14.json" % {"setName":self.setName})
        data = self.responseToJson(self.connection.getresponse())
        allStringsFromResponse = data['array']
        self.assertEqual(30,len(allStringsFromResponse))

        for i in range(15):
            userElement,scoreElement = allStringsFromResponse[0:2]
            self.assertEqual("user%d" % i,userElement['string'])
            self.assertEqual("%d" % i,scoreElement['string'])
            allStringsFromResponse = allStringsFromResponse[2:]

        # Test ZREM
        self.connection.request("DELETE","/redis/%(setName)s/user14.json" % {"setName":self.setName}) 
        self.assertJsonResponse(self.connection.getresponse(),"integer","1")

        # Ensure there are 14 members left
        self.connection.request("GET","/redis/%(setName)s/count.json" % {"setName":self.setName})
        self.assertJsonResponse(self.connection.getresponse(),"integer","14")
