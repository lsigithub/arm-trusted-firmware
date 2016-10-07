/*
 * Copyright (c) 2013, ARM Limited and Contributors. All rights reserved.
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


#include <assert.h>
#include <arch_helpers.h>
#include <debug.h>
#include <platform.h>
#include <platform_def.h>
#include <psci.h>
#include <debug.h>
#include <mmio.h>
#include <delay_timer.h>
#include <axxia_def.h>
#include <axxia_private.h>
#include <axxia_pwrc.h>

#undef DEBUG_CPU_PM

#define PM_WAIT_TIME (10000)
#define IPI_IRQ_MASK (0xFFFF)

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

bool axxia_pwrc_in_progress[PLATFORM_CORE_COUNT];
bool cluster_power_up[PLATFORM_CLUSTER_COUNT];

static const unsigned int cluster_to_node[PLATFORM_CLUSTER_COUNT] = { DKN_CLUSTER0_NODE,
DKN_CLUSTER1_NODE,
DKN_CLUSTER2_NODE,
DKN_CLUSTER3_NODE };

static const unsigned int cluster_to_poreset[PLATFORM_CLUSTER_COUNT] = {
PORESET_CLUSTER0,
PORESET_CLUSTER1,
PORESET_CLUSTER2,
PORESET_CLUSTER3 };

enum axxia_pwrc_error_code {
	PM_ERR_DICKENS_IOREMAP = 200,
	PM_ERR_DICKENS_SNOOP_DOMAIN,
	PM_ERR_FAILED_PWR_DWN_RAM,
	PM_ERR_FAILED_STAGE_1,
	PM_ERR_ACK1_FAIL,
	PM_ERR_RAM_ACK_FAIL,
	PM_ERR_FAIL_L2ACK,
	/*PM_ERR_FAIL_L2HSRAM*/
};

unsigned int axxia_pwrc_cpu_powered_down;


/*======================= LOCAL FUNCTIONS ==============================*/
static void axxia_pwrc_set_bits_syscon_register(unsigned int reg, unsigned int data);
static void axxia_pwrc_or_bits_syscon_register(unsigned int reg, unsigned int data);
static void axxia_pwrc_clear_bits_syscon_register(unsigned int reg, unsigned int data);
static unsigned int axxia_pwrc_test_for_bit_with_timeout(unsigned int reg, unsigned int bit);
static unsigned int axxia_pwrc_wait_for_bit_clear_with_timeout(unsigned int reg,
		unsigned int bit);
static int axxia_pwrc_cpu_physical_isolation_and_power_down(int cpu);
static int axxia_pwrc_cpu_physical_connection_and_power_up(int cpu);
static int axxia_pwrc_L2_physical_connection_and_power_up(unsigned int cluster);
static void axxia_pwrc_l3_logical_shutdown(unsigned int cluster);
static void axxia_pwrc_l3_logical_powerup(unsigned int cluster);

static void axxia_pwrc_disable_cache(void);

#ifdef DEBUG_CPU_PM
static void l2_debug(void);
#endif

static bool axxia_pwrc_first_cpu_of_cluster(unsigned int cpu)
{
#ifdef L2_POWER
	unsigned int count = 0;

	switch (cpu) {
	case (0):
	case (1):
	case (2):
	case (3):
		/* This will never happen because cpu 0 will never be turned off */
		break;
	case (4):
	case (5):
	case (6):
	case (7):
		if (axxia_pwrc_cpu_powered_down & (1 << 4))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 5))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 6))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 7))
			count++;
		if (count == 4)
			return TRUE;
		break;
	case (8):
	case (9):
	case (10):
	case (11):
		if (axxia_pwrc_cpu_powered_down & (1 << 8))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 9))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 10))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 11))
			count++;
		if (count == 4)
			return TRUE;
		break;
	case (12):
	case (13):
	case (14):
	case (15):
		if (axxia_pwrc_cpu_powered_down & (1 << 12))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 13))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 14))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 15))
			count++;
		if (count == 4)
			return TRUE;
		break;
	default:
		ERROR("ERROR: the cpu does not exist: %d - %s:%d\n", cpu, __FILE__,
				__LINE__);
		break;
	}
#endif
	return FALSE;
}

