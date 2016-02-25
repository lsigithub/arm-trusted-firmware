/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXXIA_PWRC_H_
#define AXXIA_PWRC_H_

#define FALSE	(0)
#define TRUE	(!FALSE)
#define bool unsigned char

#define     SYSCON_GIC_DISABLE                              (0x0000001c)

#define     SYSCON_RESET_STATUS                             (0x00000100)
#define     SYSCON_RESET_CORE_STATUS                        (0x00000108)

#define     SYSCON_KEY                                      (0x00002000)
#define     SYSCON_RESET_CTL                                (0x00002008)
#define     SYSCON_RESET_CPU                                (0x0000200c)
#define     SYSCON_HOLD_CPU                                 (0x00002010)
#define     SYSCON_PWRUP_CPU_RST                            (0x00002014)
#define     SYSCON_HOLD_L2                                  (0x00002018)
#define     SYSCON_HOLD_DBG                                 (0x0000201c)

#define     SYSCON_RESET_AXIS                               (0x000020a0)
#define     SYSCON_RESET_AXIS_ACCESS_SIZE                   (0x00000004)

#define     SYSCON_PWR_CLKEN                                (0x00002400)
#define     SYSCON_PWR_CSYSREQ_TS                           (0x00002420)
#define     SYSCON_PWR_CSYSREQ_CNT                          (0x0000241c)
#define     SYSCON_PWR_CSYSREQ_ATB                          (0x00002418)
#define     SYSCON_PWR_CSYSREQ_APB                          (0x00002410)

#define     SYSCON_PWR_STANDBYWFIL2                         (0x00002504)
#define     SYSCON_PWR_CSYSACK_TS                           (0x000024c8)
#define     SYSCON_PWR_CACTIVE_TS                           (0x00002490)
#define     SYSCON_PWR_CSYSACK_CNT                          (0x000024c4)
#define     SYSCON_PWR_CACTIVE_CNT                          (0x0000248c)
#define     SYSCON_PWR_CSYSACK_ATB                          (0x000024c0)
#define     SYSCON_PWR_CACTIVE_ATB                          (0x00002488)
#define     SYSCON_PWR_CSYSACK_APB                          (0x000024b8)
#define     SYSCON_PWR_CACTIVE_APB                          (0x00002480)

#define     SYSCON_PWR_PWRUPCPURAM                          (0x00002488)
#define     SYSCON_PWR_ISOLATECPU                           (0x00002428)

#define		RAM_BANK0_MASK			(0x0FFF0000)
#define		RAM_BANK1_LS_MASK		(0xF0000000)
#define		RAM_BANK1_MS_MASK		(0x000000FF)
#define		RAM_BANK2_MASK			(0x000FFF00)
#define		RAM_BANK3_MASK			(0xFFF00000)
#define		RAM_ALL_MASK			(0xFFFFFFFF)

/* DICKENS REGISTERS (Miscelaneous Node) */
#define		DKN_MN_NODE_ID				(0x0)
#define		DKN_DVM_DOMAIN_OFFSET		(0x0)
#define		DKN_MN_DVM_DOMAIN_CTL		(0x200)
#define		DKN_MN_DVM_DOMAIN_CTL_SET	(0x210)
#define		DKN_MN_DVM_DOMAIN_CTL_CLR	(0x220)

/* DICKENS HN-F (Fully-coherent Home Node) */
#define		DKN_HNF_NODE_ID					(0x20)
#define		DKN_HNF_TOTAL_NODES				(0x8)
#define		DKN_HNF_SNOOP_DOMAIN_CTL		(0x200)
#define		DKN_HNF_SNOOP_DOMAIN_CTL_SET	(0x210)
#define		DKN_HNF_SNOOP_DOMAIN_CTL_CLR	(0x220)

/* DICKENS clustid to Node */
#define		DKN_CLUSTER0_NODE		(1)
#define		DKN_CLUSTER1_NODE		(9)
#define		DKN_CLUSTER2_NODE		(11)
#define		DKN_CLUSTER3_NODE		(19)

/* PO RESET cluster id to bit */
#define		PORESET_CLUSTER0		(0x10000)
#define		PORESET_CLUSTER1		(0x20000)
#define		PORESET_CLUSTER2		(0x40000)
#define		PORESET_CLUSTER3		(0x80000)

/* IPI Masks */
#define		IPI0_MASK				(0x1111)
#define		IPI1_MASK				(0x2222)
#define		IPI2_MASK				(0x4444)
#define		IPI3_MASK				(0x8888)

/* SYSCON KEY Value */
#define VALID_KEY_VALUE			(0xAB)
#define MAX_IPI				(19)

int axxia_pwrc_cpu_shutdown(unsigned int cpu);
int axxia_pwrc_cpu_powerup(unsigned int cpu);
void axxia_pwrc_debug_read_pwr_registers(void);
void axxia_pwrc_dump_L2_registers(void);
unsigned int axxia_pwrc_get_powered_down_cpu(void);
bool axxia_pwrc_cpu_last_of_cluster(unsigned int cpu);
void axxia_pwrc_dump_dickens(void);
void axxia_pwrc_init_cpu(unsigned int cpu);
void axxia_pwrc_cpu_logical_powerup(void);
void axxia_pwrc_cluster_logical_powerup(void);
bool axxia_pwrc_cpu_active(unsigned int cpu);
void axxia_pwrc_init_syscon(void);
extern bool axxia_pwrc_in_progress[];
extern bool cluster_power_up[];
extern unsigned int axxia_pwrc_cpu_powered_down;
extern uint64_t axxia_sec_entry_point;

#endif /* AXXIA_PWRC_H_ */
