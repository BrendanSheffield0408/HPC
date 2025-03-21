#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

// function prototypes
int get_num_data_points(FILE*);
int read_data(FILE*, int, double*);

int main(int argc, char** argv) {
    int n;                 // number of data points
    double *x;             // pointer to array holding data points
    double *squaredDiffs;  // pointer to array holding squared differences (of x from mean of all x)
    int rank, size;

    // Initialize MPI environment
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Time total code (and elements thereof)
    double startTotalCode = MPI_Wtime();

    // Access file here (and then pass pointer to file). This allows >1 routine to access same file.
    FILE* filePtr;
    char *filename = argv[1]; // filename is 1st parameter on command line
    if (rank == 0) {
        filePtr = fopen(filename, "r"); // open file, given by sole parameter, as read-only
        if (filePtr == NULL) {
            printf("Cannot open file %s\n", filename);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    int totalNum;
    if (rank == 0) {
        totalNum = get_num_data_points(filePtr);
        printf("There are allegedly %d data points to read\n", totalNum);
    }
    MPI_Bcast(&totalNum, 1, MPI_INT, 0, MPI_COMM_WORLD);

    x = (double *) malloc(totalNum * sizeof(double));
    if (x == NULL) {
        printf("Error in allocating memory for data points\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (rank == 0) {
        double start_readData = MPI_Wtime();
        n = read_data(filePtr, totalNum, x); // this is actual number of points read
        printf("%d data points successfully read [%f seconds]\n", n, MPI_Wtime() - start_readData);
        if (n != totalNum) printf("*** WARNING ***\n actual number read (%d) differs from header value (%d)\n\n", n, totalNum);
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(x, n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    squaredDiffs = (double *) malloc(n * sizeof(double));
    if (squaredDiffs == NULL) {
        printf("Error in allocating memory for squared differences\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /*
     * Main data processing loop
     *
     */
    double sum = 0.0;
    double mean;
    double start = MPI_Wtime();

    // Calculate sum of x values
    double local_sum = 0.0;
    for (int i = rank; i < n; i += size) {
        if (i < n) {
            local_sum += x[i];
        }
    }
    MPI_Reduce(&local_sum, &sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        mean = sum / (double) n;
    }
    MPI_Bcast(&mean, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Calculate squared differences
    for (int i = rank; i < n; i += size) {
        if (i < n) {
            double val = (x[i] - mean);
            squaredDiffs[i] = val * val;
        }
    }

    // Calculate sum of squared differences
    local_sum = 0.0;
    for (int i = rank; i < n; i += size) {
        if (i < n) {
            local_sum += squaredDiffs[i];
        }
    }
    MPI_Reduce(&local_sum, &sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Calculate min and max absolute values
    double minabs, maxabs;
    if (totalNum > 0) {
        minabs = fabs(x[0]);
        maxabs = fabs(x[0]);
    } else {
        minabs = maxabs = 0.0;
    }
    for (int i = rank; i < n; i += size) {
        if (i < n) {
            double val = fabs(x[i]);
            if (val < minabs) minabs = val;
            if (val > maxabs) maxabs = val;
        }
    }
    double global_minabs, global_maxabs;
    MPI_Reduce(&minabs, &global_minabs, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&maxabs, &global_maxabs, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Total parallel regions time [%f seconds]\n", MPI_Wtime() - start);
        printf("min, max absolute values are: %f, %f\n", global_minabs, global_maxabs);
        double variance = sum / (double) n;
        printf(" with mean: %f\n", variance);
        printf("The variance is %f\n", variance);
    }

    printf("Completed. [%f seconds]\n", MPI_Wtime() - startTotalCode);

    // Finalize the MPI environment
    MPI_Finalize();

    free(x);
    free(squaredDiffs);
    return 0;
}
