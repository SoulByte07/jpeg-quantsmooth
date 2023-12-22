/*
 * idct/fdct SSE2/AVX2/NEON intrinsic optimizations:
 * Copyright (C) 2016-2020 Ilya Kurdyukov
 *
 * contains modified parts of libjpeg:
 * Copyright (C) 1991-1998, Thomas G. Lane
 *
 * This file is part of jpeg quantsmooth
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

static const char jpegqs_natural_order[DCTSIZE2] = {
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static JSAMPLE range_limit_static[CENTERJSAMPLE * 8];

#ifndef USE_JSIMD

#define CONST_BITS  13
#define PASS1_BITS  2
#define FIX_0_298631336  2446        /* FIX(0.298631336) */
#define FIX_0_390180644  3196        /* FIX(0.390180644) */
#define FIX_0_541196100  4433        /* FIX(0.541196100) */
#define FIX_0_765366865  6270        /* FIX(0.765366865) */
#define FIX_0_899976223  7373        /* FIX(0.899976223) */
#define FIX_1_175875602  9633        /* FIX(1.175875602) */
#define FIX_1_501321110  12299       /* FIX(1.501321110) */
#define FIX_1_847759065  15137       /* FIX(1.847759065) */
#define FIX_1_961570560  16069       /* FIX(1.961570560) */
#define FIX_2_053119869  16819       /* FIX(2.053119869) */
#define FIX_2_562915447  20995       /* FIX(2.562915447) */
#define FIX_3_072711026  25172       /* FIX(3.072711026) */

#define DESCALE(x,n)  (((x) + (1 << ((n)-1))) >> (n))
#define RANGE_MASK  (MAXJSAMPLE * 4 + 3) /* 2 bits wider than legal samples */

static void idct_islow(JCOEFPTR coef_block, JSAMPROW outptr, JDIMENSION stride) {

#define M3 \
	z2 = M1(2); z3 = M1(6); \
	z1 = MUL(ADD(z2, z3), SET1(FIX_0_541196100)); \
	tmp2 = SUB(z1, MUL(z3, SET1(FIX_1_847759065))); \
	tmp3 = ADD(z1, MUL(z2, SET1(FIX_0_765366865))); \
	z2 = M1(0); z3 = M1(4); \
	tmp0 = SHL(ADD(z2, z3), CONST_BITS); \
	tmp1 = SHL(SUB(z2, z3), CONST_BITS); \
	tmp10 = ADD(tmp0, tmp3); tmp13 = SUB(tmp0, tmp3); \
	tmp11 = ADD(tmp1, tmp2); tmp12 = SUB(tmp1, tmp2); \
	tmp0 = M1(7); tmp1 = M1(5); tmp2 = M1(3); tmp3 = M1(1); \
	z1 = ADD(tmp0, tmp3); z2 = ADD(tmp1, tmp2); \
	z3 = ADD(tmp0, tmp2); z4 = ADD(tmp1, tmp3); \
	z5 = MUL(ADD(z3, z4), SET1(FIX_1_175875602)); \
	tmp0 = MUL(tmp0, SET1(FIX_0_298631336)); \
	tmp1 = MUL(tmp1, SET1(FIX_2_053119869)); \
	tmp2 = MUL(tmp2, SET1(FIX_3_072711026)); \
	tmp3 = MUL(tmp3, SET1(FIX_1_501321110)); \
	z1 = MUL(z1, SET1(FIX_0_899976223)); \
	z2 = MUL(z2, SET1(FIX_2_562915447)); \
	z3 = MUL(z3, SET1(FIX_1_961570560)); \
	z4 = MUL(z4, SET1(FIX_0_390180644)); \
	z3 = SUB(z5, z3); z4 = SUB(z5, z4); \
	tmp0 = ADD(tmp0, SUB(z3, z1)); \
	tmp1 = ADD(tmp1, SUB(z4, z2)); \
	tmp2 = ADD(tmp2, SUB(z3, z2)); \
	tmp3 = ADD(tmp3, SUB(z4, z1)); \
	M2(0, ADD(tmp10, tmp3)) M2(7, SUB(tmp10, tmp3)) \
	M2(1, ADD(tmp11, tmp2)) M2(6, SUB(tmp11, tmp2)) \
	M2(2, ADD(tmp12, tmp1)) M2(5, SUB(tmp12, tmp1)) \
	M2(3, ADD(tmp13, tmp0)) M2(4, SUB(tmp13, tmp0))

//------------------------------------------------------------------------------
#if 1 && defined(USE_LASX)
	__m256i v0, v1, v2, v3, v4, v5, v6, v7, t0, t1, x0, x1, x2, x3, x4, x5, x6, x7;
	__m256i tmp0, tmp1, tmp2, tmp3;
	__m256i tmp10, tmp11, tmp12, tmp13;
	__m256i z1, z2, z3, z4, z5;

#define ADD __lasx_xvadd_w
#define SUB __lasx_xvsub_w
#define MUL __lasx_xvmul_w
#define SET1 __lasx_xvreplgr2vr_w
#define SHL __lasx_xvslli_w

#define M1(i) __lasx_xvsllwil_w_h(__lasx_xvpermi_q( \
	__lasx_xvldrepl_d(coef_block, DCTSIZE*i*2+8), \
	__lasx_xvldrepl_d(coef_block, DCTSIZE*i*2), 0x20), 0)
