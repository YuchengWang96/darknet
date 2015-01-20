#include "mini_blas.h"
#include "utils.h"

void gemm(int TA, int TB, int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float BETA,
        float *C, int ldc)
{
    gemm_cpu( TA,  TB,  M, N, K, ALPHA,A,lda, B, ldb,BETA,C,ldc);
}

void gemm_nn(int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float *C, int ldc)
{
    int i,j,k;
    for(i = 0; i < M; ++i){
        for(k = 0; k < K; ++k){
            register float A_PART = ALPHA*A[i*lda+k];
            for(j = 0; j < N; ++j){
                C[i*ldc+j] += A_PART*B[k*ldb+j];
            }
        }
    }
}

void gemm_nt(int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float *C, int ldc)
{
    int i,j,k;
    for(i = 0; i < M; ++i){
        for(j = 0; j < N; ++j){
            register float sum = 0;
            for(k = 0; k < K; ++k){
                sum += ALPHA*A[i*lda+k]*B[j*ldb + k];
            }
            C[i*ldc+j] += sum;
        }
    }
}

void gemm_tn(int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float *C, int ldc)
{
    int i,j,k;
    for(i = 0; i < M; ++i){
        for(k = 0; k < K; ++k){
            register float A_PART = ALPHA*A[k*lda+i];
            for(j = 0; j < N; ++j){
                C[i*ldc+j] += A_PART*B[k*ldb+j];
            }
        }
    }
}

void gemm_tt(int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float *C, int ldc)
{
    int i,j,k;
    for(i = 0; i < M; ++i){
        for(j = 0; j < N; ++j){
            register float sum = 0;
            for(k = 0; k < K; ++k){
                sum += ALPHA*A[i+k*lda]*B[k+j*ldb];
            }
            C[i*ldc+j] += sum;
        }
    }
}


void gemm_cpu(int TA, int TB, int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float BETA,
        float *C, int ldc)
{
    //printf("cpu: %d %d %d %d %d %f %d %d %f %d\n",TA, TB, M, N, K, ALPHA, lda, ldb, BETA, ldc);
    int i, j;
    for(i = 0; i < M; ++i){
        for(j = 0; j < N; ++j){
            C[i*ldc + j] *= BETA;
        }
    }
    if(!TA && !TB)
        gemm_nn(M, N, K, ALPHA,A,lda, B, ldb,C,ldc);
    else if(TA && !TB)
        gemm_tn(M, N, K, ALPHA,A,lda, B, ldb,C,ldc);
    else if(!TA && TB)
        gemm_nt(M, N, K, ALPHA,A,lda, B, ldb,C,ldc);
    else
        gemm_tt(M, N, K, ALPHA,A,lda, B, ldb,C,ldc);
}

#ifdef GPU

#include "opencl.h"
#include <math.h>

#ifdef CLBLAS
#include <clBLAS.h>
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifdef __APPLE__
#define BLOCK 1
#else
#define BLOCK 16
#endif

cl_kernel get_gemm_kernel()
{
    static int init = 0;
    static cl_kernel gemm_kernel;
    if(!init){
        gemm_kernel = get_kernel("src/gemm.cl", "gemm", "-D BLOCK=" STR(BLOCK) );
        init = 1;
    }
    return gemm_kernel;
}

cl_kernel get_gemm_nt_kernel()
{
    static int init = 0;
    static cl_kernel gemm_kernel;
    if(!init){
        gemm_kernel = get_kernel("src/gemm.cl", "gemm_nt", "-D BLOCK=" STR(BLOCK) );
        init = 1;
    }
    return gemm_kernel;
}

cl_kernel get_gemm_tn_kernel()
{
    static int init = 0;
    static cl_kernel gemm_kernel;
    if(!init){
        gemm_kernel = get_kernel("src/gemm.cl", "gemm_tn", "-D BLOCK=" STR(BLOCK) );
        init = 1;
    }
    return gemm_kernel;
}

cl_kernel get_gemm_nn_kernel()
{
    static int init = 0;
    static cl_kernel gemm_kernel;
    if(!init){
        gemm_kernel = get_kernel("src/gemm.cl", "gemm_nn", "-D BLOCK=" STR(BLOCK) );
        init = 1;
    }
    return gemm_kernel;
}

#define TILE 64
#define TILE_K 16
#define WPT 8
#define THREADS (TILE*TILE)/(WPT*WPT)

cl_kernel get_gemm_nn_fast_kernel()
{
    static int init = 0;
    static cl_kernel gemm_kernel;
    if(!init){
        gemm_kernel = get_kernel("src/gemm_fast.cl", "gemm_nn_fast", "-D TILE=" STR(TILE)
                                                                    " -cl-nv-verbose "
                                                                    " -D TILE_K=" STR(TILE_K)
                                                                    " -D WPT=" STR(WPT)
                                                                    " -D THREADS=" STR(THREADS));
        init = 1;
    }
    return gemm_kernel;
}

