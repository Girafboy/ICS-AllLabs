/* 
 * Name: Kang Huquan
 * LoginID: ics517021910839
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
	int i,j,k,temp,flag=0;
	/*
	 * M=32,N=32
	 * number of used local variable is 5
	 *
	 *          j-------->
	 *     j=0      j=1      j=2      j=3
	 *   k-->     k-->     k-->     k-->
	 * i -------- -------- -------- --------
	 * | -------- -------- -------- --------
	 * V ...
	 *   -------- -------- -------- --------
	 *
	 * When encounter diagonal element, record it into temp and fill it after read a line.
	 * Variable flag indicate whether a diagonal element has been recorded.
	 */
	if(M==32 && N==32){
		for(j=0; j<M/8; j++)
			for(i=0; i<N; i++){
				for(k=0; k<8; k++)
					if(i==j*8+k && j==i/8){
						temp = A[i][i];
						flag = 1;
					}
					else
						B[j*8+k][i] = A[i][j*8+k];
				if(flag){
					B[i][i] = temp;
					flag = 0;
				}
			}
	}
	/*
	 * M=64,N=64
	 * number of used local variable is 9
	 *
	 *          j-------->
	 *     j=0      j=1      j=2          j=7
	 *   k-->     k-->     k-->         k-->
	 * i -------- -------- -------- ... --------
	 * | -------- -------- -------- ... --------
	 * V ...
	 *   -------- -------- -------- ... --------
	 *
	 * For diagonal Block 8x8:
	 *     step1  step2
	 * buf1 ----  ----
	 *  |   ----  ----
	 *  V   ...
	 *      ----  ----
	 * When encounter diagonal element, record it into temp and fill it after read a line.
	 * Variable flag indicate whether a diagonal element has been recorded.
	 * 
	 * For other Block 8x8:
	 * From:
	 *	1 1 1 1 1 1 1 1
	 *	2 2 2 2 2 2 2 2
	 *	3 3 3 3 3 3 3 3
	 *      4 4 4 4 4 4 4 4
	 *      5 6 7 8 9 a b c
	 *      5 6 7 8 9 a b c
	 *      5 6 7 8 9 a b c
	 *      5 6 7 8 9 a b c
	 * To:
	 *	1 2 3 4 1 2 3 4
	 *	1 2 3 4 1 2 3 4
	 *	1 2 3 4 1 2 3 4
	 *      1 2 3 4 1 2 3 4
	 *      - - - - - - - -
	 *      - - - - - - - -
	 *      - - - - - - - -
	 *      - - - - - - - -
	 * To:
	 *	1 2 3 4 5 5 5 5 (buf1=1 buf2=2 buf3=3 buf4=4)
	 *	1 2 3 4 1 2 3 4
	 *	1 2 3 4 1 2 3 4
	 *      1 2 3 4 1 2 3 4
	 *      1 2 3 4 - - - - (copy from buf)
	 *      - - - - - - - -
	 *      - - - - - - - -
	 *      - - - - - - - -
	 * To:
	 *	1 2 3 4 5 5 5 5 
	 *	1 2 3 4 6 6 6 6
	 *	1 2 3 4 7 7 7 7
	 *      1 2 3 4 8 8 8 8
	 *      1 2 3 4 9 9 9 9
	 *      1 2 3 4 a a a a
	 *      1 2 3 4 b b b b
	 *      1 2 3 4 c c c c
	 *
	 */
	else if(M==64 && N==64){
		int buf1,buf2,buf3,buf4;
		for(j=0; j<M/8; j++)
			for(i=0; i<N; i++){
				if(j==i/8){
					for(buf1=i; buf1<i+8; buf1++){
						for(k=0; k<4; k++)
							if(buf1==j*8+k && j==buf1/8){
								temp = A[buf1][buf1];
								flag = 1;
							}
							else
				                		B[j*8+k][buf1] = A[buf1][j*8+k]; 
						if(flag){
							B[buf1][buf1] = temp;
							flag = 0;
						}
					}
					for(buf1=i; buf1<i+8; buf1++){
			                        for(k=4; k<8; k++)
							 if(buf1==j*8+k && j==buf1/8){
					                         temp = A[buf1][buf1];
								 flag = 1;
	                                                 }
                                                         else
								 B[j*8+k][buf1] = A[buf1][j*8+k];
						if(flag){
							 B[buf1][buf1] = temp;
	                                                 flag = 0;
	                                    	}
		                       	}
					i += 7;
				}
				else
					if(i%8<4){
						for(k=0; k<4; k++)
							B[j*8+k][i] = A[i][j*8+k];
						for(k=4; k<8; k++)
							B[j*8+k-4][i+4] = A[i][j*8+k];
						if(i%8==3){
							for(k=0; k<4; k++){
								buf1 = B[j*8+k][i+1];
								buf2 = B[j*8+k][i+2];
								buf3 = B[j*8+k][i+3];
								buf4 = B[j*8+k][i+4];
								B[j*8+k][i+1] = A[i+1][j*8+k];
								B[j*8+k][i+2] = A[i+2][j*8+k];
								B[j*8+k][i+3] = A[i+3][j*8+k];
								B[j*8+k][i+4] = A[i+4][j*8+k];
								B[j*8+k+4][i-3] = buf1;
								B[j*8+k+4][i-2] = buf2;
								B[j*8+k+4][i-1] = buf3;
								B[j*8+k+4][i] = buf4;
							}
							for(k=4; k<8; k++){
								B[j*8+k][i+1] = A[i+1][j*8+k];
								B[j*8+k][i+2] = A[i+2][j*8+k];
								B[j*8+k][i+3] = A[i+3][j*8+k];
								B[j*8+k][i+4] = A[i+4][j*8+k];
							}
						}
					}
			}
	}
	/*
	 * M=61,N=67 or M=67,N=61
	 * number of used local variable is 3
	 * 
	 * The algorithm is very simple:
	 * 	just do it one line by one line.
	 * 	The untidy matrix will make cache fully used automatically.
	 *
	 *          j-------->
	 *     j=0      j=1      j=2          j=?
	 *   k-->     k-->     k-->         k-->
	 * i -------- -------- -------- ... -----
	 * | -------- -------- -------- ... -----
	 * V ...
	 *   -------- -------- -------- ... -----
	 * 
	 */
	else if(M==61 && N==67){
		for(j=0 ;j<7 ;j++)
			for(i=0; i<N; i++)
				for(k=0; k<8; k++)
					B[j*8+k][i] = A[i][j*8+k];
		j=7;
		for(i=0; i<N; i++)
			for(k=0; k<5; k++)
				B[j*8+k][i] = A[i][j*8+k];
	}
	else if(M==67 && N==61){
		for(j=0 ;j<8 ;j++)
			for(i=0; i<N; i++)
				for(k=0; k<8; k++)
					B[j*8+k][i] = A[i][j*8+k];
		j=8;
		for(i=0; i<N; i++)
			for(k=0; k<3; k++)
				B[j*8+k][i] = A[i][j*8+k];
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

