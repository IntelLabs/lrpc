#pragma once

#include "lrpc_transport.h"
#include "lrpc_ofi_transport_util.h"

namespace lrpc
{

	class Lrpc_ofi_transport : public Lrpc_transport
	{

		struct fi_info *fi = NULL, *hints = NULL;
		struct fid_fabric *fabric = NULL;
		struct fid_wait *waitset = NULL;
		struct fid_domain *domain = NULL;
		struct fid_poll *pollset = NULL;
		struct fid_pep *pep = NULL;
		struct fid_ep *ep = NULL, *alias_ep = NULL;
		struct fid_cq *txcq = NULL, *rxcq = NULL;
		struct fid_mr *mr = NULL, no_mr;
		void *mr_desc = NULL;
		struct fid_av *av = NULL;
		struct fid_eq *eq = NULL;
		struct fid_mc *mc = NULL;

		struct lrpc_ofi_opts opts;

		struct fi_context tx_ctx, rx_ctx;
		struct lrpc_ofi_context *tx_ctx_arr = NULL, *rx_ctx_arr = NULL;
		fi_addr_t remote_fi_addr = FI_ADDR_UNSPEC;
		uint64_t remote_cq_data = 0;

		size_t buf_size = 0, tx_size = 0, rx_size = 0, tx_mr_size = 0, rx_mr_size = 0;
		int rx_fd = -1, tx_fd = -1;
		char *buf = NULL, *tx_buf = NULL, *rx_buf = NULL;
		void *tx_msg_buf = NULL;
		char **tx_mr_bufs = NULL, **rx_mr_bufs = NULL;

		int listen_sock = -1;
		int sock = -1;
		int oob_sock = -1;
		int timeout = -1;

		struct fi_av_attr av_attr = {
			.type = FI_AV_MAP,
			.count = 128
		};

		struct fi_eq_attr eq_attr = {.wait_obj = FI_WAIT_UNSPEC};
		struct fi_cq_attr cq_attr = {.wait_obj = FI_WAIT_NONE};

		char default_port[8] = "9228";

	public:
		Lrpc_ofi_transport(char *dst) : Lrpc_transport()
		{

			opts = (struct lrpc_ofi_opts){.transfer_size = sizeof(Rpc_message),
										  .window_size = 64,
										  .av_size = 128,
										  .verbose = 0,
										  .tx_cq_size = 0,
										  .rx_cq_size = 0,
										  .dst_addr = dst,
										  .options = OPT_RX_CQ | OPT_TX_CQ | OPT_SIZE,
										  .comp_method = COMP_SREAD,
										  //.oob_port = default_oob_port,
										  .argc = 1,
										  .num_connections = 1,
										  .address_format = 0,
										  .mr_mode = FI_MR_LOCAL | (FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR),
										  .iface = FI_HMEM_SYSTEM,
										  .device = 0,
										  .argv = NULL};
			*tx_ctx.internal = NULL;
			*rx_ctx.internal = NULL;
		}
		~Lrpc_ofi_transport()
		{
			lrpc_ofi_free_res();
		};

		int lrpc_init()
		{

			int ret;

			IRPC_OFI_LOG("info", "Starting to initialize fabric...");

			hints = fi_allocinfo();
			if (!hints)
				return (EXIT_FAILURE);

			hints->ep_attr->type = FI_EP_RDM;
			hints->caps = FI_MSG | FI_SOURCE;
			hints->mode = FI_CONTEXT;
			hints->domain_attr->mr_mode = opts.mr_mode;

			ret = lrpc_ofi_getinfo(hints, &fi);
			if (ret)
			{
				IRPC_OFI_PRINTERR("lrpc_ofi_getinfo", ret);
				return ret;
			}

			ret = fi_fabric(fi->fabric_attr, &fabric, NULL);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_fabric", ret);
				return ret;
			}

			ret = fi_eq_open(fabric, &eq_attr, &eq, NULL);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_eq_open", ret);
				return ret;
			}

