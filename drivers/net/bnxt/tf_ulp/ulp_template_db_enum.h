/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Tue Dec  8 14:57:13 2020 */

#ifndef ULP_TEMPLATE_DB_H_
#define ULP_TEMPLATE_DB_H_

#define BNXT_ULP_REGFILE_MAX_SZ 32
#define BNXT_ULP_MAX_NUM_DEVICES 4
#define BNXT_ULP_LOG2_MAX_NUM_DEV 2
#define BNXT_ULP_GEN_TBL_MAX_SZ 6
#define BNXT_ULP_CLASS_SIG_TBL_MAX_SZ 512
#define BNXT_ULP_CLASS_MATCH_LIST_MAX_SZ 81
#define BNXT_ULP_CLASS_HID_LOW_PRIME 4049
#define BNXT_ULP_CLASS_HID_HIGH_PRIME 7919
#define BNXT_ULP_CLASS_HID_SHFTR 25
#define BNXT_ULP_CLASS_HID_SHFTL 23
#define BNXT_ULP_CLASS_HID_MASK 511
#define BNXT_ULP_ACT_SIG_TBL_MAX_SZ 2048
#define BNXT_ULP_ACT_MATCH_LIST_MAX_SZ 30
#define BNXT_ULP_ACT_HID_LOW_PRIME 7919
#define BNXT_ULP_ACT_HID_HIGH_PRIME 6701
#define BNXT_ULP_ACT_HID_SHFTR 24
#define BNXT_ULP_ACT_HID_SHFTL 23
#define BNXT_ULP_ACT_HID_MASK 2047
#define BNXT_ULP_GLB_RESOURCE_TBL_MAX_SZ 8
#define BNXT_ULP_GLB_TEMPLATE_TBL_MAX_SZ 1
#define BNXT_ULP_GLB_FIELD_TBL_SHIFT 7
#define BNXT_ULP_HDR_SIG_ID_SHIFT 4
#define BNXT_ULP_GLB_FIELD_TBL_SIZE 3033
#define ULP_WH_PLUS_CLASS_TMPL_LIST_SIZE 7
#define ULP_WH_PLUS_CLASS_TBL_LIST_SIZE 41
#define ULP_WH_PLUS_CLASS_KEY_INFO_LIST_SIZE 257
#define ULP_WH_PLUS_CLASS_IDENT_LIST_SIZE 11
#define ULP_WH_PLUS_CLASS_RESULT_FIELD_LIST_SIZE 367
#define ULP_WH_PLUS_CLASS_COND_LIST_SIZE 14
#define ULP_STINGRAY_CLASS_TMPL_LIST_SIZE 7
#define ULP_STINGRAY_CLASS_TBL_LIST_SIZE 38
#define ULP_STINGRAY_CLASS_KEY_INFO_LIST_SIZE 192
#define ULP_STINGRAY_CLASS_IDENT_LIST_SIZE 10
#define ULP_STINGRAY_CLASS_RESULT_FIELD_LIST_SIZE 341
#define ULP_STINGRAY_CLASS_COND_LIST_SIZE 10
#define ULP_WH_PLUS_ACT_TMPL_LIST_SIZE 3
#define ULP_WH_PLUS_ACT_TBL_LIST_SIZE 11
#define ULP_WH_PLUS_ACT_KEY_INFO_LIST_SIZE 2
#define ULP_WH_PLUS_ACT_IDENT_LIST_SIZE 1
#define ULP_WH_PLUS_ACT_RESULT_FIELD_LIST_SIZE 132
#define ULP_WH_PLUS_ACT_COND_LIST_SIZE 13
#define ULP_STINGRAY_ACT_TMPL_LIST_SIZE 2
#define ULP_STINGRAY_ACT_TBL_LIST_SIZE 4
#define ULP_STINGRAY_ACT_KEY_INFO_LIST_SIZE 0
#define ULP_STINGRAY_ACT_IDENT_LIST_SIZE 0
#define ULP_STINGRAY_ACT_RESULT_FIELD_LIST_SIZE 65
#define ULP_STINGRAY_ACT_COND_LIST_SIZE 2

enum bnxt_ulp_act_bit {
	BNXT_ULP_ACT_BIT_MARK                = 0x0000000000000001,
	BNXT_ULP_ACT_BIT_DROP                = 0x0000000000000002,
	BNXT_ULP_ACT_BIT_COUNT               = 0x0000000000000004,
	BNXT_ULP_ACT_BIT_RSS                 = 0x0000000000000008,
	BNXT_ULP_ACT_BIT_METER               = 0x0000000000000010,
	BNXT_ULP_ACT_BIT_VXLAN_DECAP         = 0x0000000000000020,
	BNXT_ULP_ACT_BIT_POP_MPLS            = 0x0000000000000040,
	BNXT_ULP_ACT_BIT_PUSH_MPLS           = 0x0000000000000080,
	BNXT_ULP_ACT_BIT_MAC_SWAP            = 0x0000000000000100,
	BNXT_ULP_ACT_BIT_SET_MAC_SRC         = 0x0000000000000200,
	BNXT_ULP_ACT_BIT_SET_MAC_DST         = 0x0000000000000400,
	BNXT_ULP_ACT_BIT_POP_VLAN            = 0x0000000000000800,
	BNXT_ULP_ACT_BIT_PUSH_VLAN           = 0x0000000000001000,
	BNXT_ULP_ACT_BIT_SET_VLAN_PCP        = 0x0000000000002000,
	BNXT_ULP_ACT_BIT_SET_VLAN_VID        = 0x0000000000004000,
	BNXT_ULP_ACT_BIT_SET_IPV4_SRC        = 0x0000000000008000,
	BNXT_ULP_ACT_BIT_SET_IPV4_DST        = 0x0000000000010000,
	BNXT_ULP_ACT_BIT_SET_IPV6_SRC        = 0x0000000000020000,
	BNXT_ULP_ACT_BIT_SET_IPV6_DST        = 0x0000000000040000,
	BNXT_ULP_ACT_BIT_DEC_TTL             = 0x0000000000080000,
	BNXT_ULP_ACT_BIT_SET_TP_SRC          = 0x0000000000100000,
	BNXT_ULP_ACT_BIT_SET_TP_DST          = 0x0000000000200000,
	BNXT_ULP_ACT_BIT_VXLAN_ENCAP         = 0x0000000000400000,
	BNXT_ULP_ACT_BIT_JUMP                = 0x0000000000800000,
	BNXT_ULP_ACT_BIT_SHARED              = 0x0000000001000000,
	BNXT_ULP_ACT_BIT_SAMPLE              = 0x0000000002000000,
	BNXT_ULP_ACT_BIT_SHARED_SAMPLE       = 0x0000000004000000,
	BNXT_ULP_ACT_BIT_LAST                = 0x0000000008000000
};

enum bnxt_ulp_hdr_bit {
	BNXT_ULP_HDR_BIT_O_ETH               = 0x0000000000000001,
	BNXT_ULP_HDR_BIT_OO_VLAN             = 0x0000000000000002,
	BNXT_ULP_HDR_BIT_OI_VLAN             = 0x0000000000000004,
	BNXT_ULP_HDR_BIT_O_IPV4              = 0x0000000000000008,
	BNXT_ULP_HDR_BIT_O_IPV6              = 0x0000000000000010,
	BNXT_ULP_HDR_BIT_O_TCP               = 0x0000000000000020,
	BNXT_ULP_HDR_BIT_O_UDP               = 0x0000000000000040,
	BNXT_ULP_HDR_BIT_T_VXLAN             = 0x0000000000000080,
	BNXT_ULP_HDR_BIT_T_GRE               = 0x0000000000000100,
	BNXT_ULP_HDR_BIT_I_ETH               = 0x0000000000000200,
	BNXT_ULP_HDR_BIT_IO_VLAN             = 0x0000000000000400,
	BNXT_ULP_HDR_BIT_II_VLAN             = 0x0000000000000800,
	BNXT_ULP_HDR_BIT_I_IPV4              = 0x0000000000001000,
	BNXT_ULP_HDR_BIT_I_IPV6              = 0x0000000000002000,
	BNXT_ULP_HDR_BIT_I_TCP               = 0x0000000000004000,
	BNXT_ULP_HDR_BIT_I_UDP               = 0x0000000000008000,
	BNXT_ULP_HDR_BIT_F1                  = 0x0000000000010000,
	BNXT_ULP_HDR_BIT_LAST                = 0x0000000000020000
};

enum bnxt_ulp_accept_opc {
	BNXT_ULP_ACCEPT_OPC_ALWAYS = 0,
	BNXT_ULP_ACCEPT_OPC_FLOW_SIG_ID_MATCH = 1,
	BNXT_ULP_ACCEPT_OPC_LAST = 2
};