#define M2(i, tmp) x##i = __lasx_xvsrari_w(tmp, CONST_BITS-PASS1_BITS);
	M3
#undef M1
#undef M2

#define M2(v0, v1, v2, v3, k) \
v0 = __lasx_xvpermi_q(x4, x0, k); \
v1 = __lasx_xvpermi_q(x5, x1, k); \
v2 = __lasx_xvpermi_q(x6, x2, k); \
v3 = __lasx_xvpermi_q(x7, x3, k);
	M2(v0, v1, v2, v3, 0x20)
	M2(v4, v5, v6, v7, 0x31)
#undef M2
#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = __lasx_xvilvl_w(v2, v0); \
t1 = __lasx_xvilvl_w(v3, v1); \
x0 = __lasx_xvilvl_w(t1, t0); \
x1 = __lasx_xvilvh_w(t1, t0); \
t0 = __lasx_xvilvh_w(v2, v0); \
t1 = __lasx_xvilvh_w(v3, v1); \
x2 = __lasx_xvilvl_w(t1, t0); \
x3 = __lasx_xvilvh_w(t1, t0);
	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)

#define M1(i) x##i
#define M2(i, tmp) v##i = __lasx_xvsrai_w(ADD(tmp, t0), CONST_BITS+PASS1_BITS+3);
	t0 = SET1((256+1) << (CONST_BITS+PASS1_BITS+3-1));
	M3
#undef M1
#undef M2

	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M4

	x0 = __lasx_xvssrani_h_w(x1, x0, 0);
	x1 = __lasx_xvssrani_h_w(x3, x2, 0);
	x4 = __lasx_xvssrani_h_w(x5, x4, 0);
	x5 = __lasx_xvssrani_h_w(x7, x6, 0);
	x0 = __lasx_xvssrani_bu_h(x1, x0, 0);
	x4 = __lasx_xvssrani_bu_h(x5, x4, 0);
	v0 = __lasx_xvilvl_w(x4, x0);
	v1 = __lasx_xvilvh_w(x4, x0);

#define M1(i, v0, j) __lasx_xvstelm_d(v0, &outptr[stride*i], 0, j);
	M1(0, v0, 0) M1(1, v0, 1) M1(2, v1, 0) M1(3, v1, 1)
	M1(4, v0, 2) M1(5, v0, 3) M1(6, v1, 2) M1(7, v1, 3)
#undef M1
//------------------------------------------------------------------------------
#elif 1 && defined(USE_LSX)
	int ctr; __m128i *wsptr, workspace[DCTSIZE2] ALIGN(16);

	__m128i v0, v1, v2, v3, v4, v5, v6, v7, t0, t1, x0, x1, x2, x3, x4, x5, x6, x7;
	__m128i tmp0, tmp1, tmp2, tmp3;
	__m128i tmp10, tmp11, tmp12, tmp13;
	__m128i z1, z2, z3, z4, z5;

#define ADD __lsx_vadd_w
#define SUB __lsx_vsub_w
#define MUL __lsx_vmul_w
#define SET1 __lsx_vreplgr2vr_w
#define SHL __lsx_vslli_w

	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr += 4, wsptr += 4) {
#define M1(i) __lsx_vsllwil_w_h(__lsx_vldrepl_d(&coef_block[ctr], DCTSIZE*i*2), 0)
#define M2(i, tmp) wsptr[(i&3)+(i&4)*2] = __lsx_vsrari_w(tmp, CONST_BITS-PASS1_BITS);
		M3
#undef M1
#undef M2
	}

	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr += 4, wsptr += 8, outptr += 4*stride) {
#define M1(i) v##i = wsptr[i];
		M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1

#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = __lsx_vilvl_w(v2, v0); \
t1 = __lsx_vilvl_w(v3, v1); \
x0 = __lsx_vilvl_w(t1, t0); \
x1 = __lsx_vilvh_w(t1, t0); \
t0 = __lsx_vilvh_w(v2, v0); \
t1 = __lsx_vilvh_w(v3, v1); \
x2 = __lsx_vilvl_w(t1, t0); \
x3 = __lsx_vilvh_w(t1, t0);
		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)

#define M1(i) x##i
#define M2(i, tmp) v##i = __lsx_vsrai_w(ADD(tmp, t0), CONST_BITS+PASS1_BITS+3);
		t0 = __lsx_vreplgr2vr_w((256+1) << (CONST_BITS+PASS1_BITS+3-1));
		M3
#undef M1
#undef M2

		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M4

		x0 = __lsx_vssrani_h_w(x4, x0, 0);
		x1 = __lsx_vssrani_h_w(x5, x1, 0);
		x2 = __lsx_vssrani_h_w(x6, x2, 0);
		x3 = __lsx_vssrani_h_w(x7, x3, 0);
		v0 = __lsx_vssrani_bu_h(x1, x0, 0);
		v1 = __lsx_vssrani_bu_h(x3, x2, 0);

