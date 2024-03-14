/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/* adoptd and modified from https://github.com/DPDK/dpdk */

#ifndef _RTE_HASH_CRC_H_
#define _RTE_HASH_CRC_H_

/**
 * @file
 *
 * RTE CRC Hash
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline uint32_t
crc32c_sse42_u8(uint8_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32b %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return init_val;
}

static inline uint32_t
crc32c_sse42_u16(uint16_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32w %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return init_val;
}

static inline uint32_t
crc32c_sse42_u32(uint32_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32l %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return init_val;
}

static inline uint32_t
crc32c_sse42_u64_mimic(uint64_t data, uint64_t init_val)
{
	union {
		uint32_t u32[2];
		uint64_t u64;
	} d;

	d.u64 = data;
	init_val = crc32c_sse42_u32(d.u32[0], (uint32_t)init_val);
	init_val = crc32c_sse42_u32(d.u32[1], (uint32_t)init_val);
	return (uint32_t)init_val;
}

static inline uint32_t
crc32c_sse42_u64(uint64_t data, uint64_t init_val)
{
	__asm__ volatile(
			"crc32q %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return (uint32_t)init_val;
}

/*
 * Use single crc32 instruction to perform a hash on a byte value.
 * Fall back to software crc32 implementation in case SSE4.2 is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_1byte(uint8_t data, uint32_t init_val)
{
	return crc32c_sse42_u8(data, init_val);
}

/*
 * Use single crc32 instruction to perform a hash on a 2 bytes value.
 * Fall back to software crc32 implementation in case SSE4.2 is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_2byte(uint16_t data, uint32_t init_val)
{
	return crc32c_sse42_u16(data, init_val);
}

/*
 * Use single crc32 instruction to perform a hash on a 4 byte value.
 * Fall back to software crc32 implementation in case SSE4.2 is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_4byte(uint32_t data, uint32_t init_val)
{
	return crc32c_sse42_u32(data, init_val);
}

/*
 * Use single crc32 instruction to perform a hash on a 8 byte value.
 * Fall back to software crc32 implementation in case SSE4.2 is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_8byte(uint64_t data, uint32_t init_val)
{
    // RTE_ARCH_X86_64
	return crc32c_sse42_u64(data, init_val);

	// crc32_alg & CRC32_SSE42
	// return crc32c_sse42_u64_mimic(data, init_val);
}



#ifdef __DOXYGEN__

/**
 * Use single CRC32 instruction to perform a hash on a byte value.
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_1byte(uint8_t data, uint32_t init_val);

/**
 * Use single CRC32 instruction to perform a hash on a 2 bytes value.
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_2byte(uint16_t data, uint32_t init_val);

/**
 * Use single CRC32 instruction to perform a hash on a 4 bytes value.
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_4byte(uint32_t data, uint32_t init_val);

/**
 * Use single CRC32 instruction to perform a hash on a 8 bytes value.
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_8byte(uint64_t data, uint32_t init_val);

#endif /* __DOXYGEN__ */

/**
 * Calculate CRC32 hash on user-supplied byte array.
 *
 * @param data
 *   Data to perform hash on.
 * @param data_len
 *   How many bytes to use to calculate hash value.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc(const void *data, uint32_t data_len, uint32_t init_val)
{
	unsigned i;
	uintptr_t pd = (uintptr_t) data;

	for (i = 0; i < data_len / 8; i++) {
		init_val = rte_hash_crc_8byte(*(const uint64_t *)pd, init_val);
		pd += 8;
	}

	if (data_len & 0x4) {
		init_val = rte_hash_crc_4byte(*(const uint32_t *)pd, init_val);
		pd += 4;
	}

	if (data_len & 0x2) {
		init_val = rte_hash_crc_2byte(*(const uint16_t *)pd, init_val);
		pd += 2;
	}

	if (data_len & 0x1)
		init_val = rte_hash_crc_1byte(*(const uint8_t *)pd, init_val);

	return init_val;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_HASH_CRC_H_ */