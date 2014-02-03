#include "common.h"
#include <omp.h>
#include <sys/time.h>

static void update(double *primary, double *secondary, int j, int k);
static void swap (double **primary, double **secondary);

void iterate(double **sub_matrix)
{
  double start, finish;
  int p, rank, reqs = 0;
  MPI_Comm_size(MPI_COMM_WORLD, &p);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Request requests[8];
  MPI_Status statuses[8];

  double *secondary = calloc((SUB_ROW) * (SUB_COL), sizeof(double));
  if (secondary == NULL){
    bail_out(EXIT_FAILURE, "malloc secondary");
  }

  memcpy(secondary, *sub_matrix,(SUB_ROW) * (SUB_COL) * sizeof(double));
  
  //Create column datatype
  MPI_Datatype col, col2; 
  MPI_Type_vector(SUB_ROW,1,SUB_COL,MPI_DOUBLE, &col2); 
  MPI_Type_create_resized(col2, 0, sizeof(double), &col);
  MPI_Type_commit(&col); 

  MPI_Barrier(MPI_COMM_WORLD);
  if (rank == 0) fprintf(stderr, "init finished\n");
  start = MPI_Wtime();

  //start iterations
  for (int i = 0 ; i < options.iter; i++){

    for (int j = 1; j <= sub_rows; j++){
      for(int k = 1; k <= sub_cols; k++){
        update(*sub_matrix, secondary, j,k);
      }
    }
    swap(sub_matrix, &secondary);
  
    reqs = 0;    
    //exchange up
    if (coords[0] > 0){
      int tmp_coords[2] = {coords[0] - 1, coords[1]};
      int tmp_rank = 0;
      MPI_Cart_rank(cart_comm, &tmp_coords[0], &tmp_rank);
      MPI_Isend (&((*sub_matrix)[SUB_COL]), SUB_COL, MPI_DOUBLE, tmp_rank, 2, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
      MPI_Irecv (*sub_matrix, SUB_COL, MPI_DOUBLE, tmp_rank, 0, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
    }

    //exchange down
    if (coords[0] < dims[0] - 1){
      int tmp_coords[2] = {coords[0] + 1, coords[1]};
      int tmp_rank = 0;
      MPI_Cart_rank(cart_comm, &tmp_coords[0], &tmp_rank);
      MPI_Isend (&((*sub_matrix)[(SUB_COL) * (sub_rows)]), SUB_COL, MPI_DOUBLE, tmp_rank, 0, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
      MPI_Irecv (&((*sub_matrix)[(SUB_COL) * (sub_rows + 1)]), SUB_COL, MPI_DOUBLE, tmp_rank, 2, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
    }

    //exchange left
    if (coords[1] > 0){
      int tmp_coords[2] = {coords[0], coords[1] - 1};
      int tmp_rank = 0;
      MPI_Cart_rank(cart_comm, &tmp_coords[0], &tmp_rank);
      MPI_Isend (&((*sub_matrix)[1]), 1, col, tmp_rank, 1, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
      MPI_Irecv (*sub_matrix, 1, col, tmp_rank, 3, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
    }

    //exchange right
    if (coords[1] < dims[1] - 1){
      int tmp_coords[2] = {coords[0], coords[1] + 1};
      int tmp_rank = 0;
      MPI_Cart_rank(cart_comm, &tmp_coords[0], &tmp_rank);
      MPI_Isend (&((*sub_matrix)[sub_cols]), 1, col, tmp_rank, 3, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
      MPI_Irecv (&((*sub_matrix)[sub_cols + 1]), 1, col, tmp_rank, 1, MPI_COMM_WORLD, &requests[reqs]);
      reqs++;
    }

    MPI_Waitall(reqs,requests,statuses);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  finish = MPI_Wtime();
  if (rank == 0) fprintf(stderr, "loop time = %f\n", finish-start);
  
  if (rank == 0) fprintf(stderr, "loop finished\n");

  
  free(secondary);
}

static void update(double *primary, double *secondary, int j, int k){
  debug("update %d %d\n", j, k);

  double sum = 0;

  sum += primary[((SUB_COL) * j) + k-1];
  sum += primary[((SUB_COL) * (j-1)) + k];
  sum += primary[((SUB_COL) * j) + k+1];
  sum += primary[((SUB_COL) * (j+1)) + k];

  secondary[((SUB_COL) * j) + k] = sum/(double)4;
}

static void swap (double **primary, double **secondary){
  double *temp = *primary;
  *primary = *secondary;
  *secondary = temp;
  /*
  for (int i = 0; i < options.n; i++){
    memcpy((*primary)[i],(*secondary)[i],options.m * sizeof(double));
  }
  */
  
}