enum bnxt_ulp_act_type {
	BNXT_ULP_ACT_TYPE_NOT_SUPPORTED = 0,
	BNXT_ULP_ACT_TYPE_SUPPORTED = 1,
	BNXT_ULP_ACT_TYPE_END = 2,
	BNXT_ULP_ACT_TYPE_LAST = 3
};

enum bnxt_ulp_byte_order {
	BNXT_ULP_BYTE_ORDER_BE = 0,
	BNXT_ULP_BYTE_ORDER_LE = 1,
	BNXT_ULP_BYTE_ORDER_LAST = 2
};

enum bnxt_ulp_cf_idx {
	BNXT_ULP_CF_IDX_NOT_USED = 0,
	BNXT_ULP_CF_IDX_MPLS_TAG_NUM = 1,
	BNXT_ULP_CF_IDX_O_VTAG_NUM = 2,
	BNXT_ULP_CF_IDX_O_NO_VTAG = 3,
	BNXT_ULP_CF_IDX_O_ONE_VTAG = 4,
	BNXT_ULP_CF_IDX_O_TWO_VTAGS = 5,
	BNXT_ULP_CF_IDX_I_VTAG_NUM = 6,
	BNXT_ULP_CF_IDX_I_NO_VTAG = 7,
	BNXT_ULP_CF_IDX_I_ONE_VTAG = 8,
	BNXT_ULP_CF_IDX_I_TWO_VTAGS = 9,
	BNXT_ULP_CF_IDX_INCOMING_IF = 10,
	BNXT_ULP_CF_IDX_DIRECTION = 11,
	BNXT_ULP_CF_IDX_SVIF_FLAG = 12,
	BNXT_ULP_CF_IDX_O_L3 = 13,
	BNXT_ULP_CF_IDX_I_L3 = 14,
	BNXT_ULP_CF_IDX_O_L4 = 15,
	BNXT_ULP_CF_IDX_I_L4 = 16,
	BNXT_ULP_CF_IDX_O_L4_SPORT = 17,
	BNXT_ULP_CF_IDX_O_L4_DPORT = 18,
	BNXT_ULP_CF_IDX_I_L4_SPORT = 19,
	BNXT_ULP_CF_IDX_I_L4_DPORT = 20,
	BNXT_ULP_CF_IDX_DEV_PORT_ID = 21,
	BNXT_ULP_CF_IDX_DRV_FUNC_SVIF = 22,
	BNXT_ULP_CF_IDX_DRV_FUNC_SPIF = 23,
	BNXT_ULP_CF_IDX_DRV_FUNC_PARIF = 24,
	BNXT_ULP_CF_IDX_DRV_FUNC_VNIC = 25,
	BNXT_ULP_CF_IDX_DRV_FUNC_PHY_PORT = 26,
	BNXT_ULP_CF_IDX_VF_FUNC_SVIF = 27,
	BNXT_ULP_CF_IDX_VF_FUNC_SPIF = 28,
	BNXT_ULP_CF_IDX_VF_FUNC_PARIF = 29,
	BNXT_ULP_CF_IDX_VF_FUNC_VNIC = 30,
	BNXT_ULP_CF_IDX_PHY_PORT_SVIF = 31,
	BNXT_ULP_CF_IDX_PHY_PORT_SPIF = 32,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF = 33,
	BNXT_ULP_CF_IDX_PHY_PORT_VPORT = 34,
	BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG = 35,
	BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG = 36,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL = 37,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL = 38,
	BNXT_ULP_CF_IDX_ACT_PORT_IS_SET = 39,
	BNXT_ULP_CF_IDX_ACT_PORT_TYPE = 40,
	BNXT_ULP_CF_IDX_MATCH_PORT_TYPE = 41,
	BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP = 42,
	BNXT_ULP_CF_IDX_VF_TO_VF = 43,
	BNXT_ULP_CF_IDX_L3_HDR_CNT = 44,
	BNXT_ULP_CF_IDX_L4_HDR_CNT = 45,
	BNXT_ULP_CF_IDX_VFR_MODE = 46,
	BNXT_ULP_CF_IDX_L3_TUN = 47,
	BNXT_ULP_CF_IDX_L3_TUN_DECAP = 48,
	BNXT_ULP_CF_IDX_FID = 49,
	BNXT_ULP_CF_IDX_HDR_SIG_ID = 50,
	BNXT_ULP_CF_IDX_FLOW_SIG_ID = 51,
	BNXT_ULP_CF_IDX_LAST = 52
};

enum bnxt_ulp_cond_list_opc {
	BNXT_ULP_COND_LIST_OPC_TRUE = 0,
	BNXT_ULP_COND_LIST_OPC_FALSE = 1,
	BNXT_ULP_COND_LIST_OPC_OR = 2,
	BNXT_ULP_COND_LIST_OPC_AND = 3,
	BNXT_ULP_COND_LIST_OPC_LAST = 4
};

enum bnxt_ulp_cond_opc {
	BNXT_ULP_COND_OPC_CF_IS_SET = 0,
	BNXT_ULP_COND_OPC_CF_NOT_SET = 1,
	BNXT_ULP_COND_OPC_ACT_BIT_IS_SET = 2,
	BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET = 3,
	BNXT_ULP_COND_OPC_HDR_BIT_IS_SET = 4,
	BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET = 5,
	BNXT_ULP_COND_OPC_FIELD_BIT_IS_SET = 6,
	BNXT_ULP_COND_OPC_FIELD_BIT_NOT_SET = 7,
	BNXT_ULP_COND_OPC_RF_IS_SET = 8,
	BNXT_ULP_COND_OPC_RF_NOT_SET = 9,
	BNXT_ULP_COND_OPC_LAST = 10
};

enum bnxt_ulp_critical_resource {
	BNXT_ULP_CRITICAL_RESOURCE_NO = 0,
	BNXT_ULP_CRITICAL_RESOURCE_YES = 1,
	BNXT_ULP_CRITICAL_RESOURCE_LAST = 2
};

enum bnxt_ulp_device_id {
	BNXT_ULP_DEVICE_ID_WH_PLUS = 0,
	BNXT_ULP_DEVICE_ID_THOR = 1,
	BNXT_ULP_DEVICE_ID_STINGRAY = 2,
	BNXT_ULP_DEVICE_ID_STINGRAY2 = 3,
	BNXT_ULP_DEVICE_ID_LAST = 4
};

enum bnxt_ulp_df_param_type {
	BNXT_ULP_DF_PARAM_TYPE_DEV_PORT_ID = 0,
	BNXT_ULP_DF_PARAM_TYPE_LAST = 1
};

enum bnxt_ulp_direction {
	BNXT_ULP_DIRECTION_INGRESS = 0,
	BNXT_ULP_DIRECTION_EGRESS = 1,
	BNXT_ULP_DIRECTION_LAST = 2
};

enum bnxt_ulp_fdb_opc {
	BNXT_ULP_FDB_OPC_PUSH = 0,
	BNXT_ULP_FDB_OPC_ALLOC_PUSH_REGFILE = 1,
	BNXT_ULP_FDB_OPC_PUSH_REGFILE = 2,
	BNXT_ULP_FDB_OPC_NOP = 3,
	BNXT_ULP_FDB_OPC_LAST = 4
};

enum bnxt_ulp_fdb_type {
	BNXT_ULP_FDB_TYPE_REGULAR = 0,
	BNXT_ULP_FDB_TYPE_DEFAULT = 1,
	BNXT_ULP_FDB_TYPE_RID = 2,
	BNXT_ULP_FDB_TYPE_LAST = 3
};

enum bnxt_ulp_field_cond_src {
	BNXT_ULP_FIELD_COND_SRC_TRUE = 0,
	BNXT_ULP_FIELD_COND_SRC_CF = 1,
	BNXT_ULP_FIELD_COND_SRC_RF = 2,
	BNXT_ULP_FIELD_COND_SRC_ACT_BIT = 3,
	BNXT_ULP_FIELD_COND_SRC_HDR_BIT = 4,
	BNXT_ULP_FIELD_COND_SRC_FIELD_BIT = 5,
	BNXT_ULP_FIELD_COND_SRC_LAST = 6
};

enum bnxt_ulp_field_opc {
	BNXT_ULP_FIELD_OPC_COND_OP = 0,
	BNXT_ULP_FIELD_OPC_SRC1_PLUS_CONST = 1,
	BNXT_ULP_FIELD_OPC_SRC1_MINUS_CONST = 2,
	BNXT_ULP_FIELD_OPC_SRC1_PLUS_CONST_POST = 3,
	BNXT_ULP_FIELD_OPC_SRC1_MINUS_CONST_POST = 4,
	BNXT_ULP_FIELD_OPC_LAST = 5
};

