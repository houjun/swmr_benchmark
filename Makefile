HDF5_SWMR_DIR=/global/homes/h/houhun/hdf5/swmr_build/hdf5
HDF5_DEVELOP_DIR=/global/homes/h/houhun/hdf5/develop_build/hdf5

CC=cc
CFLAGS=-g -O3 
LDFLAGS=-dynamic -Wall -L/global/homes/h/houhun/swmr_benchmark -lmap-sampler-pmpi -lmap-sampler -Wl,--eh-frame-hdr -Wl,-rpath=/global/homes/h/houhun/swmr_benchmark
SWMR_FLAGS=-I$(HDF5_SWMR_DIR)/include -L$(HDF5_SWMR_DIR)/lib -lhdf5 
NONSWMR_FLAGS=-I$(HDF5_DEVELOP_DIR)/include -L$(HDF5_DEVELOP_DIR)/lib -lhdf5 

all:
	$(CC) $(CFLAGS) -o swmr_meta_benchmark.exe swmr_meta_benchmark.c $(LDFLAGS) $(SWMR_FLAGS) 
	$(CC) $(CFLAGS) -o swmr_noflag_meta_benchmark.exe swmr_noflag_meta_benchmark.c $(LDFLAGS) $(SWMR_FLAGS) 
	$(CC) $(CFLAGS) -o swmr_benchmark.exe  swmr_benchmark.c $(LDFLAGS) $(SWMR_FLAGS)
	$(CC) $(CFLAGS) -o nonswmr_meta_benchmark.exe nonswmr_meta_benchmark.c $(LDFLAGS) $(NONSWMR_FLAGS) 
	$(CC) $(CFLAGS) -o nonswmr_benchmark.exe nonswmr_benchmark.c $(LDFLAGS) $(NONSWMR_FLAGS)
clean:
	rm ./*.exe