void gemm_ongpu(int TA, int TB, int M, int N, int K, float ALPHA, 
        cl_mem A_gpu, int lda, 
        cl_mem B_gpu, int ldb,
        float BETA,
        cl_mem C_gpu, int ldc)
{
    gemm_ongpu_offset(TA, TB, M, N, K, ALPHA, A_gpu, 0, lda, B_gpu, 0, ldb, BETA, C_gpu, 0, ldc);
}

void gemm_ongpu_fast(int TA, int TB, int M, int N, int K, float ALPHA, 
        cl_mem A_gpu, int lda, 
        cl_mem B_gpu, int ldb,
        float BETA,
        cl_mem C_gpu, int ldc)
{
    int a_off = 0;
    int b_off = 0;
    int c_off = 0;
    //printf("gpu: %d %d %d %d %d\n",TA, TB, M, N, K);
    cl_kernel      gemm_kernel = get_gemm_nn_fast_kernel();
    cl_command_queue queue = cl.queue;

    cl_uint i = 0;
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(TA), (void*) &TA);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(TB), (void*) &TB);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(M), (void*) &M);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(N), (void*) &N);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(K), (void*) &K);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(ALPHA), (void*) &ALPHA);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(A_gpu), (void*) &A_gpu);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(a_off), (void*) &a_off);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(lda), (void*) &lda);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(B_gpu), (void*) &B_gpu);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(b_off), (void*) &b_off);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(ldb), (void*) &ldb);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(BETA), (void*) &BETA);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(C_gpu), (void*) &C_gpu);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(c_off), (void*) &c_off);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(ldc), (void*) &ldc);
    check_error(cl);

    const size_t global_size[] = {THREADS*((N-1)/TILE + 1), (M-1)/TILE + 1};
    const size_t local_size[] = {THREADS, 1};

    cl.error = clEnqueueNDRangeKernel(queue, gemm_kernel, 2, 0, global_size, local_size, 0, 0, 0);
    check_error(cl);
}

void gemm_ongpu_offset(int TA, int TB, int M, int N, int K, float ALPHA, 
        cl_mem A_gpu, int a_off, int lda, 
        cl_mem B_gpu, int b_off, int ldb,
        float BETA,
        cl_mem C_gpu, int c_off, int ldc)
{
#ifdef CLBLAS
    cl_command_queue queue = cl.queue;
    cl_event event;
    cl.error = clblasSgemm(clblasRowMajor, TA?clblasTrans:clblasNoTrans, TB?clblasTrans:clblasNoTrans,M, N, K,ALPHA, A_gpu, a_off, lda,B_gpu, b_off, ldb,BETA, C_gpu, c_off, ldc,1, &queue, 0, NULL, &event);
    check_error(cl);
#else
    //printf("gpu: %d %d %d %d %d\n",TA, TB, M, N, K);
    cl_kernel      gemm_kernel = get_gemm_kernel();
    if(!TA && !TB) gemm_kernel = get_gemm_nn_kernel();
    if(!TA && TB)  gemm_kernel = get_gemm_nt_kernel();
    if(TA && !TB)  gemm_kernel = get_gemm_tn_kernel();
    cl_command_queue queue = cl.queue;

    cl_uint i = 0;
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(TA), (void*) &TA);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(TB), (void*) &TB);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(M), (void*) &M);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(N), (void*) &N);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(K), (void*) &K);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(ALPHA), (void*) &ALPHA);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(A_gpu), (void*) &A_gpu);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(a_off), (void*) &a_off);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(lda), (void*) &lda);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(B_gpu), (void*) &B_gpu);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(b_off), (void*) &b_off);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(ldb), (void*) &ldb);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(BETA), (void*) &BETA);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(C_gpu), (void*) &C_gpu);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(c_off), (void*) &c_off);
    cl.error = clSetKernelArg(gemm_kernel, i++, sizeof(ldc), (void*) &ldc);
    check_error(cl);

    const size_t global_size[] = {ceil((float)N/BLOCK)*BLOCK, ceil((float)M/BLOCK)*BLOCK};
    const size_t local_size[] = {BLOCK, BLOCK};

    cl.error = clEnqueueNDRangeKernel(queue, gemm_kernel, 2, 0, global_size, local_size, 0, 0, 0);
    check_error(cl);
#endif
}

