#include "x17-gate.h"

#if defined(X17_4WAY)

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "algo/blake/blake-hash-4way.h"
#include "algo/bmw/bmw-hash-4way.h"
#include "algo/groestl/aes_ni/hash-groestl.h"
#include "algo/skein/skein-hash-4way.h"
#include "algo/jh/jh-hash-4way.h"
#include "algo/keccak/keccak-hash-4way.h"
#include "algo/luffa/luffa-hash-2way.h"
#include "algo/cubehash/cube-hash-2way.h"
#include "algo/shavite/sph_shavite.h"
#include "algo/simd/simd-hash-2way.h"
#include "algo/echo/aes_ni/hash_api.h"
#include "algo/hamsi/hamsi-hash-4way.h"
#include "algo/fugue/sph_fugue.h"
#include "algo/shabal/shabal-hash-4way.h"
#include "algo/whirlpool/sph_whirlpool.h"
#include "algo/haval/haval-hash-4way.h"
#include "algo/sha/sha2-hash-4way.h"

typedef struct {
    blake512_4way_context   blake;
    bmw512_4way_context     bmw;
    hashState_groestl       groestl;
    skein512_4way_context   skein;
    jh512_4way_context      jh;
    keccak512_4way_context  keccak;
    luffa_2way_context      luffa;
    cube_2way_context       cube;
    sph_shavite512_context  shavite;
    simd_2way_context       simd;
    hashState_echo          echo;
    hamsi512_4way_context   hamsi;
    sph_fugue512_context    fugue;
    shabal512_4way_context  shabal;
    sph_whirlpool_context   whirlpool;
    sha512_4way_context     sha512;
    haval256_5_4way_context haval;
} x17_4way_ctx_holder;

x17_4way_ctx_holder x17_4way_ctx __attribute__ ((aligned (64)));

void init_x17_4way_ctx()
{
     blake512_4way_init( &x17_4way_ctx.blake );
     bmw512_4way_init( &x17_4way_ctx.bmw );
     init_groestl( &x17_4way_ctx.groestl, 64 );
     skein512_4way_init( &x17_4way_ctx.skein );
     jh512_4way_init( &x17_4way_ctx.jh );
     keccak512_4way_init( &x17_4way_ctx.keccak );
     luffa_2way_init( &x17_4way_ctx.luffa, 512 );
     cube_2way_init( &x17_4way_ctx.cube, 512, 16, 32 );
     sph_shavite512_init( &x17_4way_ctx.shavite );
     simd_2way_init( &x17_4way_ctx.simd, 512 );
     init_echo( &x17_4way_ctx.echo, 512 );
     hamsi512_4way_init( &x17_4way_ctx.hamsi );
     sph_fugue512_init( &x17_4way_ctx.fugue );
     shabal512_4way_init( &x17_4way_ctx.shabal );
     sha512_4way_init( &x17_4way_ctx.sha512 );
     haval256_5_4way_init( &x17_4way_ctx.haval );
};

