// Copyright 2020 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matteo Perotti


//vmv Vector Register Move.
//vle64 Vector Load Element. It's used for loading 64-bit elements from memory into a vector register.
//vfmul Vector Floating-Point Multiply
//vfmacc Vector Floating-Point Multiply-Add with a scalar or vector. It multiplies each element in a source
      //vector by a scalar or another vector and adds the product to the corresponding element in another source vector.
//fld Floating-Point Load Double. It loads a 64-bit double-precision floating-point value into a floating-point register.
//vslidedown  It shifts elements down in a vector register, and the vacated positions at the top are filled with zeroes or a specified value.
//vsetvli Set Vector Length and Vector Configuration. It sets the vector length and the type of the elements for subsequent vector operations.
//vse64 Vector Store Element. It stores 64-bit elements from a vector register to memory.


#include "fconv2d.h"
      //o = output matrix i = input matrix f = filter or kernel R = rows C = columns F = Filter's dimension with a stride or padding
void fconv2d_3x3(double *o, double *i, double *f, int64_t R, int64_t C,
                 int64_t F) {
  // We work on 4 rows of the output matrix at once
  int64_t block_size_o = 4;
  // We work on block_size_o + F - 1 rows of the input matrix at once

  // First iteration round, r = 0
  double *i_ = i;
  double *o_ = o;

  // Preload the first two input rows -> This is not needed in the other rounds
  fconv2d_vec_4xC_slice_preload_3x3(i_, C, F);
  // The first F-1 rows have already been loaded by
  // fconv2d_vec_4xC_slice_preload_3x3()
  double *i__ = i_ + (F - 1) * (C + F - 1);
  fconv2d_vec_4xC_3x3(o_, i__, f, C, F);
  // Re-use some of the already-loaded input rows
  fconv2d_vec_4xC_slice_move_3x3(C, F);

  i_ = i + block_size_o * (C + F - 1);
  i__ = i_ + (F - 1) * (C + F - 1);

  int64_t ldi = (C + F - 1) << 3;
  int64_t ldf = F << 3;

  // Temporary variables
  double t0, t1, t2;
  // Helper variables
  double *f_;
  f_ = f;
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t0)
               : "r"(ldf));
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t1)
               : "r"(ldf));
  asm volatile("fld %1, (%0);" : "+&r"(f_), "=&f"(t2));
