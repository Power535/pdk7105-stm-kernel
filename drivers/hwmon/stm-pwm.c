/*
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Contains code copyright (C) Echostar Technologies Corporation
 * Author: Anthony Jackson <anthony.jackson@echostar.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/stm/platform.h>
#include <asm/io.h>

struct stm_pwm {
	struct resource *mem;
	void* base;
	struct device *hwmon_dev;
	struct stm_plat_pwm_data *platform_data;
	struct stm_pad_state *pad_state[STM_PLAT_PWM_NUM_CHANNELS];
#ifdef CONFIG_PM_SLEEP
	u8 suspend_value[STM_PLAT_PWM_NUM_CHANNELS];
#endif
};

/* PWM registers */
#define PWM_VAL(x)		(0x00 + (4 * (x)))
#define PWM_CTRL		0x50
#define PWM_CTRL_PWM_EN			(1<<9)
#define PWM_CTRL_PWM_CLK_VAL0_SHIFT	0
#define PWM_CTRL_PWM_CLK_VAL0_MASK	0x0f
#define PWM_CTRL_PWM_CLK_VAL4_SHIFT	11
#define PWM_CTRL_PWM_CLK_VAL4_MASK	0xf0
#define PWM_INT_EN		0x54

/* Prescale value (clock dividor):
 * 0: divide by 1
 * 0xff: divide by 256 */
#define PWM_CLK_VAL		0

static ssize_t show_pwm(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;
	struct stm_pwm *pwm = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", readl(pwm->base + PWM_VAL(channel)));
}

static ssize_t store_pwm(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int channel = to_sensor_dev_attr(attr)->index;
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	char* p;
	long val = simple_strtol(buf, &p, 10);

	if (p != buf) {
		val &= 0xff;
		writel(val, pwm->base + PWM_VAL(channel));
		return p-buf;
	}
	return -EINVAL;
}

static mode_t stm_pwm_is_visible(struct kobject *kobj, struct attribute *attr,
				int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	struct device_attribute *devattr;
	int channel;

	devattr = container_of(attr, struct device_attribute, attr);
	channel = to_sensor_dev_attr(devattr)->index;

	if (!pwm->platform_data->pwm_channel_config[channel].enabled)
		return 0;

	if (pwm->platform_data->pwm_channel_config[channel].locked)
		return S_IRUGO;

	return S_IRUGO | S_IWUSR;
}

static SENSOR_DEVICE_ATTR(pwm0, 0, show_pwm, store_pwm, 0);
static SENSOR_DEVICE_ATTR(pwm1, 0, show_pwm, store_pwm, 1);
static SENSOR_DEVICE_ATTR(pwm2, 0, show_pwm, store_pwm, 2);
static SENSOR_DEVICE_ATTR(pwm3, 0, show_pwm, store_pwm, 3);

static struct attribute *stm_pwm_attributes[] = {
	&sensor_dev_attr_pwm0.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	NULL
};

static struct attribute_group stm_pwm_attr_group = {
	.is_visible = stm_pwm_is_visible,
	.attrs = stm_pwm_attributes,
};

static void stm_pwm_enable(struct stm_pwm *pwm)
{
	u32 reg = 0;

	/* disable PWM if currently running */
	reg = readl(pwm->base + PWM_CTRL);
	reg &= ~PWM_CTRL_PWM_EN;
	writel(reg, pwm->base + PWM_CTRL);

	/* disable all PWM related interrupts */
	reg = 0;
	writel(reg, pwm->base + PWM_INT_EN);

	/* Set global PWM state:
	 * disable capture... */
	reg = 0;

	/* set prescale value... */
	reg |= (PWM_CLK_VAL & PWM_CTRL_PWM_CLK_VAL0_MASK) << PWM_CTRL_PWM_CLK_VAL0_SHIFT;
	reg |= (PWM_CLK_VAL & PWM_CTRL_PWM_CLK_VAL4_MASK) << PWM_CTRL_PWM_CLK_VAL4_SHIFT;

	/* enable */
	reg |= PWM_CTRL_PWM_EN;
	writel(reg, pwm->base + PWM_CTRL);
}