void x17_4way_hash( void *state, const void *input )
{
     uint64_t hash0[8] __attribute__ ((aligned (64)));
     uint64_t hash1[8] __attribute__ ((aligned (64)));
     uint64_t hash2[8] __attribute__ ((aligned (64)));
     uint64_t hash3[8] __attribute__ ((aligned (64)));
     uint64_t vhash[8*4] __attribute__ ((aligned (64)));
     uint64_t vhashB[8*4] __attribute__ ((aligned (64)));
     x17_4way_ctx_holder ctx;
     memcpy( &ctx, &x17_4way_ctx, sizeof(x17_4way_ctx) );

     // 1 Blake 4 way 64 bit
     blake512_4way( &ctx.blake, input, 80 );
     blake512_4way_close( &ctx.blake, vhash );

     // 2 Bmw
     bmw512_4way( &ctx.bmw, vhash, 64 );
     bmw512_4way_close( &ctx.bmw, vhash );

     // Serial
     mm256_deinterleave_4x64( hash0, hash1, hash2, hash3, vhash, 512 );

     // 3 Groestl
     update_and_final_groestl( &ctx.groestl, (char*)hash0, (char*)hash0, 512 );
     memcpy( &ctx.groestl, &x17_4way_ctx.groestl, sizeof(hashState_groestl) );
     update_and_final_groestl( &ctx.groestl, (char*)hash1, (char*)hash1, 512 );
     memcpy( &ctx.groestl, &x17_4way_ctx.groestl, sizeof(hashState_groestl) );
     update_and_final_groestl( &ctx.groestl, (char*)hash2, (char*)hash2, 512 );
     memcpy( &ctx.groestl, &x17_4way_ctx.groestl, sizeof(hashState_groestl) );
     update_and_final_groestl( &ctx.groestl, (char*)hash3, (char*)hash3, 512 );

     // Parallel 4way
     mm256_interleave_4x64( vhash, hash0, hash1, hash2, hash3, 512 );

     // 4 Skein
     skein512_4way( &ctx.skein, vhash, 64 );
     skein512_4way_close( &ctx.skein, vhash );

     // 5 JH
     jh512_4way( &ctx.jh, vhash, 64 );
     jh512_4way_close( &ctx.jh, vhash );

     // 6 Keccak
     keccak512_4way( &ctx.keccak, vhash, 64 );
     keccak512_4way_close( &ctx.keccak, vhash );

     mm256_deinterleave_4x64( hash0, hash1, hash2, hash3, vhash, 512 );

     // 7 Luffa  parallel 2 way
     mm256_interleave_2x128( vhash,  hash0, hash1, 512 );
     mm256_interleave_2x128( vhashB, hash2, hash3, 512 );
     luffa_2way_update_close( &ctx.luffa, vhash, vhash, 64 );
     luffa_2way_init( &ctx.luffa, 512 );
     luffa_2way_update_close( &ctx.luffa, vhashB, vhashB, 64 );

     // 8 Cubehash parallel 2 way
     cube_2way_update_close( &ctx.cube, vhash, vhash, 64 );
     cube_2way_reinit( &ctx.cube );
     cube_2way_update_close( &ctx.cube, vhashB, vhashB, 64 );

     mm256_deinterleave_2x128( hash0, hash1, vhash, 512 );
     mm256_deinterleave_2x128( hash2, hash3, vhashB, 512 );

     // 9 Shavite serial
     sph_shavite512( &ctx.shavite, hash0, 64 );
     sph_shavite512_close( &ctx.shavite, hash0 );
     memcpy( &ctx.shavite, &x17_4way_ctx.shavite,
             sizeof(sph_shavite512_context) );
     sph_shavite512( &ctx.shavite, hash1, 64 );
     sph_shavite512_close( &ctx.shavite, hash1 );
     memcpy( &ctx.shavite, &x17_4way_ctx.shavite,
             sizeof(sph_shavite512_context) );
     sph_shavite512( &ctx.shavite, hash2, 64 );
     sph_shavite512_close( &ctx.shavite, hash2 );
     memcpy( &ctx.shavite, &x17_4way_ctx.shavite,
             sizeof(sph_shavite512_context) );
     sph_shavite512( &ctx.shavite, hash3, 64 );
     sph_shavite512_close( &ctx.shavite, hash3 );

     // 10 Simd parallel 2 way 128 bit
     mm256_interleave_2x128( vhash, hash0, hash1, 512 );
     simd_2way_update_close( &ctx.simd, vhash, vhash, 512 );
     mm256_deinterleave_2x128( hash0, hash1, vhash, 512 );
     mm256_interleave_2x128( vhash, hash2, hash3, 512 );
     simd_2way_init( &ctx.simd, 512 );
     simd_2way_update_close( &ctx.simd, vhash, vhash, 512 );
     mm256_deinterleave_2x128( hash2, hash3, vhash, 512 );

     // 11 Echo serial
     update_final_echo( &ctx.echo, (BitSequence *)hash0,
                       (const BitSequence *) hash0, 512 );
     memcpy( &ctx.echo, &x17_4way_ctx.echo, sizeof(hashState_echo) );
     update_final_echo( &ctx.echo, (BitSequence *)hash1,
                       (const BitSequence *) hash1, 512 );
     memcpy( &ctx.echo, &x17_4way_ctx.echo, sizeof(hashState_echo) );
     update_final_echo( &ctx.echo, (BitSequence *)hash2,
                       (const BitSequence *) hash2, 512 );
     memcpy( &ctx.echo, &x17_4way_ctx.echo, sizeof(hashState_echo) );
     update_final_echo( &ctx.echo, (BitSequence *)hash3,
                       (const BitSequence *) hash3, 512 );

     // 12 Hamsi parallel 4 way 64 bit
     mm256_interleave_4x64( vhash, hash0, hash1, hash2, hash3, 512 );
     hamsi512_4way( &ctx.hamsi, vhash, 64 );
     hamsi512_4way_close( &ctx.hamsi, vhash );
     mm256_deinterleave_4x64( hash0, hash1, hash2, hash3, vhash, 512 );

     // 13 Fugue serial
     sph_fugue512( &ctx.fugue, hash0, 64 );
     sph_fugue512_close( &ctx.fugue, hash0 );
     memcpy( &ctx.fugue, &x17_4way_ctx.fugue, sizeof(sph_fugue512_context) );
     sph_fugue512( &ctx.fugue, hash1, 64 );
     sph_fugue512_close( &ctx.fugue, hash1 );
     memcpy( &ctx.fugue, &x17_4way_ctx.fugue, sizeof(sph_fugue512_context) );
     sph_fugue512( &ctx.fugue, hash2, 64 );
     sph_fugue512_close( &ctx.fugue, hash2 );
     memcpy( &ctx.fugue, &x17_4way_ctx.fugue, sizeof(sph_fugue512_context) );
     sph_fugue512( &ctx.fugue, hash3, 64 );
     sph_fugue512_close( &ctx.fugue, hash3 );

     // 14 Shabal, parallel 4 way 32 bit SSE
     mm128_interleave_4x32( vhash, hash0, hash1, hash2, hash3, 512 );
     shabal512_4way( &ctx.shabal, vhash, 64 );
     shabal512_4way_close( &ctx.shabal, vhash );
     mm128_deinterleave_4x32( hash0, hash1, hash2, hash3, vhash, 512 );
       
     // 15 Whirlpool
     sph_whirlpool( &ctx.whirlpool, hash0, 64 );
     sph_whirlpool_close( &ctx.whirlpool, hash0 );
     memcpy( &ctx.whirlpool, &x17_4way_ctx.whirlpool,
             sizeof(sph_whirlpool_context) );
     sph_whirlpool( &ctx.whirlpool, hash1, 64 );
     sph_whirlpool_close( &ctx.whirlpool, hash1 );
     memcpy( &ctx.whirlpool, &x17_4way_ctx.whirlpool, 
             sizeof(sph_whirlpool_context) );
     sph_whirlpool( &ctx.whirlpool, hash2, 64 );
     sph_whirlpool_close( &ctx.whirlpool, hash2 );
     memcpy( &ctx.whirlpool, &x17_4way_ctx.whirlpool, 
             sizeof(sph_whirlpool_context) );
     sph_whirlpool( &ctx.whirlpool, hash3, 64 );
     sph_whirlpool_close( &ctx.whirlpool, hash3 );

     // 16 SHA512 parallel 64 bit 
     mm256_interleave_4x64( vhash, hash0, hash1, hash2, hash3, 512 );
     sha512_4way( &ctx.sha512, vhash, 64 );
     sha512_4way_close( &ctx.sha512, vhash );     

     // 17 Haval parallel 32 bit
     mm256_reinterleave_4x32( vhashB, vhash,  512 );
     haval256_5_4way( &ctx.haval, vhashB, 64 );
     haval256_5_4way_close( &ctx.haval, state );

}