//Loading a value from memory (f_) into a floating-point register (t0).
//Advancing the memory pointer (f_) by some offset (ldf).
//Repeating the two steps for another floating-point register (t1).
//Loading one more value from the advanced memory pointer (f_) into a third floating-point register (t2).


  // Iterate over the output rows
  for (int64_t r = block_size_o; r < R; r += block_size_o) {
    //The loop is iterating over blocks of rows, starting from block_size_o and incrementing 
    //by block_size_o each iteration until it reaches R. Within each iteration of the loop, it 
    //seems to be performing a portion of the 2D convolution for that specific block of rows.
    
    // The first F-1 rows have already been loaded by
    // fconv2d_vec_4xC_slice_init()

    double t3, t4, t5;

    // Fetch C + F - 1 elements (padding included)
    asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(C + F - 1));
            //This sets the vector length to C + F - 1. The vsetvli instruction sets the vector length 
            //to the smallest legal value greater or equal to the given vl that doesn’t exceed the
            //architectural maximum length.
            //zero is a RISC-V register, always holding the value 0. It’s being used here as the 
            //destination register for the vsetvli instruction, essentially discarding the
            //returned value (which is the previous vector length).
            //%0 is a placeholder for the first output operand (described below). It's where the C + F - 1
            //value will be inserted when the assembly code is executed.
            //e64 specifies the element width for the vector operations. In this case, it is 64 bits.
            //m2 is a vector register configuration option that specifies the operating mode for 
            //the masking registers.
            //ta and ma are the type agnostic and mask agnostic options, respectively, to tell 
            //the instruction that it does not change its behavior depending on the element 
            //type and does not require a mask register.
            //This is an operand, specifying both an input and an output (due to the +).
            //r means the operand is a register operand.
            //C + F - 1 is a calculation done in C/C++ and then passed into the assembly language code at %0.
    f_ = f;

    // Fetch the first column of the filter, and start calculating its
    // contribution on the four output rows (v0, v2, v4, v6)

    // Fetch 4 + F - 1 - 2 rows of the input matrix
    // Compute on C + F - 1 elements, instead of C elements, to cover the
    // latency of the load instructions
    asm volatile("vmv.v.v v8, v16");
            //This instruction copies data from one vector register to another (v16 to v8), 
            //likely part of a preparation step for processing the current block of rows.
    asm volatile("vle64.v v12, (%0); add %0, %0, %1" : "+&r"(i__) : "r"(ldi));
    asm volatile("vfmul.vf v0, v8, %0" ::"f"(t0));
            //data is loaded from memory into a vector register and then processed. It loads 64-bit elements from memory 
            //into vector register v12 and then multiplies elements in vector register v8 by a scalar value t0, 
            //storing the result in vector register v0.


    asm volatile("vmv.v.v v10, v18");
    asm volatile("vfmul.vf v2, v10, %0" ::"f"(t0));
    asm volatile("vle64.v v14, (%0); add %0, %0, %1" : "+&r"(i__) : "r"(ldi));
    asm volatile("vfmacc.vf v0, %0, v10" ::"f"(t1));
            //These are part of the core computation, performing multiply-accumulate operations using elements 
            //in vector registers and a scalar value, effectively calculating part of the convolution.

    asm volatile("vfmacc.vf v2, %0, v12" ::"f"(t1));
    asm volatile("vle64.v v16, (%0); add %0, %0, %1" : "+&r"(i__) : "r"(ldi));
    asm volatile("vfmacc.vf v0, %0, v12" ::"f"(t2));
    asm volatile("vslidedown.vi v20, v8,  1");
            //This instruction slides elements within a vector register down by a given immediate value. 
            //It's likely part of aligning data for the next step of the convolution operation.
    
    asm volatile("vfmul.vf v4, v12, %0" ::"f"(t0));

    asm volatile("vle64.v v18, (%0); add %0, %0, %1" : "+&r"(i__) : "r"(ldi));

    asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(C));

    asm volatile("vfmul.vf v6, v14, %0" ::"f"(t0));
    asm volatile("vslidedown.vi v22, v10, 1");
    asm volatile("vfmacc.vf v4, %0, v14" ::"f"(t1));
    asm volatile("vfmacc.vf v2, %0, v14" ::"f"(t2));
    asm volatile("vslidedown.vi v24, v12, 1");

    asm volatile("vfmacc.vf v6, %0, v16" ::"f"(t1));
    asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t2));

    asm volatile("vslidedown.vi v26, v14, 1");

    asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t2));

    f_ = f + 1;
    // Fetch the middle column of the filter, and start calculating its
    // contributions on the output rows To do so, slide down the input rows by
    // one
    asm volatile("fld %1, (%0); add %0, %0, %2"
                 : "+&r"(f_), "=&f"(t3)
                 : "r"(ldf));
    asm volatile("fld %1, (%0); add %0, %0, %2"
                 : "+&r"(f_), "=&f"(t4)
                 : "r"(ldf));
    asm volatile("fld %1, (%0);" : "+&r"(f_), "=&f"(t5));

    asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t3));

    asm volatile("vfmacc.vf v0, %0, v22" ::"f"(t4));
    asm volatile("vslidedown.vi v28, v16, 1");
    asm volatile("vfmacc.vf v2, %0, v22" ::"f"(t3));

    i_ = i + (r + block_size_o) * (C + F - 1);
    asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t5));
    asm volatile("vslidedown.vi v30, v18, 1");
    asm volatile("vfmacc.vf v2, %0, v24" ::"f"(t4));
    asm volatile("vfmacc.vf v4, %0, v24" ::"f"(t3));
    asm volatile("vslidedown.vi v20, v8,  2");

    asm volatile("vfmacc.vf v2, %0, v26" ::"f"(t5));
    asm volatile("vfmacc.vf v4, %0, v26" ::"f"(t4));
    asm volatile("vslidedown.vi v22, v10, 2");
    asm volatile("vfmacc.vf v6, %0, v26" ::"f"(t3));
    i__ = i_ + (F - 1) * (C + F - 1);

    asm volatile("vfmacc.vf v4, %0, v28" ::"f"(t5));
    f_ = f + 2;
    asm volatile("fld %1, (%0); add %0, %0, %2"
                 : "+&r"(f_), "=&f"(t3)
                 : "r"(ldf));
    asm volatile("vfmacc.vf v6, %0, v28" ::"f"(t4));
    asm volatile("vslidedown.vi v24, v12, 2");

    asm volatile("vfmacc.vf v6, %0, v30" ::"f"(t5));
    asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t3));
    asm volatile("vslidedown.vi v26, v14, 2");

    // Repeat for the last filter column, and then store the output rows
    asm volatile("fld %1, (%0); add %0, %0, %2"
                 : "+&r"(f_), "=&f"(t4)
                 : "r"(ldf));
    asm volatile("fld %1, (%0);" : "+&r"(f_), "=&f"(t5));

    asm volatile("vfmacc.vf v0, %0, v22" ::"f"(t4));
    o_ = o + r * C;

    // Compute on C elements
    int64_t ldo = C << 3;
    asm volatile("vfmacc.vf v2, %0, v22" ::"f"(t3));
    asm volatile("vslidedown.vi v28, v16, 2");

    asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t5));
    asm volatile("vfmacc.vf v2, %0, v24" ::"f"(t4));
    asm volatile("vslidedown.vi v30, v18, 2");
    asm volatile("vse64.v  v0, (%0); add %0, %0, %1" : "+&r"(o_) : "r"(ldo));
    asm volatile("vfmacc.vf v4, %0, v24" ::"f"(t3));

    asm volatile("vfmacc.vf v2, %0, v26" ::"f"(t5));
    asm volatile("vse64.v  v2, (%0); add %0, %0, %1" : "+&r"(o_) : "r"(ldo));
    asm volatile("vfmacc.vf v4, %0, v26" ::"f"(t4));
    asm volatile("vfmacc.vf v6, %0, v26" ::"f"(t3));

    asm volatile("vfmacc.vf v4, %0, v28" ::"f"(t5));
    asm volatile("vse64.v  v4, (%0); add %0, %0, %1" : "+&r"(o_) : "r"(ldo));
    asm volatile("vfmacc.vf v6, %0, v28" ::"f"(t4));

    asm volatile("vfmacc.vf v6, %0, v30" ::"f"(t5));
    asm volatile("vse64.v  v6, (%0);" : "+r"(o_));
  }
}

