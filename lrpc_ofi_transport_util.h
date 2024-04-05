//Copyright(C) 2022 Intel Corporation
//SPDX-License-Identifier: Apache-2.0
#ifndef _IRPC_OFI_UTIL_H_
#define _IRPC_OFI_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>

#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <inttypes.h>
#include <netinet/tcp.h>

#include <rdma/fabric.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_collective.h>
#include <rdma/fi_cm.h>

#ifndef IRPC_OFI_FIVERSION
#define IRPC_OFI_FIVERSION FI_VERSION(1, 9)
#endif

#define fieldsize(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))

#define NO_CQ_DATA 0

#define MR_KEY 0xC0DE
#define TX_MR_KEY (MR_KEY + 1)
#define RX_MR_KEY 0xFFFF
#define MSG_MR_ACCESS (FI_SEND | FI_RECV)
#define RMA_MR_ACCESS (FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE)

#define MAX_CTRL_MSG 256

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define IRPC_OFI_PRINTERR(call, retv)                    \
	do                                                   \
	{                                                    \
		int saved_errno = errno;                         \
		fprintf(stderr, call "(): %s:%d, ret=%d (%s)\n", \
				__FILE__, __LINE__, (int)(retv),         \
				fi_strerror((int)-(retv)));              \
		errno = saved_errno;                             \
	} while (0)

#define IRPC_OFI_LOG(level, fmt, ...)                               \
	do                                                              \
	{                                                               \
		int saved_errno = errno;                                    \
		fprintf(stderr, "[%s] lrpc_ofi_transport:%s:%d: " fmt "\n", \
				level, __FILE__, __LINE__, ##__VA_ARGS__);          \
		errno = saved_errno;                                        \
	} while (0)

#define IRPC_OFI_ERR(fmt, ...) IRPC_OFI_LOG("error", fmt, ##__VA_ARGS__)
#define IRPC_OFI_WARN(fmt, ...) IRPC_OFI_LOG("warn", fmt, ##__VA_ARGS__)

#define IRPC_OFI_CQ_ERR(cq, entry, buf, len)                    \
	IRPC_OFI_ERR("cq_readerr %d (%s), provider errno: %d (%s)", \
				 entry.err, fi_strerror(entry.err),             \
				 entry.prov_errno, fi_cq_strerror(cq, entry.prov_errno, entry.err_data, buf, len))

#define IRPC_OFI_EP_BIND(ep, fd, flags)                  \
	do                                                   \
	{                                                    \
		int ret;                                         \
		if ((fd))                                        \
		{                                                \
			ret = fi_ep_bind((ep), &(fd)->fid, (flags)); \
			if (ret)                                     \
			{                                            \
				IRPC_OFI_PRINTERR("fi_ep_bind", ret);    \
				return ret;                              \
			}                                            \
		}                                                \
	} while (0)

#define IRPC_OFI_POST(post_fn, progress_fn, cq, op_str, ...)         \
	do                                                               \
	{                                                                \
		int timeout_save;                                            \
		int ret, rc;                                                 \
                                                                     \
		while (1)                                                    \
		{                                                            \
			ret = post_fn(__VA_ARGS__);                              \
			if (!ret)                                                \
				break;                                               \
                                                                     \
			if (ret != -FI_EAGAIN)                                   \
			{                                                        \
				IRPC_OFI_PRINTERR(op_str, ret);                      \
				return ret;                                          \
			}                                                        \
                                                                     \
			timeout_save = timeout;                                  \
			timeout = 0;                                             \
			rc = progress_fn(cq);                                    \
			timeout = timeout_save;                                  \
			if (rc && rc != -FI_EAGAIN)                              \
			{                                                        \
				IRPC_OFI_ERR("Failed to get " op_str " completion"); \
				return rc;                                           \
			}                                                        \
			timeout = timeout_save;                                  \
		}                                                            \
	} while (0)

#define CLOSE_FID(fd)                                   \
	do                                                  \
	{                                                   \
		int ret;                                        \
		if ((fd))                                       \
		{                                               \
			ret = fi_close(&(fd)->fid);                 \
			if (ret)                                    \
				IRPC_OFI_ERR("fi_close: %s(%d) fid %d", \
							 fi_strerror(-ret),         \
							 ret,                       \
							 (int)(fd)->fid.fclass);    \
			fd = NULL;                                  \
		}                                               \
	} while (0)

enum
{
	OPT_ACTIVE = 1 << 0,
	OPT_ITER = 1 << 1,
	OPT_SIZE = 1 << 2,
	OPT_RX_CQ = 1 << 3,
	OPT_TX_CQ = 1 << 4,
	OPT_RX_CNTR = 1 << 5,
	OPT_TX_CNTR = 1 << 6,
	OPT_VERIFY_DATA = 1 << 7,
	OPT_ALIGN = 1 << 8,
	OPT_BW = 1 << 9,
	OPT_CQ_SHARED = 1 << 10,
	OPT_OOB_SYNC = 1 << 11,
	OPT_SKIP_MSG_ALLOC = 1 << 12,
	OPT_SKIP_REG_MR = 1 << 13,
	OPT_OOB_ADDR_EXCH = 1 << 14,
	OPT_ALLOC_MULT_MR = 1 << 15,
	OPT_SERVER_PERSIST = 1 << 16,
	OPT_ENABLE_HMEM = 1 << 17,
	OPT_USE_DEVICE = 1 << 18,
	OPT_DOMAIN_EQ = 1 << 19,
	OPT_FORK_CHILD = 1 << 20,
	OPT_SRX = 1 << 21,
	OPT_STX = 1 << 22,
	OPT_SKIP_ADDR_EXCH = 1 << 23,
	OPT_PERF = 1 << 24,
	OPT_OOB_CTRL = OPT_OOB_SYNC | OPT_OOB_ADDR_EXCH,
};

enum comp_method
{
	COMP_SPIN = 0,
	COMP_SREAD,
	COMP_WAITSET,
	COMP_WAIT_FD,
	COMP_YIELD,
};

enum op_state
{
	OP_DONE = 0,
	OP_PENDING
};

struct lrpc_ofi_opts
{
	size_t transfer_size;
	int window_size;
	int av_size;
	int verbose;
	int tx_cq_size;
	int rx_cq_size;
	char *src_port;
	char *dst_port;
	char *src_addr;
	char *dst_addr;
	char *av_name;
	int options;
	enum comp_method comp_method;
	int machr;
	char *oob_port;
	int argc;
	int num_connections;
	int address_format;

	uint64_t mr_mode;
	/* Fail if the selected provider does not support FI_MSG_PREFIX.  */
	int force_prefix;
	enum fi_hmem_iface iface;
	uint64_t device;

	char **argv;
};

struct lrpc_ofi_context
{
	char *buf;
	void *desc;
	enum op_state state;
	struct fid_mr *mr;
	struct fi_context2 context;
};

#endif /* _IRPC_OFI_UTIL_H_ */