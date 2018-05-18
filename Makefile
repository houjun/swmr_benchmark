HDF5_SWMR_DIR=/global/u1/h/houhun/hdf5/build/hdf5
HDF5_DEVELOP_DIR=/global/cscratch1/sd/houhun/hdf5/build/hdf5

LDFLAGS=-lhdf5 -lz -ldl -lm -Wall
all:
	cc swmr_benchmark.c 	  -o swmr_benchmark.exe  -g -I$(HDF5_SWMR_DIR)/include -L$(HDF5_SWMR_DIR)/lib $(LDFLAGS)
	cc nonswmr_benchmark.c  -o nonswmr_benchmark.exe -g -I$(HDF5_DEVELOP_DIR)/include -L$(HDF5_DEVELOP_DIR)/lib $(LDFLAGS)
clean:
	rm ./*.exe

