#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "mpi.h"
#include "hdf5.h"

#define NDIM 1

#define OK_TO_OPEN   1
#define FLUSH_COMING 2
#define FILE_CLOSED  3
#define FAILED       9
#define FILENAME     "nonswmr_test"

void print_usage(const char *exename)
{
    printf("Usage: ./%s write_size(Bytes) n_writes filepath(optional)\n", exename);
}

int main(int argc, char *argv[])
{
    int rank, size, i, j;
    int n_writes = 0;
    int send_msg = 0, recv_msg = 0;
    char *data;
    char *filepath = NULL;
    char filename[128] = {0};
    int  *gather_msg;
    hsize_t write_size = 0;
    herr_t   status;
    hid_t    file_id, dset_id, fspace_id, dspace_id, plist;
    hsize_t dims[NDIM], new_dims[NDIM], chk_dims[NDIM], maxdims[NDIM], start[NDIM], count[NDIM];
    double total_time = 0.0, write_time = 0.0, read_time = 0.0, comm_time = 0.0, iter_comm_time = 0.0;
    double total_time_start, write_time_start, read_time_start, comm_time_start;
    double total_time_end, write_time_end, read_time_end, comm_time_end;


    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    sprintf(filename, "./%s", FILENAME);

    if (argc != 4) {
        print_usage(argv[0]);
        goto exit;
    }
    else {
        write_size = (hsize_t)atol(argv[1]);
        /* write_size *= 1048576; */
        n_writes   = atoi(argv[2]);
        filepath   = argv[3];
        sprintf(filename, "%s/%s_%lluB.h5", filepath, FILENAME, write_size);
    }

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

    MPI_Barrier(MPI_COMM_WORLD);
    total_time_start = MPI_Wtime();

    if (rank == 0) 
        printf("Writing to %s\n", filename);
    

    // Rank 0 is the writer, others are readers
    if (rank == 0) {

        // Create a file
        file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if (file_id < 0) {
            printf("Error creating file %s\n", filename);
            goto done;
        }

        for (i = 0; i < n_writes; i++) {

            // Do something
            if (delay > 0) 
                sleep(delay);

            // Generate new data 
            memset(data, 'a' + i, write_size);

            // Send "flush coming" signal to all readers
            send_msg = FLUSH_COMING;
            comm_time_start = MPI_Wtime();
            MPI_Bcast(&send_msg, 1, MPI_INT, 0, MPI_COMM_WORLD);
            comm_time_end = MPI_Wtime();
            iter_comm_time = comm_time_end - comm_time_start;
            comm_time     += comm_time_end - comm_time_start;

            // Confirms all readers have closed file
            send_msg = FILE_CLOSED;
            comm_time_start = MPI_Wtime();
            MPI_Gather(&send_msg, 1, MPI_INT, gather_msg, 1, MPI_INT, 0, MPI_COMM_WORLD);
            comm_time_end = MPI_Wtime();
            iter_comm_time += comm_time_end - comm_time_start;
            comm_time      += comm_time_end - comm_time_start;
            for (j = 1; j < size; j++) {
                if (gather_msg[j] != FILE_CLOSED) {
                    printf("Error receiving comfirm message from readers!\n");
                }
            }
 
            // Write data
            if (i == 0) {
                // Create an unlimited dataset
                fspace_id = H5Screate_simple(NDIM, dims, maxdims);
                plist     = H5Pcreate(H5P_DATASET_CREATE);
                H5Pset_layout(plist, H5D_CHUNKED);
                
                H5Pset_chunk(plist, NDIM, chk_dims);
                dset_id   = H5Dcreate(file_id, "dat", H5T_NATIVE_CHAR, fspace_id, H5P_DEFAULT, plist, H5P_DEFAULT);

                // "Cork" the cache
                if (H5Odisable_mdc_flushes(dset_id) < 0)
                    printf("Error with H5Odisable_mdc_flushes!\n");

                H5Pclose(plist);

                // Create a memory dataspace to indicate the size of buffer
                dspace_id = H5Screate_simple(NDIM, dims, NULL);

                start[0] = 0;
                count[0] = write_size;
                H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

                // Write data
                write_time_start = MPI_Wtime();
                status = H5Dwrite(dset_id, H5T_NATIVE_CHAR, dspace_id, fspace_id, H5P_DEFAULT, data);
                write_time_end = MPI_Wtime();
                write_time += write_time_end - write_time_start;
                if (status < 0) {
                    printf("Error writing dataset!\n");
                }

                H5Sclose(fspace_id);
                H5Sclose(dspace_id);
            }
            else {
                file_id   = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
                dset_id   = H5Dopen(file_id, "/dat", H5P_DEFAULT);
                dspace_id = H5Screate_simple(NDIM, dims, NULL);

                // Extend dataset
                new_dims[0] += write_size;
                H5Dset_extent(dset_id, new_dims);

                fspace_id = H5Dget_space(dset_id);

                start[0] = new_dims[0] - write_size;
                count[0] = write_size;
                status = H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

                status = H5Dwrite(dset_id, H5T_NATIVE_CHAR, dspace_id, fspace_id, H5P_DEFAULT, data);
                if (status < 0) {
                    printf("Error appending dataset!\n");
                }

                H5Sclose(fspace_id);
                H5Sclose(dspace_id);
            }
           
            // Flush
            H5Fflush(file_id, H5F_SCOPE_LOCAL);

            // Close writer's file
            status = H5Dclose(dset_id);
            status = H5Fclose(file_id); 

            // Send "OK to reopen" signal to all readers
            send_msg = OK_TO_OPEN;
            comm_time_start = MPI_Wtime();
            MPI_Bcast(&send_msg, 1, MPI_INT, 0, MPI_COMM_WORLD);
            comm_time_end = MPI_Wtime();
            iter_comm_time += comm_time_end - comm_time_start;
            comm_time      += comm_time_end - comm_time_start;

            printf("Iter %2d - Rank %d: comm time: %.4f, write time: %.4f\n", 
                    i, rank, iter_comm_time, write_time);
            fflush(stdout);
        } // end for n_writes

    }
    else {
        // I am a reader

        dims[0] = 0;
        for (i = 0; i < n_writes; i++) {

            // Wait for "flush coming" signal from writer
            comm_time_start = MPI_Wtime();
            MPI_Bcast(&recv_msg, 1, MPI_INT, 0, MPI_COMM_WORLD);
            comm_time_end = MPI_Wtime();
            iter_comm_time = comm_time_end - comm_time_start;
            comm_time     += comm_time_end - comm_time_start;
            if (recv_msg == FLUSH_COMING) {
                send_msg = FILE_CLOSED;
                if (i != 0) {
                    // Close file
                    status = H5Sclose(fspace_id);
                    status = H5Sclose(dspace_id);
                    status = H5Dclose(dset_id);
                    status = H5Fclose(file_id); 
                    if (status < 0) {
                        printf("Error closing the file [%s]!\n", filename);
                        send_msg = FAILED;
                    }
                }

                // Send "file closed" message to writer
                comm_time_start = MPI_Wtime();
                MPI_Gather(&send_msg, 1, MPI_INT, gather_msg, 1, MPI_INT, 0, MPI_COMM_WORLD);
                comm_time_end = MPI_Wtime();
                iter_comm_time += comm_time_end - comm_time_start;
                comm_time      += comm_time_end - comm_time_start;
            }

            // Recieves "OK to reopen" signal from writer
            comm_time_start = MPI_Wtime();
            MPI_Bcast(&recv_msg, 1, MPI_INT, 0, MPI_COMM_WORLD);
            comm_time_end = MPI_Wtime();
            iter_comm_time += comm_time_end - comm_time_start;
            comm_time      += comm_time_end - comm_time_start;
            if (recv_msg == OK_TO_OPEN) {
                // Open file
                file_id   = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
                if (file_id < 0) {
                    printf("Error with opening dataset\n");
                    goto done;
                }
                dset_id   = H5Dopen(file_id, "/dat", H5P_DEFAULT);
                fspace_id = H5Dget_space(dset_id);
                H5Sget_simple_extent_dims(fspace_id, new_dims, NULL);

                start[0]  = dims[0];
                count[0]  = new_dims[0] - dims[0];
                H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

                dspace_id = H5Screate_simple (NDIM, count, NULL); 

                // Select new data and read 
                status = H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, start, NULL, count, NULL);

                read_time_start = MPI_Wtime();
                status = H5Dread (dset_id, H5T_NATIVE_CHAR, dspace_id, fspace_id, H5P_DEFAULT, data);
                read_time_end = MPI_Wtime();
                read_time = read_time_end - read_time_start;

                /* // Verify */
                /* int varified = 1; */
                /* for (j = 0; j < write_size; j++) */ 
                /*     if (data[j] != 'a' + i) { */
                /*         printf("Error with read data.\n"); */
                /*         varified = 0; */
                /*         break; */
                /*     } */

                /* if (varified == 1) */ 
                /*     printf("Iter %d - Reader #%d: Successfully read data %.2f MB.\n", i, rank, count[0]/1048576.0); */
                /* fflush(stdout); */
                
                // Record the current dims for next read
                dims[0] = new_dims[0];

            } // end if OK_TO_OPEN

            /* printf("Iter %2d - Rank %d: comm time: %.4f, read  time: %.4f\n", */ 
            /*         i, rank, iter_comm_time, read_time); */
            /* fflush(stdout); */
        } // end for n_writes

    } // end else


    MPI_Barrier(MPI_COMM_WORLD);
    total_time_end = MPI_Wtime();
    total_time = total_time_end - total_time_start;

    if (rank == 0) 
        printf("Rank %d: Total time:%.4f, comm time: %.4f, write total time: %.4f\n", 
                rank, total_time, comm_time, write_time);
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank != 0) 
        printf("Rank %d: Total time:%.4f, comm time: %.4f, read  total time: %.4f\n", 
                rank, total_time, comm_time, read_time);

done:
    free(data);
    free(gather_msg);
exit:
    MPI_Finalize();
    return 0;
}
