#!/usr/bin/python

from optparse import OptionParser

import sys
import telnetlib

def pingRedis(host,port):
    tn = telnetlib.Telnet(host,port)

    tn.write("PING\r\n")
    response = tn.read_until("+PONG",5)
    print response

    return response == "+PONG"
    

def parseOptions():

    parser = OptionParser()
    parser.add_option("-s", "--servername", dest="servername",
                  help="Redis server hostname", type="string", default="localhost")
    parser.add_option("-p", "--port", dest="port",
                  help="Redis server port", type="int", default="6379")

    (options, args) = parser.parse_args()
    return (options.servername,options.port)

def main():

    host,port = parseOptions()
    sys.exit(pingRedis(host,port) == 0)

if __name__ == '__main__':
    main()