enum bnxt_ulp_field_src {
	BNXT_ULP_FIELD_SRC_ZERO = 0,
	BNXT_ULP_FIELD_SRC_CONST = 1,
	BNXT_ULP_FIELD_SRC_CF = 2,
	BNXT_ULP_FIELD_SRC_RF = 3,
	BNXT_ULP_FIELD_SRC_ACT_PROP = 4,
	BNXT_ULP_FIELD_SRC_ACT_PROP_SZ = 5,
	BNXT_ULP_FIELD_SRC_GLB_RF = 6,
	BNXT_ULP_FIELD_SRC_HF = 7,
	BNXT_ULP_FIELD_SRC_HDR_BIT = 8,
	BNXT_ULP_FIELD_SRC_ACT_BIT = 9,
	BNXT_ULP_FIELD_SRC_FIELD_BIT = 10,
	BNXT_ULP_FIELD_SRC_SKIP = 11,
	BNXT_ULP_FIELD_SRC_REJECT = 12,
	BNXT_ULP_FIELD_SRC_LAST = 13
};

enum bnxt_ulp_generic_tbl_opc {
	BNXT_ULP_GENERIC_TBL_OPC_NOT_USED = 0,
	BNXT_ULP_GENERIC_TBL_OPC_READ = 1,
	BNXT_ULP_GENERIC_TBL_OPC_WRITE = 2,
	BNXT_ULP_GENERIC_TBL_OPC_LAST = 3
};

enum bnxt_ulp_glb_rf_idx {
	BNXT_ULP_GLB_RF_IDX_NOT_USED = 0,
	BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID = 1,
	BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR = 2,
	BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID = 3,
	BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID = 4,
	BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR = 5,
	BNXT_ULP_GLB_RF_IDX_LAST = 6
};

enum bnxt_ulp_hdr_type {
	BNXT_ULP_HDR_TYPE_NOT_SUPPORTED = 0,
	BNXT_ULP_HDR_TYPE_SUPPORTED = 1,
	BNXT_ULP_HDR_TYPE_END = 2,
	BNXT_ULP_HDR_TYPE_LAST = 3
};

enum bnxt_ulp_if_tbl_opc {
	BNXT_ULP_IF_TBL_OPC_NOT_USED = 0,
	BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD = 1,
	BNXT_ULP_IF_TBL_OPC_WR_REGFILE = 2,
	BNXT_ULP_IF_TBL_OPC_WR_CONST = 3,
	BNXT_ULP_IF_TBL_OPC_RD_COMP_FIELD = 4,
	BNXT_ULP_IF_TBL_OPC_LAST = 5
};

enum bnxt_ulp_index_tbl_opc {
	BNXT_ULP_INDEX_TBL_OPC_NOT_USED = 0,
	BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE = 1,
	BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE = 2,
	BNXT_ULP_INDEX_TBL_OPC_SRCH_ALLOC_WR_REGFILE = 3,
	BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE = 4,
	BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE = 5,
	BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE = 6,
	BNXT_ULP_INDEX_TBL_OPC_LAST = 7
};

enum bnxt_ulp_mark_db_opc {
	BNXT_ULP_MARK_DB_OPC_NOP = 0,
	BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION = 1,
	BNXT_ULP_MARK_DB_OPC_PUSH_AND_SET_VFR_FLAG = 2,
	BNXT_ULP_MARK_DB_OPC_LAST = 3
};

enum bnxt_ulp_match_type {
	BNXT_ULP_MATCH_TYPE_EM = 0,
	BNXT_ULP_MATCH_TYPE_WM = 1,
	BNXT_ULP_MATCH_TYPE_LAST = 2
};

enum bnxt_ulp_mem_type_opc {
	BNXT_ULP_MEM_TYPE_OPC_NOP = 0,
	BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT = 1,
	BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT = 2,
	BNXT_ULP_MEM_TYPE_OPC_LAST = 3
};

enum bnxt_ulp_pri_opc {
	BNXT_ULP_PRI_OPC_NOT_USED = 0,
	BNXT_ULP_PRI_OPC_CONST = 1,
	BNXT_ULP_PRI_OPC_APP_PRI = 2,
	BNXT_ULP_PRI_OPC_LAST = 3
};

enum bnxt_ulp_rf_idx {
	BNXT_ULP_RF_IDX_NOT_USED = 0,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 = 1,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_1 = 2,
	BNXT_ULP_RF_IDX_PROF_FUNC_ID_0 = 3,
	BNXT_ULP_RF_IDX_PROF_FUNC_ID_1 = 4,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 = 5,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_1 = 6,
	BNXT_ULP_RF_IDX_WC_PROFILE_ID_0 = 7,
	BNXT_ULP_RF_IDX_WC_PROFILE_ID_1 = 8,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR = 9,
	BNXT_ULP_RF_IDX_ACTION_PTR_0 = 10,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 = 11,
	BNXT_ULP_RF_IDX_ENCAP_PTR_1 = 12,
	BNXT_ULP_RF_IDX_CRITICAL_RESOURCE = 13,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 = 14,
	BNXT_ULP_RF_IDX_MAIN_SP_PTR = 15,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 = 16,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 = 17,
	BNXT_ULP_RF_IDX_ACTION_REC_SIZE = 18,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 = 19,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_1 = 20,
	BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 = 21,
	BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_1 = 22,
	BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0 = 23,
	BNXT_ULP_RF_IDX_WC_TCAM_INDEX_1 = 24,
	BNXT_ULP_RF_IDX_SRC_PROPERTY_PTR = 25,
	BNXT_ULP_RF_IDX_GENERIC_TBL_HIT = 26,
	BNXT_ULP_RF_IDX_MIRROR_PTR_0 = 27,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 = 28,
	BNXT_ULP_RF_IDX_HDR_SIG_ID = 29,
	BNXT_ULP_RF_IDX_FLOW_SIG_ID = 30,
	BNXT_ULP_RF_IDX_RID = 31,
	BNXT_ULP_RF_IDX_LAST = 32
};

enum bnxt_ulp_tcam_tbl_opc {
	BNXT_ULP_TCAM_TBL_OPC_NOT_USED = 0,
	BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE = 1,
	BNXT_ULP_TCAM_TBL_OPC_SRCH_ALLOC_WR_REGFILE = 2,
	BNXT_ULP_TCAM_TBL_OPC_LAST = 3
};

enum bnxt_ulp_template_type {
	BNXT_ULP_TEMPLATE_TYPE_CLASS = 0,
	BNXT_ULP_TEMPLATE_TYPE_ACTION = 1,
	BNXT_ULP_TEMPLATE_TYPE_LAST = 2
};

enum bnxt_ulp_fdb_resource_flags {
	BNXT_ULP_FDB_RESOURCE_FLAGS_DIR_INGR = 0x00,
	BNXT_ULP_FDB_RESOURCE_FLAGS_DIR_EGR = 0x01
};

enum bnxt_ulp_flow_dir_bitmask {
	BNXT_ULP_FLOW_DIR_BITMASK_ING = 0x0000000000000000,
	BNXT_ULP_FLOW_DIR_BITMASK_EGR = 0x8000000000000000
};

enum bnxt_ulp_match_type_bitmask {
	BNXT_ULP_MATCH_TYPE_BITMASK_EM = 0x0000000000000000,
	BNXT_ULP_MATCH_TYPE_BITMASK_WM = 0x8000000000000000
};

enum bnxt_ulp_resource_func {
	BNXT_ULP_RESOURCE_FUNC_INVALID = 0x00,
	BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE = 0x20,
	BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE = 0x40,
	BNXT_ULP_RESOURCE_FUNC_RSVD2 = 0x60,
	BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE = 0x80,
	BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE = 0x81,
	BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE = 0x82,
	BNXT_ULP_RESOURCE_FUNC_IDENTIFIER = 0x83,
	BNXT_ULP_RESOURCE_FUNC_IF_TABLE = 0x84,
	BNXT_ULP_RESOURCE_FUNC_HW_FID = 0x85,
	BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW = 0x86,
	BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW = 0x87,
	BNXT_ULP_RESOURCE_FUNC_BRANCH_TABLE = 0x88
};

enum bnxt_ulp_resource_sub_type {
	BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED = 0,
	BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL = 0,
	BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION = 1,
	BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT = 2,
	BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT_ACC = 3,
	BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_EXT_COUNT = 4,
	BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM = 0,
	BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM = 1,
	BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR = 2
};

