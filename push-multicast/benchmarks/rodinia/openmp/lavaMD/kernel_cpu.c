#ifdef __cplusplus
extern "C" {
#endif

//========================================================================================================================================================================================================200
//	DEFINE/INCLUDE
//========================================================================================================================================================================================================200

//======================================================================================================================================================150
//	LIBRARIES
//======================================================================================================================================================150

#include <math.h> // (in path known to compiler)			needed by exp
#include <omp.h>  // (in path known to compiler)			needed by openmp
#include <stdint.h>
#include <stdio.h>  // (in path known to compiler)			needed by printf
#include <stdlib.h> // (in path known to compiler)			needed by malloc

//======================================================================================================================================================150
//	MAIN FUNCTION HEADER
//======================================================================================================================================================150

#include "main.h" // (in the main program folder)	needed to recognized input variables

//======================================================================================================================================================150
//	UTILITIES
//======================================================================================================================================================150

#include "timer.h" // (in library path specified to compiler)	needed by timer

//======================================================================================================================================================150
//	KERNEL_CPU FUNCTION HEADER
//======================================================================================================================================================150

#include "kernel_cpu.h" // (in the current directory)

//========================================================================================================================================================================================================200
//	PLASMAKERNEL_GPU
//========================================================================================================================================================================================================200

void kernel_cpu(par_str par, dim_str dim, box_str *box, FOUR_VECTOR *rv, fp *qv,
                FOUR_VECTOR *fv) {

  //======================================================================================================================================================150
  //	Variables
  //======================================================================================================================================================150

  //======================================================================================================================================================150
  //	MCPU SETUP
  //======================================================================================================================================================150

  omp_set_num_threads(dim.cores_arg);

  //======================================================================================================================================================150
  //	INPUTS
  //======================================================================================================================================================150

  const fp alpha = par.alpha;
  const fp a2 = 2.0 * alpha * alpha;

  //======================================================================================================================================================150
  //	PROCESS INTERACTIONS
  //======================================================================================================================================================150

#pragma omp parallel for schedule(static)
  for (uint64_t l = 0; l < dim.number_boxes; l++) {

    //------------------------------------------------------------------------------------------100
    //	home box - box parameters
    //------------------------------------------------------------------------------------------100

    const long first_i = box[l].offset; // offset to common arrays

    //------------------------------------------------------------------------------------------100
    //	home box - distance, force, charge and type parameters from common
    // arrays
    //------------------------------------------------------------------------------------------100

    const FOUR_VECTOR *rA = &rv[first_i];
    FOUR_VECTOR *fA = &fv[first_i];

    //------------------------------------------------------------------------------------------100
    //	Do for the # of (home+neighbor) boxes
    //------------------------------------------------------------------------------------------100

    for (uint64_t k = 0; k < (1 + box[l].nn); k++) {

      //----------------------------------------50
      //	neighbor box - get pointer to the right box
      //----------------------------------------50
      uint64_t pointer;
      if (k == 0) {
        pointer = l; // set first box to be processed to home box
      } else {
        pointer =
            box[l].nei[k - 1].number; // remaining boxes are neighbor boxes
      }

      //----------------------------------------50
      //	neighbor box - box parameters
      //----------------------------------------50

      const long first_j = box[pointer].offset;

      //----------------------------------------50
      //	neighbor box - distance, force, charge and type parameters
      //----------------------------------------50

      const FOUR_VECTOR *rB = &rv[first_j];
      const fp *qB = &qv[first_j];

      //----------------------------------------50
      //	Do for the # of particles in home box
      //----------------------------------------50

      for (uint64_t i = 0; i < NUMBER_PAR_PER_BOX; i++) {
        const fp rAx = rA[i].x;
        const fp rAy = rA[i].y;
        const fp rAz = rA[i].z;
        const fp rAv = rA[i].v;

        fp sumfAv = 0.0f;
        fp sumfAx = 0.0f;
        fp sumfAy = 0.0f;
        fp sumfAz = 0.0f;

        // do for the # of particles in current (home or neighbor) box
        for (uint64_t j = 0; j < NUMBER_PAR_PER_BOX; j++) {

          // // coefficients
          const fp rBx = rB[j].x;
          const fp rBy = rB[j].y;
          const fp rBz = rB[j].z;
          const fp rBv = rB[j].v;
          const fp dotAB = rAx * rBx + rAy * rBy + rAz * rBz;
          const fp r2 = rAv + rBv - dotAB;
          const fp u2 = a2 * r2;
          const fp vij = exp(-u2);
          const fp fs = 2. * vij;
          const fp dx = rAx - rBx;
          const fp dy = rAy - rBy;
          const fp dz = rAz - rBz;
          const fp fxij = fs * dx;
          const fp fyij = fs * dy;
          const fp fzij = fs * dz;

          // forces
          const fp qBValue = qB[j];
          sumfAv += qBValue * vij;
          sumfAx += qBValue * fxij;
          sumfAy += qBValue * fyij;
          sumfAz += qBValue * fzij;

        } // for j

        fA[i].v += sumfAv;
        fA[i].x += sumfAx;
        fA[i].y += sumfAy;
        fA[i].z += sumfAz;

      } // for i

    } // for k

  } // for l

} // main

#ifdef __cplusplus
}
#endif