bool axxia_pwrc_cpu_last_of_cluster(unsigned int cpu)
{
#ifdef L2_POWER
	unsigned int count = 0;

	switch (cpu) {
	case (0):
	case (1):
	case (2):
	case (3):
		/* This will never happen because cpu 0 will never be turned off */
		break;
	case (4):
	case (5):
	case (6):
	case (7):
		if (axxia_pwrc_cpu_powered_down & (1 << 4))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 5))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 6))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 7))
			count++;
		if (count == 3)
			return TRUE;
		break;
	case (8):
	case (9):
	case (10):
	case (11):
		if (axxia_pwrc_cpu_powered_down & (1 << 8))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 9))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 10))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 11))
			count++;
		if (count == 3)
			return TRUE;
		break;
	case (12):
	case (13):
	case (14):
	case (15):
		if (axxia_pwrc_cpu_powered_down & (1 << 12))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 13))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 14))
			count++;
		if (axxia_pwrc_cpu_powered_down & (1 << 15))
			count++;
		if (count == 3)
			return TRUE;
		break;
	default:
		ERROR("ERROR: the cpu does not exist: %d - %s:%d\n", cpu,  __FILE__,
				__LINE__);
		break;
	}
#endif
	return FALSE;
}

static void axxia_pwrc_set_bits_syscon_register(unsigned int reg, unsigned int data)
{
	mmio_write_32((SYSCON_BASE + reg), data);
}

static void axxia_pwrc_or_bits_syscon_register(unsigned int reg, unsigned int data)
{
	unsigned int tmp;

	tmp = mmio_read_32(SYSCON_BASE + reg);
	tmp |= data;
	mmio_write_32((SYSCON_BASE + reg), tmp);
}


static void axxia_pwrc_clear_bits_syscon_register(unsigned int reg, unsigned int data)
{
	unsigned int tmp;

	tmp = mmio_read_32(SYSCON_BASE + reg);
	tmp &= ~(data);
	mmio_write_32((SYSCON_BASE + reg), tmp);
}

static unsigned int axxia_pwrc_test_for_bit_with_timeout(unsigned int reg, unsigned int bit)
{

	unsigned int tmp = 0;
	long long cnt = 0;

	while (cnt < PM_WAIT_TIME) {
		tmp = mmio_read_32(SYSCON_BASE + reg);
		if (CHECK_BIT(tmp, bit))
			break;
		cnt++;
	}
	if (cnt == PM_WAIT_TIME) {
		ERROR("reg=0x%x tmp:=0x%x\n", reg, tmp);
		return PSCI_E_INTERN_FAIL;
	}
	return PSCI_E_SUCCESS;
}

static unsigned int axxia_pwrc_wait_for_bit_clear_with_timeout(unsigned int reg, unsigned int bit)
{
	long long cnt = 0;
	unsigned int tmp = 0;

	while (cnt < PM_WAIT_TIME) {
		tmp = mmio_read_32(SYSCON_BASE + reg);
		if (!(CHECK_BIT(tmp, bit)))
			break;
		cnt++;
	}
	if (cnt == PM_WAIT_TIME) {
		ERROR("reg=0x%x tmp:=0x%x\n", reg, tmp);
		return PSCI_E_INTERN_FAIL;
	}

	return PSCI_E_SUCCESS;
}

static void axxia_pwrc_l3_logical_shutdown(unsigned int cluster)
{
	if (0 != set_cluster_coherency(cluster, 0))
		tf_printf("Removing cluster %u to the coherency domain failed!\n", cluster);
}

static void axxia_pwrc_l3_logical_powerup(unsigned int cluster)
{
	if (0 != set_cluster_coherency(cluster, 1))
		tf_printf("Adding cluster %u to the coherency domain failed!\n", cluster);
}