enum bnxt_ulp_act_prop_sz {
	BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN_SZ = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SZ = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_SZ = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_TYPE = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_NUM = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_L3_TYPE = 4,
	BNXT_ULP_ACT_PROP_SZ_MPLS_POP_NUM = 4,
	BNXT_ULP_ACT_PROP_SZ_MPLS_PUSH_NUM = 4,
	BNXT_ULP_ACT_PROP_SZ_PORT_ID = 4,
	BNXT_ULP_ACT_PROP_SZ_VNIC = 4,
	BNXT_ULP_ACT_PROP_SZ_VPORT = 4,
	BNXT_ULP_ACT_PROP_SZ_MARK = 4,
	BNXT_ULP_ACT_PROP_SZ_COUNT = 4,
	BNXT_ULP_ACT_PROP_SZ_METER = 4,
	BNXT_ULP_ACT_PROP_SZ_SET_MAC_SRC = 8,
	BNXT_ULP_ACT_PROP_SZ_SET_MAC_DST = 8,
	BNXT_ULP_ACT_PROP_SZ_PUSH_VLAN = 2,
	BNXT_ULP_ACT_PROP_SZ_SET_VLAN_PCP = 1,
	BNXT_ULP_ACT_PROP_SZ_SET_VLAN_VID = 2,
	BNXT_ULP_ACT_PROP_SZ_SET_IPV4_SRC = 4,
	BNXT_ULP_ACT_PROP_SZ_SET_IPV4_DST = 4,
	BNXT_ULP_ACT_PROP_SZ_SET_IPV6_SRC = 16,
	BNXT_ULP_ACT_PROP_SZ_SET_IPV6_DST = 16,
	BNXT_ULP_ACT_PROP_SZ_SET_TP_SRC = 2,
	BNXT_ULP_ACT_PROP_SZ_SET_TP_DST = 2,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_0 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_1 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_2 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_3 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_4 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_5 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_6 = 4,
	BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_7 = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC = 6,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC = 6,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG = 8,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_IP = 32,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SRC = 16,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_UDP = 4,
	BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN = 32,
	BNXT_ULP_ACT_PROP_SZ_JUMP = 4,
	BNXT_ULP_ACT_PROP_SZ_SHARED_HANDLE = 8,
	BNXT_ULP_ACT_PROP_SZ_LAST = 4
};

enum bnxt_ulp_act_prop_idx {
	BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ = 0,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ = 4,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ = 8,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE = 12,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_NUM = 16,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE = 20,
	BNXT_ULP_ACT_PROP_IDX_MPLS_POP_NUM = 24,
	BNXT_ULP_ACT_PROP_IDX_MPLS_PUSH_NUM = 28,
	BNXT_ULP_ACT_PROP_IDX_PORT_ID = 32,
	BNXT_ULP_ACT_PROP_IDX_VNIC = 36,
	BNXT_ULP_ACT_PROP_IDX_VPORT = 40,
	BNXT_ULP_ACT_PROP_IDX_MARK = 44,
	BNXT_ULP_ACT_PROP_IDX_COUNT = 48,
	BNXT_ULP_ACT_PROP_IDX_METER = 52,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC = 56,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST = 64,
	BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN = 72,
	BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP = 74,
	BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID = 75,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC = 77,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST = 81,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC = 85,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST = 101,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC = 117,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_DST = 119,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_0 = 121,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_1 = 125,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_2 = 129,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_3 = 133,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_4 = 137,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_5 = 141,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_6 = 145,
	BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_7 = 149,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC = 153,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC = 159,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG = 165,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_IP = 173,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC = 205,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP = 221,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN = 225,
	BNXT_ULP_ACT_PROP_IDX_JUMP = 257,
	BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE = 261,
	BNXT_ULP_ACT_PROP_IDX_LAST = 269
};

