#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "mpi.h"
#include "hdf5.h"

#define NDIM 1

#define FILENAME     "swmr_test.h5"

void print_usage(const char *exename)
{
    printf("Usage: ./%s write_size n_writes filepath(optional)\n", exename);
}

int main(int argc, char *argv[])
{
    int rank, size, i, j, varified;
    int amount_read, total_written;
    int n_writes = 0;
    char *data;
    int  *gather_msg;
    char *filepath = NULL;
    char filename[128] = {0};
    hsize_t write_size = 0;
    herr_t   status;
    hid_t    file_id, dset_id, fspace_id, dspace_id, plist, fapl;
    hsize_t dims[NDIM], new_dims[NDIM], chk_dims[NDIM], maxdims[NDIM], start[NDIM], count[NDIM];
    double total_time = 0.0, write_time = 0.0, read_time = 0.0;
    double total_time_start, write_time_start, read_time_start;
    double total_time_end, write_time_end, read_time_end;


    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    sprintf(filename, "./%s", FILENAME);

    if (argc < 3) {
        print_usage(argv[0]);
        goto exit;
    }
    else if (argc == 4) {
        filepath   = argv[3];
        sprintf(filename, "%s/%s", filepath, FILENAME);
    }

    write_size = (hsize_t)atoi(argv[1]);
    n_writes   = atoi(argv[2]);
    total_written = write_size * n_writes;

    dims[0]    = write_size;
    new_dims[0]= write_size;
    maxdims[0] = H5S_UNLIMITED;

    chk_dims[0]= write_size;
    if (write_size > 1048576) {
        chk_dims[0]= 1048576;
    }

    int delay = 0;
    char *delay_str = getenv("DELAY");
    if (delay_str != NULL) {
        delay = atoi(delay_str);
        if (rank == 0) {
            printf("%ds sleep time\n", delay);
            fflush(stdout);
        }
    }

    data       = (char*)malloc(write_size);

    gather_msg = (int*)malloc(size*sizeof(int));

    if (rank == 0) 
        printf("Writing to %s\n", filename);

    MPI_Barrier(MPI_COMM_WORLD);
    total_time_start = MPI_Wtime();

    // Rank 0 is the writer, others are readers
    if (rank == 0) {


        fapl = H5Pcreate (H5P_FILE_ACCESS);
        H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST);

        // Create a file
        file_id = H5Fcreate(filename, H5F_ACC_TRUNC|H5F_ACC_SWMR_WRITE, H5P_DEFAULT, fapl);
        if (file_id < 0) {
            printf("Error creating file %s\n", filename);
            goto done;
        }

        // Create an unlimited dataset
        fspace_id = H5Screate_simple(NDIM, dims, maxdims);
        plist     = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_layout(plist, H5D_CHUNKED);

        H5Pset_chunk(plist, NDIM, chk_dims);
        dset_id   = H5Dcreate(file_id, "dat", H5T_NATIVE_CHAR, fspace_id, H5P_DEFAULT, plist, H5P_DEFAULT);

        H5Pclose(plist);

        // Generate new data 
        i = 0;
        for (j = 0; j < write_size; j++) 
            data[j] = 'a' + i;

        if (delay > 0) 
            sleep(delay);

        // Create a memory dataspace to indicate the size of buffer
        dspace_id = H5Screate_simple(NDIM, dims, NULL);

        start[0] = 0;
        count[0] = write_size;
        H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

        // Write data
        status = H5Dwrite(dset_id, H5T_NATIVE_CHAR, dspace_id, fspace_id, H5P_DEFAULT, data);
        if (status < 0) {
            printf("Error writing dataset!\n");
        }

        status = H5Sclose(fspace_id);

        // Flush
        status = H5Fflush(file_id, H5F_SCOPE_GLOBAL);

        MPI_Barrier(MPI_COMM_WORLD); /* A */

        for (i = 1; i < n_writes; i++) {
            // Do something
            if (delay > 0) 
                sleep(delay);
            

            // Generate new data 
            for (j = 0; j < write_size; j++) 
                data[j] = 'a' + i;


            // Use H5DOappend?
            /* H5DOappend(dset_id, H5P_DEFAULT, 0, write_size, H5T_NATIVE_CHAR, data) */

            // Extend dataset
            new_dims[0] += write_size;
            H5Dset_extent(dset_id, new_dims);

            fspace_id = H5Dget_space(dset_id);

            start[0] = new_dims[0] - write_size;
            count[0] = write_size;
            status = H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

            write_time_start = MPI_Wtime();
            status = H5Dwrite(dset_id, H5T_NATIVE_CHAR, dspace_id, fspace_id, H5P_DEFAULT, data);
            write_time_end = MPI_Wtime();
            write_time += write_time_end - write_time_start;
            if (status < 0) {
                printf("Error appending dataset!\n");
            }

            status = H5Sclose(fspace_id);
       
            // Flush
            status = H5Dflush(dset_id);

        } // end for n_writes
H5Dclose(dset_id);

        status = H5Sclose(dspace_id);

H5Fclose(file_id);
// MPI_Barrier(MPI_COMM_WORLD); /* B */
    }
    else {
        MPI_Barrier(MPI_COMM_WORLD); /* A */
// MPI_Barrier(MPI_COMM_WORLD); /* B */
        // I am reader
        dims[0] = 0;

        file_id = H5Fopen(filename, H5F_ACC_RDONLY|H5F_ACC_SWMR_READ, H5P_DEFAULT);
        dset_id = H5Dopen(file_id, "/dat", H5P_DEFAULT);
        if (dset_id < 0) {
            printf("Error with opening dataset\n");
            goto exit;
        }

        amount_read = 0;
        i = 0;
        while (amount_read < total_written) {
            H5Drefresh(dset_id);
            fspace_id = H5Dget_space(dset_id);
            if (fspace_id < 0) {
                printf("Error with fspace\n");
                goto exit;
            }
            status = H5Sget_simple_extent_dims(fspace_id, new_dims, NULL);

            // If no change, continue
            if (new_dims[0] == dims[0]) {
                H5Sclose(fspace_id);
                continue;
            }

            // Select new data
            start[0]  = dims[0];
            count[0]  = new_dims[0] - dims[0];
/* HDfprintf(stderr, "count = {%Hu}\n", count[0]); */
            status = H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

            dspace_id = H5Screate_simple (NDIM, count, NULL); 
            data = realloc(data, count[0]);

            // Read 
            read_time_start = MPI_Wtime();
            status = H5Dread (dset_id, H5T_NATIVE_CHAR, dspace_id, fspace_id, H5P_DEFAULT, data);
            read_time_end = MPI_Wtime();
            read_time = read_time_end - read_time_start;

            amount_read += count[0];

            // Verify
            varified = 1;
            for (j = 0; j < count[0]; j++, i++) 
                if (data[j] != 'a' + (i / write_size)) {
                    printf("Reader #%d: Error with read data, data[%u] = '%u'.\n", 
                            rank, (unsigned)j, (unsigned)data[j]);
                    varified = 0;
                    break;
                }

            if (varified == 1) 
                printf("Reader #%d: Iter %d - Successfully read data %.2f MB.\n", 
                        rank, (int)(i/write_size), count[0] / 1048576.0);
            fflush(stdout);
            
            // Record the current dims for next read
            dims[0] = new_dims[0];
        } // end while


H5Dclose(dset_id);
H5Fclose(file_id);

    } // end else


    MPI_Barrier(MPI_COMM_WORLD); /* C */
    total_time_end = MPI_Wtime();
    total_time = total_time_end - total_time_start;

    if (rank == 0) 
        printf("Rank %d: Total time:%.2f, write total time: %.2f\n", rank, total_time, write_time);
    fflush(stdout);

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != 0) 
        printf("Rank %d: Total time:%.2f, read  total time: %.2f\n", rank, total_time, read_time);

done:
    free(data);
    free(gather_msg);

exit:
    MPI_Finalize();
    return 0;
}