int axxia_pwrc_cpu_shutdown(unsigned int reqcpu)
{

	unsigned int cluster = reqcpu / PLATFORM_MAX_CPUS_PER_CLUSTER;
	unsigned int cluster_mask = (0x01 << cluster);
	bool last_cpu;
	int rval = PSCI_E_SUCCESS;

	/* Check to see if the cpu is powered up */
	if (axxia_pwrc_cpu_powered_down & (1 << reqcpu)) {
		ERROR("CPU %u is already powered off - %s:%d\n", reqcpu, __FILE__, __LINE__);
		return PSCI_E_INTERN_FAIL;
	}

	/*
	 * Is this the last cpu of a cluster then turn off the L2 cache
	 * along with the CPU.
	 */
	last_cpu = axxia_pwrc_cpu_last_of_cluster(reqcpu);
	if (last_cpu) {

		/* Shut down the ACP interface is a step in power down however the AXXIA has not ACP so it is skipped*/
		/* Signal that the ACP interface is idle */
		axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_AINACTS, cluster_mask);

		/* Disable and invalidate the L1 data cache */
		axxia_pwrc_disable_cache();

		/* CLear and invalidate the the L2 cache */
		plat_flush_dcache_l2();

		/* Remove the cluster from the CCN-504 coherency domain to ensure there will be no snoop requests */
		axxia_pwrc_l3_logical_shutdown(cluster);

		/* Power off the L2 */

		/* Enable self power down of last CPU */
		axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_ENABLE_SELF_PWRDN, (1 << reqcpu));

		/* Enable power down cpu */
		axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_DOWN_CPU, (1 << reqcpu));

		/* Arm the hardware cluster power down sequence. */
		axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_DOWN_CLUSTER, cluster_mask);

		/* Signal that the cluster's logic that no snoop request will be directed to the cluster */
		axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_SINACT, cluster_mask);

		INFO("CPU %u is powered down with cluster: %u\n", reqcpu, cluster);
		axxia_pwrc_cpu_powered_down |= (1 << reqcpu);

	} else {

		axxia_pwrc_disable_cache();

		rval = axxia_pwrc_cpu_physical_isolation_and_power_down(reqcpu);
		if (rval == PSCI_E_SUCCESS)
			axxia_pwrc_cpu_powered_down |= (1 << reqcpu);
		else
			ERROR("CPU %u failed to power down\n", reqcpu);
	}

	return rval;
}

int axxia_pwrc_cpu_powerup(unsigned int reqcpu)
{

	bool first_cpu = FALSE;
	int rval = PSCI_E_SUCCESS;
	unsigned int cpu_mask = (0x01 << reqcpu);

	unsigned int cluster = reqcpu / PLATFORM_MAX_CPUS_PER_CLUSTER;

	/* Put the CPU into reset */
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWRUP_CPU_RST, cpu_mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);

	/* Toggle the CPU hold (not in the documentation but needed to work */
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_or_bits_syscon_register(SYSCON_HOLD_CPU, cpu_mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);

	/*
	 * Power up the CPU
	 */
	/* Set up reset vector for cpu */
	mmio_write_32(0x8031000000,
			(0x14000000 |
					(axxia_sec_entry_point - 0x8031000000) / 4));
	isb();
	dsb();

	rval = axxia_pwrc_cpu_physical_connection_and_power_up(reqcpu);
	if (rval) {
		ERROR("Failed to power up physical connection of cpu: %u\n", reqcpu);
		goto axxia_pwrc_power_up;
	}

	/*
	 * Is this the first cpu of a cluster to come back on?
	 * Then power up the L2 cache.
	 */
	first_cpu = axxia_pwrc_first_cpu_of_cluster(reqcpu);
	if (first_cpu) {

		rval = axxia_pwrc_L2_physical_connection_and_power_up(cluster);
		if (rval) {
			ERROR("CPU: Failed the logical L2 power up\n");
			goto axxia_pwrc_power_up;
		} else {
			INFO("CPU %u is powered up with cluster: %u\n", reqcpu, cluster);
		}

		cluster_power_up[cluster] = TRUE;
	}

	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_clear_bits_syscon_register(SYSCON_HOLD_CPU, cpu_mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);

	/* Take the CPU out of reset and let it go. */
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWRUP_CPU_RST,	cpu_mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);

	/*
	 * Clear the powered down mask
	 */
	axxia_pwrc_cpu_powered_down &= ~(1 << reqcpu);

axxia_pwrc_power_up:

	return rval;
}

unsigned int axxia_pwrc_get_powered_down_cpu(void)
{
	return axxia_pwrc_cpu_powered_down;
}

inline void axxia_pwrc_disable_data_cache_hyp_mode(void)
{
	unsigned int v;

	__asm__ volatile(
	"	mrs	%0, SCTLR_EL2\n"
	"	bic	%0, %0, %1\n"
	"	msr	SCTLR_EL2, %0\n"
	: "=&r" (v)
	: "Ir" (1 << 2)
	: "cc");

	isb();
	dsb();
}

inline void axxia_pwrc_disable_data_cache(void)
{
	unsigned int v;

	__asm__ volatile(
	"	mrs	%0, SCTLR_EL3\n"
	"	bic	%0, %0, %1\n"
	"	msr	SCTLR_EL3, %0\n"
	: "=&r" (v)
	: "Ir" (1 << 2)
	: "cc");

	isb();
	dsb();
}

