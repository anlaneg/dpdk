/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn10k_ethdev.h"
#include "cn10k_rx.h"

#define R(name, f5, f4, f3, f2, f1, f0, flags)				       \
	uint16_t __rte_noinline __rte_hot				       \
		cn10k_nix_recv_pkts_vec_##name(void *rx_queue,                 \
					       struct rte_mbuf **rx_pkts,      \
					       uint16_t pkts)                  \
	{                                                                      \
		/* TSTMP is not supported by vector */                         \
		if ((flags) & NIX_RX_OFFLOAD_TSTAMP_F)                         \
			return 0;                                              \
		return cn10k_nix_recv_pkts_vector(rx_queue, rx_pkts, pkts,     \
						  (flags));		       \
	}

NIX_RX_FASTPATH_MODES
#undef R
