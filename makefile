package: package.c
	gcc package.c -pthread -Wall -o package
compute: compute.c
	gcc compute.c -pthread -Wall -o compute