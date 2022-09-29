/*
  This is a maximally equidistributed combined Tausworthe generator
  based on code from GNU Scientific Library 1.5 (30 Jun 2004)

   x_n = (s1_n ^ s2_n ^ s3_n)

   s1_{n+1} = (((s1_n & 4294967294) <<12) ^ (((s1_n <<13) ^ s1_n) >>19))
   s2_{n+1} = (((s2_n & 4294967288) << 4) ^ (((s2_n << 2) ^ s2_n) >>25))
   s3_{n+1} = (((s3_n & 4294967280) <<17) ^ (((s3_n << 3) ^ s3_n) >>11))

   The period of this generator is about 2^88.

   From: P. L'Ecuyer, "Maximally Equidistributed Combined Tausworthe
   Generators", Mathematics of Computation, 65, 213 (1996), 203--213.

   This is available on the net from L'Ecuyer's home page,

   http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme.ps
   ftp://ftp.iro.umontreal.ca/pub/simulation/lecuyer/papers/tausme.ps

   There is an erratum in the paper "Tables of Maximally
   Equidistributed Combined LFSR Generators", Mathematics of
   Computation, 68, 225 (1999), 261--269:
   http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme2.ps

        ... the k_j most significant bits of z_j must be non-
        zero, for each j. (Note: this restriction also applies to the
        computer code given in [4], but was mistakenly not mentioned in
        that paper.)

   This affects the seeding procedure by imposing the requirement
   s1 > 1, s2 > 7, s3 > 15.

*/

#include <string.h>
#include "rand.h"

int arch_random;

static inline uint64_t __seed(uint64_t x, uint64_t m)
{
	return (x < m) ? x + m : x;
}

static void __init_rand32(struct taus88_state *state, unsigned int seed)
{
	int cranks = 6;

#define LCG(x, seed)  ((x) * 69069 ^ (seed))

	state->s1 = __seed(LCG((2^31) + (2^17) + (2^7), seed), 1);
	state->s2 = __seed(LCG(state->s1, seed), 7);
	state->s3 = __seed(LCG(state->s2, seed), 15);

	while (cranks--)
		__rand32(state);
}

void __init_rand64(struct taus258_state *state, uint64_t seed)
{
	int cranks = 6;

#define LCG64(x, seed)  ((x) * 6906969069ULL ^ (seed))

	state->s1 = __seed(LCG64((2^31) + (2^17) + (2^7), seed), 1);
	state->s2 = __seed(LCG64(state->s1, seed), 7);
	state->s3 = __seed(LCG64(state->s2, seed), 15);
	state->s4 = __seed(LCG64(state->s3, seed), 33);
	state->s5 = __seed(LCG64(state->s4, seed), 49);

	while (cranks--)
		__rand64(state);
}

void init_rand(struct frand_state *state, bool use64)
{
	state->use64 = use64;

	if (!use64)
		__init_rand32(&state->state32, 1);
	else
		__init_rand64(&state->state64, 1);
}

void init_rand_seed(struct frand_state *state, uint64_t seed, bool use64)
{
	state->use64 = use64;

	if (!use64)
		__init_rand32(&state->state32, (unsigned int) seed);
	else
		__init_rand64(&state->state64, seed);
}