enum ulp_wp_sym {
	ULP_WP_SYM_PKT_TYPE_IGNORE = 0,
	ULP_WP_SYM_PKT_TYPE_L2 = 0,
	ULP_WP_SYM_PKT_TYPE_0_IGNORE = 0,
	ULP_WP_SYM_PKT_TYPE_0_L2 = 0,
	ULP_WP_SYM_PKT_TYPE_1_IGNORE = 0,
	ULP_WP_SYM_PKT_TYPE_1_L2 = 0,
	ULP_WP_SYM_RECYCLE_CNT_IGNORE = 0,
	ULP_WP_SYM_RECYCLE_CNT_ZERO = 0,
	ULP_WP_SYM_RECYCLE_CNT_ONE = 1,
	ULP_WP_SYM_RECYCLE_CNT_TWO = 2,
	ULP_WP_SYM_RECYCLE_CNT_THREE = 3,
	ULP_WP_SYM_AGG_ERROR_IGNORE = 0,
	ULP_WP_SYM_AGG_ERROR_NO = 0,
	ULP_WP_SYM_AGG_ERROR_YES = 1,
	ULP_WP_SYM_RESERVED_IGNORE = 0,
	ULP_WP_SYM_HREC_NEXT_IGNORE = 0,
	ULP_WP_SYM_HREC_NEXT_NO = 0,
	ULP_WP_SYM_HREC_NEXT_YES = 1,
	ULP_WP_SYM_TL2_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_TL2_HDR_VALID_NO = 0,
	ULP_WP_SYM_TL2_HDR_VALID_YES = 1,
	ULP_WP_SYM_TL2_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_TL2_HDR_TYPE_DIX = 0,
	ULP_WP_SYM_TL2_UC_MC_BC_IGNORE = 0,
	ULP_WP_SYM_TL2_UC_MC_BC_UC = 0,
	ULP_WP_SYM_TL2_UC_MC_BC_MC = 2,
	ULP_WP_SYM_TL2_UC_MC_BC_BC = 3,
	ULP_WP_SYM_TL2_VTAG_PRESENT_IGNORE = 0,
	ULP_WP_SYM_TL2_VTAG_PRESENT_NO = 0,
	ULP_WP_SYM_TL2_VTAG_PRESENT_YES = 1,
	ULP_WP_SYM_TL2_TWO_VTAGS_IGNORE = 0,
	ULP_WP_SYM_TL2_TWO_VTAGS_NO = 0,
	ULP_WP_SYM_TL2_TWO_VTAGS_YES = 1,
	ULP_WP_SYM_TL3_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_TL3_HDR_VALID_NO = 0,
	ULP_WP_SYM_TL3_HDR_VALID_YES = 1,
	ULP_WP_SYM_TL3_HDR_ERROR_IGNORE = 0,
	ULP_WP_SYM_TL3_HDR_ERROR_NO = 0,
	ULP_WP_SYM_TL3_HDR_ERROR_YES = 1,
	ULP_WP_SYM_TL3_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_TL3_HDR_TYPE_IPV4 = 0,
	ULP_WP_SYM_TL3_HDR_TYPE_IPV6 = 1,
	ULP_WP_SYM_TL3_HDR_ISIP_IGNORE = 0,
	ULP_WP_SYM_TL3_HDR_ISIP_NO = 0,
	ULP_WP_SYM_TL3_HDR_ISIP_YES = 1,
	ULP_WP_SYM_TL3_IPV6_CMP_SRC_IGNORE = 0,
	ULP_WP_SYM_TL3_IPV6_CMP_SRC_NO = 0,
	ULP_WP_SYM_TL3_IPV6_CMP_SRC_YES = 1,
	ULP_WP_SYM_TL3_IPV6_CMP_DST_IGNORE = 0,
	ULP_WP_SYM_TL3_IPV6_CMP_DST_NO = 0,
	ULP_WP_SYM_TL3_IPV6_CMP_DST_YES = 1,
	ULP_WP_SYM_TL4_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_TL4_HDR_VALID_NO = 0,
	ULP_WP_SYM_TL4_HDR_VALID_YES = 1,
	ULP_WP_SYM_TL4_HDR_ERROR_IGNORE = 0,
	ULP_WP_SYM_TL4_HDR_ERROR_NO = 0,
	ULP_WP_SYM_TL4_HDR_ERROR_YES = 1,
	ULP_WP_SYM_TL4_HDR_IS_UDP_TCP_IGNORE = 0,
	ULP_WP_SYM_TL4_HDR_IS_UDP_TCP_NO = 0,
	ULP_WP_SYM_TL4_HDR_IS_UDP_TCP_YES = 1,
	ULP_WP_SYM_TL4_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_TL4_HDR_TYPE_TCP = 0,
	ULP_WP_SYM_TL4_HDR_TYPE_UDP = 1,
	ULP_WP_SYM_TUN_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_TUN_HDR_VALID_NO = 0,
	ULP_WP_SYM_TUN_HDR_VALID_YES = 1,
	ULP_WP_SYM_TUN_HDR_ERROR_IGNORE = 0,
	ULP_WP_SYM_TUN_HDR_ERROR_NO = 0,
	ULP_WP_SYM_TUN_HDR_ERROR_YES = 1,
	ULP_WP_SYM_TUN_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_TUN_HDR_TYPE_VXLAN = 0,
	ULP_WP_SYM_TUN_HDR_TYPE_GENEVE = 1,
	ULP_WP_SYM_TUN_HDR_TYPE_NVGRE = 2,
	ULP_WP_SYM_TUN_HDR_TYPE_GRE = 3,
	ULP_WP_SYM_TUN_HDR_TYPE_IPV4 = 4,
	ULP_WP_SYM_TUN_HDR_TYPE_IPV6 = 5,
	ULP_WP_SYM_TUN_HDR_TYPE_PPPOE = 6,
	ULP_WP_SYM_TUN_HDR_TYPE_MPLS = 7,
	ULP_WP_SYM_TUN_HDR_TYPE_UPAR1 = 8,
	ULP_WP_SYM_TUN_HDR_TYPE_UPAR2 = 9,
	ULP_WP_SYM_TUN_HDR_TYPE_NONE = 15,
	ULP_WP_SYM_TUN_HDR_FLAGS_IGNORE = 0,
	ULP_WP_SYM_L2_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_L2_HDR_VALID_NO = 0,
	ULP_WP_SYM_L2_HDR_VALID_YES = 1,
	ULP_WP_SYM_L2_HDR_ERROR_IGNORE = 0,
	ULP_WP_SYM_L2_HDR_ERROR_NO = 0,
	ULP_WP_SYM_L2_HDR_ERROR_YES = 1,
	ULP_WP_SYM_L2_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_L2_HDR_TYPE_DIX = 0,
	ULP_WP_SYM_L2_HDR_TYPE_LLC_SNAP = 1,
	ULP_WP_SYM_L2_HDR_TYPE_LLC = 2,
	ULP_WP_SYM_L2_UC_MC_BC_IGNORE = 0,
	ULP_WP_SYM_L2_UC_MC_BC_UC = 0,
	ULP_WP_SYM_L2_UC_MC_BC_MC = 2,
	ULP_WP_SYM_L2_UC_MC_BC_BC = 3,
	ULP_WP_SYM_L2_VTAG_PRESENT_IGNORE = 0,
	ULP_WP_SYM_L2_VTAG_PRESENT_NO = 0,
	ULP_WP_SYM_L2_VTAG_PRESENT_YES = 1,
	ULP_WP_SYM_L2_TWO_VTAGS_IGNORE = 0,
	ULP_WP_SYM_L2_TWO_VTAGS_NO = 0,
	ULP_WP_SYM_L2_TWO_VTAGS_YES = 1,
	ULP_WP_SYM_L3_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_L3_HDR_VALID_NO = 0,
	ULP_WP_SYM_L3_HDR_VALID_YES = 1,
	ULP_WP_SYM_L3_HDR_ERROR_IGNORE = 0,
	ULP_WP_SYM_L3_HDR_ERROR_NO = 0,
	ULP_WP_SYM_L3_HDR_ERROR_YES = 1,
	ULP_WP_SYM_L3_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_L3_HDR_TYPE_IPV4 = 0,
	ULP_WP_SYM_L3_HDR_TYPE_IPV6 = 1,
	ULP_WP_SYM_L3_HDR_TYPE_ARP = 2,
	ULP_WP_SYM_L3_HDR_TYPE_PTP = 3,
	ULP_WP_SYM_L3_HDR_TYPE_EAPOL = 4,
	ULP_WP_SYM_L3_HDR_TYPE_ROCE = 5,
	ULP_WP_SYM_L3_HDR_TYPE_FCOE = 6,
	ULP_WP_SYM_L3_HDR_TYPE_UPAR1 = 7,
	ULP_WP_SYM_L3_HDR_TYPE_UPAR2 = 8,
	ULP_WP_SYM_L3_HDR_ISIP_IGNORE = 0,
	ULP_WP_SYM_L3_HDR_ISIP_NO = 0,
	ULP_WP_SYM_L3_HDR_ISIP_YES = 1,
	ULP_WP_SYM_L3_IPV6_CMP_SRC_IGNORE = 0,
	ULP_WP_SYM_L3_IPV6_CMP_SRC_NO = 0,
	ULP_WP_SYM_L3_IPV6_CMP_SRC_YES = 1,
	ULP_WP_SYM_L3_IPV6_CMP_DST_IGNORE = 0,
	ULP_WP_SYM_L3_IPV6_CMP_DST_NO = 0,
	ULP_WP_SYM_L3_IPV6_CMP_DST_YES = 1,
	ULP_WP_SYM_L4_HDR_VALID_IGNORE = 0,
	ULP_WP_SYM_L4_HDR_VALID_NO = 0,
	ULP_WP_SYM_L4_HDR_VALID_YES = 1,
	ULP_WP_SYM_L4_HDR_ERROR_IGNORE = 0,
	ULP_WP_SYM_L4_HDR_ERROR_NO = 0,
	ULP_WP_SYM_L4_HDR_ERROR_YES = 1,
	ULP_WP_SYM_L4_HDR_TYPE_IGNORE = 0,
	ULP_WP_SYM_L4_HDR_TYPE_TCP = 0,
	ULP_WP_SYM_L4_HDR_TYPE_UDP = 1,
	ULP_WP_SYM_L4_HDR_TYPE_ICMP = 2,
	ULP_WP_SYM_L4_HDR_TYPE_UPAR1 = 3,
	ULP_WP_SYM_L4_HDR_TYPE_UPAR2 = 4,
	ULP_WP_SYM_L4_HDR_TYPE_BTH_V1 = 5,
	ULP_WP_SYM_L4_HDR_IS_UDP_TCP_IGNORE = 0,
	ULP_WP_SYM_L4_HDR_IS_UDP_TCP_NO = 0,
	ULP_WP_SYM_L4_HDR_IS_UDP_TCP_YES = 1,
	ULP_WP_SYM_POP_VLAN_NO = 0,
	ULP_WP_SYM_POP_VLAN_YES = 1,
	ULP_WP_SYM_DECAP_FUNC_NONE = 0,
	ULP_WP_SYM_DECAP_FUNC_THRU_TL2 = 3,
	ULP_WP_SYM_DECAP_FUNC_THRU_TL3 = 8,
	ULP_WP_SYM_DECAP_FUNC_THRU_TL4 = 9,
	ULP_WP_SYM_DECAP_FUNC_THRU_TUN = 10,
	ULP_WP_SYM_DECAP_FUNC_THRU_L2 = 11,
	ULP_WP_SYM_DECAP_FUNC_THRU_L3 = 12,
	ULP_WP_SYM_DECAP_FUNC_THRU_L4 = 13,
	ULP_WP_SYM_ECV_VALID_NO = 0,
	ULP_WP_SYM_ECV_VALID_YES = 1,
	ULP_WP_SYM_ECV_CUSTOM_EN_NO = 0,
	ULP_WP_SYM_ECV_CUSTOM_EN_YES = 1,
	ULP_WP_SYM_ECV_L2_EN_NO = 0,
	ULP_WP_SYM_ECV_L2_EN_YES = 1,
	ULP_WP_SYM_ECV_VTAG_TYPE_NOP = 0,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI = 1,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_IVLAN_PRI = 2,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_REMAP_DIFFSERV = 3,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_2_ENCAP_PRI = 4,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_2_REMAP_DIFFSERV = 5,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_ENCAP_PRI = 6,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_REMAP_DIFFSERV = 7,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_0 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_1 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_2 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_3 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_4 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_5 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_6 = 8,
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_0_PRI_7 = 8,
	ULP_WP_SYM_ECV_L3_TYPE_NONE = 0,
	ULP_WP_SYM_ECV_L3_TYPE_IPV4 = 4,
	ULP_WP_SYM_ECV_L3_TYPE_IPV6 = 5,
	ULP_WP_SYM_ECV_L3_TYPE_MPLS_8847 = 6,
	ULP_WP_SYM_ECV_L3_TYPE_MPLS_8848 = 7,
	ULP_WP_SYM_ECV_L4_TYPE_NONE = 0,
	ULP_WP_SYM_ECV_L4_TYPE_UDP = 4,
	ULP_WP_SYM_ECV_L4_TYPE_UDP_CSUM = 5,
	ULP_WP_SYM_ECV_L4_TYPE_UDP_ENTROPY = 6,
	ULP_WP_SYM_ECV_L4_TYPE_UDP_ENTROPY_CSUM = 7,
	ULP_WP_SYM_ECV_TUN_TYPE_NONE = 0,
	ULP_WP_SYM_ECV_TUN_TYPE_GENERIC = 1,
	ULP_WP_SYM_ECV_TUN_TYPE_VXLAN = 2,
	ULP_WP_SYM_ECV_TUN_TYPE_NGE = 3,
	ULP_WP_SYM_ECV_TUN_TYPE_NVGRE = 4,
	ULP_WP_SYM_ECV_TUN_TYPE_GRE = 5,
	ULP_WP_SYM_EEM_ACT_REC_INT = 1,
	ULP_WP_SYM_EEM_EXT_FLOW_CNTR = 0,
	ULP_WP_SYM_UC_ACT_REC = 0,
	ULP_WP_SYM_MC_ACT_REC = 1,
	ULP_WP_SYM_ACT_REC_DROP_YES = 1,
	ULP_WP_SYM_ACT_REC_DROP_NO = 0,
	ULP_WP_SYM_ACT_REC_POP_VLAN_YES = 1,
	ULP_WP_SYM_ACT_REC_POP_VLAN_NO = 0,
	ULP_WP_SYM_ACT_REC_METER_EN_YES = 1,
	ULP_WP_SYM_ACT_REC_METER_EN_NO = 0,
	ULP_WP_SYM_LOOPBACK_PORT = 4,
	ULP_WP_SYM_LOOPBACK_PARIF = 15,
	ULP_WP_SYM_EXT_EM_MAX_KEY_SIZE = 448,
	ULP_WP_SYM_MATCH_TYPE_EM = 0,
	ULP_WP_SYM_MATCH_TYPE_WM = 1,
	ULP_WP_SYM_IP_PROTO_ICMP = 1,
	ULP_WP_SYM_IP_PROTO_IGMP = 2,
	ULP_WP_SYM_IP_PROTO_IP_IN_IP = 4,
	ULP_WP_SYM_IP_PROTO_TCP = 6,
	ULP_WP_SYM_IP_PROTO_UDP = 17,
	ULP_WP_SYM_VF_FUNC_PARIF = 15,
	ULP_WP_SYM_NO = 0,
	ULP_WP_SYM_YES = 1,
	ULP_WP_SYM_RECYCLE_DST = 0x800
};