#define M1(i, v0) __lsx_vstelm_d(v0, &outptr[i*stride], 0, i & 1);
		M1(0, v0) M1(1, v0) M1(2, v1) M1(3, v1)
#undef M1
	}
//------------------------------------------------------------------------------
#elif 1 && defined(USE_NEON)
	int ctr; int32x4_t *wsptr, workspace[DCTSIZE2] ALIGN(16);

	int32x4_t tmp0, tmp1, tmp2, tmp3;
	int32x4_t tmp10, tmp11, tmp12, tmp13;
	int32x4_t z1, z2, z3, z4, z5;

#define ADD vaddq_s32
#define SUB vsubq_s32
#if 0
	static const int32_t tab[12] = {
		FIX_0_298631336, FIX_0_390180644, FIX_0_541196100, FIX_0_765366865,
		FIX_0_899976223, FIX_1_175875602, FIX_1_501321110, FIX_1_847759065,
		FIX_1_961570560, FIX_2_053119869, FIX_2_562915447, FIX_3_072711026 };
	int32x4_t t1 = vld1q_s32(tab), t2 = vld1q_s32(tab + 4), t3 = vld1q_s32(tab + 8);

#define IDCT_FIX_0_298631336 vget_low_s32(t1), 0
#define IDCT_FIX_0_390180644 vget_low_s32(t1), 1
#define IDCT_FIX_0_541196100 vget_high_s32(t1), 0
#define IDCT_FIX_0_765366865 vget_high_s32(t1), 1
#define IDCT_FIX_0_899976223 vget_low_s32(t2), 0
#define IDCT_FIX_1_175875602 vget_low_s32(t2), 1
#define IDCT_FIX_1_501321110 vget_high_s32(t2), 0
#define IDCT_FIX_1_847759065 vget_high_s32(t2), 1
#define IDCT_FIX_1_961570560 vget_low_s32(t3), 0
#define IDCT_FIX_2_053119869 vget_low_s32(t3), 1
#define IDCT_FIX_2_562915447 vget_high_s32(t3), 0
#define IDCT_FIX_3_072711026 vget_high_s32(t3), 1

#define MUL(a, b) vmulq_lane_s32(a, b)
#define SET1(a) IDCT_##a
#else
#define MUL vmulq_s32
#define SET1 vdupq_n_s32
#endif
#define SHL vshlq_n_s32

	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr += 4, wsptr += 4) {
#define M1(i) vmovl_s16(vld1_s16((int16_t*)&coef_block[DCTSIZE*i+ctr]))
#define M2(i, tmp) wsptr[(i&3)+(i&4)*2] = vrshrq_n_s32(tmp, CONST_BITS-PASS1_BITS);
		M3
#undef M1
#undef M2
	}

	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr += 4, wsptr += 8, outptr += 4*stride) {
		int32x4x4_t q0 = vld4q_s32((int32_t*)&wsptr[0]), q1 = vld4q_s32((int32_t*)&wsptr[4]);
#define M1(i, n) int32x4_t x##i = q##n.val[i & 3];
		M1(0, 0) M1(1, 0) M1(2, 0) M1(3, 0) M1(4, 1) M1(5, 1) M1(6, 1) M1(7, 1)
#undef M1

#define M1(i) x##i
#define M2(i, tmp) x##i = tmp;
		M3
#undef M1
#undef M2

		{
			int8x8_t t0 = vdup_n_s8(-128);
#define M1(i, j) int8x8_t v##i = vqrshrn_n_s16(vuzpq_s16( \
	vreinterpretq_s16_s32(x##i), vreinterpretq_s16_s32(x##j)).val[1], \
	CONST_BITS+PASS1_BITS+3-16);
			M1(0, 4) M1(1, 5) M1(2, 6) M1(3, 7)
#undef M1
			int16x4x2_t p0, p1; int8x8x2_t p2, p3;
			p0 = vtrn_s16(vreinterpret_s16_s8(v0), vreinterpret_s16_s8(v2));
			p1 = vtrn_s16(vreinterpret_s16_s8(v1), vreinterpret_s16_s8(v3));
			p2 = vtrn_s8(vreinterpret_s8_s16(p0.val[0]), vreinterpret_s8_s16(p1.val[0]));
			p3 = vtrn_s8(vreinterpret_s8_s16(p0.val[1]), vreinterpret_s8_s16(p1.val[1]));
#define M1(i, p2, j) vst1_s8((int8_t*)&outptr[stride*i], veor_s8(p2.val[j], t0));
			M1(0, p2, 0); M1(1, p2, 1); M1(2, p3, 0); M1(3, p3, 1);
#undef M1
		}
	}
//------------------------------------------------------------------------------
#elif 1 && defined(USE_AVX2)
	__m256i v0, v1, v2, v3, v4, v5, v6, v7, t0, t1, x0, x1, x2, x3, x4, x5, x6, x7;
	__m256i tmp0, tmp1, tmp2, tmp3;
	__m256i tmp10, tmp11, tmp12, tmp13;
	__m256i z1, z2, z3, z4, z5;