inline void axxia_pwrc_enable_data_cache_hyp_mode(void)
{
	unsigned int v;

	__asm__ volatile(
	"	mrs	%0, SCTLR_EL2\n"
	"	orr	%0, %0, %1\n"
	"	msr	SCTLR_EL2, %0\n"
	: "=&r" (v)
	: "Ir" (1 << 2)
	: "cc");

	isb();
	dsb();
}

inline void axxia_pwrc_enable_data_cache(void)
{
	unsigned int v;

	__asm__ volatile(
	"	mrs	%0, SCTLR_EL3\n"
	"	orr	%0, %0, %1\n"
	"	msr	SCTLR_EL3, %0\n"
	: "=&r" (v)
	: "Ir" (1 << 2)
	: "cc");

	isb();
	dsb();
}

inline void axxia_pwrc_disable_data_coherency(void)
{
	unsigned int v;

	__asm__ volatile(
	"	mrs	%0, S3_1_c15_c2_1\n"
	"	bic	%0, %0, %1\n"
	"	msr	S3_1_c15_c2_1, %0\n"
	: "=&r" (v)
	: "Ir" (1 << 6)
	: "cc");

	isb();
	dsb();
}

inline void axxia_pwrc_enable_data_coherency(void)
{
	unsigned int v;

	__asm__ volatile(
	"	mrs	%0, S3_1_c15_c2_1\n"
	"	orr	%0, %0, %1\n"
	"	msr	S3_1_c15_c2_1, %0\n"
	: "=&r" (v)
	: "Ir" (1 << 6)
	: "cc");

	isb();
	dsb();
}

inline void axxia_pwrc_disable_l2_prefetch(void)
{
	long long v;

	__asm__ volatile(
	"	mrs	%0, S3_1_c15_c2_1\n"
	"	orr	%0, %0, %1\n"
	"	msr	S3_1_c15_c2_1, %0\n"
	: "=&r" (v)
	: "Ir" (0x4000000000)
	: "cc");

	__asm__ volatile(
	"	mrs	%0, S3_1_c15_c2_1\n"
	"	bic	%0, %0, %1\n"
	"	msr	S3_1_c15_c2_1, %0\n"
	: "=&r" (v)
	: "Ir" (0x1B00000000)
	: "cc");

	isb();
	dsb();

}


static void axxia_pwrc_disable_cache(void)
{
	/*
	 * Check for Hypervisor mode and clear the HSCTLR.C bit otherwise clear the SCTLR.C bit
	 * The prevents more data cache allocations and causes cacheable memory attributes to change to
	 * Normal Non-cacheable. Subsequent loads and stores will not access the L1 or L2 cachees.
	 */
	if (IS_IN_EL(2))
		axxia_pwrc_disable_data_cache_hyp_mode();
	else
		axxia_pwrc_disable_data_cache();

	/* Disable L2 pre-fetches */
	axxia_pwrc_disable_l2_prefetch();

	/*
	 * Clean and invalidate all data from the L1 Data cache. At completion, the L2 duplicate snoop
	 * tag RAMfor the CPU core is empty. This prevents any new data cache snoops or data cache
	 * maintenance operations from other cores in the cluster being issued to s new CPU core.
	 */
	plat_flush_dcache_l1();

	/*
	 * Disable data coherency with other cores in the cluster by clearing the CPUECTLR.SMPEN bit. Clearing the
	 * SMPEN bit removes the CPU core from the cluster coherency domain and prevents it from receiving cache or TLB
	 * maintenence operations broadcast by other cores in the cluster.
	 */
	axxia_pwrc_disable_data_coherency();

}

static int axxia_pwrc_cpu_physical_isolation_and_power_down(int cpu)
{

	int rval = PSCI_E_SUCCESS;

	/*bool failure; */
	unsigned int mask = (0x01 << cpu);

	/* Enable self powerdown */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_ENABLE_SELF_PWRDN, mask);

	/* Arm the the CPU core hardware power down state machine */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_DOWN_CPU, mask);

	isb();
	dsb();

	return rval;
}

