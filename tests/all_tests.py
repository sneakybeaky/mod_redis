import unittest 

import test_sorted_set
import test_ping


if __name__ == '__main__':

    loader = unittest.TestLoader()
    suiteFew = unittest.TestSuite() 
    suiteFew.addTests(loader.loadTestsFromModule(test_sorted_set))       
    suiteFew.addTests(loader.loadTestsFromModule(test_ping))   
    unittest.TextTestRunner(verbosity=2).run(suiteFew)
