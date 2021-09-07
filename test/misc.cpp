#include <stdio.h>
#include <string.h>
#include <string>
#define XBYAK_NO_OP_NAMES
#include <xbyak/xbyak.h>
#include <cybozu/inttype.hpp>
#include <cybozu/test.hpp>

using namespace Xbyak;

CYBOZU_TEST_AUTO(setSize)
{
	struct Code : Xbyak::CodeGenerator {
		Code() : Xbyak::CodeGenerator(4096)
		{
			setSize(4095);
			db(1);
			size_t size = getSize();
			CYBOZU_TEST_EQUAL(size, 4096u);
			CYBOZU_TEST_NO_EXCEPTION(setSize(size));
			CYBOZU_TEST_EXCEPTION(db(1), Xbyak::Error);
		}
	} code;
}

CYBOZU_TEST_AUTO(compOperand)
{
	using namespace Xbyak::util;
	CYBOZU_TEST_ASSERT(eax == eax);
	CYBOZU_TEST_ASSERT(ecx != xmm0);
	CYBOZU_TEST_ASSERT(ptr[eax] == ptr[eax]);
	CYBOZU_TEST_ASSERT(dword[eax] != ptr[eax]);
	CYBOZU_TEST_ASSERT(ptr[eax] != ptr[eax+3]);
}

CYBOZU_TEST_AUTO(mov_const)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			const struct {
				uint64_t v;
				int bit;
				bool error;
			} tbl[] = {
				{ uint64_t(-1), 8, false },
				{ 0x12, 8, false },
				{ 0x80, 8, false },
				{ 0xff, 8, false },
				{ 0x100, 8, true },

				{ 1, 16, false },
				{ uint64_t(-1), 16, false },
				{ 0x7fff, 16, false },
				{ 0xffff, 16, false },
				{ 0x10000, 16, true },

				{ uint64_t(-1), 32, false },
				{ 0x7fffffff, 32, false },
				{ uint64_t(-0x7fffffff), 32, false },
				{ 0xffffffff, 32, false },
				{ 0x100000000ull, 32, true },

#ifdef XBYAK64
				{ uint64_t(-1), 64, false },
				{ 0x7fffffff, 64, false },
				{ 0xffffffffffffffffull, 64, false },
				{ 0x80000000, 64, true },
				{ 0xffffffff, 64, true },
#endif
			};
			for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
				const int bit = tbl[i].bit;
				const uint64_t v = tbl[i].v;
				const Xbyak::AddressFrame& af = bit == 8 ? byte : bit == 16 ? word : bit == 32 ? dword : qword;
				if (tbl[i].error) {
					CYBOZU_TEST_EXCEPTION(mov(af[eax], v), Xbyak::Error);
				} else {
					CYBOZU_TEST_NO_EXCEPTION(mov(af[eax], v));
				}
			}
		}
	} code;
}

CYBOZU_TEST_AUTO(align)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			const size_t alignSize = 16;
			for (int padding = 0; padding < 20; padding++) {
				for (int i = 0; i < padding; i++) {
					db(1);
				}
				align(alignSize);
				CYBOZU_TEST_EQUAL(size_t(getCurr()) % alignSize, 0u);
			}
			align(alignSize);
			const uint8_t *p = getCurr();
			// do nothing if aligned
			align(alignSize);
			CYBOZU_TEST_EQUAL(p, getCurr());
		}
	} c;
}
CYBOZU_TEST_AUTO(kmask)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			CYBOZU_TEST_EXCEPTION(kmovb(k1, ax), std::exception);
			CYBOZU_TEST_EXCEPTION(kmovw(k1, ax), std::exception);
			CYBOZU_TEST_EXCEPTION(kmovd(k1, ax), std::exception);
			CYBOZU_TEST_EXCEPTION(kmovq(k1, eax), std::exception);
#ifdef XBYAK64
			CYBOZU_TEST_EXCEPTION(kmovb(k1, rax), std::exception);
			CYBOZU_TEST_EXCEPTION(kmovw(k1, rax), std::exception);
			CYBOZU_TEST_EXCEPTION(kmovd(k1, rax), std::exception);
			CYBOZU_TEST_NO_EXCEPTION(kmovq(k1, rax));
#endif
			CYBOZU_TEST_NO_EXCEPTION(vmovaps(xm0|k0, ptr[eax]));
			checkT_z();
		}
		void checkT_z()
		{
			const uint8_t *p1 = getCurr();
			vmovaps(zm0, ptr[eax]);
			const uint8_t *p2 = getCurr();
			vmovaps(zm0|T_z, ptr[eax]);
			const uint8_t *end = getCurr();
			CYBOZU_TEST_EQUAL(p2 - p1, end - p2);
			CYBOZU_TEST_EQUAL_ARRAY(p1, p2, end - p2);
		}
	} c;
}

CYBOZU_TEST_AUTO(gather)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			CYBOZU_TEST_NO_EXCEPTION(vgatherdpd(xmm1, ptr[eax+xmm2], xmm3));
			CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm1, ptr[eax+xmm1], xmm2), std::exception);
			CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm1, ptr[eax+xmm2], xmm1), std::exception);
			CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm2, ptr[eax+xmm1], xmm1), std::exception);

			CYBOZU_TEST_NO_EXCEPTION(vgatherdpd(xmm1|k2, ptr[eax+xmm2]));
			CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm1, ptr[eax+xmm2]), std::exception);
			CYBOZU_TEST_EXCEPTION(vgatherdpd(xmm1|k2, ptr[eax+xmm1]), std::exception);

			CYBOZU_TEST_NO_EXCEPTION(vpscatterdd(ptr[eax+xmm2]|k2, xmm1));
			CYBOZU_TEST_NO_EXCEPTION(vpscatterdd(ptr[eax+xmm2], xmm1|k2));
			CYBOZU_TEST_NO_EXCEPTION(vpscatterdd(ptr[eax+xmm2]|k3, xmm2));

			CYBOZU_TEST_EXCEPTION(vpscatterdd(ptr[eax+xmm2], xmm1), std::exception);
		}
	} c;
}

