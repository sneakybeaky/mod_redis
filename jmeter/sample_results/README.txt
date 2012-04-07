This directory holds load test results from a 2.53Ghz Intel Core 2 Duo MacBook Pro running OS X 10.6.8 with 4GB memory, Apache 2.2.22 and Redis 2.4.10.

An export of the JMeter Aggregate report follows (all times in ms) :

Label				# Samples	Average	Median	90% Line	Min	Max		Error %		Throughput / sec	KB/sec

Form to sorted set	24914		37		35		52			0	2689	0			143.1				37.7
Create				25086		37		35		51			0	2522	0			144.1				37.9
Get score			24914		37		35		51			0	2475	0			143.1				38.7
Read				75258		36		35		51			0	2678	0			432.6				117.0
Get rank			24914		37		35		51			0	2486	0			143.3				38.3
Update				25086		37		35		52			0	2285	0			144.3				37.9
Delete				25086		37		35		51			0	2534	0			144.3				38.0
TOTAL				225258		37		35		51			0	2689	0			1,293.9				345.3
