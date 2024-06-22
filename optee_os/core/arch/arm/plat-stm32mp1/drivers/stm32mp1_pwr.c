// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018-2023, STMicroelectronics
 */

#include <assert.h>
#include <drivers/regulator.h>
#include <drivers/stm32_shared_io.h>
#include <drivers/stm32mp1_pwr.h>
#include <io.h>
#include <kernel/delay.h>
#include <kernel/dt_driver.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <mm/core_memprot.h>
#include <platform_config.h>

#define PWR_CR3_USB33_EN	BIT(24)
#define PWR_CR3_USB33_RDY	BIT(26)
#define PWR_CR3_REG18_EN	BIT(28)
#define PWR_CR3_REG18_RDY	BIT(29)
#define PWR_CR3_REG11_EN	BIT(30)
#define PWR_CR3_REG11_RDY	BIT(31)

#define TIMEOUT_US_10MS		U(10000)

struct pwr_regu_desc {
	unsigned int level_mv;
	uint32_t cr3_enable_mask;
	uint32_t cr3_ready_mask;
};

static const struct pwr_regu_desc pwr_regulators[PWR_REGU_COUNT] = {
	 [PWR_REG11] = {
		 .level_mv = 1100,
		 .cr3_enable_mask = PWR_CR3_REG11_EN,
		 .cr3_ready_mask = PWR_CR3_REG11_RDY,
	 },
	 [PWR_REG18] = {
		 .level_mv = 1800,
		 .cr3_enable_mask = PWR_CR3_REG18_EN,
		 .cr3_ready_mask = PWR_CR3_REG18_RDY,
	 },
	 [PWR_USB33] = {
		 .level_mv = 3300,
		 .cr3_enable_mask = PWR_CR3_USB33_EN,
		 .cr3_ready_mask = PWR_CR3_USB33_RDY,
	 },
};

vaddr_t stm32_pwr_base(void)
{
	static struct io_pa_va base = { .pa = PWR_BASE };

	return io_pa_or_va_secure(&base, 1);
}

unsigned int stm32mp1_pwr_regulator_mv(enum pwr_regulator id)
{
	assert(id < PWR_REGU_COUNT);

	return pwr_regulators[id].level_mv;
}

void stm32mp1_pwr_regulator_set_state(enum pwr_regulator id, bool enable)
{
	uintptr_t cr3 = stm32_pwr_base() + PWR_CR3_OFF;
	uint32_t enable_mask = pwr_regulators[id].cr3_enable_mask;

	assert(id < PWR_REGU_COUNT);

	if (enable) {
		uint32_t ready_mask = pwr_regulators[id].cr3_ready_mask;
		uint64_t to = 0;

		io_setbits32(cr3, enable_mask);

		to = timeout_init_us(10 * 1000);
		while (!timeout_elapsed(to))
			if (io_read32(cr3) & ready_mask)
				break;

		if (!(io_read32(cr3) & ready_mask))
			panic();
	} else {
		io_clrbits32(cr3, enable_mask);
	}
}

bool stm32mp1_pwr_regulator_is_enabled(enum pwr_regulator id)
{
	assert(id < PWR_REGU_COUNT);

	return io_read32(stm32_pwr_base() + PWR_CR3_OFF) &
	       pwr_regulators[id].cr3_enable_mask;
}

static TEE_Result stm32mp1_pwr_regu_set_state(struct regulator *regu,
					      bool enable)
{
	const struct pwr_regu_desc *desc = regu->priv;
	uintptr_t cr3 = stm32_pwr_base() + PWR_CR3_OFF;
	uint64_t to = 0;

	assert(desc);

	if (enable) {
		io_setbits32_stm32shregs(cr3, desc->cr3_enable_mask);

		to = timeout_init_us(TIMEOUT_US_10MS);
		while (!timeout_elapsed(to))
			if (io_read32(cr3) & desc->cr3_ready_mask)
				break;

		if (!(io_read32(cr3) & desc->cr3_ready_mask))
			return TEE_ERROR_GENERIC;
	} else {
		io_clrbits32_stm32shregs(cr3, desc->cr3_enable_mask);
	}

	return TEE_SUCCESS;
}