static int axxia_pwrc_cpu_physical_connection_and_power_up(int cpu)
{
	int rval = PSCI_E_SUCCESS;

	bool failure;
	unsigned int mask = (0x01 << cpu);

	/* Initiate power up of the CPU */

	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_DOWN_CPU, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_DOWN_CPU, cpu);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to reset power down cpu\n");
		goto power_up_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PWRUPCPU, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_PWRUPCPU, cpu);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to add physical power to CPU\n");
		goto power_up_cleanup;
	}

	/* De-isolate the CPU */
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_ISOLATECPU, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_ISOLATECPU, cpu);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to de-isolate CPU\n");
		goto power_up_cleanup;
	}

power_up_cleanup:

	return rval;

}
/*========================================== L2 FUNCTIONS ========================================*/

static int axxia_pwrc_L2_physical_connection_and_power_up(unsigned int cluster)
{

	unsigned int mask = (0x1 << cluster);
	int rval = PSCI_E_SUCCESS;
	unsigned int failure;

	/* Make sure that all cpus are in reset */


	/* Ensure the L2 is held in reset */
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_or_bits_syscon_register(SYSCON_HOLD_L2, mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_HOLD_L2, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: L2 not in reset\n");
		goto power_up_l2_cleanup;
	}

	/* Keep the ADB interfaces logically powered off */
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_CSYSREQ_APB, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_CSYSACK_APB, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear CSYSREQ_APB\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_CSYSREQ_ATB, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_CSYSACK_ATB, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear CSYSREQ_ATB\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_CSYSREQ_CNT, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_CSYSACK_CNT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear CSYSREQ_CNT\n");
		goto power_up_l2_cleanup;
	}
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_CSYSREQ_TS, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_CSYSACK_TS, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear CSYSREQ_TS\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_PREQ, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_PACCEPT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear PREQ\n");
		goto power_up_l2_cleanup;
	}

	/* Keep the ACP interfaces logically powered off */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PWRDNREQ_ACP, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_PWRDNACK_ACP, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set PWRDNREQ_ACP\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PWRDNREQ_ICCT, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_PWRDNACK_ICCT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set PWRDNREQ_ICCT\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PWRDNREQ_ICDT, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_PWRDNACK_ICDT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set PWRDNREQ_ICDT\n");
		goto power_up_l2_cleanup;
	}

	/* Power on the L2 power domain */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PWRUPL2PLUS, mask);
	udelay(12);

	/* Activate L2 interfaces */
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_ISOLATEL2PLUS, mask);
	udelay(12);

	/* Release the cluster bridges from reset */
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_clear_bits_syscon_register(SYSCON_HOLD_DSSB, mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_HOLD_DSSB, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear SYSCON_HOLD_DSSB\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_clear_bits_syscon_register(SYSCON_HOLD_INFRA, mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_HOLD_INFRA, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear SYSCON_HOLD_INFRA\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_clear_bits_syscon_register(SYSCON_HOLD_STREAM, mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_HOLD_STREAM, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear SYSCON_HOLD_STREAM\n");
		goto power_up_l2_cleanup;
	}

	/* Power up the cluster */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_CSYSREQ_APB, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_CSYSACK_APB, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set CSYSREQ_APB\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_CSYSREQ_ATB, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_CSYSACK_ATB, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set CSYSREQ_ATB\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_CSYSREQ_TS, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_CSYSACK_TS, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set CSYSREQ_TS\n");
		goto power_up_l2_cleanup;
	}

	/* Set the cluster state to power on before requesting the change */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PSTATE, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_PSTATE, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set SYSCON_PWR_PSTATE\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_PREQ, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_PACCEPT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set PREQ\n");
		goto power_up_l2_cleanup;
	}

	/* Keep the ACP interfaces logically power on*/
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_PWRDNREQ_ACP, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_PWRDNACK_ACP, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear PWRDNREQ_ACP\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_PWRDNREQ_ICCT, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_PWRDNACK_ICCT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear PWRDNREQ_ICCT\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_PWRDNREQ_ICDT, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_PWRDNACK_ICDT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear PWRDNREQ_ICDT\n");
		goto power_up_l2_cleanup;
	}

	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_CSYSREQ_CNT, mask);
#if 0
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_CSYSACK_CNT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set CSYSREQ_CNT\n");
		goto power_up_l2_cleanup;
	}