#ifdef XBYAK64
CYBOZU_TEST_AUTO(vfmaddps)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			v4fmaddps(zmm1, zmm8, ptr [rdx + 64]);
			v4fmaddss(xmm15, xmm8, ptr [rax + 64]);
			v4fnmaddps(zmm5 | k5, zmm2, ptr [rcx + 0x80]);
			v4fnmaddss(xmm31, xmm2, ptr [rsp + 0x80]);
			vp4dpwssd(zmm23 | k7 | T_z, zmm1, ptr [rax + 64]);
			vp4dpwssds(zmm10 | k4, zmm3, ptr [rsp + rax * 4 + 64]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf2, 0x3f, 0x48, 0x9a, 0x4a, 0x04,
		0x62, 0x72, 0x3f, 0x08, 0x9b, 0x78, 0x04,
		0x62, 0xf2, 0x6f, 0x4d, 0xaa, 0x69, 0x08,
		0x62, 0x62, 0x6f, 0x08, 0xab, 0x7c, 0x24, 0x08,
		0x62, 0xe2, 0x77, 0xcf, 0x52, 0x78, 0x04,
		0x62, 0x72, 0x67, 0x4c, 0x53, 0x54, 0x84, 0x04,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(vaes)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vaesdec(xmm20, xmm30, ptr [rcx + 64]);
			vaesdec(ymm1, ymm2, ptr [rcx + 64]);
			vaesdec(zmm1, zmm2, ptr [rcx + 64]);

			vaesdeclast(xmm20, xmm30, ptr [rax + 64]);
			vaesdeclast(ymm20, ymm30, ptr [rax + 64]);
			vaesdeclast(zmm20, zmm30, ptr [rax + 64]);

			vaesenc(xmm20, xmm30, ptr [rcx + 64]);
			vaesenc(ymm1, ymm2, ptr [rcx + 64]);
			vaesenc(zmm1, zmm2, ptr [rcx + 64]);

			vaesenclast(xmm20, xmm30, ptr [rax + 64]);
			vaesenclast(ymm20, ymm30, ptr [rax + 64]);
			vaesenclast(zmm20, zmm30, ptr [rax + 64]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xE2, 0x0D, 0x00, 0xDE, 0x61, 0x04,
		0xC4, 0xE2, 0x6D, 0xDE, 0x49, 0x40,
		0x62, 0xF2, 0x6D, 0x48, 0xDE, 0x49, 0x01,

		0x62, 0xE2, 0x0D, 0x00, 0xDF, 0x60, 0x04,
		0x62, 0xE2, 0x0D, 0x20, 0xDF, 0x60, 0x02,
		0x62, 0xE2, 0x0D, 0x40, 0xDF, 0x60, 0x01,

		0x62, 0xE2, 0x0D, 0x00, 0xDC, 0x61, 0x04,
		0xC4, 0xE2, 0x6D, 0xDC, 0x49, 0x40,
		0x62, 0xF2, 0x6D, 0x48, 0xDC, 0x49, 0x01,

		0x62, 0xE2, 0x0D, 0x00, 0xDD, 0x60, 0x04,
		0x62, 0xE2, 0x0D, 0x20, 0xDD, 0x60, 0x02,
		0x62, 0xE2, 0x0D, 0x40, 0xDD, 0x60, 0x01,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(vpclmulqdq)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vpclmulqdq(xmm2, xmm3, ptr [rax + 64], 3);
			vpclmulqdq(ymm2, ymm3, ptr [rax + 64], 3);
			vpclmulqdq(zmm2, zmm3, ptr [rax + 64], 3);

			vpclmulqdq(xmm20, xmm3, ptr [rax + 64], 3);
			vpclmulqdq(ymm20, ymm3, ptr [rax + 64], 3);
			vpclmulqdq(zmm20, zmm3, ptr [rax + 64], 3);
		}
	} c;
	const uint8_t tbl[] = {
		0xc4, 0xe3, 0x61, 0x44, 0x50, 0x40, 0x03,
		0xc4, 0xe3, 0x65, 0x44, 0x50, 0x40, 0x03,
		0x62, 0xf3, 0x65, 0x48, 0x44, 0x50, 0x01, 0x03,
		0x62, 0xe3, 0x65, 0x08, 0x44, 0x60, 0x04, 0x03,
		0x62, 0xe3, 0x65, 0x28, 0x44, 0x60, 0x02, 0x03,
		0x62, 0xe3, 0x65, 0x48, 0x44, 0x60, 0x01, 0x03,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(vcompressb_w)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vcompressb(ptr[rax + 64], xmm1);
			vcompressb(xmm30 | k5, xmm1);
			vcompressb(ptr[rax + 64], ymm1);
			vcompressb(ymm30 | k3 |T_z, ymm1);
			vcompressb(ptr[rax + 64], zmm1);
			vcompressb(zmm30 | k2 |T_z, zmm1);

			vcompressw(ptr[rax + 64], xmm1);
			vcompressw(xmm30 | k5, xmm1);
			vcompressw(ptr[rax + 64], ymm1);
			vcompressw(ymm30 | k3 |T_z, ymm1);
			vcompressw(ptr[rax + 64], zmm1);
			vcompressw(zmm30 | k2 |T_z, zmm1);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf2, 0x7d, 0x08, 0x63, 0x48, 0x40,
		0x62, 0x92, 0x7d, 0x0d, 0x63, 0xce,
		0x62, 0xf2, 0x7d, 0x28, 0x63, 0x48, 0x40,
		0x62, 0x92, 0x7d, 0xab, 0x63, 0xce,
		0x62, 0xf2, 0x7d, 0x48, 0x63, 0x48, 0x40,
		0x62, 0x92, 0x7d, 0xca, 0x63, 0xce,

		0x62, 0xf2, 0xfd, 0x08, 0x63, 0x48, 0x20,
		0x62, 0x92, 0xfd, 0x0d, 0x63, 0xce,
		0x62, 0xf2, 0xfd, 0x28, 0x63, 0x48, 0x20,
		0x62, 0x92, 0xfd, 0xab, 0x63, 0xce,
		0x62, 0xf2, 0xfd, 0x48, 0x63, 0x48, 0x20,
		0x62, 0x92, 0xfd, 0xca, 0x63, 0xce,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(shld)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vpshldw(xmm5|k3|T_z, xmm2, ptr [rax + 0x40], 5);
			vpshldw(ymm5|k3|T_z, ymm2, ptr [rax + 0x40], 5);
			vpshldw(zmm5|k3|T_z, zmm2, ptr [rax + 0x40], 5);

			vpshldd(xmm5|k3|T_z, xmm2, ptr [rax + 0x40], 5);
			vpshldd(ymm5|k3|T_z, ymm2, ptr [rax + 0x40], 5);
			vpshldd(zmm5|k3|T_z, zmm2, ptr [rax + 0x40], 5);

			vpshldq(xmm5|k3|T_z, xmm2, ptr [rax + 0x40], 5);
			vpshldq(ymm5|k3|T_z, ymm2, ptr [rax + 0x40], 5);
			vpshldq(zmm5|k3|T_z, zmm2, ptr [rax + 0x40], 5);

			vpshldvw(xmm5|k3|T_z, xmm2, ptr [rax + 0x40]);
			vpshldvw(ymm5|k3|T_z, ymm2, ptr [rax + 0x40]);
			vpshldvw(zmm5|k3|T_z, zmm2, ptr [rax + 0x40]);

			vpshldvd(xmm5|k3|T_z, xmm2, ptr [rax + 0x40]);
			vpshldvd(ymm5|k3|T_z, ymm2, ptr [rax + 0x40]);
			vpshldvd(zmm5|k3|T_z, zmm2, ptr [rax + 0x40]);

			vpshldvq(xmm5|k3|T_z, xmm2, ptr [rax + 0x40]);
			vpshldvq(ymm5|k3|T_z, ymm2, ptr [rax + 0x40]);
			vpshldvq(zmm5|k3|T_z, zmm2, ptr [rax + 0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf3, 0xed, 0x8b, 0x70, 0x68, 0x04, 0x05,
		0x62, 0xf3, 0xed, 0xab, 0x70, 0x68, 0x02, 0x05,
		0x62, 0xf3, 0xed, 0xcb, 0x70, 0x68, 0x01, 0x05,

		0x62, 0xf3, 0x6d, 0x8b, 0x71, 0x68, 0x04, 0x05,
		0x62, 0xf3, 0x6d, 0xab, 0x71, 0x68, 0x02, 0x05,
		0x62, 0xf3, 0x6d, 0xcb, 0x71, 0x68, 0x01, 0x05,

		0x62, 0xf3, 0xed, 0x8b, 0x71, 0x68, 0x04, 0x05,
		0x62, 0xf3, 0xed, 0xab, 0x71, 0x68, 0x02, 0x05,
		0x62, 0xf3, 0xed, 0xcb, 0x71, 0x68, 0x01, 0x05,

		0x62, 0xf2, 0xed, 0x8b, 0x70, 0x68, 0x04,
		0x62, 0xf2, 0xed, 0xab, 0x70, 0x68, 0x02,
		0x62, 0xf2, 0xed, 0xcb, 0x70, 0x68, 0x01,

		0x62, 0xf2, 0x6d, 0x8b, 0x71, 0x68, 0x04,
		0x62, 0xf2, 0x6d, 0xab, 0x71, 0x68, 0x02,
		0x62, 0xf2, 0x6d, 0xcb, 0x71, 0x68, 0x01,

		0x62, 0xf2, 0xed, 0x8b, 0x71, 0x68, 0x04,
		0x62, 0xf2, 0xed, 0xab, 0x71, 0x68, 0x02,
		0x62, 0xf2, 0xed, 0xcb, 0x71, 0x68, 0x01,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(shrd)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vpshrdw(xmm5|k3|T_z, xmm2, ptr [rax + 0x40], 5);
			vpshrdw(ymm5|k3|T_z, ymm2, ptr [rax + 0x40], 5);
			vpshrdw(zmm5|k3|T_z, zmm2, ptr [rax + 0x40], 5);

			vpshrdd(xmm5|k3|T_z, xmm2, ptr [rax + 0x40], 5);
			vpshrdd(ymm5|k3|T_z, ymm2, ptr [rax + 0x40], 5);
			vpshrdd(zmm5|k3|T_z, zmm2, ptr [rax + 0x40], 5);

			vpshrdq(xmm5|k3|T_z, xmm2, ptr [rax + 0x40], 5);
			vpshrdq(ymm5|k3|T_z, ymm2, ptr [rax + 0x40], 5);
			vpshrdq(zmm5|k3|T_z, zmm2, ptr [rax + 0x40], 5);

			vpshrdvw(xmm5|k3|T_z, xmm2, ptr [rax + 0x40]);
			vpshrdvw(ymm5|k3|T_z, ymm2, ptr [rax + 0x40]);
			vpshrdvw(zmm5|k3|T_z, zmm2, ptr [rax + 0x40]);

			vpshrdvd(xmm5|k3|T_z, xmm2, ptr [rax + 0x40]);
			vpshrdvd(ymm5|k3|T_z, ymm2, ptr [rax + 0x40]);
			vpshrdvd(zmm5|k3|T_z, zmm2, ptr [rax + 0x40]);

			vpshrdvq(xmm5|k3|T_z, xmm2, ptr [rax + 0x40]);
			vpshrdvq(ymm5|k3|T_z, ymm2, ptr [rax + 0x40]);
			vpshrdvq(zmm5|k3|T_z, zmm2, ptr [rax + 0x40]);

			vpshrdd(xmm5|k3|T_z, xmm2, ptr_b [rax + 0x40], 5);
			vpshrdd(ymm5|k3|T_z, ymm2, ptr_b [rax + 0x40], 5);
			vpshrdd(zmm5|k3|T_z, zmm2, ptr_b [rax + 0x40], 5);

			vpshrdq(xmm5|k3|T_z, xmm2, ptr_b [rax + 0x40], 5);
			vpshrdq(ymm5|k3|T_z, ymm2, ptr_b [rax + 0x40], 5);
			vpshrdq(zmm5|k3|T_z, zmm2, ptr_b [rax + 0x40], 5);

			vpshrdvd(xmm5|k3|T_z, xmm2, ptr_b [rax + 0x40]);
			vpshrdvd(ymm5|k3|T_z, ymm2, ptr_b [rax + 0x40]);
			vpshrdvd(zmm5|k3|T_z, zmm2, ptr_b [rax + 0x40]);

			vpshrdvq(xmm5|k3|T_z, xmm2, ptr_b [rax + 0x40]);
			vpshrdvq(ymm5|k3|T_z, ymm2, ptr_b [rax + 0x40]);
			vpshrdvq(zmm5|k3|T_z, zmm2, ptr_b [rax + 0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf3, 0xed, 0x8b, 0x72, 0x68, 0x04, 0x05,
		0x62, 0xf3, 0xed, 0xab, 0x72, 0x68, 0x02, 0x05,
		0x62, 0xf3, 0xed, 0xcb, 0x72, 0x68, 0x01, 0x05,

		0x62, 0xf3, 0x6d, 0x8b, 0x73, 0x68, 0x04, 0x05,
		0x62, 0xf3, 0x6d, 0xab, 0x73, 0x68, 0x02, 0x05,
		0x62, 0xf3, 0x6d, 0xcb, 0x73, 0x68, 0x01, 0x05,

		0x62, 0xf3, 0xed, 0x8b, 0x73, 0x68, 0x04, 0x05,
		0x62, 0xf3, 0xed, 0xab, 0x73, 0x68, 0x02, 0x05,
		0x62, 0xf3, 0xed, 0xcb, 0x73, 0x68, 0x01, 0x05,

		0x62, 0xf2, 0xed, 0x8b, 0x72, 0x68, 0x04,
		0x62, 0xf2, 0xed, 0xab, 0x72, 0x68, 0x02,
		0x62, 0xf2, 0xed, 0xcb, 0x72, 0x68, 0x01,

		0x62, 0xf2, 0x6d, 0x8b, 0x73, 0x68, 0x04,
		0x62, 0xf2, 0x6d, 0xab, 0x73, 0x68, 0x02,
		0x62, 0xf2, 0x6d, 0xcb, 0x73, 0x68, 0x01,

		0x62, 0xf2, 0xed, 0x8b, 0x73, 0x68, 0x04,
		0x62, 0xf2, 0xed, 0xab, 0x73, 0x68, 0x02,
		0x62, 0xf2, 0xed, 0xcb, 0x73, 0x68, 0x01,

		0x62, 0xf3, 0x6d, 0x9b, 0x73, 0x68, 0x10, 0x05,
		0x62, 0xf3, 0x6d, 0xbb, 0x73, 0x68, 0x10, 0x05,
		0x62, 0xf3, 0x6d, 0xdb, 0x73, 0x68, 0x10, 0x05,

		0x62, 0xf3, 0xed, 0x9b, 0x73, 0x68, 0x08, 0x05,
		0x62, 0xf3, 0xed, 0xbb, 0x73, 0x68, 0x08, 0x05,
		0x62, 0xf3, 0xed, 0xdb, 0x73, 0x68, 0x08, 0x05,

		0x62, 0xf2, 0x6d, 0x9b, 0x73, 0x68, 0x10,
		0x62, 0xf2, 0x6d, 0xbb, 0x73, 0x68, 0x10,
		0x62, 0xf2, 0x6d, 0xdb, 0x73, 0x68, 0x10,

		0x62, 0xf2, 0xed, 0x9b, 0x73, 0x68, 0x08,
		0x62, 0xf2, 0xed, 0xbb, 0x73, 0x68, 0x08,
		0x62, 0xf2, 0xed, 0xdb, 0x73, 0x68, 0x08,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(vpopcnt)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vpopcntb(xmm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntb(ymm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntb(zmm5|k3|T_z, ptr [rax + 0x40]);

			vpopcntw(xmm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntw(ymm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntw(zmm5|k3|T_z, ptr [rax + 0x40]);

			vpopcntd(xmm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntd(ymm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntd(zmm5|k3|T_z, ptr [rax + 0x40]);

			vpopcntd(xmm5|k3|T_z, ptr_b [rax + 0x40]);
			vpopcntd(ymm5|k3|T_z, ptr_b [rax + 0x40]);
			vpopcntd(zmm5|k3|T_z, ptr_b [rax + 0x40]);

			vpopcntq(xmm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntq(ymm5|k3|T_z, ptr [rax + 0x40]);
			vpopcntq(zmm5|k3|T_z, ptr [rax + 0x40]);

			vpopcntq(xmm5|k3|T_z, ptr_b [rax + 0x40]);
			vpopcntq(ymm5|k3|T_z, ptr_b [rax + 0x40]);
			vpopcntq(zmm5|k3|T_z, ptr_b [rax + 0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf2, 0x7d, 0x8b, 0x54, 0x68, 0x04,
		0x62, 0xf2, 0x7d, 0xab, 0x54, 0x68, 0x02,
		0x62, 0xf2, 0x7d, 0xcb, 0x54, 0x68, 0x01,

		0x62, 0xf2, 0xfd, 0x8b, 0x54, 0x68, 0x04,
		0x62, 0xf2, 0xfd, 0xab, 0x54, 0x68, 0x02,
		0x62, 0xf2, 0xfd, 0xcb, 0x54, 0x68, 0x01,

		0x62, 0xf2, 0x7d, 0x8b, 0x55, 0x68, 0x04,
		0x62, 0xf2, 0x7d, 0xab, 0x55, 0x68, 0x02,
		0x62, 0xf2, 0x7d, 0xcb, 0x55, 0x68, 0x01,

		0x62, 0xf2, 0x7d, 0x9b, 0x55, 0x68, 0x10,
		0x62, 0xf2, 0x7d, 0xbb, 0x55, 0x68, 0x10,
		0x62, 0xf2, 0x7d, 0xdb, 0x55, 0x68, 0x10,

		0x62, 0xf2, 0xfd, 0x8b, 0x55, 0x68, 0x04,
		0x62, 0xf2, 0xfd, 0xab, 0x55, 0x68, 0x02,
		0x62, 0xf2, 0xfd, 0xcb, 0x55, 0x68, 0x01,

		0x62, 0xf2, 0xfd, 0x9b, 0x55, 0x68, 0x08,
		0x62, 0xf2, 0xfd, 0xbb, 0x55, 0x68, 0x08,
		0x62, 0xf2, 0xfd, 0xdb, 0x55, 0x68, 0x08,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(vpdpbus)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vpdpbusd(xmm5|k3|T_z, xmm20, ptr [rax + 0x40]);
			vpdpbusd(ymm5|k3|T_z, ymm20, ptr [rax + 0x40]);
			vpdpbusd(zmm5|k3|T_z, zmm20, ptr [rax + 0x40]);

			vpdpbusd(xmm5|k3|T_z, xmm20, ptr_b [rax + 0x40]);
			vpdpbusd(ymm5|k3|T_z, ymm20, ptr_b [rax + 0x40]);
			vpdpbusd(zmm5|k3|T_z, zmm20, ptr_b [rax + 0x40]);

			vpdpbusds(xmm5|k3|T_z, xmm20, ptr [rax + 0x40]);
			vpdpbusds(ymm5|k3|T_z, ymm20, ptr [rax + 0x40]);
			vpdpbusds(zmm5|k3|T_z, zmm20, ptr [rax + 0x40]);

			vpdpbusds(xmm5|k3|T_z, xmm20, ptr_b [rax + 0x40]);
			vpdpbusds(ymm5|k3|T_z, ymm20, ptr_b [rax + 0x40]);
			vpdpbusds(zmm5|k3|T_z, zmm20, ptr_b [rax + 0x40]);

			vpdpwssd(xmm5|k3|T_z, xmm20, ptr [rax + 0x40]);
			vpdpwssd(ymm5|k3|T_z, ymm20, ptr [rax + 0x40]);
			vpdpwssd(zmm5|k3|T_z, zmm20, ptr [rax + 0x40]);

			vpdpwssd(xmm5|k3|T_z, xmm20, ptr_b [rax + 0x40]);
			vpdpwssd(ymm5|k3|T_z, ymm20, ptr_b [rax + 0x40]);
			vpdpwssd(zmm5|k3|T_z, zmm20, ptr_b [rax + 0x40]);

			vpdpwssds(xmm5|k3|T_z, xmm20, ptr [rax + 0x40]);
			vpdpwssds(ymm5|k3|T_z, ymm20, ptr [rax + 0x40]);
			vpdpwssds(zmm5|k3|T_z, zmm20, ptr [rax + 0x40]);

			vpdpwssds(xmm5|k3|T_z, xmm20, ptr_b [rax + 0x40]);
			vpdpwssds(ymm5|k3|T_z, ymm20, ptr_b [rax + 0x40]);
			vpdpwssds(zmm5|k3|T_z, zmm20, ptr_b [rax + 0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf2, 0x5d, 0x83, 0x50, 0x68, 0x04,
		0x62, 0xf2, 0x5d, 0xa3, 0x50, 0x68, 0x02,
		0x62, 0xf2, 0x5d, 0xc3, 0x50, 0x68, 0x01,

		0x62, 0xf2, 0x5d, 0x93, 0x50, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xb3, 0x50, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xd3, 0x50, 0x68, 0x10,

		0x62, 0xf2, 0x5d, 0x83, 0x51, 0x68, 0x04,
		0x62, 0xf2, 0x5d, 0xa3, 0x51, 0x68, 0x02,
		0x62, 0xf2, 0x5d, 0xc3, 0x51, 0x68, 0x01,

		0x62, 0xf2, 0x5d, 0x93, 0x51, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xb3, 0x51, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xd3, 0x51, 0x68, 0x10,

		0x62, 0xf2, 0x5d, 0x83, 0x52, 0x68, 0x04,
		0x62, 0xf2, 0x5d, 0xa3, 0x52, 0x68, 0x02,
		0x62, 0xf2, 0x5d, 0xc3, 0x52, 0x68, 0x01,

		0x62, 0xf2, 0x5d, 0x93, 0x52, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xb3, 0x52, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xd3, 0x52, 0x68, 0x10,

		0x62, 0xf2, 0x5d, 0x83, 0x53, 0x68, 0x04,
		0x62, 0xf2, 0x5d, 0xa3, 0x53, 0x68, 0x02,
		0x62, 0xf2, 0x5d, 0xc3, 0x53, 0x68, 0x01,

		0x62, 0xf2, 0x5d, 0x93, 0x53, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xb3, 0x53, 0x68, 0x10,
		0x62, 0xf2, 0x5d, 0xd3, 0x53, 0x68, 0x10,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(vexpand_vpshufbitqmb)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vpexpandb(xmm5|k3|T_z, xmm30);
			vpexpandb(ymm5|k3|T_z, ymm30);
			vpexpandb(zmm5|k3|T_z, zmm30);
			vpexpandb(xmm5|k3|T_z, ptr [rax + 0x40]);
			vpexpandb(ymm5|k3|T_z, ptr [rax + 0x40]);
			vpexpandb(zmm5|k3|T_z, ptr [rax + 0x40]);

			vpexpandw(xmm5|k3|T_z, xmm30);
			vpexpandw(ymm5|k3|T_z, ymm30);
			vpexpandw(zmm5|k3|T_z, zmm30);
			vpexpandw(xmm5|k3|T_z, ptr [rax + 0x40]);
			vpexpandw(ymm5|k3|T_z, ptr [rax + 0x40]);
			vpexpandw(zmm5|k3|T_z, ptr [rax + 0x40]);

			vpshufbitqmb(k1|k2, xmm2, ptr [rax + 0x40]);
			vpshufbitqmb(k1|k2, ymm2, ptr [rax + 0x40]);
			vpshufbitqmb(k1|k2, zmm2, ptr [rax + 0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0x92, 0x7d, 0x8b, 0x62, 0xee,
		0x62, 0x92, 0x7d, 0xab, 0x62, 0xee,
		0x62, 0x92, 0x7d, 0xcb, 0x62, 0xee,
		0x62, 0xf2, 0x7d, 0x8b, 0x62, 0x68, 0x40,
		0x62, 0xf2, 0x7d, 0xab, 0x62, 0x68, 0x40,
		0x62, 0xf2, 0x7d, 0xcb, 0x62, 0x68, 0x40,

		0x62, 0x92, 0xfd, 0x8b, 0x62, 0xee,
		0x62, 0x92, 0xfd, 0xab, 0x62, 0xee,
		0x62, 0x92, 0xfd, 0xcb, 0x62, 0xee,
		0x62, 0xf2, 0xfd, 0x8b, 0x62, 0x68, 0x20,
		0x62, 0xf2, 0xfd, 0xab, 0x62, 0x68, 0x20,
		0x62, 0xf2, 0xfd, 0xcb, 0x62, 0x68, 0x20,

		0x62, 0xf2, 0x6d, 0x0a, 0x8f, 0x48, 0x04,
		0x62, 0xf2, 0x6d, 0x2a, 0x8f, 0x48, 0x02,
		0x62, 0xf2, 0x6d, 0x4a, 0x8f, 0x48, 0x01,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
CYBOZU_TEST_AUTO(gf2)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			///
			gf2p8affineinvqb(xmm1, xmm2, 3);
			gf2p8affineinvqb(xmm1, ptr [rax + 0x40], 3);

			vgf2p8affineinvqb(xmm1, xmm5, xmm2, 3);
			vgf2p8affineinvqb(ymm1, ymm5, ymm2, 3);
			vgf2p8affineinvqb(xmm1, xmm5, ptr [rax + 0x40], 3);
			vgf2p8affineinvqb(ymm1, ymm5, ptr [rax + 0x40], 3);

			vgf2p8affineinvqb(xmm30, xmm31, xmm4, 5);
			vgf2p8affineinvqb(ymm30, ymm31, ymm4, 5);
			vgf2p8affineinvqb(zmm30, zmm31, zmm4, 5);

			vgf2p8affineinvqb(xmm30|k1|T_z, xmm5, ptr [rax + 0x40], 5);
			vgf2p8affineinvqb(ymm30|k1|T_z, ymm5, ptr [rax + 0x40], 5);
			vgf2p8affineinvqb(zmm30|k1|T_z, zmm5, ptr [rax + 0x40], 5);

			vgf2p8affineinvqb(xmm30|k1|T_z, xmm5, ptr_b [rax + 0x40], 5);
			vgf2p8affineinvqb(ymm30|k1|T_z, ymm5, ptr_b [rax + 0x40], 5);
			vgf2p8affineinvqb(zmm30|k1|T_z, zmm5, ptr_b [rax + 0x40], 5);
			///
			gf2p8affineqb(xmm1, xmm2, 3);
			gf2p8affineqb(xmm1, ptr [rax + 0x40], 3);

			vgf2p8affineqb(xmm1, xmm5, xmm2, 3);
			vgf2p8affineqb(ymm1, ymm5, ymm2, 3);
			vgf2p8affineqb(xmm1, xmm5, ptr [rax + 0x40], 3);
			vgf2p8affineqb(ymm1, ymm5, ptr [rax + 0x40], 3);

			vgf2p8affineqb(xmm30, xmm31, xmm4, 5);
			vgf2p8affineqb(ymm30, ymm31, ymm4, 5);
			vgf2p8affineqb(zmm30, zmm31, zmm4, 5);

			vgf2p8affineqb(xmm30|k1|T_z, xmm5, ptr [rax + 0x40], 5);
			vgf2p8affineqb(ymm30|k1|T_z, ymm5, ptr [rax + 0x40], 5);
			vgf2p8affineqb(zmm30|k1|T_z, zmm5, ptr [rax + 0x40], 5);

			vgf2p8affineqb(xmm30|k1|T_z, xmm5, ptr_b [rax + 0x40], 5);
			vgf2p8affineqb(ymm30|k1|T_z, ymm5, ptr_b [rax + 0x40], 5);
			vgf2p8affineqb(zmm30|k1|T_z, zmm5, ptr_b [rax + 0x40], 5);
			///
			gf2p8mulb(xmm1, xmm2);
			gf2p8mulb(xmm1, ptr [rax + 0x40]);

			vgf2p8mulb(xmm1, xmm5, xmm2);
			vgf2p8mulb(ymm1, ymm5, ymm2);
			vgf2p8mulb(xmm1, xmm5, ptr [rax + 0x40]);
			vgf2p8mulb(ymm1, ymm5, ptr [rax + 0x40]);

			vgf2p8mulb(xmm30, xmm31, xmm4);
			vgf2p8mulb(ymm30, ymm31, ymm4);
			vgf2p8mulb(zmm30, zmm31, zmm4);

			vgf2p8mulb(xmm30|k1|T_z, xmm5, ptr [rax + 0x40]);
			vgf2p8mulb(ymm30|k1|T_z, ymm5, ptr [rax + 0x40]);
			vgf2p8mulb(zmm30|k1|T_z, zmm5, ptr [rax + 0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		0x66, 0x0f, 0x3a, 0xcf, 0xca, 0x03,
		0x66, 0x0f, 0x3a, 0xcf, 0x48, 0x40, 0x03,
		0xc4, 0xe3, 0xd1, 0xcf, 0xca, 0x03,
		0xc4, 0xe3, 0xd5, 0xcf, 0xca, 0x03,
		0xc4, 0xe3, 0xd1, 0xcf, 0x48, 0x40, 0x03,
		0xc4, 0xe3, 0xd5, 0xcf, 0x48, 0x40, 0x03,
		0x62, 0x63, 0x85, 0x00, 0xcf, 0xf4, 0x05,
		0x62, 0x63, 0x85, 0x20, 0xcf, 0xf4, 0x05,
		0x62, 0x63, 0x85, 0x40, 0xcf, 0xf4, 0x05,
		0x62, 0x63, 0xd5, 0x89, 0xcf, 0x70, 0x04, 0x05,
		0x62, 0x63, 0xd5, 0xa9, 0xcf, 0x70, 0x02, 0x05,
		0x62, 0x63, 0xd5, 0xc9, 0xcf, 0x70, 0x01, 0x05,
		0x62, 0x63, 0xd5, 0x99, 0xcf, 0x70, 0x08, 0x05,
		0x62, 0x63, 0xd5, 0xb9, 0xcf, 0x70, 0x08, 0x05,
		0x62, 0x63, 0xd5, 0xd9, 0xcf, 0x70, 0x08, 0x05,

		0x66, 0x0f, 0x3a, 0xce, 0xca, 0x03,
		0x66, 0x0f, 0x3a, 0xce, 0x48, 0x40, 0x03,
		0xc4, 0xe3, 0xd1, 0xce, 0xca, 0x03,
		0xc4, 0xe3, 0xd5, 0xce, 0xca, 0x03,
		0xc4, 0xe3, 0xd1, 0xce, 0x48, 0x40, 0x03,
		0xc4, 0xe3, 0xd5, 0xce, 0x48, 0x40, 0x03,
		0x62, 0x63, 0x85, 0x00, 0xce, 0xf4, 0x05,
		0x62, 0x63, 0x85, 0x20, 0xce, 0xf4, 0x05,
		0x62, 0x63, 0x85, 0x40, 0xce, 0xf4, 0x05,
		0x62, 0x63, 0xd5, 0x89, 0xce, 0x70, 0x04, 0x05,
		0x62, 0x63, 0xd5, 0xa9, 0xce, 0x70, 0x02, 0x05,
		0x62, 0x63, 0xd5, 0xc9, 0xce, 0x70, 0x01, 0x05,
		0x62, 0x63, 0xd5, 0x99, 0xce, 0x70, 0x08, 0x05,
		0x62, 0x63, 0xd5, 0xb9, 0xce, 0x70, 0x08, 0x05,
		0x62, 0x63, 0xd5, 0xd9, 0xce, 0x70, 0x08, 0x05,

		0x66, 0x0f, 0x38, 0xcf, 0xca,
		0x66, 0x0f, 0x38, 0xcf, 0x48, 0x40,
		0xc4, 0xe2, 0x51, 0xcf, 0xca,
		0xc4, 0xe2, 0x55, 0xcf, 0xca,
		0xc4, 0xe2, 0x51, 0xcf, 0x48, 0x40,
		0xc4, 0xe2, 0x55, 0xcf, 0x48, 0x40,
		0x62, 0x62, 0x05, 0x00, 0xcf, 0xf4,
		0x62, 0x62, 0x05, 0x20, 0xcf, 0xf4,
		0x62, 0x62, 0x05, 0x40, 0xcf, 0xf4,
		0x62, 0x62, 0x55, 0x89, 0xcf, 0x70, 0x04,
		0x62, 0x62, 0x55, 0xa9, 0xcf, 0x70, 0x02,
		0x62, 0x62, 0x55, 0xc9, 0xcf, 0x70, 0x01,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}

CYBOZU_TEST_AUTO(bf16)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vcvtne2ps2bf16(xmm0 | k1, xmm1, ptr [rax + 64]);
			vcvtne2ps2bf16(ymm0 | k1 | T_z, ymm0, ptr [rax + 64]);
			vcvtne2ps2bf16(zmm0 | k1, zmm1, ptr [rax + 64]);

			vcvtneps2bf16(xmm0, xword [rax + 64]);
			vcvtneps2bf16(xmm0 | k1, yword [rax + 64]);
			vcvtneps2bf16(ymm0 | k1, zword [rax + 64]);
			vcvtneps2bf16(ymm0 | k1, ptr [rax + 64]);

			vdpbf16ps(xmm0 | k1, xmm1, ptr [rax + 64]);
			vdpbf16ps(ymm0 | k1, ymm1, ptr [rax + 64]);
			vdpbf16ps(zmm0 | k1, zmm1, ptr [rax + 64]);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xf2, 0x77, 0x09, 0x72, 0x40, 0x04,
		0x62, 0xf2, 0x7f, 0xa9, 0x72, 0x40, 0x02,
		0x62, 0xf2, 0x77, 0x49, 0x72, 0x40, 0x01,

		0x62, 0xf2, 0x7e, 0x08, 0x72, 0x40, 0x04,
		0x62, 0xf2, 0x7e, 0x29, 0x72, 0x40, 0x02,
		0x62, 0xf2, 0x7e, 0x49, 0x72, 0x40, 0x01,
		0x62, 0xf2, 0x7e, 0x49, 0x72, 0x40, 0x01,

		0x62, 0xf2, 0x76, 0x09, 0x52, 0x40, 0x04,
		0x62, 0xf2, 0x76, 0x29, 0x52, 0x40, 0x02,
		0x62, 0xf2, 0x76, 0x49, 0x52, 0x40, 0x01,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}

CYBOZU_TEST_AUTO(AMX)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			ldtilecfg(ptr[rax + rcx * 4 + 64]);
			sttilecfg(ptr[rsp + rax * 8 + 128]);
			tileloadd(tmm3, ptr[rdi + rdx * 2 + 8]);
			tileloaddt1(tmm4, ptr[r8 + r9 + 32]);
			tilerelease();
			tilestored(ptr[r10 + r11 * 2 + 32], tmm2);
			tilezero(tmm7);
			tdpbssd(tmm1, tmm2, tmm3);
			tdpbsud(tmm2, tmm3, tmm4);
			tdpbusd(tmm3, tmm4, tmm5);
			tdpbuud(tmm4, tmm5, tmm6);
			tdpbf16ps(tmm5, tmm6, tmm7);
		}
	} c;
	// generated code by patch
	const uint8_t tbl[] = {
		0xc4, 0xe2, 0x78, 0x49, 0x44, 0x88, 0x40, 0xc4, 0xe2, 0x79, 0x49, 0x84, 0xc4, 0x80, 0x00, 0x00,
		0x00, 0xc4, 0xe2, 0x7b, 0x4b, 0x5c, 0x57, 0x08, 0xc4, 0x82, 0x79, 0x4b, 0x64, 0x08, 0x20, 0xc4,
		0xe2, 0x78, 0x49, 0xc0, 0xc4, 0x82, 0x7a, 0x4b, 0x54, 0x5a, 0x20, 0xc4, 0xe2, 0x7b, 0x49, 0xf8,
		0xc4, 0xe2, 0x63, 0x5e, 0xca, 0xc4, 0xe2, 0x5a, 0x5e, 0xd3, 0xc4, 0xe2, 0x51, 0x5e, 0xdc, 0xc4,
		0xe2, 0x48, 0x5e, 0xe5, 0xc4, 0xe2, 0x42, 0x5c, 0xee,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}

CYBOZU_TEST_AUTO(tileloadd)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			tileloadd(tmm1, ptr[r8+r8]);
			tileloadd(tmm1, ptr[rax+rcx*4]);
			tileloadd(tmm1, ptr[r8+r9*1+0x40]);
		}
		void notSupported()
		{
			tileloadd(tmm1, ptr[r8]);
		}
		void notSupported2()
		{
			tileloadd(tmm1, ptr[r8*2]);
		}
	} c;
	const uint8_t tbl[] = {
		0xC4, 0x82, 0x7B, 0x4B, 0x0C, 0x00,
		0xC4, 0xE2, 0x7B, 0x4B, 0x0C, 0x88,
		0xC4, 0x82, 0x7B, 0x4B, 0x4C, 0x08, 0x40,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);

	// current version does not support this sibmem format
	CYBOZU_TEST_EXCEPTION(c.notSupported(), std::exception);
	CYBOZU_TEST_EXCEPTION(c.notSupported2(), std::exception);
}

CYBOZU_TEST_AUTO(vnni)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			// default encoding is EVEX
			vpdpbusd(xm0, xm1, xm2);
			vpdpbusd(xm0, xm1, xm2, EvexEncoding); // EVEX
			vpdpbusd(xm0, xm1, xm2, VexEncoding); // VEX
		}
		void badVex()
		{
			vpdpbusd(xm0, xm1, xm31, VexEncoding);
		}
	} c;
	const uint8_t tbl[] = {
		0x62, 0xF2, 0x75, 0x08, 0x50, 0xC2,
		0x62, 0xF2, 0x75, 0x08, 0x50, 0xC2,
		0xC4, 0xE2, 0x71, 0x50, 0xC2,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);

	CYBOZU_TEST_EXCEPTION(c.badVex(), std::exception);
}

CYBOZU_TEST_AUTO(vaddph)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			vaddph(zmm0, zmm1, ptr[rax+64]);
			vaddph(ymm0, ymm1, ptr[rax+64]);
			vaddph(xmm0, xmm1, ptr[rax+64]);

			vaddph(zmm0, zmm1, ptr_b[rax+64]);
			vaddph(ymm0, ymm1, ptr_b[rax+64]);
			vaddph(xmm0, xmm1, ptr_b[rax+64]);

			vaddsh(xmm0, xmm15, ptr[rax+64]);
			vaddsh(xmm0|k5|T_z|T_rd_sae, xmm15, xmm3);

			vcmpph(k1, xm15, ptr[rax+64], 1);
			vcmpph(k2, ym15, ptr[rax+64], 2);
			vcmpph(k3, zm15, ptr[rax+64], 3);
			vcmpph(k1, xm15, ptr_b[rax+64], 1);
			vcmpph(k2, ym15, ptr_b[rax+64], 2);
			vcmpph(k3, zm15, ptr_b[rax+64], 3);

			vcmpsh(k1, xm15, ptr[rax+64], 1);
			vcmpsh(k3|k5, xmm1, xmm25|T_sae, 4);

			vcomish(xmm1, ptr[rax+64]);
			vcomish(xmm1|T_sae, xmm15);

			vucomish(xmm1, ptr [rax+0x40]);
			vucomish(xmm1|T_sae, xmm15);

			vfmaddsub213ph(xmm1, xmm2, ptr [rax+0x40]);
			vfmaddsub213ph(xmm1, xmm2, ptr_b [rax+0x40]);
			vfmaddsub213ph(xmm1|k3, xmm2, xmm5);
			vfmaddsub213ph(ymm1, ymm2, ptr [rax+0x40]);
			vfmaddsub213ph(ymm1, ymm2, ptr_b[rax+0x40]);
			vfmaddsub213ph(ymm1|k3, ymm2, ymm5);
			vfmaddsub213ph(zmm1, zmm2, ptr [rax+0x40]);
			vfmaddsub213ph(zmm1, zmm2, ptr_b [rax+0x40]);
			vfmaddsub213ph(zmm1|T_ru_sae, zmm2, zmm5);

			vfmsubadd132ph(xmm1, xmm2, ptr [rax+0x40]);
			vfmsubadd132ph(xmm1, xmm2, ptr_b [rax+0x40]);
			vfmsubadd132ph(ymm1, ymm2, ptr [rax+0x40]);
			vfmsubadd132ph(ymm1, ymm2, ptr_b [rax+0x40]);
			vfmsubadd132ph(zmm1, zmm2, ptr [rax+0x40]);
			vfmsubadd132ph(zmm1, zmm2, ptr_b [rax+0x40]);
			vfmsubadd132ph(zmm1|T_ru_sae, zmm2, zmm5);

			vfmadd132ph(xmm1, xmm2, ptr [rax+0x40]);
			vfmadd132ph(xmm1, xmm2, ptr_b [rax+0x40]);
			vfmadd132ph(ymm1, ymm2, ptr [rax+0x40]);
			vfmadd132ph(ymm1, ymm2, ptr_b [rax+0x40]);
			vfmadd132ph(zmm1, zmm2, ptr [rax+0x40]);
			vfmadd132ph(zmm1, zmm2, ptr_b [rax+0x40]);
			vfmadd132ph(zmm1|T_rd_sae, zmm2, zmm5);

			vfmsub231ph(xmm1, xmm2, ptr [rax+0x40]);
			vfmsub231ph(xmm1, xmm2, ptr_b [rax+0x40]);
			vfmsub231ph(ymm1, ymm2, ptr [rax+0x40]);
			vfmsub231ph(ymm1, ymm2, ptr_b [rax+0x40]);
			vfmsub231ph(zmm1, zmm2, ptr [rax+0x40]);
			vfmsub231ph(zmm1, zmm2, ptr_b [rax+0x40]);
			vfmsub231ph(zmm1|T_rd_sae, zmm2, zmm5);

			vfnmsub231ph(xmm1, xmm2, ptr [rax+0x40]);
			vfnmsub231ph(ymm1, ymm2, ptr_b [rax+0x40]);
			vfnmsub231ph(zmm1, zmm2, ptr_b [rax+0x40]);
			vfnmsub231ph(zmm1|T_rd_sae, zmm2, zmm5);

			vfmadd132sh(xmm1|k1|T_z|T_rd_sae, xmm2, xmm3);
			vfmadd132sh(xmm1, xmm2, ptr [rax+0x40]);

			vfnmadd132sh(xmm1|k1|T_z|T_rd_sae, xmm2, xmm3);
			vfnmadd132sh(xmm1, xmm2, ptr [rax+0x40]);

			vfmsub132sh(xmm1|k1|T_z|T_rd_sae, xmm2, xmm3);
			vfmsub132sh(xmm1, xmm2, ptr [rax+0x40]);
			vfnmsub132sh(xmm1|k1|T_z|T_rd_sae, xmm2, xmm3);
			vfnmsub132sh(xmm1, xmm2, ptr [rax+0x40]);

			vfcmaddcph(xmm1|k1|T_z, xmm2, ptr [rax+0x40]);
			vfcmaddcph(ymm1|k1|T_z, ymm2, ptr [rax+0x40]);
			vfcmaddcph(zmm1|k1, zmm2, ptr [rax+0x40]);
			vfcmaddcph(zmm1|k1|T_rd_sae, zmm2, zmm5);
			vfcmaddcph(xmm1|k1|T_z, xmm2, ptr_b [rax+0x40]);
			vfcmaddcph(ymm1|k1|T_z, ymm2, ptr_b [rax+0x40]);
			vfcmaddcph(zmm1|k1|T_z, zmm2, ptr_b [rax+0x40]);

			vfmaddcph(xm1, xm2, ptr[rax+0x40]);
			vfmaddcph(ym1|k1|T_z, ym2, ptr_b[rax+0x40]);
			vfmaddcph(zm1, zm2, ptr_b[rax+0x40]);

			vfcmulcph(xmm1, xmm2, ptr [rax+0x40]);
			vfcmulcph(ymm1|k1|T_z, ymm2, ptr_b [rax+0x40]);
			vfcmulcph(zmm1, zmm2, ptr_b [rax+0x40]);

			vfmulcph(xmm1, xmm2, ptr [rax+0x40]);
			vfmulcph(ymm1|k1|T_z, ymm2, ptr_b [rax+0x40]);
			vfmulcph(zmm1, zmm2, ptr_b [rax+0x40]);
		}
	} c;
	const uint8_t tbl[] = {
		// vaddph
		0x62, 0xF5, 0x74, 0x48, 0x58, 0x40, 0x01,
		0x62, 0xF5, 0x74, 0x28, 0x58, 0x40, 0x02,
		0x62, 0xF5, 0x74, 0x08, 0x58, 0x40, 0x04,

		0x62, 0xF5, 0x74, 0x58, 0x58, 0x40, 0x20,
		0x62, 0xF5, 0x74, 0x38, 0x58, 0x40, 0x20,
		0x62, 0xF5, 0x74, 0x18, 0x58, 0x40, 0x20,

		// vaddsh
		0x62, 0xF5, 0x06, 0x08, 0x58, 0x40, 0x20,
		0x62, 0xF5, 0x06, 0xBD, 0x58, 0xC3,

		// vcmpph
		0x62, 0xf3, 0x04, 0x08, 0xc2, 0x48, 0x04, 0x01,
		0x62, 0xf3, 0x04, 0x28, 0xc2, 0x50, 0x02, 0x02,
		0x62, 0xf3, 0x04, 0x48, 0xc2, 0x58, 0x01, 0x03,
		0x62, 0xf3, 0x04, 0x18, 0xc2, 0x48, 0x20, 0x01,
		0x62, 0xf3, 0x04, 0x38, 0xc2, 0x50, 0x20, 0x02,
		0x62, 0xf3, 0x04, 0x58, 0xc2, 0x58, 0x20, 0x03,

		// vcmpsh
		0x62, 0xf3, 0x06, 0x08, 0xc2, 0x48, 0x20, 0x01,
		0x62, 0x93, 0x76, 0x1d, 0xc2, 0xd9, 0x04,

		// vcomish
		0x62, 0xf5, 0x7c, 0x08, 0x2f, 0x48, 0x20,
		0x62, 0xd5, 0x7c, 0x18, 0x2f, 0xcf,

		// vucomish
		0x62, 0xf5, 0x7c, 0x08, 0x2e, 0x48, 0x20,
		0x62, 0xd5, 0x7c, 0x18, 0x2e, 0xcf,

		// vfmaddsub213ph
		0x62, 0xf6, 0x6d, 0x08, 0xa6, 0x48, 0x04,
		0x62, 0xf6, 0x6d, 0x18, 0xa6, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x0b, 0xa6, 0xcd,
		0x62, 0xf6, 0x6d, 0x28, 0xa6, 0x48, 0x02,
		0x62, 0xf6, 0x6d, 0x38, 0xa6, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x2b, 0xa6, 0xcd,
		0x62, 0xf6, 0x6d, 0x48, 0xa6, 0x48, 0x01,
		0x62, 0xf6, 0x6d, 0x58, 0xa6, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x58, 0xa6, 0xcd,

		// vfmsubadd132ph
		0x62, 0xf6, 0x6d, 0x08, 0x97, 0x48, 0x04,
		0x62, 0xf6, 0x6d, 0x18, 0x97, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x28, 0x97, 0x48, 0x02,
		0x62, 0xf6, 0x6d, 0x38, 0x97, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x48, 0x97, 0x48, 0x01,
		0x62, 0xf6, 0x6d, 0x58, 0x97, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x58, 0x97, 0xcd,

		// vfmadd132ph
		0x62, 0xf6, 0x6d, 0x08, 0x98, 0x48, 0x04,
		0x62, 0xf6, 0x6d, 0x18, 0x98, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x28, 0x98, 0x48, 0x02,
		0x62, 0xf6, 0x6d, 0x38, 0x98, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x48, 0x98, 0x48, 0x01,
		0x62, 0xf6, 0x6d, 0x58, 0x98, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x38, 0x98, 0xcd,

		// vfmsub231ph
		0x62, 0xf6, 0x6d, 0x08, 0xba, 0x48, 0x04,
		0x62, 0xf6, 0x6d, 0x18, 0xba, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x28, 0xba, 0x48, 0x02,
		0x62, 0xf6, 0x6d, 0x38, 0xba, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x48, 0xba, 0x48, 0x01,
		0x62, 0xf6, 0x6d, 0x58, 0xba, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x38, 0xba, 0xcd,

		// vfnmsub231ph
		0x62, 0xf6, 0x6d, 0x08, 0xbe, 0x48, 0x04,
		0x62, 0xf6, 0x6d, 0x38, 0xbe, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x58, 0xbe, 0x48, 0x20,
		0x62, 0xf6, 0x6d, 0x38, 0xbe, 0xcd,

		// vfmadd132sh
		0x62, 0xf6, 0x6d, 0xb9, 0x99, 0xcb,
		0x62, 0xf6, 0x6d, 0x08, 0x99, 0x48, 0x20,

		// vfnmadd132sh
		0x62, 0xf6, 0x6d, 0xb9, 0x9d, 0xcb,
		0x62, 0xf6, 0x6d, 0x08, 0x9d, 0x48, 0x20,

		// vfmsub132sh
		0x62, 0xf6, 0x6d, 0xb9, 0x9b, 0xcb,
		0x62, 0xf6, 0x6d, 0x08, 0x9b, 0x48, 0x20,

		// vfnmsub132sh
		0x62, 0xf6, 0x6d, 0xb9, 0x9f, 0xcb,
		0x62, 0xf6, 0x6d, 0x08, 0x9f, 0x48, 0x20,

		// vfcmaddcph
		0x62, 0xf6, 0x6f, 0x89, 0x56, 0x48, 0x04,
		0x62, 0xf6, 0x6f, 0xa9, 0x56, 0x48, 0x02,
		0x62, 0xf6, 0x6f, 0x49, 0x56, 0x48, 0x01,
		0x62, 0xf6, 0x6f, 0x39, 0x56, 0xcd,
		0x62, 0xf6, 0x6f, 0x99, 0x56, 0x48, 0x10,
		0x62, 0xf6, 0x6f, 0xb9, 0x56, 0x48, 0x10,
		0x62, 0xf6, 0x6f, 0xd9, 0x56, 0x48, 0x10,

		// vfmaddcph
		0x62, 0xf6, 0x6e, 0x08, 0x56, 0x48, 0x04,
		0x62, 0xf6, 0x6e, 0xb9, 0x56, 0x48, 0x10,
		0x62, 0xf6, 0x6e, 0x58, 0x56, 0x48, 0x10,

		// vfcmulcph
		0x62, 0xf6, 0x6f, 0x08, 0xd6, 0x48, 0x04,
		0x62, 0xf6, 0x6f, 0xb9, 0xd6, 0x48, 0x10,
		0x62, 0xf6, 0x6f, 0x58, 0xd6, 0x48, 0x10,

		// vfmulcph
		0x62, 0xf6, 0x6e, 0x08, 0xd6, 0x48, 0x04,
		0x62, 0xf6, 0x6e, 0xb9, 0xd6, 0x48, 0x10,
		0x62, 0xf6, 0x6e, 0x58, 0xd6, 0x48, 0x10,
	};
	const size_t n = sizeof(tbl) / sizeof(tbl[0]);
	CYBOZU_TEST_EQUAL(c.getSize(), n);
	CYBOZU_TEST_EQUAL_ARRAY(c.getCode(), tbl, n);
}
#endif
