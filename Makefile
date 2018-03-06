all:
	cc swmr_benchmark.c 	  -o swmr_benchmark.exe  -g -I/global/cscratch1/sd/houhun/hdf5-1.10.1/build/hdf5/include -L/global/cscratch1/sd/houhun/hdf5-1.10.1/build/hdf5/lib -lhdf5 -lz -ldl -lm -Wall
	cc nonswmr_benchmark.c  -o nonswmr_benchmark.exe -g -I/global/cscratch1/sd/houhun/hdf5-1.10.1/build/hdf5/include -L/global/cscratch1/sd/houhun/hdf5-1.10.1/build/hdf5/lib -lhdf5 -lz -ldl -lm -Wall
clean:
	rm ./*.exe