#endif

	/* Prevent inadvertent snoops from corrupting the L2 */
	axxia_pwrc_or_bits_syscon_register(SYSCON_PWR_SINACT, mask);
	failure = axxia_pwrc_test_for_bit_with_timeout(SYSCON_PWR_SINACT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to set SYSCON_PWR_SINACT\n");
		goto power_up_l2_cleanup;
	}

	/* Release the L2 from reset */
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, VALID_KEY_VALUE);
	axxia_pwrc_clear_bits_syscon_register(SYSCON_HOLD_L2, mask);
	axxia_pwrc_set_bits_syscon_register(SYSCON_KEY, 0x00);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_HOLD_L2, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear SYSCON_HOLD_L2\n");
		goto power_up_l2_cleanup;
	}

	/* Start L2 snooping */
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_SINACT, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_SINACT, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear SYSCON_PWR_SINACT\n");
		goto power_up_l2_cleanup;
	}

	/* Release the ACP */
	axxia_pwrc_clear_bits_syscon_register(SYSCON_PWR_AINACTS, mask);
	failure = axxia_pwrc_wait_for_bit_clear_with_timeout(SYSCON_PWR_AINACTS, cluster);
	if (failure) {
		rval = PSCI_E_INTERN_FAIL;
		ERROR("CPU: Failed to clear SYSCON_PWR_AINACTS\n");
		goto power_up_l2_cleanup;
	}

	/* Add the cluster back to the CCN-504 and DVM domain */
	axxia_pwrc_l3_logical_powerup(cluster);

power_up_l2_cleanup:
	return rval;
}

#ifdef DEBUG_CPU_PM

static void l2_debug(void)
{
	unsigned int val;

	printf("L2 DEBUG -------------------------------------\n");

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_AINACTS);
	printf("SYSCON_PWR_AINACTS: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_DBGPWRDUP);
	printf("SYSCON_PWR_DBGPWRDUP: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWRUP_CPU_RST);
	printf("SYSCON_PWRUP_CPU_RST: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_DOWN_CPU);
	printf("SYSCON_PWR_DOWN_CPU: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_ISOLATECPU);
	printf("SYSCON_PWR_ISOLATECPU: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRUPCPU);
	printf("SYSCON_PWR_PWRUPCPU: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_HOLD_L2);
	printf("SYSCON_HOLD_L2: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSREQ_APB);
	printf("SYSCON_PWR_CSYSREQ_APB: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSACK_APB);
	printf("SYSCON_PWR_CSYSACK_APB: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSREQ_ATB);
	printf("SYSCON_PWR_CSYSREQ_ATB: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSACK_ATB);
	printf("SYSCON_PWR_CSYSACK_ATB: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSREQ_CNT);
	printf("SYSCON_PWR_CSYSREQ_CNT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSACK_CNT);
	printf("SYSCON_PWR_CSYSACK_CNT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSREQ_TS);
	printf("SYSCON_PWR_CSYSREQ_TS: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_CSYSACK_TS);
	printf("SYSCON_PWR_CSYSACK_TS: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PREQ);
	printf("SYSCON_PWR_PREQ: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PSTATE);
	printf("SYSCON_PWR_PSTATE: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRDNREQ_ACP);
	printf("SYSCON_PWR_PWRDNREQ_ACP: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRDNACK_ACP);
	printf("SYSCON_PWR_PWRDNACK_ACP: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRDNREQ_ICCT);
	printf("SYSCON_PWR_PWRDNREQ_ICCT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRDNACK_ICCT);
	printf("SYSCON_PWR_PWRDNACK_ICCT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRDNREQ_ICDT);
	printf("SYSCON_PWR_PWRDNREQ_ICDT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRDNACK_ICDT);
	printf("SYSCON_PWR_PWRDNACK_ICDT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_HOLD_DSSB);
	printf("SYSCON_HOLD_DSSB: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_HOLD_INFRA);
	printf("SYSCON_HOLD_INFRA: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_HOLD_STREAM);
	printf("SYSCON_HOLD_STREAM: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_ISOLATEL2PLUS);
	printf("SYSCON_PWR_ISOLATEL2PLUS: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_PWRUPL2PLUS);
	printf("SYSCON_PWR_PWRUPL2PLUS: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_SINACT);
	printf("SYSCON_PWR_SINACT: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_RESET_CTL);
	printf("SYSCON_RESET_CTL: 0x%x\n", val);

	val =	mmio_read_32(SYSCON_BASE + SYSCON_PWR_STANDBYWFI);
	printf("SYSCON_PWR_STANDBYWFI: 0x%x\n", val);

}

#endif
