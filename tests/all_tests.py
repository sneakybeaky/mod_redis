import unittest 

import test_sorted_set
import test_ping
import test_put_get_delete
import test_unicode
import test_jsonp
import test_post_form
import sys
from test_mod_redis import TestModRedis

from optparse import OptionParser

def main():
    parseOptions()
    testResults = runTests()
    
    sys.exit(testResults.wasSuccessful() == 0)

def parseOptions():

    parser = OptionParser()
    parser.add_option("-s", "--servername", dest="servername",
                  help="Apache server hostname", type="string", default="localhost")
    parser.add_option("-p", "--port", dest="port",
                  help="Apache server port", type="int", default="8081")

    (options, args) = parser.parse_args()
    TestModRedis.servername = options.servername
    TestModRedis.port = options.port

def runTests():
    loader = unittest.TestLoader()
    suite = unittest.TestSuite() 
    suite.addTests(loader.loadTestsFromModule(test_ping))   
    suite.addTests(loader.loadTestsFromModule(test_sorted_set))       
    suite.addTests(loader.loadTestsFromModule(test_put_get_delete))           
    suite.addTests(loader.loadTestsFromModule(test_unicode))           
    suite.addTests(loader.loadTestsFromModule(test_jsonp))       
    suite.addTests(loader.loadTestsFromModule(test_post_form))       
    return unittest.TextTestRunner(verbosity=2).run(suite)


if __name__ == '__main__':
    main()



