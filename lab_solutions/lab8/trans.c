/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int t1, t2, t3, t4, t5, t6, t7, t8, m, n;
    if (M == 32 && N == 32) {
        for (int i = 0; i < N; i += 8) {
            for (int j = 0; j < M; j += 8) {
                for (n = i; n < i + 8; n++) {
                    m = j;
                    t1 = A[n][m];
                    t2 = A[n][m + 1];
                    t3 = A[n][m + 2];
                    t4 = A[n][m + 3];
                    t5 = A[n][m + 4];
                    t6 = A[n][m + 5];
                    t7 = A[n][m + 6];
                    t8 = A[n][m + 7];

                    B[m][n] = t1;
                    B[m + 1][n] = t2;
                    B[m + 2][n] = t3;
                    B[m + 3][n] = t4;
                    B[m + 4][n] = t5;
                    B[m + 5][n] = t6;
                    B[m + 6][n] = t7;
                    B[m + 7][n] = t8;
                }
            }
        }
    } else if (M == 64 && N == 64) {
        for (int i = 0; i < N; i += 8) {
            for (int j = 0; j < M; j += 8) {
                for (n = i; n < i + 4; n++) {
                    m = j;
                    t1 = A[n][m];
                    t2 = A[n][m + 1];
                    t3 = A[n][m + 2];
                    t4 = A[n][m + 3];
                    t5 = A[n][m + 4];
                    t6 = A[n][m + 5];
                    t7 = A[n][m + 6];
                    t8 = A[n][m + 7];

                    B[m][n] = t1;
                    B[m + 1][n] = t2;
                    B[m + 2][n] = t3;
                    B[m + 3][n] = t4;

                    int temp = n + 4; // for efficient computation
                    B[m][temp] = t5;
                    B[m + 1][temp] = t6;
                    B[m + 2][temp] = t7;
                    B[m + 3][temp] = t8;
                }

                for (m = j; m < j + 4; m++) {
                    n = i;
                    t1 = A[n + 4][m];
                    t2 = A[n + 5][m];
                    t3 = A[n + 6][m];
                    t4 = A[n + 7][m];

                    t5 = B[m][n + 4];
                    t6 = B[m][n + 5];
                    t7 = B[m][n + 6];
                    t8 = B[m][n + 7];

                    B[m][n + 4] = t1;
                    B[m][n + 5] = t2;
                    B[m][n + 6] = t3;
                    B[m][n + 7] = t4;

                    int temp = m + 4;
                    B[temp][n] = t5;
                    B[temp][n + 1] = t6;
                    B[temp][n + 2] = t7;
                    B[temp][n + 3] = t8;
                }

                for (n = i + 4; n < i + 8; n += 2) {
                    m = j + 4;
                    t1 = A[n][m];
                    t2 = A[n][m + 1];
                    t3 = A[n][m + 2];
                    t4 = A[n][m + 3];

                    int temp = n + 1;
                    t5 = A[temp][m];
                    t6 = A[temp][m + 1];
                    t7 = A[temp][m + 2];
                    t8 = A[temp][m + 3];

                    B[m][n] = t1;
                    B[m + 1][n] = t2;
                    B[m + 2][n] = t3;
                    B[m + 3][n] = t4;

                    B[m][temp] = t5;
                    B[m + 1][temp] = t6;
                    B[m + 2][temp] = t7;
                    B[m + 3][temp] = t8;
                }
            }
        }
    } else if (M == 61 && N == 67) {
        int block_size = 16;
        for (int i = 0; i < N; i += block_size) {
            for (int j = 0; j < M; j += block_size) {
                for (n = i; n < i + block_size && n < N; n++) {
                    for (int m = j; m < j + block_size && m < M; m++) {
                        B[m][n] = A[n][m];
                    }
                }
            }
        }        
    } else {
        trans(M, N, A, B);
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}