#define ADD _mm256_add_epi32
#define SUB _mm256_sub_epi32
#define MUL _mm256_mullo_epi32
#define SET1 _mm256_set1_epi32
#define SHL _mm256_slli_epi32

#define M1(i) _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i*)&coef_block[DCTSIZE*i]))
#define M2(i, tmp) x##i = _mm256_srai_epi32(ADD(tmp, t0), CONST_BITS-PASS1_BITS);
	t0 = SET1(1 << (CONST_BITS-PASS1_BITS-1));
	M3
#undef M1
#undef M2

#define M2(v0, v1, v2, v3, k) \
v0 = _mm256_permute2x128_si256(x0, x4, k); \
v1 = _mm256_permute2x128_si256(x1, x5, k); \
v2 = _mm256_permute2x128_si256(x2, x6, k); \
v3 = _mm256_permute2x128_si256(x3, x7, k);
	M2(v0, v1, v2, v3, 0x20)
	M2(v4, v5, v6, v7, 0x31)
#undef M2
#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = _mm256_unpacklo_epi32(v0, v2); \
t1 = _mm256_unpacklo_epi32(v1, v3); \
x0 = _mm256_unpacklo_epi32(t0, t1); \
x1 = _mm256_unpackhi_epi32(t0, t1); \
t0 = _mm256_unpackhi_epi32(v0, v2); \
t1 = _mm256_unpackhi_epi32(v1, v3); \
x2 = _mm256_unpacklo_epi32(t0, t1); \
x3 = _mm256_unpackhi_epi32(t0, t1);
	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)

#define M1(i) x##i
#define M2(i, tmp) v##i = _mm256_srai_epi32(ADD(tmp, t0), CONST_BITS+PASS1_BITS+3);
	t0 = SET1((256+1) << (CONST_BITS+PASS1_BITS+3-1));
	M3
#undef M1
#undef M2

	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M4

	x0 = _mm256_packs_epi32(x0, x1);
	x1 = _mm256_packs_epi32(x2, x3);
	x4 = _mm256_packs_epi32(x4, x5);
	x5 = _mm256_packs_epi32(x6, x7);
	x0 = _mm256_packus_epi16(x0, x1);
	x4 = _mm256_packus_epi16(x4, x5);
	v0 = _mm256_unpacklo_epi32(x0, x4);
	v1 = _mm256_unpackhi_epi32(x0, x4);
#if 1 && defined(__x86_64__)
#define M1(i, v0, j) *(int64_t*)&outptr[stride*i] = _mm256_extract_epi64(v0, j);
	M1(0, v0, 0) M1(1, v0, 1) M1(2, v1, 0) M1(3, v1, 1)
	M1(4, v0, 2) M1(5, v0, 3) M1(6, v1, 2) M1(7, v1, 3)
#undef M1
#else
#define M1(i, v0, l) _mm_store##l##_pd((double*)&outptr[i*stride], _mm_castsi128_pd(_mm256_castsi256_si128(v0)));
#define M2(i, v0, l) _mm_store##l##_pd((double*)&outptr[i*stride], _mm_castsi128_pd(_mm256_extracti128_si256(v0, 1)));
	M1(0, v0, l) M1(1, v0, h) M1(2, v1, l) M1(3, v1, h)
	M2(4, v0, l) M2(5, v0, h) M2(6, v1, l) M2(7, v1, h)
#undef M2
#undef M1
#endif
//------------------------------------------------------------------------------
#elif 1 && defined(USE_SSE2)
	int ctr; __m128i *wsptr, workspace[DCTSIZE2] ALIGN(16);

	__m128i v0, v1, v2, v3, v4, v5, v6, v7, t0, t1, x0, x1, x2, x3, x4, x5, x6, x7;
	__m128i tmp0, tmp1, tmp2, tmp3;
	__m128i tmp10, tmp11, tmp12, tmp13;
	__m128i z1, z2, z3, z4, z5;

#define ADD _mm_add_epi32
#define SUB _mm_sub_epi32
#if BITS_IN_JSAMPLE == 8 && !defined(USE_SSE4)
#define MUL _mm_madd_epi16
#define SET1(a) _mm_set1_epi32(a & 0xffff)
#else
#define MUL _mm_mullo_epi32
#define SET1 _mm_set1_epi32
#endif
#define SHL _mm_slli_epi32

	t0 = _mm_set1_epi32(1 << (CONST_BITS-PASS1_BITS-1));
	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr += 4, wsptr += 4) {
#define M1(i) _mm_cvtepi16_epi32(_mm_loadl_epi64((__m128i*)&coef_block[DCTSIZE*i+ctr]))
#define M2(i, tmp) wsptr[(i&3)+(i&4)*2] = _mm_srai_epi32(ADD(tmp, t0), CONST_BITS-PASS1_BITS);
		M3
#undef M1
#undef M2
	}

	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr += 4, wsptr += 8, outptr += 4*stride) {
#define M1(i) v##i = wsptr[i];
		M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1

#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = _mm_unpacklo_epi32(v0, v2); \
t1 = _mm_unpacklo_epi32(v1, v3); \
x0 = _mm_unpacklo_epi32(t0, t1); \
x1 = _mm_unpackhi_epi32(t0, t1); \
t0 = _mm_unpackhi_epi32(v0, v2); \
t1 = _mm_unpackhi_epi32(v1, v3); \
x2 = _mm_unpacklo_epi32(t0, t1); \
x3 = _mm_unpackhi_epi32(t0, t1);
		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)