// Load 4 rows of the output matrix
void fconv2d_vec_4xC_slice_preload_3x3(double *i, int64_t C, int64_t F) {
  // Helper variables
  int64_t ldi = (C + F - 1) << 3;

  // Set the vector configuration
  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(C + F - 1));
  // Fetch the first floor(F/2) + 1 input rows
  asm volatile("vle64.v v8,  (%0); add %0, %0, %1" : "+&r"(i) : "r"(ldi));
  asm volatile("vle64.v v10, (%0); add %0, %0, %1" : "+r"(i));
}

// Calculate 4 output matrix rows
void fconv2d_vec_4xC_3x3(double *o, double *i, double *f, int64_t C,
                         int64_t F) {

  // Temporary variables to hold the filter coefficients during each iteration
  double t0, t1, t2;

  // Helper variables
  //ldo ldi ldf are offsets for iterating through the output, input, and filter matrices.
  int64_t ldo = C << 3; 
  int64_t ldi = (C + F - 1) << 3;
  int64_t ldf = F << 3;
  double *f_; //A temporary pointer for iterating through the filter matrix.

  // Fetch C + F - 1 elements (padding included)
  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(C + F - 1));
      // this line is used to set the vector length and configuration for subsequent vector operations
  f_ = f;
  // Fetch the first column of the filter, and start calculating its
  // contribution on the four output rows (v0, v2, v4, v6)
  asm volatile("fld %1, (%0); add %0, %0, %2" //fld is used to load a floating-point value from the memory address pointed to by (%0) into the floating-point register %1. Add is used to update the memory address pointer after the load operation.
               : "+&r"(f_), "=&f"(t0)
        //The + indicates that f_ is a read-write operand. & specifies that this operand is early-clobbered, meaning it should not be used as an input operand for another instruction
        // in this inline assembly block. "r" means that the operand is held in a general-purpose register. f_ is the C variable (a pointer) associated with the operand.
        //"=&f"(t0): =& indicates a write-only operand that is also early-clobbered. "f" denotes that the operand is held in a floating-point register. 
        //t0 is a temporary floating-point variable in the C code that will hold the loaded value.
               : "r"(ldf));//Specifies an input operand held in a general-purpose register. 
                           //ldf is a C variable that is used as an operand for the add instruction.

      //Loads a floating-point value from the memory address pointed to by f_ into the floating-point variable t0.
      //Then it increments the f_ pointer by ldf to point to the next element in the memory.
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t1)
               : "r"(ldf));
  asm volatile("fld %1, (%0);" : "+&r"(f_), "=&f"(t2));
      //Another floating-point value is loaded from the updated memory address into the floating-point variable t2.
      //This time, there is no addition operation to increment the pointer.

