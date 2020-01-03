// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/input.h>
#include <linux/kernel.h>

#include "context.h"
#include "protocol/touch.h"

// HACK: Workaround for DKMS build without BUS_MEI patch
#ifndef BUS_MEI
#define BUS_MEI 0x44
#endif

static void ipts_stylus_handle_report(struct ipts_context *ipts,
		struct ipts_stylus_report *report)
{
	__u16 tool;
	__u16 mode = le16_to_cpu(report->mode);
	__u16 x = le16_to_cpu(report->x);
	__u16 y = le16_to_cpu(report->y);
	__u16 pressure = le16_to_cpu(report->pressure);

	__u8 prox = mode & 0x1;
	__u8 touch = mode & 0x2;
	__u8 button = mode & 0x4;
	__u8 rubber = mode & 0x8;

	if (prox && rubber)
		tool = BTN_TOOL_RUBBER;
	else
		tool = BTN_TOOL_PEN;

	// Fake proximity out to switch tools
	if (ipts->stylus_tool != tool) {
		input_report_key(ipts->stylus, ipts->stylus_tool, 0);
		input_sync(ipts->stylus);
		ipts->stylus_tool = tool;
	}

	input_report_key(ipts->stylus, BTN_TOUCH, touch);
	input_report_key(ipts->stylus, ipts->stylus_tool, prox);
	input_report_key(ipts->stylus, BTN_STYLUS, button);

	input_report_abs(ipts->stylus, ABS_X, x);
	input_report_abs(ipts->stylus, ABS_Y, y);
	input_report_abs(ipts->stylus, ABS_PRESSURE, pressure);

	input_report_abs(ipts->stylus, ABS_TILT_X, 9000);
	input_report_abs(ipts->stylus, ABS_TILT_Y, 9000);

	input_sync(ipts->stylus);
}

void ipts_stylus_parse_report(struct ipts_context *ipts,
		struct ipts_touch_data *data)
{
	int count, i;
	struct ipts_stylus_report *reports;

	count = data->data[32];
	reports = (struct ipts_stylus_report *)&data->data[40];

	for (i = 0; i < count; i++)
		ipts_stylus_handle_report(ipts, &reports[i]);
}

int ipts_stylus_init(struct ipts_context *ipts)
{
	int ret;

	ipts->stylus = devm_input_allocate_device(ipts->dev);
	if (!ipts->stylus)
		return -ENOMEM;

	ipts->stylus_tool = BTN_TOOL_PEN;

	__set_bit(INPUT_PROP_DIRECT, ipts->stylus->propbit);
	__set_bit(INPUT_PROP_POINTER, ipts->stylus->propbit);

	input_set_abs_params(ipts->stylus, ABS_X, 0, 9600, 0, 0);
	input_abs_set_res(ipts->stylus, ABS_X, 34);
	input_set_abs_params(ipts->stylus, ABS_Y, 0, 7200, 0, 0);
	input_abs_set_res(ipts->stylus, ABS_Y, 38);
	input_set_abs_params(ipts->stylus, ABS_PRESSURE, 0, 4096, 0, 0);
	input_set_abs_params(ipts->stylus, ABS_TILT_X, 0, 18000, 0, 0);
	input_abs_set_res(ipts->stylus, ABS_TILT_X, 5730);
	input_set_abs_params(ipts->stylus, ABS_TILT_Y, 0, 18000, 0, 0);
	input_abs_set_res(ipts->stylus, ABS_TILT_Y, 5730);
	input_set_capability(ipts->stylus, EV_KEY, BTN_TOUCH);
	input_set_capability(ipts->stylus, EV_KEY, BTN_STYLUS);
	input_set_capability(ipts->stylus, EV_KEY, BTN_TOOL_PEN);
	input_set_capability(ipts->stylus, EV_KEY, BTN_TOOL_RUBBER);

	ipts->stylus->id.bustype = BUS_MEI;
	ipts->stylus->id.vendor = ipts->device_info.vendor_id;
	ipts->stylus->id.product = ipts->device_info.device_id;
	ipts->stylus->id.version = ipts->device_info.fw_rev;

	ipts->stylus->phys = "heci3";
	ipts->stylus->name = "Intel Precise Stylus";

	ret = input_register_device(ipts->stylus);
	if (ret) {
		dev_err(ipts->dev, "Failed to register stylus device: %d\n",
				ret);
		return ret;
	}

	return 0;
}