int scanhash_x17_4way( int thr_id, struct work *work, uint32_t max_nonce,
                       uint64_t *hashes_done )
{
     uint32_t hash[4*8] __attribute__ ((aligned (64)));
     uint32_t *hash7 = &(hash[7<<2]);
     uint32_t lane_hash[8];
     uint32_t vdata[24*4] __attribute__ ((aligned (64)));
     uint32_t endiandata[20] __attribute__((aligned(64)));
     uint32_t *pdata = work->data;
     uint32_t *ptarget = work->target;
     uint32_t n = pdata[19];
     const uint32_t first_nonce = pdata[19];
     uint32_t *nonces = work->nonces;
     int num_found = 0;
     uint32_t *noncep = vdata + 73;   // 9*8 + 1
     const uint32_t Htarg = ptarget[7];
     uint64_t htmax[] = {          0,        0xF,       0xFF,
                               0xFFF,     0xFFFF, 0x10000000  };
     uint32_t masks[] = { 0xFFFFFFFF, 0xFFFFFFF0, 0xFFFFFF00,
                          0xFFFFF000, 0xFFFF0000,          0  };

     // big endian encode 0..18 uint32_t, 64 bits at a time
     swab32_array( endiandata, pdata, 20 );

     uint64_t *edata = (uint64_t*)endiandata;
     mm256_interleave_4x64( (uint64_t*)vdata, edata, edata, edata, edata, 640 );

     for ( int m=0; m < 6; m++ )
       if ( Htarg <= htmax[m] )
       {
         uint32_t mask = masks[m];
         do
         {
            be32enc( noncep,   n   );
            be32enc( noncep+2, n+1 );
            be32enc( noncep+4, n+2 );
            be32enc( noncep+6, n+3 );

            x17_4way_hash( hash, vdata );

            for ( int lane = 0; lane < 4; lane++ )
            if ( ( ( hash7[ lane ] & mask ) == 0 ) )
            {
               mm128_extract_lane_4x32( lane_hash, hash, lane, 256 );

               if ( fulltest( lane_hash, ptarget ) )
               {
                  pdata[19] = n + lane;
                  nonces[ num_found++ ] = n + lane;
                  work_set_target_ratio( work, lane_hash );
               }
            }
	    n += 4;
         } while ( ( num_found == 0 ) && ( n < max_nonce )
                   && !work_restart[thr_id].restart );
         break;
       }

     *hashes_done = n - first_nonce + 1;
     return num_found;
}

#endif