			ret = fi_domain(fabric, fi, &domain, NULL);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_domain", ret);
				return ret;
			}

			ret = lrpc_ofi_alloc_ep_res(fi, &txcq, &rxcq);
			if (ret)
			{
				IRPC_OFI_PRINTERR("lrpc_ofi_alloc_eo_res", ret);
				return ret;
			}

			ret = fi_endpoint(domain, fi, &ep, NULL);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_endpoint", ret);
				return ret;
			}

			ret = lrpc_ofi_enable_ep(ep, eq, av, txcq, rxcq);
			if (ret)
				return ret;

			// Post the initial receive, as soon as the EP is enabled
			IRPC_OFI_POST(fi_recv, lrpc_ofi_progress, rxcq, "receive", ep, rx_buf, MAX(rx_size, MAX_CTRL_MSG), mr_desc, remote_fi_addr, &rx_ctx);

			if (opts.dst_addr)
			{   // If client, say Hi to the server
				ret = lrpc_ofi_init_client(av, ep, &remote_fi_addr);
				if (ret)
					return ret;
			}

			return 0;
		}

		// Dequeue from the RPC queue
		inline int lrpc_snd(lrpc_addr_t *to, Rpc_message *rpc_msg)
		{

			size_t n;

			if (to->qword[0] == IRPC_ADDR_UNSPEC)
			{
				// Must be a client trying to send to the only server it knows
				to->qword[0] = (uint64_t) remote_fi_addr;
			}

			n = sizeof(Rpc_message) - fieldsize(Rpc_message, rpc_parameters) + rpc_msg->rpc_parameters_size;

			// Call into OFI Layer to Send The Buffer
			return (int)lrpc_ofi_send_buffer(ep, (fi_addr_t *) &to->qword[0], (char *)rpc_msg, n);
		}

		// Enqueue into an RPC queue
		inline int lrpc_rcv(lrpc_addr_t *from, Rpc_message *rpc_msg)
		{

			ssize_t ret;

			// Call into OFI Layer to Receive The Buffer
			ret = lrpc_ofi_recv_buffer(ep, (fi_addr_t *) &from->qword[0], (char *)rpc_msg);

			if (ret != 1)
			        IRPC_OFI_LOG("info", "lrpc_rcv unsuccessfull...");

			return (int)ret;
		}

		// Signal CQs to unblock wait ops
		inline int lrpc_signal_cqs()
		{
			fi_cq_signal(txcq);
			fi_cq_signal(rxcq);
			return 0;
		}

		// current size of an RPC queue
		inline int lrpc_queue_size()
		{

			// TBA
			return 0;
		}

		// setup a new client
		inline int lrpc_new_client(Rpc_message *rpc_msg)
		{
			int ret = 0;
			struct fi_cq_err_entry comp;

			if (opts.dst_addr)
			{
				ret = -1;
				IRPC_OFI_PRINTERR("lrpc_new_client error: I am not a server!", ret);
				return ret;
			}

			ret = fi_av_insert(av, rpc_msg->rpc_parameters, 1, &remote_fi_addr, 0, NULL);
			if (ret < 0)
			{
				IRPC_OFI_PRINTERR("fi_av_insert", ret);
				return ret;
			}
			else if (ret != 1)
			{
				IRPC_OFI_ERR("fi_av_insert: number of addresses inserted = %d number of addresses given = %d\n", ret, 1);
				return -EXIT_FAILURE;
			}

			// Send the Server's response to the Client
			IRPC_OFI_POST(fi_send, lrpc_ofi_progress, txcq, "transmit", ep, tx_buf, 1, mr_desc, remote_fi_addr, &tx_ctx);
			// Fetch the completion for the send
			ret = (int)fi_cq_sread(txcq, &comp, 1, NULL, timeout);
			if (ret < 0)
			{
				if (ret == -FI_EAVAIL)
					ret = lrpc_ofi_cq_readerr(txcq);
				else if (ret != -FI_EAGAIN)
					IRPC_OFI_PRINTERR("get_cq_comp", ret);
				return ret;
			}

			return 0;
		}


	private:
		void lrpc_ofi_free_res(void)
		{
			if (fi)
			{
				fi_freeinfo(fi);
				fi = NULL;
			}
			if (hints)
			{
				lrpc_ofi_freehints(hints);
				hints = NULL;
			}
		}

		void lrpc_ofi_freehints(struct fi_info *hints)
		{
			if (!hints)
				return;

			if (hints->domain_attr->name)
			{
				free(hints->domain_attr->name);
				hints->domain_attr->name = NULL;
			}
			if (hints->fabric_attr->name)
			{
				free(hints->fabric_attr->name);
				hints->fabric_attr->name = NULL;
			}
			if (hints->fabric_attr->prov_name)
			{
				free(hints->fabric_attr->prov_name);
				hints->fabric_attr->prov_name = NULL;
			}
			if (hints->src_addr)
			{
				free(hints->src_addr);
				hints->src_addr = NULL;
				hints->src_addrlen = 0;
			}
			if (hints->dest_addr)
			{
				free(hints->dest_addr);
				hints->dest_addr = NULL;

				hints->dest_addrlen = 0;
			}

			fi_freeinfo(hints);
		}

		int lrpc_ofi_dupaddr(void **dst_addr, size_t *dst_addrlen, void *src_addr, size_t src_addrlen)
		{
			*dst_addr = malloc(src_addrlen);
			if (!*dst_addr)
			{
				IRPC_OFI_ERR("address allocation failed");
				return EAI_MEMORY;
			}
			*dst_addrlen = src_addrlen;
			memcpy(*dst_addr, src_addr, src_addrlen);
			return 0;
		}

		int lrpc_ofi_getaddr(char *node, char *service, struct fi_info *hints, uint64_t flags)
		{
			int ret;
			struct fi_info *fi;

			if (!node && !service)
			{
				if (flags & FI_SOURCE)
				{
					hints->src_addr = NULL;
					hints->src_addrlen = 0;
				}
				else
				{
					hints->dest_addr = NULL;
					hints->dest_addrlen = 0;
				}
				return 0;
			}

			ret = fi_getinfo(IRPC_OFI_FIVERSION, node, service, flags, hints, &fi);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_getinfo", ret);
				return ret;
			}
			hints->addr_format = fi->addr_format;

			if (flags & FI_SOURCE)
			{
				ret = lrpc_ofi_dupaddr(&hints->src_addr, &hints->src_addrlen,
									   fi->src_addr, fi->src_addrlen);
			}
			else
			{
				ret = lrpc_ofi_dupaddr(&hints->dest_addr, &hints->dest_addrlen,
									   fi->dest_addr, fi->dest_addrlen);
			}

			fi_freeinfo(fi);
			return ret;
		}

		int lrpc_ofi_read_addr_opts(char **node, char **service, struct fi_info *hints,
									uint64_t *flags, struct lrpc_ofi_opts *opts)
		{
			int ret;

			if (opts->dst_addr && (opts->src_addr || !opts->oob_port))
			{
				if (!opts->dst_port)
					opts->dst_port = default_port;

				ret = lrpc_ofi_getaddr(opts->src_addr, opts->src_port, hints, FI_SOURCE);
				if (ret)
					return ret;
				*node = opts->dst_addr;
				*service = opts->dst_port;
			}
			else
			{
				if (!opts->src_port)
					opts->src_port = default_port;

				*node = opts->src_addr;
				*service = opts->src_port;
				*flags = FI_SOURCE;
			}

			return 0;
		}

		int lrpc_ofi_init_client(struct fid_av *av_ptr, struct fid_ep *ep_ptr, fi_addr_t *remote_addr)
		{
			char temp[MAX_CTRL_MSG];
			size_t addrlen, n, rcvsize;
			int ret;
			struct fi_cq_err_entry comp;
			Rpc_message hi;


			// Insert the Server's address found in the config, to the av
			ret = fi_av_insert(av_ptr, fi->dest_addr, 1, remote_addr, 0, NULL);
			if (ret < 0)
			{
				IRPC_OFI_PRINTERR("fi_av_insert", ret);
				return ret;
			}
			else if (ret != 1)
			{
				IRPC_OFI_ERR("fi_av_insert: number of addresses inserted = %d;"
								" number of addresses given = %d\n",
								ret, 1);
				return -EXIT_FAILURE;
			}

			addrlen = MAX_CTRL_MSG;
			n = sizeof(Rpc_message) - fieldsize(Rpc_message, rpc_parameters) + addrlen;
			hi.rpc_type = kRpcTypeHi;
			hi.rpc_parameters_size = addrlen;

			// Obtain self address (client) and place in Hi message
			ret = fi_getname(&ep_ptr->fid, hi.rpc_parameters, &addrlen);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_getname", ret);
				return ret;
			}
			
			// Place the Hi msg into the transmit buffer
			memcpy(tx_buf, &hi, n);
			n += ((fi->tx_attr->mode & FI_MSG_PREFIX) ? fi->ep_attr->msg_prefix_size : 0);

			// Send the address containing tx_buf to the server
			IRPC_OFI_POST(fi_send, lrpc_ofi_progress, txcq, "transmit", ep, tx_buf, n, mr_desc, *remote_addr, &tx_ctx);

			// Fetch the completion for the send
			ret = (int)fi_cq_sread(txcq, &comp, 1, NULL, timeout);
			if (ret < 0)
			{
				if (ret == -FI_EAVAIL)
					ret = lrpc_ofi_cq_readerr(txcq);
				else if (ret != -FI_EAGAIN)
					IRPC_OFI_PRINTERR("get_cq_comp", ret);
				return ret;
			}

			// Get the response from Server
			ret = (int)fi_cq_sread(rxcq, &comp, 1, NULL, timeout);
			if (ret < 0)
			{
				if (ret == -FI_EAVAIL)
					ret = lrpc_ofi_cq_readerr(rxcq);
				else if (ret != -FI_EAGAIN)
					IRPC_OFI_PRINTERR("get_cq_comp", ret);
				return ret;
			}

			// Post an RX Buffer/OP
			rcvsize = MAX(rx_size, MAX_CTRL_MSG) + ((fi->rx_attr->mode & FI_MSG_PREFIX) ? fi->ep_attr->msg_prefix_size : 0);
			IRPC_OFI_POST(fi_recv, lrpc_ofi_progress, rxcq, "receive", ep, rx_buf, rcvsize, mr_desc, *remote_addr, &rx_ctx);

			return 0;
		}

		int lrpc_ofi_reg_mr(struct fi_info *fi, void *buf, size_t size, uint64_t access,
							uint64_t key, struct fid_mr **mr, void **desc)
		{
			struct fi_mr_attr attr = {0};
			struct iovec iov = {0};
			int ret;
			uint64_t flags;

			if (!(fi->domain_attr->mr_mode & FI_MR_LOCAL) && !(fi->caps & (FI_RMA | FI_ATOMIC)))
				return 0;

			iov.iov_base = buf;
			iov.iov_len = size;
			attr.mr_iov = &iov;
			attr.iov_count = 1;
			attr.access = access;
			attr.offset = 0;
			attr.requested_key = key;
			attr.context = NULL;
			attr.iface = opts.iface;

			flags = (opts.iface) ? FI_HMEM_DEVICE_ONLY : 0;
			ret = fi_mr_regattr(domain, &attr, flags, mr);
			if (ret)
				return ret;

			if (desc)
				*desc = fi_mr_desc(*mr);

			return FI_SUCCESS;
		}

		uint64_t lrpc_ofi_info_to_mr_access(struct fi_info *info)
		{
			uint64_t mr_access = 0;
			if ((info->mode & FI_LOCAL_MR) || (info->domain_attr->mr_mode & FI_MR_LOCAL))
			{
				if (info->caps & (FI_MSG | FI_TAGGED))
				{
					if (info->caps & MSG_MR_ACCESS)
					{
						mr_access |= info->caps & MSG_MR_ACCESS;
					}
					else
					{
						mr_access |= MSG_MR_ACCESS;
					}
				}

				if (info->caps & (FI_RMA | FI_ATOMIC))
				{
					if (info->caps & RMA_MR_ACCESS)
					{
						mr_access |= info->caps & RMA_MR_ACCESS;
					}
					else
					{
						mr_access |= RMA_MR_ACCESS;
					}
				}
			}
			else
			{
				if (info->caps & (FI_RMA | FI_ATOMIC))
				{
					if (rma_read_target_allowed(info->caps))
					{
						mr_access |= FI_REMOTE_READ;
					}
					if (rma_write_target_allowed(info->caps))
					{
						mr_access |= FI_REMOTE_WRITE;
					}
				}
			}
			return mr_access;
		}

		int lrpc_ofi_alloc_ctx_array(struct lrpc_ofi_context **mr_array, char ***mr_bufs,
									 char *default_buf, size_t mr_size, uint64_t start_key, struct fi_info *info)
		{
			int i, ret;
			struct lrpc_ofi_context *context;
			uint64_t access = lrpc_ofi_info_to_mr_access(info);

			*mr_array = (struct lrpc_ofi_context *)calloc(opts.window_size, sizeof(**mr_array));
			if (!*mr_array)
				return -FI_ENOMEM;

			for (i = 0; i < opts.window_size; i++)
			{

				context = &(*mr_array)[i];
				if (!(opts.options & OPT_ALLOC_MULT_MR))
				{
					context->buf = default_buf + mr_size * i;
					context->mr = mr;
					context->desc = mr_desc;
					continue;
				}

				(*mr_bufs)[i] = (char *)malloc(mr_size);
				return !(*mr_bufs)[i] ? -FI_ENOMEM : FI_SUCCESS;

				context->buf = (*mr_bufs)[i];

				ret = lrpc_ofi_reg_mr(fi, context->buf, mr_size, access, start_key + i, &context->mr, &context->desc);
				if (ret)
					return ret;

			} // for

			return 0;
		}

		int rma_read_target_allowed(uint64_t caps)
		{
			if (caps & (FI_RMA | FI_ATOMIC))
			{
				if (caps & FI_REMOTE_READ)
					return 1;
				return !(caps & (FI_READ | FI_WRITE | FI_REMOTE_WRITE));
			}
			return 0;
		}

		int rma_write_target_allowed(uint64_t caps)
		{
			if (caps & (FI_RMA | FI_ATOMIC))
			{
				if (caps & FI_REMOTE_WRITE)
					return 1;
				return !(caps & (FI_READ | FI_WRITE | FI_REMOTE_WRITE));
			}
			return 0;
		}

		/*
		 * Include FI_MSG_PREFIX space in the allocated buffer, and ensure that the
		 * buffer is large enough for a control message used to exchange addressing
		 * data.
		 */
		int lrpc_ofi_alloc_msgs(struct fi_info *fi)
		{
			int ret;
			long alignment = 1;

			// Return if already allocated
			if (buf)
				return 0;

			// Set TX/RX and Total Buffer Sizes
			tx_size = (opts.transfer_size > fi->ep_attr->max_msg_size) ? fi->ep_attr->max_msg_size : opts.transfer_size;
			tx_size += ((fi->tx_attr->mode & FI_MSG_PREFIX) ? fi->ep_attr->msg_prefix_size : 0);
			rx_size = tx_size;
			rx_size += ((fi->rx_attr->mode & FI_MSG_PREFIX) ? fi->ep_attr->msg_prefix_size : 0);

			tx_mr_size = 0;
			rx_mr_size = 0;
			buf_size = MAX(tx_size, MAX_CTRL_MSG) * opts.window_size + MAX(rx_size, MAX_CTRL_MSG) * opts.window_size;

			// Allocate and zero-out memory
			buf = (char *)malloc(buf_size);
			if (!buf)
				return -FI_ENOMEM;

			tx_msg_buf = malloc(MAX(tx_size, MAX_CTRL_MSG));
			if (!tx_msg_buf)
				return -FI_ENOMEM;

			memset(buf, 0, buf_size);

			// Earmark the buffer into two parts	for RX and TX
			rx_buf = buf;
			tx_buf = (char *)buf + MAX(rx_size, MAX_CTRL_MSG) * opts.window_size;

			if (fi->domain_attr->cq_data_size >= sizeof(uint64_t))
				remote_cq_data = 0x0123456789abcdefULL;
			else
				remote_cq_data = 0x0123456789abcdef & ((0x1ULL << (fi->domain_attr->cq_data_size * 8)) - 1);

			mr = &no_mr;

			ret = lrpc_ofi_reg_mr(fi, buf, buf_size, lrpc_ofi_info_to_mr_access(fi), MR_KEY, &mr, &mr_desc);
			if (ret)
				return ret;

			ret = lrpc_ofi_alloc_ctx_array(&tx_ctx_arr, &tx_mr_bufs, tx_buf, tx_mr_size, TX_MR_KEY, fi);
			if (ret)
				return -FI_ENOMEM;

			ret = lrpc_ofi_alloc_ctx_array(&rx_ctx_arr, &rx_mr_bufs, rx_buf, rx_mr_size, RX_MR_KEY, fi);
			if (ret)
				return -FI_ENOMEM;

			return 0;
		}

		int lrpc_ofi_alloc_ep_res(struct fi_info *fi, struct fid_cq **new_txcq, struct fid_cq **new_rxcq)
		{
			int ret;

			ret = lrpc_ofi_alloc_msgs(fi);
			if (ret)
				return ret;

			if (cq_attr.format == FI_CQ_FORMAT_UNSPEC)
			{
				if (fi->caps & FI_TAGGED)
					cq_attr.format = FI_CQ_FORMAT_TAGGED;
				else
					cq_attr.format = FI_CQ_FORMAT_CONTEXT;
			}

			// Setup TX & RX Completion Queues ()
			switch (opts.comp_method)
			{
			case COMP_SREAD:
				cq_attr.wait_obj = FI_WAIT_UNSPEC;
				cq_attr.wait_cond = FI_CQ_COND_NONE;
				break;
			default:
				cq_attr.wait_obj = FI_WAIT_NONE;
				break;
			}

			if (opts.tx_cq_size)
				cq_attr.size = opts.tx_cq_size;
			else
				cq_attr.size = fi->tx_attr->size;

			if (opts.rx_cq_size)
				cq_attr.size = opts.rx_cq_size;
			else
				cq_attr.size = fi->rx_attr->size;

			ret = fi_cq_open(domain, &cq_attr, new_txcq, new_txcq);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_cq_open", ret);
				return ret;
			}

			ret = fi_cq_open(domain, &cq_attr, new_rxcq, new_rxcq);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_cq_open", ret);
				return ret;
			}

			// Setup Address Vector
			if (!av && (fi->ep_attr->type == FI_EP_RDM || fi->ep_attr->type == FI_EP_DGRAM))
			{
				if (fi->domain_attr->av_type != FI_AV_UNSPEC)
					av_attr.type = fi->domain_attr->av_type;

				if (opts.av_name)
				{
					av_attr.name = opts.av_name;
				}
				av_attr.count = opts.av_size;
				ret = fi_av_open(domain, &av_attr, &av, NULL);
				if (ret)
				{
					IRPC_OFI_PRINTERR("fi_av_open", ret);
					return ret;
				}
			}

			return 0;
		}

		int lrpc_ofi_get_cq_fd(struct fid_cq *cq, int *fd)
		{
			int ret = FI_SUCCESS;

			if (cq && opts.comp_method == COMP_WAIT_FD)
			{
				ret = fi_control(&cq->fid, FI_GETWAIT, fd);
				if (ret)
					IRPC_OFI_PRINTERR("fi_control(FI_GETWAIT)", ret);
			}

			return ret;
		}

		int lrpc_ofi_enable_ep(struct fid_ep *bind_ep, struct fid_eq *bind_eq, struct fid_av *bind_av,
							   struct fid_cq *bind_txcq, struct fid_cq *bind_rxcq)
		{
			uint64_t flags;
			int ret;

			// Bind Event Queue
			if (fi->ep_attr->type == FI_EP_MSG || fi->caps & FI_MULTICAST || fi->caps & FI_COLLECTIVE)
				IRPC_OFI_EP_BIND(bind_ep, bind_eq, 0);

			// Bind Address Vector
			IRPC_OFI_EP_BIND(bind_ep, bind_av, 0);

			// Bind TX Completion Queue
			flags = FI_TRANSMIT;
			if (!(opts.options & OPT_TX_CQ))
				flags |= FI_SELECTIVE_COMPLETION;
			IRPC_OFI_EP_BIND(bind_ep, bind_txcq, flags);

			// Bind RX Completion Queue
			flags = FI_RECV;
			if (!(opts.options & OPT_RX_CQ))
				flags |= FI_SELECTIVE_COMPLETION;
			IRPC_OFI_EP_BIND(bind_ep, bind_rxcq, flags);

			ret = lrpc_ofi_get_cq_fd(bind_txcq, &tx_fd);
			if (ret)
				return ret;

			ret = lrpc_ofi_get_cq_fd(bind_rxcq, &rx_fd);
			if (ret)
				return ret;

			/* TODO: use control structure to select counter bindings explicitly */
			if (opts.options & OPT_TX_CQ)
				flags = 0;
			else
				flags = FI_SEND;

			if (hints->caps & (FI_WRITE | FI_READ))
				flags |= hints->caps & (FI_WRITE | FI_READ);
			else if (hints->caps & FI_RMA)
				flags |= FI_WRITE | FI_READ;

			if (opts.options & OPT_RX_CQ)
				flags = 0;
			else
				flags = FI_RECV;

			if (hints->caps & (FI_REMOTE_WRITE | FI_REMOTE_READ))
				flags |= hints->caps & (FI_REMOTE_WRITE | FI_REMOTE_READ);
			else if (hints->caps & FI_RMA)
				flags |= FI_REMOTE_WRITE | FI_REMOTE_READ;

			ret = fi_enable(bind_ep);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_enable", ret);
				return ret;
			}

			return 0;
		}

		int lrpc_ofi_getinfo(struct fi_info *hints, struct fi_info **info)
		{
			char *node, *service;
			uint64_t flags = 0;
			int ret;

			ret = lrpc_ofi_read_addr_opts(&node, &service, hints, &flags, &opts);
			if (ret)
				return ret;

			ret = fi_getinfo(IRPC_OFI_FIVERSION, node, service, flags, hints, info);
			if (ret)
			{
				IRPC_OFI_PRINTERR("fi_getinfo", ret);
				return ret;
			}

			return 0;
		}

		int lrpc_ofi_cq_readerr(struct fid_cq *cq)
		{
			struct fi_cq_err_entry cq_err;
			int ret;

			memset(&cq_err, 0, sizeof(cq_err));
			ret = fi_cq_readerr(cq, &cq_err, 0);
			if (ret < 0)
			{
				IRPC_OFI_PRINTERR("fi_cq_readerr", ret);
			}
			else
			{
				IRPC_OFI_CQ_ERR(cq, cq_err, NULL, 0);
				ret = -cq_err.err;
			}
			return ret;
		}

		int lrpc_ofi_progress(struct fid_cq *cq)
		{
			struct fi_cq_err_entry comp;
			int ret;

			ret = fi_cq_read(cq, &comp, 1);

			if (ret >= 0 || ret == -FI_EAGAIN)
				return 0;

			if (ret == -FI_EAVAIL)
			{
				ret = lrpc_ofi_cq_readerr(cq);
			}
			else
			{
				IRPC_OFI_PRINTERR("fi_cq_read/sread", ret);
			}
			return ret;
		}

		// Returns 1 if the buffer was sent successfully
		ssize_t lrpc_ofi_send_buffer(struct fid_ep *ep, fi_addr_t *to, char *tmpbuf, size_t message_len)
		{
			ssize_t ret;
			struct fi_cq_err_entry comp;

			// fprintf(stdout, "OFI is asked to send a buffer of size = %d\n", (int) message_len);
			if (message_len >= tx_size)
			{
				fprintf(stderr, "OFI Error: Transmit buffer too small.\n");
				return -FI_ETOOSMALL;
			}

			// Copy data from upper layer buffer
			// memcpy(tx_buf, tmpbuf, message_len);

			message_len += ((fi->tx_attr->mode & FI_MSG_PREFIX) ? fi->ep_attr->msg_prefix_size : 0);
			IRPC_OFI_POST(fi_send, lrpc_ofi_progress, txcq, "transmit", ep, tmpbuf, message_len, mr_desc, *to, &tx_ctx);

			ret = fi_cq_sread(txcq, &comp, 1, NULL, timeout);
			if (ret < 0)
			{
				if (ret == -FI_EAVAIL)
					ret = lrpc_ofi_cq_readerr(txcq);
				else if (ret != -FI_EAGAIN)
					IRPC_OFI_PRINTERR("get_cq_comp", ret);
			}

			return ret;
		}

		// Returns 1 if the buffer is received successfully
		ssize_t lrpc_ofi_recv_buffer(struct fid_ep *ep, fi_addr_t *from, char *tmpbuf)
		{
			ssize_t ret;
			size_t rcvsize;
			struct fi_cq_err_entry comp;

			// Read a Receive Completion
			ret = fi_cq_sreadfrom(rxcq, &comp, 1, from, NULL, timeout);
			if (ret < 0)
			{
				if (ret == -FI_EAVAIL)
					ret = lrpc_ofi_cq_readerr(rxcq);
				else if (ret != -FI_EAGAIN)
					IRPC_OFI_PRINTERR("get_cq_comp", ret);
				return ret;
			}
			
			// Copy received data into the upper layer buffer
			memcpy(tmpbuf, rx_buf, rx_size);

			// Post an RX Buffer
			rcvsize = MAX(rx_size, MAX_CTRL_MSG) + ((fi->rx_attr->mode & FI_MSG_PREFIX) ? fi->ep_attr->msg_prefix_size : 0);
			IRPC_OFI_POST(fi_recv, lrpc_ofi_progress, rxcq, "receive", ep, rx_buf, rcvsize, mr_desc, remote_fi_addr, &rx_ctx);

			// fprintf(stdout, "OFI Received data.\n");

			return ret;
		}

	}; // class Lrpc_ofi_transport

}