static int
stm_pwm_init(struct platform_device  *pdev, struct stm_pwm *pwm)
{
	int channel;

	stm_pwm_enable(pwm);

	for (channel = 0; channel < STM_PLAT_PWM_NUM_CHANNELS; channel++) {
		struct stm_plat_pwm_channel_config *channel_config;
		channel_config = &pwm->platform_data->pwm_channel_config[channel];
		if (channel_config->enabled) {
			if (!channel_config->retain_hw_value)
				writel(channel_config->initial_value,
				       pwm->base + PWM_VAL(channel));

			pwm->pad_state[channel] =
				devm_stm_pad_claim(&pdev->dev,
				pwm->platform_data->pwm_pad_config[channel],
				dev_name(&pdev->dev));
			if (!pwm->pad_state[channel])
				return -ENODEV;
		}
	}

	return sysfs_create_group(&pdev->dev.kobj, &stm_pwm_attr_group);
}
#ifdef CONFIG_OF
static struct stm_plat_pwm_data *
stm_pwm_dt_get_pdata(struct platform_device *pdev)
{
	struct stm_plat_pwm_data  *data;
	struct device_node *node;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

	for_each_child_of_node(pdev->dev.of_node, node) {
		u32 channel;
		struct stm_plat_pwm_channel_config *channel_config;
		u32 initial_value;
		bool retain_hw_value;
		bool locked;

		if (!of_device_is_available(node))
			continue;

		if (of_property_read_u32(node, "reg", &channel)) {
			dev_err(&pdev->dev, "unable to read \"reg\" for %s\n",
				node->full_name);
			continue;
		}

		if (channel >= STM_PLAT_PWM_NUM_CHANNELS) {
			dev_err(&pdev->dev,
				"invalid channel index %d on %s\n",
				(unsigned int)channel, node->full_name);
			continue;
		}

		initial_value = 0;
		of_property_read_u32(node, "st,initial-value", &initial_value);
		retain_hw_value = of_property_read_bool(node,
			"st,retain-hw-value");
		locked = of_property_read_bool(node, "st,locked");

		channel_config = &data->pwm_channel_config[channel];

		channel_config->enabled = true;
		channel_config->initial_value = initial_value;
		channel_config->retain_hw_value = retain_hw_value;
		channel_config->locked = locked;

		data->pwm_pad_config[channel] =
			stm_of_get_pad_config_index(&pdev->dev, channel);
	}

	return data;
}
#else
static void *stm_pwm_dt_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

static int stm_pwm_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct stm_pwm *pwm;
	int err;

	pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm)
		return -ENOMEM;

	if (pdev->dev.of_node)
		pwm->platform_data = stm_pwm_dt_get_pdata(pdev);
	else
		pwm->platform_data = pdev->dev.platform_data;

	if (!pwm->platform_data) {
		dev_err(&pdev->dev, "No platform data found\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No memory region in device\n");
		return -ENODEV;
	}

	pwm->mem = devm_request_mem_region(&pdev->dev,
			res->start, resource_size(res), "stm-pwm");
	if (pwm->mem == NULL) {
		dev_err(&pdev->dev, "failed to claim memory region\n");
		return -EBUSY;
	}

	pwm->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (pwm->base == NULL) {
		dev_err(&pdev->dev, "failed ioremap");
		return -EINVAL;
	}

	pwm->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(pwm->hwmon_dev)) {
		err = PTR_ERR(pwm->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		return err;
	}

	platform_set_drvdata(pdev, pwm);
	dev_info(&pdev->dev, "registers at 0x%x, mapped to 0x%p\n",
		 res->start, pwm->base);

	err = stm_pwm_init(pdev, pwm);

	if (err)
		hwmon_device_unregister(pwm->hwmon_dev);

	return err;
}

static int stm_pwm_remove(struct platform_device *pdev)
{
	struct stm_pwm *pwm = platform_get_drvdata(pdev);

	if (pwm) {
		hwmon_device_unregister(pwm->hwmon_dev);
		sysfs_remove_group(&pdev->dev.kobj, &stm_pwm_attr_group);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int stm_pwm_suspend(struct device *dev)
{
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	int channel;

	for (channel = 0; channel < STM_PLAT_PWM_NUM_CHANNELS; channel++) {
		if (!pwm->platform_data->pwm_channel_config[channel].enabled)
			continue;
		pwm->suspend_value[channel] =
			(u8)readl(pwm->base + PWM_VAL(channel));
	}

	return 0;
}

static int stm_pwm_restore(struct device *dev)
{
	struct stm_pwm *pwm = dev_get_drvdata(dev);
	int channel;

	stm_pwm_enable(pwm);

	for (channel = 0; channel < STM_PLAT_PWM_NUM_CHANNELS; channel++) {
		if (!pwm->platform_data->pwm_channel_config[channel].enabled)
			continue;
		writel(pwm->suspend_value, pwm->base + PWM_VAL(channel));
		stm_pad_setup(pwm->pad_state[channel]);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(stm_pwm_pm_ops, stm_pwm_suspend, stm_pwm_restore);

#ifdef CONFIG_OF
static struct of_device_id stm_pwm_match[] = {
	{
		.compatible = "st,pwm",
	},
	{},
};
MODULE_DEVICE_TABLE(of, stm_pwm_match);
#endif

static struct platform_driver stm_pwm_driver = {
	.driver = {
		.name		= "stm-pwm",
		.of_match_table = of_match_ptr(stm_pwm_match),
		.pm		= &stm_pwm_pm_ops,
	},
	.probe		= stm_pwm_probe,
	.remove		= stm_pwm_remove,
};

static int __init stm_pwm_module_init(void)
{
	return platform_driver_register(&stm_pwm_driver);
}

static void __exit stm_pwm_module_exit(void)
{
	platform_driver_unregister(&stm_pwm_driver);
}

module_init(stm_pwm_module_init);
module_exit(stm_pwm_module_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics simple PWM driver");
MODULE_LICENSE("GPL");