#define M1(i) x##i
#define M2(i, tmp) v##i = _mm_srai_epi32(ADD(tmp, t0), CONST_BITS+PASS1_BITS+3);
		t0 = _mm_set1_epi32((256+1) << (CONST_BITS+PASS1_BITS+3-1));
		M3
#undef M1
#undef M2

		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M4

		x0 = _mm_packs_epi32(x0, x4);
		x1 = _mm_packs_epi32(x1, x5);
		x2 = _mm_packs_epi32(x2, x6);
		x3 = _mm_packs_epi32(x3, x7);
		v0 = _mm_packus_epi16(x0, x1);
		v1 = _mm_packus_epi16(x2, x3);
#define M1(i, v0, l) _mm_store##l##_pd((double*)&outptr[i*stride], _mm_castsi128_pd(v0));
		M1(0, v0, l) M1(1, v0, h) M1(2, v1, l) M1(3, v1, h)
#undef M1
	}
//------------------------------------------------------------------------------
#else
#define NEED_RANGELIMIT
	int32_t tmp0, tmp1, tmp2, tmp3;
	int32_t tmp10, tmp11, tmp12, tmp13;
	int32_t z1, z2, z3, z4, z5;
	JCOEFPTR inptr = coef_block;
	JSAMPLE *range_limit = range_limit_static;
	int ctr;
	int32_t *wsptr, workspace[DCTSIZE2];	/* buffers data between passes */

#define ADD(a, b) ((a) + (b))
#define SUB(a, b) ((a) - (b))
#define MUL(a, b) ((a) * (b))
#define SET1(a) (a)
#define SHL(a, b) ((a) << (b))

#define M1(i) inptr[DCTSIZE*i]
#define M2(i, tmp) wsptr[DCTSIZE*i] = DESCALE(tmp, CONST_BITS-PASS1_BITS);
	wsptr = workspace;
	for (ctr = DCTSIZE; ctr > 0; ctr--, inptr++, wsptr++) {
		if (!(M1(1) | M1(2) | M1(3) | M1(4) | M1(5) | M1(6) | M1(7))) {
			/* AC terms all zero */
			int dcval = SHL(M1(0), PASS1_BITS);
			wsptr[DCTSIZE*0] = dcval;
			wsptr[DCTSIZE*1] = dcval;
			wsptr[DCTSIZE*2] = dcval;
			wsptr[DCTSIZE*3] = dcval;
			wsptr[DCTSIZE*4] = dcval;
			wsptr[DCTSIZE*5] = dcval;
			wsptr[DCTSIZE*6] = dcval;
			wsptr[DCTSIZE*7] = dcval;
			continue;
		}

		M3
	}
#undef M1
#undef M2

#define M1(i) wsptr[i]
#define M2(i, tmp) outptr[i] = range_limit[DESCALE(tmp, CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
	wsptr = workspace;
	for (ctr = 0; ctr < DCTSIZE; ctr++, wsptr += DCTSIZE, outptr += stride) {
#ifndef NO_ZERO_ROW_TEST
		if (!(M1(1) | M1(2) | M1(3) | M1(4) | M1(5) | M1(6) | M1(7))) {
			/* AC terms all zero */
			JSAMPLE dcval = range_limit[DESCALE(wsptr[0], PASS1_BITS+3) & RANGE_MASK];
			outptr[0] = dcval;
			outptr[1] = dcval;
			outptr[2] = dcval;
			outptr[3] = dcval;
			outptr[4] = dcval;
			outptr[5] = dcval;
			outptr[6] = dcval;
			outptr[7] = dcval;
			continue;
		}
#endif
		M3
	}
#undef M1
#undef M2
#endif

#undef M3
#undef ADD
#undef SUB
#undef MUL
#undef SET1
#undef SHL
}
#endif // USE_JSIMD

static void range_limit_init() {
	JSAMPLE *t = range_limit_static;
#ifdef NEED_RANGELIMIT
	int i, c = CENTERJSAMPLE, m = c * 2;

	for (i = 0; i < c; i++) t[i] = i + c;
	while (i < 2 * m) t[i++] = m - 1;
	while (i < 3 * m + c) t[i++] = 0;
	for (i = 0; i < c; i++) t[3 * m + c + i] = i;
#else
	(void)t;
#endif
}