enum ulp_sr_sym {
	ULP_SR_SYM_PKT_TYPE_IGNORE = 0,
	ULP_SR_SYM_PKT_TYPE_L2 = 0,
	ULP_SR_SYM_PKT_TYPE_0_IGNORE = 0,
	ULP_SR_SYM_PKT_TYPE_0_L2 = 0,
	ULP_SR_SYM_PKT_TYPE_1_IGNORE = 0,
	ULP_SR_SYM_PKT_TYPE_1_L2 = 0,
	ULP_SR_SYM_RECYCLE_CNT_IGNORE = 0,
	ULP_SR_SYM_RECYCLE_CNT_ZERO = 0,
	ULP_SR_SYM_RECYCLE_CNT_ONE = 1,
	ULP_SR_SYM_RECYCLE_CNT_TWO = 2,
	ULP_SR_SYM_RECYCLE_CNT_THREE = 3,
	ULP_SR_SYM_AGG_ERROR_IGNORE = 0,
	ULP_SR_SYM_AGG_ERROR_NO = 0,
	ULP_SR_SYM_AGG_ERROR_YES = 1,
	ULP_SR_SYM_RESERVED_IGNORE = 0,
	ULP_SR_SYM_HREC_NEXT_IGNORE = 0,
	ULP_SR_SYM_HREC_NEXT_NO = 0,
	ULP_SR_SYM_HREC_NEXT_YES = 1,
	ULP_SR_SYM_TL2_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_TL2_HDR_VALID_NO = 0,
	ULP_SR_SYM_TL2_HDR_VALID_YES = 1,
	ULP_SR_SYM_TL2_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_TL2_HDR_TYPE_DIX = 0,
	ULP_SR_SYM_TL2_UC_MC_BC_IGNORE = 0,
	ULP_SR_SYM_TL2_UC_MC_BC_UC = 0,
	ULP_SR_SYM_TL2_UC_MC_BC_MC = 2,
	ULP_SR_SYM_TL2_UC_MC_BC_BC = 3,
	ULP_SR_SYM_TL2_VTAG_PRESENT_IGNORE = 0,
	ULP_SR_SYM_TL2_VTAG_PRESENT_NO = 0,
	ULP_SR_SYM_TL2_VTAG_PRESENT_YES = 1,
	ULP_SR_SYM_TL2_TWO_VTAGS_IGNORE = 0,
	ULP_SR_SYM_TL2_TWO_VTAGS_NO = 0,
	ULP_SR_SYM_TL2_TWO_VTAGS_YES = 1,
	ULP_SR_SYM_TL3_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_TL3_HDR_VALID_NO = 0,
	ULP_SR_SYM_TL3_HDR_VALID_YES = 1,
	ULP_SR_SYM_TL3_HDR_ERROR_IGNORE = 0,
	ULP_SR_SYM_TL3_HDR_ERROR_NO = 0,
	ULP_SR_SYM_TL3_HDR_ERROR_YES = 1,
	ULP_SR_SYM_TL3_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_TL3_HDR_TYPE_IPV4 = 0,
	ULP_SR_SYM_TL3_HDR_TYPE_IPV6 = 1,
	ULP_SR_SYM_TL3_HDR_ISIP_IGNORE = 0,
	ULP_SR_SYM_TL3_HDR_ISIP_NO = 0,
	ULP_SR_SYM_TL3_HDR_ISIP_YES = 1,
	ULP_SR_SYM_TL3_IPV6_CMP_SRC_IGNORE = 0,
	ULP_SR_SYM_TL3_IPV6_CMP_SRC_NO = 0,
	ULP_SR_SYM_TL3_IPV6_CMP_SRC_YES = 1,
	ULP_SR_SYM_TL3_IPV6_CMP_DST_IGNORE = 0,
	ULP_SR_SYM_TL3_IPV6_CMP_DST_NO = 0,
	ULP_SR_SYM_TL3_IPV6_CMP_DST_YES = 1,
	ULP_SR_SYM_TL4_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_TL4_HDR_VALID_NO = 0,
	ULP_SR_SYM_TL4_HDR_VALID_YES = 1,
	ULP_SR_SYM_TL4_HDR_ERROR_IGNORE = 0,
	ULP_SR_SYM_TL4_HDR_ERROR_NO = 0,
	ULP_SR_SYM_TL4_HDR_ERROR_YES = 1,
	ULP_SR_SYM_TL4_HDR_IS_UDP_TCP_IGNORE = 0,
	ULP_SR_SYM_TL4_HDR_IS_UDP_TCP_NO = 0,
	ULP_SR_SYM_TL4_HDR_IS_UDP_TCP_YES = 1,
	ULP_SR_SYM_TL4_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_TL4_HDR_TYPE_TCP = 0,
	ULP_SR_SYM_TL4_HDR_TYPE_UDP = 1,
	ULP_SR_SYM_TUN_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_TUN_HDR_VALID_NO = 0,
	ULP_SR_SYM_TUN_HDR_VALID_YES = 1,
	ULP_SR_SYM_TUN_HDR_ERROR_IGNORE = 0,
	ULP_SR_SYM_TUN_HDR_ERROR_NO = 0,
	ULP_SR_SYM_TUN_HDR_ERROR_YES = 1,
	ULP_SR_SYM_TUN_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_TUN_HDR_TYPE_VXLAN = 0,
	ULP_SR_SYM_TUN_HDR_TYPE_GENEVE = 1,
	ULP_SR_SYM_TUN_HDR_TYPE_NVGRE = 2,
	ULP_SR_SYM_TUN_HDR_TYPE_GRE = 3,
	ULP_SR_SYM_TUN_HDR_TYPE_IPV4 = 4,
	ULP_SR_SYM_TUN_HDR_TYPE_IPV6 = 5,
	ULP_SR_SYM_TUN_HDR_TYPE_PPPOE = 6,
	ULP_SR_SYM_TUN_HDR_TYPE_MPLS = 7,
	ULP_SR_SYM_TUN_HDR_TYPE_UPAR1 = 8,
	ULP_SR_SYM_TUN_HDR_TYPE_UPAR2 = 9,
	ULP_SR_SYM_TUN_HDR_TYPE_NONE = 15,
	ULP_SR_SYM_TUN_HDR_FLAGS_IGNORE = 0,
	ULP_SR_SYM_L2_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_L2_HDR_VALID_NO = 0,
	ULP_SR_SYM_L2_HDR_VALID_YES = 1,
	ULP_SR_SYM_L2_HDR_ERROR_IGNORE = 0,
	ULP_SR_SYM_L2_HDR_ERROR_NO = 0,
	ULP_SR_SYM_L2_HDR_ERROR_YES = 1,
	ULP_SR_SYM_L2_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_L2_HDR_TYPE_DIX = 0,
	ULP_SR_SYM_L2_HDR_TYPE_LLC_SNAP = 1,
	ULP_SR_SYM_L2_HDR_TYPE_LLC = 2,
	ULP_SR_SYM_L2_UC_MC_BC_IGNORE = 0,
	ULP_SR_SYM_L2_UC_MC_BC_UC = 0,
	ULP_SR_SYM_L2_UC_MC_BC_MC = 2,
	ULP_SR_SYM_L2_UC_MC_BC_BC = 3,
	ULP_SR_SYM_L2_VTAG_PRESENT_IGNORE = 0,
	ULP_SR_SYM_L2_VTAG_PRESENT_NO = 0,
	ULP_SR_SYM_L2_VTAG_PRESENT_YES = 1,
	ULP_SR_SYM_L2_TWO_VTAGS_IGNORE = 0,
	ULP_SR_SYM_L2_TWO_VTAGS_NO = 0,
	ULP_SR_SYM_L2_TWO_VTAGS_YES = 1,
	ULP_SR_SYM_L3_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_L3_HDR_VALID_NO = 0,
	ULP_SR_SYM_L3_HDR_VALID_YES = 1,
	ULP_SR_SYM_L3_HDR_ERROR_IGNORE = 0,
	ULP_SR_SYM_L3_HDR_ERROR_NO = 0,
	ULP_SR_SYM_L3_HDR_ERROR_YES = 1,
	ULP_SR_SYM_L3_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_L3_HDR_TYPE_IPV4 = 0,
	ULP_SR_SYM_L3_HDR_TYPE_IPV6 = 1,
	ULP_SR_SYM_L3_HDR_TYPE_ARP = 2,
	ULP_SR_SYM_L3_HDR_TYPE_PTP = 3,
	ULP_SR_SYM_L3_HDR_TYPE_EAPOL = 4,
	ULP_SR_SYM_L3_HDR_TYPE_ROCE = 5,
	ULP_SR_SYM_L3_HDR_TYPE_FCOE = 6,
	ULP_SR_SYM_L3_HDR_TYPE_UPAR1 = 7,
	ULP_SR_SYM_L3_HDR_TYPE_UPAR2 = 8,
	ULP_SR_SYM_L3_HDR_ISIP_IGNORE = 0,
	ULP_SR_SYM_L3_HDR_ISIP_NO = 0,
	ULP_SR_SYM_L3_HDR_ISIP_YES = 1,
	ULP_SR_SYM_L3_IPV6_CMP_SRC_IGNORE = 0,
	ULP_SR_SYM_L3_IPV6_CMP_SRC_NO = 0,
	ULP_SR_SYM_L3_IPV6_CMP_SRC_YES = 1,
	ULP_SR_SYM_L3_IPV6_CMP_DST_IGNORE = 0,
	ULP_SR_SYM_L3_IPV6_CMP_DST_NO = 0,
	ULP_SR_SYM_L3_IPV6_CMP_DST_YES = 1,
	ULP_SR_SYM_L4_HDR_VALID_IGNORE = 0,
	ULP_SR_SYM_L4_HDR_VALID_NO = 0,
	ULP_SR_SYM_L4_HDR_VALID_YES = 1,
	ULP_SR_SYM_L4_HDR_ERROR_IGNORE = 0,
	ULP_SR_SYM_L4_HDR_ERROR_NO = 0,
	ULP_SR_SYM_L4_HDR_ERROR_YES = 1,
	ULP_SR_SYM_L4_HDR_TYPE_IGNORE = 0,
	ULP_SR_SYM_L4_HDR_TYPE_TCP = 0,
	ULP_SR_SYM_L4_HDR_TYPE_UDP = 1,
	ULP_SR_SYM_L4_HDR_TYPE_ICMP = 2,
	ULP_SR_SYM_L4_HDR_TYPE_UPAR1 = 3,
	ULP_SR_SYM_L4_HDR_TYPE_UPAR2 = 4,
	ULP_SR_SYM_L4_HDR_TYPE_BTH_V1 = 5,
	ULP_SR_SYM_L4_HDR_IS_UDP_TCP_IGNORE = 0,
	ULP_SR_SYM_L4_HDR_IS_UDP_TCP_NO = 0,
	ULP_SR_SYM_L4_HDR_IS_UDP_TCP_YES = 1,
	ULP_SR_SYM_POP_VLAN_NO = 0,
	ULP_SR_SYM_POP_VLAN_YES = 1,
	ULP_SR_SYM_DECAP_FUNC_NONE = 0,
	ULP_SR_SYM_DECAP_FUNC_THRU_TL2 = 3,
	ULP_SR_SYM_DECAP_FUNC_THRU_TL3 = 8,
	ULP_SR_SYM_DECAP_FUNC_THRU_TL4 = 9,
	ULP_SR_SYM_DECAP_FUNC_THRU_TUN = 10,
	ULP_SR_SYM_DECAP_FUNC_THRU_L2 = 11,
	ULP_SR_SYM_DECAP_FUNC_THRU_L3 = 12,
	ULP_SR_SYM_DECAP_FUNC_THRU_L4 = 13,
	ULP_SR_SYM_ECV_VALID_NO = 0,
	ULP_SR_SYM_ECV_VALID_YES = 1,
	ULP_SR_SYM_ECV_CUSTOM_EN_NO = 0,
	ULP_SR_SYM_ECV_CUSTOM_EN_YES = 1,
	ULP_SR_SYM_ECV_L2_EN_NO = 0,
	ULP_SR_SYM_ECV_L2_EN_YES = 1,
	ULP_SR_SYM_ECV_VTAG_TYPE_NOP = 0,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI = 1,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_1_IVLAN_PRI = 2,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_1_REMAP_DIFFSERV = 3,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_2_ENCAP_PRI = 4,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_2_REMAP_DIFFSERV = 5,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_ENCAP_PRI = 6,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_REMAP_DIFFSERV = 7,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_0 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_1 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_2 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_3 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_4 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_5 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_6 = 8,
	ULP_SR_SYM_ECV_VTAG_TYPE_ADD_0_PRI_7 = 8,
	ULP_SR_SYM_ECV_L3_TYPE_NONE = 0,
	ULP_SR_SYM_ECV_L3_TYPE_IPV4 = 4,
	ULP_SR_SYM_ECV_L3_TYPE_IPV6 = 5,
	ULP_SR_SYM_ECV_L3_TYPE_MPLS_8847 = 6,
	ULP_SR_SYM_ECV_L3_TYPE_MPLS_8848 = 7,
	ULP_SR_SYM_ECV_L4_TYPE_NONE = 0,
	ULP_SR_SYM_ECV_L4_TYPE_UDP = 4,
	ULP_SR_SYM_ECV_L4_TYPE_UDP_CSUM = 5,
	ULP_SR_SYM_ECV_L4_TYPE_UDP_ENTROPY = 6,
	ULP_SR_SYM_ECV_L4_TYPE_UDP_ENTROPY_CSUM = 7,
	ULP_SR_SYM_ECV_TUN_TYPE_NONE = 0,
	ULP_SR_SYM_ECV_TUN_TYPE_GENERIC = 1,
	ULP_SR_SYM_ECV_TUN_TYPE_VXLAN = 2,
	ULP_SR_SYM_ECV_TUN_TYPE_NGE = 3,
	ULP_SR_SYM_ECV_TUN_TYPE_NVGRE = 4,
	ULP_SR_SYM_ECV_TUN_TYPE_GRE = 5,
	ULP_SR_SYM_EEM_ACT_REC_INT = 0,
	ULP_SR_SYM_EEM_EXT_FLOW_CNTR = 1,
	ULP_SR_SYM_UC_ACT_REC = 0,
	ULP_SR_SYM_MC_ACT_REC = 1,
	ULP_SR_SYM_ACT_REC_DROP_YES = 1,
	ULP_SR_SYM_ACT_REC_DROP_NO = 0,
	ULP_SR_SYM_ACT_REC_POP_VLAN_YES = 1,
	ULP_SR_SYM_ACT_REC_POP_VLAN_NO = 0,
	ULP_SR_SYM_ACT_REC_METER_EN_YES = 1,
	ULP_SR_SYM_ACT_REC_METER_EN_NO = 0,
	ULP_SR_SYM_LOOPBACK_PORT = 16,
	ULP_SR_SYM_LOOPBACK_PARIF = 15,
	ULP_SR_SYM_EXT_EM_MAX_KEY_SIZE = 448,
	ULP_SR_SYM_MATCH_TYPE_EM = 0,
	ULP_SR_SYM_MATCH_TYPE_WM = 1,
	ULP_SR_SYM_IP_PROTO_ICMP = 1,
	ULP_SR_SYM_IP_PROTO_IGMP = 2,
	ULP_SR_SYM_IP_PROTO_IP_IN_IP = 4,
	ULP_SR_SYM_IP_PROTO_TCP = 6,
	ULP_SR_SYM_IP_PROTO_UDP = 17,
	ULP_SR_SYM_VF_FUNC_PARIF = 15,
	ULP_SR_SYM_NO = 0,
	ULP_SR_SYM_YES = 1,
	ULP_SR_SYM_RECYCLE_DST = 0x800
};