void gemm_gpu(int TA, int TB, int M, int N, int K, float ALPHA, 
        float *A, int lda, 
        float *B, int ldb,
        float BETA,
        float *C, int ldc)
{
    cl_context context = cl.context;
    cl_command_queue queue = cl.queue;

    size_t size = sizeof(float)*(TA ? lda*K:lda*M);
    cl_mem A_gpu = clCreateBuffer(context,
            CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,
            size, A, &cl.error);
    check_error(cl);

    size = sizeof(float)*(TB ? ldb*N:ldb*K);
    cl_mem B_gpu = clCreateBuffer(context,
            CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,
            size, B, &cl.error);
    check_error(cl);

    size = sizeof(float)*(ldc*M);
    cl_mem C_gpu = clCreateBuffer(context,
            CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,
            size, C, &cl.error);
    check_error(cl);

    // TODO
    //gemm_ongpu(TA, TB, M, N, K, ALPHA, A_gpu, lda, B_gpu, ldb, BETA, C_gpu, ldc);
    gemm_ongpu_fast(TA, TB, M, N, K, ALPHA, A_gpu, lda, B_gpu, ldb, BETA, C_gpu, ldc);

    clEnqueueReadBuffer(queue, C_gpu, CL_TRUE, 0, size, C, 0, 0, 0);
    check_error(cl);

    clReleaseMemObject(A_gpu);
    clReleaseMemObject(B_gpu);
    clReleaseMemObject(C_gpu);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void time_gpu_random_matrix(int TA, int TB, int m, int k, int n)
{
    float *a;
    if(!TA) a = random_matrix(m,k);
    else a = random_matrix(k,m);
    int lda = (!TA)?k:m;
    float *b;
    if(!TB) b = random_matrix(k,n);
    else b = random_matrix(n,k);
    int ldb = (!TB)?n:k;

    float *c = random_matrix(m,n);
    int i;
    clock_t start = clock(), end;
    for(i = 0; i<32; ++i){
        gemm_gpu(TA,TB,m,n,k,1,a,lda,b,ldb,1,c,n);
    }
    end = clock();
    printf("Matrix Multiplication %dx%d * %dx%d, TA=%d, TB=%d: %lf s\n",m,k,k,n, TA, TB, (float)(end-start)/CLOCKS_PER_SEC);
    free(a);
    free(b);
    free(c);
}

void time_ongpu(int TA, int TB, int m, int k, int n)
{
    int iter = 10;
    float *a = random_matrix(m,k);
    float *b = random_matrix(k,n);

    int lda = (!TA)?k:m;
    int ldb = (!TB)?n:k;

    float *c = random_matrix(m,n);

    cl_mem a_cl = cl_make_array(a, m*k);
    cl_mem b_cl = cl_make_array(b, k*n);
    cl_mem c_cl = cl_make_array(c, m*n);

    int i;
    clock_t start = clock(), end;
    for(i = 0; i<iter; ++i){
        gemm_ongpu(TA,TB,m,n,k,1,a_cl,lda,b_cl,ldb,1,c_cl,n);
    }
    double flop = ((double)m)*n*(2.*k + 2.)*iter;
    double gflop = flop/pow(10., 9);
    end = clock();
    double seconds = sec(end-start);
    printf("Matrix Multiplication %dx%d * %dx%d, TA=%d, TB=%d: %lf s, %lf GFLOPS\n",m,k,k,n, TA, TB, seconds, gflop/seconds);
    clReleaseMemObject(a_cl);
    clReleaseMemObject(b_cl);
    clReleaseMemObject(c_cl);
    free(a);
    free(b);
    free(c);
}

void time_ongpu_fast(int TA, int TB, int m, int k, int n)
{
    int iter = 10;
    float *a = random_matrix(m,k);
    float *b = random_matrix(k,n);

    int lda = (!TA)?k:m;
    int ldb = (!TB)?n:k;

    float *c = random_matrix(m,n);

    cl_mem a_cl = cl_make_array(a, m*k);
    cl_mem b_cl = cl_make_array(b, k*n);
    cl_mem c_cl = cl_make_array(c, m*n);

    int i;
    clock_t start = clock(), end;
    for(i = 0; i<iter; ++i){
        gemm_ongpu_fast(TA,TB,m,n,k,1,a_cl,lda,b_cl,ldb,1,c_cl,n);
    }
    double flop = ((double)m)*n*(2.*k + 2.)*iter;
    double gflop = flop/pow(10., 9);
    end = clock();
    double seconds = sec(end-start);
    printf("Fast   Multiplication %dx%d * %dx%d, TA=%d, TB=%d: %lf s, %lf GFLOPS\n",m,k,k,n, TA, TB, seconds, gflop/seconds);
    clReleaseMemObject(a_cl);
    clReleaseMemObject(b_cl);
    clReleaseMemObject(c_cl);
    free(a);
    free(b);
    free(c);
}

void test_gpu_accuracy(int TA, int TB, int m, int k, int n)
{
    srand(0);
    float *a;
    if(!TA) a = random_matrix(m,k);
    else a = random_matrix(k,m);
    int lda = (!TA)?k:m;
    float *b;
    if(!TB) b = random_matrix(k,n);
    else b = random_matrix(n,k);
    int ldb = (!TB)?n:k;

    float *c = random_matrix(m,n);
    float *c_gpu = random_matrix(m,n);
    memset(c, 0, m*n*sizeof(float));
    memset(c_gpu, 0, m*n*sizeof(float));
    int i;
    //pm(m,k,b);
    gemm_gpu(TA,TB,m,n,k,1,a,lda,b,ldb,1,c_gpu,n);
    //printf("GPU\n");
    //pm(m, n, c_gpu);
    gemm_cpu(TA,TB,m,n,k,1,a,lda,b,ldb,1,c,n);
    //printf("\n\nCPU\n");
    //pm(m, n, c);
    double sse = 0;
    for(i = 0; i < m*n; ++i) {
        //printf("%f %f\n", c[i], c_gpu[i]);
        sse += pow(c[i]-c_gpu[i], 2);
    }
    printf("Matrix Multiplication %dx%d * %dx%d, TA=%d, TB=%d: %g SSE\n",m,k,k,n, TA, TB, sse/(m*n));
    free(a);
    free(b);
    free(c);
    free(c_gpu);
}

void test_gpu_blas()
{
    /*
       test_gpu_accuracy(0,0,10,576,75); 

       test_gpu_accuracy(0,0,17,10,10); 
       test_gpu_accuracy(1,0,17,10,10); 
       test_gpu_accuracy(0,1,17,10,10); 
       test_gpu_accuracy(1,1,17,10,10); 

       test_gpu_accuracy(0,0,1000,10,100); 
       test_gpu_accuracy(1,0,1000,10,100); 
       test_gpu_accuracy(0,1,1000,10,100); 
       test_gpu_accuracy(1,1,1000,10,100); 
     */

    test_gpu_accuracy(0,0,128,128,128); 

/*
    time_ongpu(0,0,64,2916,363); 
    time_ongpu_fast(0,0,64,2916,363); 
    time_ongpu(0,0,64,2916,363); 
    time_ongpu_fast(0,0,64,2916,363); 
    time_ongpu(0,0,64,2916,363); 
    time_ongpu_fast(0,0,64,2916,363); 
    time_ongpu(0,0,192,729,1600); 
    time_ongpu_fast(0,0,192,729,1600); 
    time_ongpu(0,0,384,196,1728); 
    time_ongpu_fast(0,0,384,196,1728); 
    time_ongpu(0,0,256,196,3456); 
    time_ongpu_fast(0,0,256,196,3456); 
    time_ongpu(0,0,256,196,2304); 
    time_ongpu_fast(0,0,256,196,2304); 
    time_ongpu(0,0,128,4096,12544); 
    time_ongpu_fast(0,0,128,4096,12544); 
    time_ongpu(0,0,128,4096,4096); 
    time_ongpu_fast(0,0,128,4096,4096); 
    */
//    time_ongpu(1,0,2304,196,256); 
//    time_ongpu_fast(1,0,2304,196,256); 
//    time_ongpu(0,1,256,2304,196); 
//    time_ongpu_fast(0,1,256,2304,196); 

    time_ongpu(0,0,2048,2048,2048); 
    time_ongpu_fast(0,0,2048,2048,2048); 
    time_ongpu(0,0,2048,2048,2048); 
    time_ongpu_fast(0,0,2048,2048,2048); 
    time_ongpu(0,0,2048,2048,2048); 
    time_ongpu_fast(0,0,2048,2048,2048); 

    /*
       test_gpu_accuracy(0,0,131,4093,1199); 
       test_gpu_accuracy(0,1,131,4093,1199); 
       test_gpu_accuracy(1,0,131,4093,1199); 
       test_gpu_accuracy(1,1,131,4093,1199); 
     */
    /*

       time_ongpu(0,0,1024,1024,1024); 
       time_ongpu(0,1,1024,1024,1024); 
       time_ongpu(1,0,1024,1024,1024); 
       time_ongpu(1,1,1024,1024,1024); 

       time_ongpu(0,0,128,4096,1200); 
       time_ongpu(0,1,128,4096,1200); 
       time_ongpu(1,0,128,4096,1200); 
       time_ongpu(1,1,128,4096,1200); 
     */

    /*
       time_gpu_random_matrix(0,0,1000,1000,100); 
       time_random_matrix(0,0,1000,1000,100); 

       time_gpu_random_matrix(0,1,1000,1000,100); 
       time_random_matrix(0,1,1000,1000,100); 

       time_gpu_random_matrix(1,0,1000,1000,100); 
       time_random_matrix(1,0,1000,1000,100); 

       time_gpu_random_matrix(1,1,1000,1000,100); 
       time_random_matrix(1,1,1000,1000,100); 
     */

}
#endif