static void idct_float(float *in, float *out) {
	float t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;
	float *ws, buf[DCTSIZE2]; int i;
#define M3(inc1, inc2) ws = buf; \
	for (i = 0; i < DCTSIZE; i++, inc1, inc2) { \
		z2 = M1(2); z3 = M1(6); \
		z1 = (z2 + z3) * 0.541196100f; \
		t2 = z1 - z3 * 1.847759065f; \
		t3 = z1 + z2 * 0.765366865f; \
		z2 = M1(0); z3 = M1(4); \
		t0 = z2 + z3; t1 = z2 - z3; \
		t4 = t0 + t3; t7 = t0 - t3; \
		t5 = t1 + t2; t6 = t1 - t2; \
		t0 = M1(7); t1 = M1(5); t2 = M1(3); t3 = M1(1); \
		z1 = t0 + t3; z2 = t1 + t2; \
		z3 = t0 + t2; z4 = t1 + t3; \
		z5 = (z3 + z4) * 1.175875602f; \
		t0 *= 0.298631336f; t1 *= 2.053119869f; \
		t2 *= 3.072711026f; t3 *= 1.501321110f; \
		z1 *= 0.899976223f; z2 *= 2.562915447f; \
		z3 *= 1.961570560f; z4 *= 0.390180644f; \
		z3 -= z5; t0 -= z1 + z3; t2 -= z2 + z3; \
		z4 -= z5; t1 -= z2 + z4; t3 -= z1 + z4; \
		M2(0, t4 + t3) M2(7, t4 - t3) \
		M2(1, t5 + t2) M2(6, t5 - t2) \
		M2(2, t6 + t1) M2(5, t6 - t1) \
		M2(3, t7 + t0) M2(4, t7 - t0) \
	}
#define M1(i) in[DCTSIZE*i]
#define M2(i, t) ws[DCTSIZE*i] = t;
	M3(in++, ws++)
#undef M1
#undef M2
#define M1(i) ws[i]
#define M2(i, t) out[i] = (t) * 0.125f;
	M3(ws += DCTSIZE, out += DCTSIZE)
#undef M1
#undef M2
#undef M3
}

static void fdct_float(float *in, float *out) {

#define M3 \
	z1 = M1(0); z2 = M1(7); t0 = ADD(z1, z2); t7 = SUB(z1, z2); \
	z1 = M1(1); z2 = M1(6); t1 = ADD(z1, z2); t6 = SUB(z1, z2); \
	z1 = M1(2); z2 = M1(5); t2 = ADD(z1, z2); t5 = SUB(z1, z2); \
	z1 = M1(3); z2 = M1(4); t3 = ADD(z1, z2); t4 = SUB(z1, z2); \
	z1 = ADD(t0, t3); z4 = SUB(t0, t3); \
	z2 = ADD(t1, t2); z3 = SUB(t1, t2); \
	M2(0, ADD(z1, z2)) M2(4, SUB(z1, z2)) \
	z1 = MUL((ADD(z3, z4)), SET1(0.541196100f)); \
	M2(2, ADD(z1, MUL(z4, SET1(0.765366865f)))) \
	M2(6, SUB(z1, MUL(z3, SET1(1.847759065f)))) \
	z1 = ADD(t4, t7); z2 = ADD(t5, t6); \
	z3 = ADD(t4, t6); z4 = ADD(t5, t7); \
	z5 = MUL(ADD(z3, z4), SET1(1.175875602f)); \
	t4 = MUL(t4, SET1(0.298631336f)); t5 = MUL(t5, SET1(2.053119869f)); \
	t6 = MUL(t6, SET1(3.072711026f)); t7 = MUL(t7, SET1(1.501321110f)); \
	z1 = MUL(z1, SET1(0.899976223f)); z2 = MUL(z2, SET1(2.562915447f)); \
	z3 = SUB(MUL(z3, SET1(1.961570560f)), z5); \
	z4 = SUB(MUL(z4, SET1(0.390180644f)), z5); \
	M2(7, SUB(t4, ADD(z1, z3))) M2(5, SUB(t5, ADD(z2, z4))) \
	M2(3, SUB(t6, ADD(z2, z3))) M2(1, SUB(t7, ADD(z1, z4)))

//------------------------------------------------------------------------------
#if 1 && defined(USE_LASX)
	__m256 v0, v1, v2, v3, v4, v5, v6, v7, x0, x1, x2, x3, x4, x5, x6, x7;
	__m256 t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;

#define ADD __lasx_xvfadd_s
#define SUB __lasx_xvfsub_s
#define MUL __lasx_xvfmul_s
#define SET1 __lasx_xvfreplgr2vr_s

#define M1(i) (__m256)__lasx_xvld(in + i * DCTSIZE, 0)
#define M2(i, t) x##i = t;
	M3
#undef M1
#undef M2

#define M4_LASX(lo, a, b) \
	(__m256)__lasx_xvilv##lo##_w((__m256i)b, (__m256i)a)
#define M5_LASX(a, b, k) \
	(__m256)__lasx_xvpermi_q((__m256i)b, (__m256i)a, k)
#define M5(v0, v1, v2, v3, k) \
v0 = M5_LASX(x0, x4, k); \
v1 = M5_LASX(x1, x5, k); \
v2 = M5_LASX(x2, x6, k); \
v3 = M5_LASX(x3, x7, k);
#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = M4_LASX(l, v0, v2); t1 = M4_LASX(l, v1, v3); \
x0 = M4_LASX(l, t0, t1); x1 = M4_LASX(h, t0, t1); \
t0 = M4_LASX(h, v0, v2); t1 = M4_LASX(h, v1, v3); \
x2 = M4_LASX(l, t0, t1); x3 = M4_LASX(h, t0, t1);
	M5(v0, v1, v2, v3, 0x20)
	M5(v4, v5, v6, v7, 0x31)
	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)