enum bnxt_ulp_class_hid {
	BNXT_ULP_CLASS_HID_005c = 0x005c,
	BNXT_ULP_CLASS_HID_0003 = 0x0003,
	BNXT_ULP_CLASS_HID_0132 = 0x0132,
	BNXT_ULP_CLASS_HID_00e1 = 0x00e1,
	BNXT_ULP_CLASS_HID_0044 = 0x0044,
	BNXT_ULP_CLASS_HID_001b = 0x001b,
	BNXT_ULP_CLASS_HID_012a = 0x012a,
	BNXT_ULP_CLASS_HID_00f9 = 0x00f9,
	BNXT_ULP_CLASS_HID_018d = 0x018d,
	BNXT_ULP_CLASS_HID_00a7 = 0x00a7,
	BNXT_ULP_CLASS_HID_006f = 0x006f,
	BNXT_ULP_CLASS_HID_0181 = 0x0181,
	BNXT_ULP_CLASS_HID_0195 = 0x0195,
	BNXT_ULP_CLASS_HID_00bf = 0x00bf,
	BNXT_ULP_CLASS_HID_0077 = 0x0077,
	BNXT_ULP_CLASS_HID_0199 = 0x0199,
	BNXT_ULP_CLASS_HID_009a = 0x009a,
	BNXT_ULP_CLASS_HID_0192 = 0x0192,
	BNXT_ULP_CLASS_HID_01e2 = 0x01e2,
	BNXT_ULP_CLASS_HID_00fa = 0x00fa,
	BNXT_ULP_CLASS_HID_0165 = 0x0165,
	BNXT_ULP_CLASS_HID_0042 = 0x0042,
	BNXT_ULP_CLASS_HID_00cd = 0x00cd,
	BNXT_ULP_CLASS_HID_01aa = 0x01aa,
	BNXT_ULP_CLASS_HID_0178 = 0x0178,
	BNXT_ULP_CLASS_HID_0070 = 0x0070,
	BNXT_ULP_CLASS_HID_00f3 = 0x00f3,
	BNXT_ULP_CLASS_HID_01d8 = 0x01d8,
	BNXT_ULP_CLASS_HID_005b = 0x005b,
	BNXT_ULP_CLASS_HID_0153 = 0x0153,
	BNXT_ULP_CLASS_HID_01a3 = 0x01a3,
	BNXT_ULP_CLASS_HID_00bb = 0x00bb,
	BNXT_ULP_CLASS_HID_0082 = 0x0082,
	BNXT_ULP_CLASS_HID_018a = 0x018a,
	BNXT_ULP_CLASS_HID_01fa = 0x01fa,
	BNXT_ULP_CLASS_HID_00e2 = 0x00e2,
	BNXT_ULP_CLASS_HID_017d = 0x017d,
	BNXT_ULP_CLASS_HID_005a = 0x005a,
	BNXT_ULP_CLASS_HID_00d5 = 0x00d5,
	BNXT_ULP_CLASS_HID_01b2 = 0x01b2,
	BNXT_ULP_CLASS_HID_0160 = 0x0160,
	BNXT_ULP_CLASS_HID_0068 = 0x0068,
	BNXT_ULP_CLASS_HID_00eb = 0x00eb,
	BNXT_ULP_CLASS_HID_01c0 = 0x01c0,
	BNXT_ULP_CLASS_HID_0043 = 0x0043,
	BNXT_ULP_CLASS_HID_014b = 0x014b,
	BNXT_ULP_CLASS_HID_01bb = 0x01bb,
	BNXT_ULP_CLASS_HID_00a3 = 0x00a3,
	BNXT_ULP_CLASS_HID_00cb = 0x00cb,
	BNXT_ULP_CLASS_HID_00b4 = 0x00b4,
	BNXT_ULP_CLASS_HID_0013 = 0x0013,
	BNXT_ULP_CLASS_HID_001c = 0x001c,
	BNXT_ULP_CLASS_HID_017b = 0x017b,
	BNXT_ULP_CLASS_HID_0164 = 0x0164,
	BNXT_ULP_CLASS_HID_00c3 = 0x00c3,
	BNXT_ULP_CLASS_HID_00cc = 0x00cc,
	BNXT_ULP_CLASS_HID_01a5 = 0x01a5,
	BNXT_ULP_CLASS_HID_0196 = 0x0196,
	BNXT_ULP_CLASS_HID_010d = 0x010d,
	BNXT_ULP_CLASS_HID_00fe = 0x00fe,
	BNXT_ULP_CLASS_HID_0084 = 0x0084,
	BNXT_ULP_CLASS_HID_0046 = 0x0046,
	BNXT_ULP_CLASS_HID_01ec = 0x01ec,
	BNXT_ULP_CLASS_HID_01ae = 0x01ae,
	BNXT_ULP_CLASS_HID_00d3 = 0x00d3,
	BNXT_ULP_CLASS_HID_00ac = 0x00ac,
	BNXT_ULP_CLASS_HID_000b = 0x000b,
	BNXT_ULP_CLASS_HID_0004 = 0x0004,
	BNXT_ULP_CLASS_HID_0163 = 0x0163,
	BNXT_ULP_CLASS_HID_017c = 0x017c,
	BNXT_ULP_CLASS_HID_00db = 0x00db,
	BNXT_ULP_CLASS_HID_00d4 = 0x00d4,
	BNXT_ULP_CLASS_HID_01bd = 0x01bd,
	BNXT_ULP_CLASS_HID_018e = 0x018e,
	BNXT_ULP_CLASS_HID_0115 = 0x0115,
	BNXT_ULP_CLASS_HID_00e6 = 0x00e6,
	BNXT_ULP_CLASS_HID_009c = 0x009c,
	BNXT_ULP_CLASS_HID_005e = 0x005e,
	BNXT_ULP_CLASS_HID_01f4 = 0x01f4,
	BNXT_ULP_CLASS_HID_01b6 = 0x01b6
};

