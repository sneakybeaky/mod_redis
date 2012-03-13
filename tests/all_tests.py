import unittest 

import test_sorted_set
import test_ping
import test_put_get_delete
import test_unicode
import test_jsonp
import sys

if __name__ == '__main__':

    loader = unittest.TestLoader()
    suite = unittest.TestSuite() 
    suite.addTests(loader.loadTestsFromModule(test_ping))   
    suite.addTests(loader.loadTestsFromModule(test_sorted_set))       
    suite.addTests(loader.loadTestsFromModule(test_put_get_delete))           
    suite.addTests(loader.loadTestsFromModule(test_unicode))           
    suite.addTests(loader.loadTestsFromModule(test_jsonp))       
    unittest.TextTestRunner(verbosity=2).run(suite)