#define M1(i) x##i
#define M2(i, t) x##i = MUL(t, SET1(0.125f));
	M3
#undef M1
#undef M2
	M5(v0, v1, v2, v3, 0x20)
	M5(v4, v5, v6, v7, 0x31)
	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M5
#undef M4
#undef M5_LASX
#undef M4_LASX

#define M1(i) __lasx_xvst(x##i, out + i * DCTSIZE, 0);
	M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1
//------------------------------------------------------------------------------
#elif 1 && defined(USE_LSX)
	__m128 *ws, buf[DCTSIZE2] ALIGN(16); int i;
	__m128 v0, v1, v2, v3, v4, v5, v6, v7, x0, x1, x2, x3, x4, x5, x6, x7;
	__m128 t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;

#define ADD __lsx_vfadd_s
#define SUB __lsx_vfsub_s
#define MUL __lsx_vfmul_s
#define SET1 __lsx_vfreplgr2vr_s

	ws = buf;
	for (i = 0; i < DCTSIZE; i += 4, in += 4, ws += 4) {
#define M1(i) (__m128)__lsx_vld(in, i * DCTSIZE * 4)
#define M2(i, t) ws[(i & 3) + (i & 4) * 2] = t;
		M3
#undef M1
#undef M2
	}

	ws = buf;
	for (i = 0; i < DCTSIZE; i += 4, ws += 8, out += 4 * DCTSIZE) {
		__m128 c0 = SET1(0.125f);
#define M1(i) v##i = ws[i];
		M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1

#define M4_LSX(lo, a, b) \
	(__m128)__lsx_vilv##lo##_w((__m128i)b, (__m128i)a)
#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = M4_LSX(l, v0, v2); t1 = M4_LSX(l, v1, v3); \
x0 = M4_LSX(l, t0, t1); x1 = M4_LSX(h, t0, t1); \
t0 = M4_LSX(h, v0, v2); t1 = M4_LSX(h, v1, v3); \
x2 = M4_LSX(l, t0, t1); x3 = M4_LSX(h, t0, t1);
		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)
#define M1(i) x##i
#define M2(i, t) v##i = MUL(t, c0);
		M3
#undef M1
#undef M2
		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M4
#undef M4_LSX

#define M1(i) __lsx_vst(x##i, out, ((i & 3) * DCTSIZE + (i & 4)) * 4);
		M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1
	}
//------------------------------------------------------------------------------
#elif 1 && defined(USE_NEON)
	float32x4_t *ws, buf[DCTSIZE2] ALIGN(16); int i;
	float32x4_t t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;

#define ADD vaddq_f32
#define SUB vsubq_f32
#define MUL vmulq_f32
#define SET1 vdupq_n_f32

	ws = buf;
	for (i = 0; i < DCTSIZE; i += 4, in += 4, ws += 4) {
#define M1(i) vld1q_f32(in + i * DCTSIZE)
#define M2(i, t) ws[(i & 3) + (i & 4) * 2] = t;
		M3
#undef M1
#undef M2
	}

	ws = buf;
	for (i = 0; i < DCTSIZE; i += 4, ws += 8, out += 4 * DCTSIZE) {
		float32x4_t c0 = SET1(0.125f);
		float32x4x4_t q0 = vld4q_f32((float*)&ws[0]), q1 = vld4q_f32((float*)&ws[4]);
#define M1(i, n) float32x4_t x##i = q##n.val[i & 3];
		M1(0, 0) M1(1, 0) M1(2, 0) M1(3, 0) M1(4, 1) M1(5, 1) M1(6, 1) M1(7, 1)
#undef M1

#define M1(i) x##i
#define M2(i, t) x##i = vmulq_f32(t, c0);
		M3
#undef M1
#undef M2

		{
			float32x4x2_t p0, p1, p2;
#define M1(i, j) p2 = vzipq_f32(p0.val[i], p1.val[i]); \
	vst1q_f32(out + (i * 2) * DCTSIZE + j, p2.val[0]); \
	vst1q_f32(out + (i * 2 + 1) * DCTSIZE + j, p2.val[1]);
			p0 = vzipq_f32(x0, x2); p1 = vzipq_f32(x1, x3); M1(0, 0) M1(1, 0)
			p0 = vzipq_f32(x4, x6); p1 = vzipq_f32(x5, x7); M1(0, 4) M1(1, 4)
#undef M1
		}
	}