enum bnxt_ulp_act_hid {
	BNXT_ULP_ACT_HID_0000 = 0x0000,
	BNXT_ULP_ACT_HID_0001 = 0x0001,
	BNXT_ULP_ACT_HID_0400 = 0x0400,
	BNXT_ULP_ACT_HID_0325 = 0x0325,
	BNXT_ULP_ACT_HID_0010 = 0x0010,
	BNXT_ULP_ACT_HID_0725 = 0x0725,
	BNXT_ULP_ACT_HID_0335 = 0x0335,
	BNXT_ULP_ACT_HID_0002 = 0x0002,
	BNXT_ULP_ACT_HID_0003 = 0x0003,
	BNXT_ULP_ACT_HID_0402 = 0x0402,
	BNXT_ULP_ACT_HID_0327 = 0x0327,
	BNXT_ULP_ACT_HID_0012 = 0x0012,
	BNXT_ULP_ACT_HID_0727 = 0x0727,
	BNXT_ULP_ACT_HID_0337 = 0x0337,
	BNXT_ULP_ACT_HID_01de = 0x01de,
	BNXT_ULP_ACT_HID_00c6 = 0x00c6,
	BNXT_ULP_ACT_HID_0506 = 0x0506,
	BNXT_ULP_ACT_HID_01ed = 0x01ed,
	BNXT_ULP_ACT_HID_03ef = 0x03ef,
	BNXT_ULP_ACT_HID_0516 = 0x0516,
	BNXT_ULP_ACT_HID_01df = 0x01df,
	BNXT_ULP_ACT_HID_01e4 = 0x01e4,
	BNXT_ULP_ACT_HID_00cc = 0x00cc,
	BNXT_ULP_ACT_HID_0504 = 0x0504,
	BNXT_ULP_ACT_HID_01ef = 0x01ef,
	BNXT_ULP_ACT_HID_03ed = 0x03ed,
	BNXT_ULP_ACT_HID_0514 = 0x0514,
	BNXT_ULP_ACT_HID_00db = 0x00db,
	BNXT_ULP_ACT_HID_00df = 0x00df
};

enum bnxt_ulp_df_tpl {
	BNXT_ULP_DF_TPL_PORT_TO_VS = 2,
	BNXT_ULP_DF_TPL_VS_TO_PORT = 3,
	BNXT_ULP_DF_TPL_VFREP_TO_VF = 4,
	BNXT_ULP_DF_TPL_VF_TO_VFREP = 5,
	BNXT_ULP_DF_TPL_LOOPBACK_ACTION_REC = 6
};

#endif