//In many signal processing or image processing algorithms like convolution, multiple elements of a filter kernel are 
//used to perform calculations on the input data. Each element of the filter kernel needs to be loaded into a separate 
//variable or register so that it can be used in parallel calculations or operations. So, t0, t1, and t2 are likely 
//being used to hold different coefficients of a filter kernel that are used simultaneously in the subsequent computations.
//In short, each t0, t1, and t2 are holding different values that are fetched sequentially from the memory. 
//These values are then likely used in parallel to perform computations, which is a common approach to optimize performance in such algorithms.

  // Fetch 4 + F - 1 - 2 rows of the input matrix
  // Compute on C + F - 1 elements, instead of C elements, to cover the latency
  // of the load instructions
  asm volatile("vle64.v v12, (%0); add %0, %0, %1" : "+&r"(i) : "r"(ldi));
  asm volatile("vfmul.vf v0, v8, %0" ::"f"(t0));

  asm volatile("vfmul.vf v2, v10, %0" ::"f"(t0));
  asm volatile("vle64.v v14, (%0); add %0, %0, %1" : "+&r"(i) : "r"(ldi));
  asm volatile("vfmacc.vf v0, %0, v10" ::"f"(t1));

  asm volatile("vfmacc.vf v2, %0, v12" ::"f"(t1));
  asm volatile("vle64.v v16, (%0); add %0, %0, %1" : "+&r"(i) : "r"(ldi));
  asm volatile("vfmacc.vf v0, %0, v12" ::"f"(t2));
  asm volatile("vslidedown.vi v20, v8,  1");
  asm volatile("vfmul.vf v4, v12, %0" ::"f"(t0));

  asm volatile("vle64.v v18, (%0); add %0, %0, %1" : "+&r"(i) : "r"(ldi));

  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(C));

  asm volatile("vfmul.vf v6, v14, %0" ::"f"(t0));
  asm volatile("vfmacc.vf v4, %0, v14" ::"f"(t1));
  asm volatile("vslidedown.vi v22, v10, 1");
  asm volatile("vfmacc.vf v2, %0, v14" ::"f"(t2));

  asm volatile("vfmacc.vf v6, %0, v16" ::"f"(t1));
  asm volatile("vfmacc.vf v4, %0, v16" ::"f"(t2));

  asm volatile("vslidedown.vi v24, v12, 1");
  asm volatile("vfmacc.vf v6, %0, v18" ::"f"(t2));

  f_ = f + 1;
  // Fetch the middle column of the filter, and start calculating its
  // contributions on the output rows To do so, slide down the input rows by one
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t0)
               : "r"(ldf));
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t1)
               : "r"(ldf));
  asm volatile("fld %1, (%0);" : "+&r"(f_), "=&f"(t2));

  asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));

  asm volatile("vfmacc.vf v0, %0, v22" ::"f"(t1));
  asm volatile("vslidedown.vi v26, v14, 1");
  asm volatile("vfmacc.vf v2, %0, v22" ::"f"(t0));

  asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t2));
  asm volatile("vfmacc.vf v2, %0, v24" ::"f"(t1));
  asm volatile("vslidedown.vi v28, v16, 1");
  asm volatile("vfmacc.vf v4, %0, v24" ::"f"(t0));

  asm volatile("vfmacc.vf v2, %0, v26" ::"f"(t2));
  asm volatile("vfmacc.vf v4, %0, v26" ::"f"(t1));
  asm volatile("vslidedown.vi v30, v18, 1");
  asm volatile("vfmacc.vf v6, %0, v26" ::"f"(t0));

  asm volatile("vfmacc.vf v4, %0, v28" ::"f"(t2));
  asm volatile("vslidedown.vi v20, v8,  2");
  asm volatile("vfmacc.vf v6, %0, v28" ::"f"(t1));

  asm volatile("vfmacc.vf v6, %0, v30" ::"f"(t2));
  asm volatile("vslidedown.vi v22, v10, 2");

  f_ = f + 2;
  // Repeat for the last filter column, and then store the output rows
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t0)
               : "r"(ldf));
  asm volatile("fld %1, (%0); add %0, %0, %2"
               : "+&r"(f_), "=&f"(t1)
               : "r"(ldf));
  asm volatile("fld %1, (%0);" : "+&r"(f_), "=&f"(t2));

  asm volatile("vfmacc.vf v0, %0, v20" ::"f"(t0));

  asm volatile("vfmacc.vf v0, %0, v22" ::"f"(t1));
  asm volatile("vslidedown.vi v24, v12, 2");
  asm volatile("vfmacc.vf v2, %0, v22" ::"f"(t0));

  // Compute on C elements

  asm volatile("vfmacc.vf v0, %0, v24" ::"f"(t2));
  asm volatile("vse64.v  v0, (%0); add %0, %0, %1" : "+&r"(o) : "r"(ldo));
  asm volatile("vslidedown.vi v26, v14, 2");
  asm volatile("vfmacc.vf v2, %0, v24" ::"f"(t1));
  asm volatile("vfmacc.vf v4, %0, v24" ::"f"(t0));

  asm volatile("vfmacc.vf v2, %0, v26" ::"f"(t2));
  asm volatile("vse64.v  v2, (%0); add %0, %0, %1" : "+&r"(o) : "r"(ldo));
  asm volatile("vslidedown.vi v28, v16, 2");
  asm volatile("vfmacc.vf v4, %0, v26" ::"f"(t1));
  asm volatile("vfmacc.vf v6, %0, v26" ::"f"(t0));

  asm volatile("vfmacc.vf v4, %0, v28" ::"f"(t2));
  asm volatile("vslidedown.vi v30, v18, 2");
  asm volatile("vse64.v  v4, (%0); add %0, %0, %1" : "+&r"(o) : "r"(ldo));
  asm volatile("vfmacc.vf v6, %0, v28" ::"f"(t1));

  asm volatile("vfmacc.vf v6, %0, v30" ::"f"(t2));
  asm volatile("vse64.v  v6, (%0);" : "+r"(o));
}

void fconv2d_vec_4xC_slice_move_3x3(int64_t C, int64_t F) {
  // Move C+F-1 elements
  asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(C + F - 1));
  // Move the last floor(F/2) + 1 input rows
  asm volatile("vmv.v.v v8, v16");
  asm volatile("vmv.v.v v10, v18");
}