static TEE_Result stm32mp1_pwr_regu_read_state(struct regulator *regu,
					       bool *enabled)
{
	const struct pwr_regu_desc *desc = regu->priv;

	assert(desc);

	*enabled = io_read32(stm32_pwr_base() + PWR_CR3_OFF) &
		   desc->cr3_enable_mask;

	return TEE_SUCCESS;
}

static TEE_Result stm32mp1_pwr_regu_read_voltage(struct regulator *regu,
						 int *level_uv)
{
	const struct pwr_regu_desc *desc = regu->priv;

	assert(desc);

	*level_uv = (int)desc->level_mv * 1000;

	return TEE_SUCCESS;
}

static const struct regulator_ops stm32mp1_pwr_regu_ops = {
	.set_state = stm32mp1_pwr_regu_set_state,
	.get_state = stm32mp1_pwr_regu_read_state,
	.get_voltage = stm32mp1_pwr_regu_read_voltage,
};

/* Preallocated regulator devices */
static struct regulator pwr_regu_device[PWR_REGU_COUNT];

#define DEFINE_REG(_id, _name, _supply) { \
	.ops = &stm32mp1_pwr_regu_ops, \
	.name = _name, \
	.supply_name = _supply, \
	.priv = (void *)(pwr_regulators + (_id)), \
	.regulator = pwr_regu_device + (_id), \
}

static const struct regu_dt_desc stm32mp1_pwr_regu_dt_desc[] = {
	[PWR_REG11] = DEFINE_REG(PWR_REG11, "reg11", "vdd"),
	[PWR_REG18] = DEFINE_REG(PWR_REG18, "reg18", "vdd"),
	[PWR_USB33] = DEFINE_REG(PWR_USB33, "usb33", "vdd_3v3_usbfs"),
};
DECLARE_KEEP_PAGER(stm32mp1_pwr_regu_dt_desc);

struct regulator *stm32mp1_pwr_get_regulator(enum pwr_regulator id)
{
	if (id < ARRAY_SIZE(pwr_regu_device))
		return pwr_regu_device + id;

	return NULL;
}

static TEE_Result stm32mp1_pwr_regu_probe(const void *fdt, int node,
					  const void *compat_data __unused)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	const struct regu_dt_desc *dt_desc = stm32mp1_pwr_regu_dt_desc;
	int subnode = 0;

	fdt_for_each_subnode(subnode, fdt, node) {
		const char *node_name = fdt_get_name(fdt, subnode, NULL);
		unsigned int n = 0;

		for (n = 0; n < ARRAY_SIZE(stm32mp1_pwr_regu_dt_desc); n++)
			if (!strcmp(dt_desc[n].name, node_name))
				break;

		if (n >= ARRAY_SIZE(stm32mp1_pwr_regu_dt_desc)) {
			EMSG("Invalid PWR regulator node %s", node_name);
			panic();
		}

		if (IS_ENABLED(CFG_DRIVERS_REGULATOR)) {
			res = regulator_dt_register(fdt, subnode, node,
						    dt_desc + n);
			if (res) {
				EMSG("Can't register %s: %#"PRIx32, node_name,
				     res);
				panic();
			}
		}
	}

	return TEE_SUCCESS;
}

static const struct dt_device_match stm32mp1_pwr_regu_match_table[] = {
	{ .compatible = "st,stm32mp1,pwr-reg" },
	{ }
};

DEFINE_DT_DRIVER(stm32mp1_pwr_regu_dt_driver) = {
	.name = "stm32mp1-pwr-regu",
	.match_table = stm32mp1_pwr_regu_match_table,
	.probe = stm32mp1_pwr_regu_probe,
};