//------------------------------------------------------------------------------
#elif 1 && defined(USE_AVX2)
	__m256 v0, v1, v2, v3, v4, v5, v6, v7, x0, x1, x2, x3, x4, x5, x6, x7;
	__m256 t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;

#define ADD _mm256_add_ps
#define SUB _mm256_sub_ps
#define MUL _mm256_mul_ps
#define SET1 _mm256_set1_ps

#define M1(i) _mm256_loadu_ps(in + i * DCTSIZE)
#define M2(i, t) x##i = t;
	M3
#undef M1
#undef M2

#define M5(v0, v1, v2, v3, k) \
v0 = _mm256_permute2f128_ps(x0, x4, k); \
v1 = _mm256_permute2f128_ps(x1, x5, k); \
v2 = _mm256_permute2f128_ps(x2, x6, k); \
v3 = _mm256_permute2f128_ps(x3, x7, k);
#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = _mm256_unpacklo_ps(v0, v2); \
t1 = _mm256_unpacklo_ps(v1, v3); \
x0 = _mm256_unpacklo_ps(t0, t1); \
x1 = _mm256_unpackhi_ps(t0, t1); \
t0 = _mm256_unpackhi_ps(v0, v2); \
t1 = _mm256_unpackhi_ps(v1, v3); \
x2 = _mm256_unpacklo_ps(t0, t1); \
x3 = _mm256_unpackhi_ps(t0, t1);
	M5(v0, v1, v2, v3, 0x20)
	M5(v4, v5, v6, v7, 0x31)
	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)
#define M1(i) x##i
#define M2(i, t) x##i = MUL(t, SET1(0.125f));
	M3
#undef M1
#undef M2
	M5(v0, v1, v2, v3, 0x20)
	M5(v4, v5, v6, v7, 0x31)
	M4(v0, v1, v2, v3, x0, x1, x2, x3)
	M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M5
#undef M4

#define M1(i) _mm256_storeu_ps(out + i * DCTSIZE, x##i);
	M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1
//------------------------------------------------------------------------------
#elif 1 && defined(USE_SSE2)
	__m128 *ws, buf[DCTSIZE2] ALIGN(16); int i;
	__m128 v0, v1, v2, v3, v4, v5, v6, v7, x0, x1, x2, x3, x4, x5, x6, x7;
	__m128 t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;

#define ADD _mm_add_ps
#define SUB _mm_sub_ps
#define MUL _mm_mul_ps
#define SET1 _mm_set1_ps

	ws = buf;
	for (i = 0; i < DCTSIZE; i += 4, in += 4, ws += 4) {
#define M1(i) _mm_loadu_ps(in + i * DCTSIZE)
#define M2(i, t) ws[(i & 3) + (i & 4) * 2] = t;
		M3
#undef M1
#undef M2
	}

	ws = buf;
	for (i = 0; i < DCTSIZE; i += 4, ws += 8, out += 4 * DCTSIZE) {
		__m128 c0 = SET1(0.125f);
#define M1(i) v##i = ws[i];
		M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1

#define M4(v0, v1, v2, v3, x0, x1, x2, x3) \
t0 = _mm_unpacklo_ps(v0, v2); \
t1 = _mm_unpacklo_ps(v1, v3); \
x0 = _mm_unpacklo_ps(t0, t1); \
x1 = _mm_unpackhi_ps(t0, t1); \
t0 = _mm_unpackhi_ps(v0, v2); \
t1 = _mm_unpackhi_ps(v1, v3); \
x2 = _mm_unpacklo_ps(t0, t1); \
x3 = _mm_unpackhi_ps(t0, t1);
		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)
#define M1(i) x##i
#define M2(i, t) v##i = MUL(t, c0);
		M3
#undef M1
#undef M2
		M4(v0, v1, v2, v3, x0, x1, x2, x3)
		M4(v4, v5, v6, v7, x4, x5, x6, x7)
#undef M4

#define M1(i) _mm_storeu_ps(out + (i & 3) * DCTSIZE + (i & 4), x##i);
		M1(0) M1(1) M1(2) M1(3) M1(4) M1(5) M1(6) M1(7)
#undef M1
	}
//------------------------------------------------------------------------------
#else
	float *ws, buf[DCTSIZE2]; int i;
	float t0, t1, t2, t3, t4, t5, t6, t7, z1, z2, z3, z4, z5;

#define ADD(a, b) ((a) + (b))
#define SUB(a, b) ((a) - (b))
#define MUL(a, b) ((a) * (b))
#define SET1(a) (a)

#define M1(i) in[i * DCTSIZE]
#define M2(i, t) ws[i * DCTSIZE] = t;
	ws = buf;
	for (i = 0; i < DCTSIZE; i++, in++, ws++) { M3 }
#undef M1
#undef M2
#define M1(i) ws[i]
#define M2(i, t) out[i] = (t) * 0.125f;
	ws = buf;
	for (i = 0; i < DCTSIZE; i++, ws += DCTSIZE, out += DCTSIZE) { M3 }
#undef M1
#undef M2
#endif
#undef M3
#undef ADD
#undef SUB
#undef MUL
#undef SET1
}

