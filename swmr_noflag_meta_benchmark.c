#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include "hdf5.h"

#define NDIM 1

void print_usage(const char *exename)
{
    printf("Usage: ./%s n_groups n_dset_per_group n_attr_per_dset file_path\n", exename);
}

double get_elapsed_time_double(struct timeval *tstart, struct timeval *tend)
{
    return (double)(((tend->tv_sec-tstart->tv_sec)*1000000LL + tend->tv_usec-tstart->tv_usec) / 1000000.0);
}

int main(int argc, char *argv[])
{
    int n_grp, n_dset_per_grp, n_attr_per_dset, i, j, k;
    char *filepath = NULL;
    char filename[128] = {0};
    char grp_name[128];
    char dset_name[128];
    char attr_name[128];
    herr_t   status;
    hid_t    fid, dset, grp, fapl, dcpl, sid, attr_sid, attr_id;
    struct timeval  timer1;
    struct timeval  timer2;
    double total_time;

    hsize_t dims[1] = {1};                       /* Dimension sizes */
    hsize_t max_dims[1] = {H5S_UNLIMITED};       /* Maximum dimension sizes */
    hsize_t chunk_dims[1] = {2};                 /* Chunk dimension sizes */
    hsize_t attr_dims[1] = {1};


    if (argc != 5) {
        print_usage(argv[0]);
        goto exit;
    }
    else {
        n_grp            = atol(argv[1]);
        n_dset_per_grp   = atoi(argv[2]);
        n_attr_per_dset  = atoi(argv[3]);
        filepath         = argv[4];
        sprintf(filename, "%s/swmr_%dgrp_%ddsets_%dattr.h5", filepath, n_grp, n_attr_per_dset, n_dset_per_grp);
    }

    printf("Writing to %s, %d groups, %d dset per group, %d attr per dset\n", 
            filename, n_grp, n_dset_per_grp, n_attr_per_dset);

    fapl = H5Pcreate (H5P_FILE_ACCESS);
    H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST);

    /* Create a chunked dataset with 1 extendible dimension for all created dsets */
    sid = H5Screate_simple(1, dims, max_dims);
    dcpl = H5Pcreate(H5P_DATASET_CREATE);
    H5Pset_chunk(dcpl, 1, chunk_dims);

    /* Create the data space for the attribute. */
    attr_sid = H5Screate_simple(1, attr_dims, NULL);

    // Create a file
    /* fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl); */
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    if (fid < 0) {
        printf("Error creating file %s\n", filename);
        goto exit;
    }

    gettimeofday(&timer1, 0);
    for (i = 0; i < n_grp; i++) {
        sprintf(grp_name, "Group%d", i);
        if((grp = H5Gcreate(fid, grp_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT)) < 0)
            printf("Error creating group [%s]\n", grp_name);

        for (j = 0; j < n_dset_per_grp; j++) {
            sprintf(dset_name, "Dset%d", j);
            if((dset = H5Dcreate(grp, dset_name, H5T_NATIVE_INT, sid, H5P_DEFAULT, dcpl, H5P_DEFAULT)) < 0)
                printf("Error creating dset [%s]\n", dset_name);

            for (k = 0; k < n_attr_per_dset; k++) {
                sprintf(attr_name, "Attr%d", k);
                if((attr_id = H5Acreate2(dset, attr_name, H5T_STD_I32BE, attr_sid, H5P_DEFAULT, H5P_DEFAULT)) < 0)
                    printf("Error creating attr [%s]\n", attr_name);
                H5Aclose(attr_id);
            }
            H5Dclose(dset);
        }
        H5Gclose(grp);

    } 
    // Flush
    status = H5Fflush(fid, H5F_SCOPE_GLOBAL);

    gettimeofday(&timer2, 0);
    total_time = get_elapsed_time_double(&timer1, &timer2);
    printf("Total Creation time:%.4f\n", total_time);


    status = H5Pclose(dcpl);
    status = H5Sclose(sid);
    status = H5Sclose(attr_sid);

    /* status = H5Pclose(fapl); */
    /* status = H5Fclose(fid); */

    /* // ==================DELETE================== */
    /* fapl = H5Pcreate (H5P_FILE_ACCESS); */
    /* H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST); */
    /* fid = H5Fopen(filename, H5F_ACC_SWMR_WRITE, fapl); */
    /* if (fid < 0) { */
    /*     printf("Error opening file [%s]\n", filename); */
    /*     goto exit; */
    /* } */

    /* gettimeofday(&timer1, 0); */
    /* for (i = 0; i < n_grp; i++) { */

    /*     sprintf(grp_name, "Group%d", i); */
    /*     if((grp = H5Gopen(fid, grp_name, H5P_DEFAULT)) < 0) */
    /*         printf("Error opening group [%s]\n", grp_name); */

    /*     for (j = 0; j < n_dset_per_grp; j++) { */
    /*         sprintf(dset_name, "Dset%d", j); */
    /*         if((dset = H5Dopen(grp, dset_name, H5P_DEFAULT)) < 0) */
    /*             printf("Error opening dset [%s]\n", dset_name); */

    /*         for (k = 0; k < n_attr_per_dset; k++) { */
    /*             sprintf(attr_name, "Attr%d", k); */
    /*             if(H5Adelete(dset, attr_name) < 0) */
    /*                 printf("Error deleting attr [%s]\n", attr_name); */
    /*         } */
    /*         H5Dclose(dset); */

    /*         if((H5Ldelete(grp, dset_name, H5P_DEFAULT)) < 0) */
    /*             printf("Error deleting dset [%s]\n", dset_name); */

    /*     } */
    /*     H5Gclose(grp); */

    /*     if((H5Ldelete(fid, grp_name, H5P_DEFAULT)) < 0) */
    /*         printf("Error deleting group [%s]\n", grp_name); */

    /*     // Flush */
    /*     status = H5Fflush(fid, H5F_SCOPE_GLOBAL); */

    /* } // end for n_dset_per_grp */
    /* gettimeofday(&timer2, 0); */
    /* total_time = get_elapsed_time_double(&timer1, &timer2); */
    /* printf("Total deletion time:%.4f\n", total_time); */

    status = H5Pclose(fapl);
    status = H5Fclose(fid);

exit:
    return 0;
}

