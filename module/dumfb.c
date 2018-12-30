// Copyright (C) 2018 Martin Ejdestig <marejde@gmail.com>
// SPDX-License-Identifier: GPL-2.0+

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/types.h>

#define DUMFB_DEFAULT_WIDTH 1920
#define DUMFB_DEFAULT_HEIGHT 1080
#define DUMFB_BYTES_PER_PIXEL 3
#define DUMFB_BITS_PER_PIXEL (DUMFB_BYTES_PER_PIXEL * 8)
#define DUMFB_PSEUDO_PALETTE_SIZE 16

struct dumfb_par {
	u32 pseudo_palette[DUMFB_PSEUDO_PALETTE_SIZE];
};

// TODO: Can be set to 0 and ignoring potential overflow in dumfb_screen_size().
static struct {
	ushort width;
	ushort height;
} dumfb_parameters = { .width = DUMFB_DEFAULT_WIDTH,
                       .height = DUMFB_DEFAULT_HEIGHT };

static inline u32 dumfb_bytes_per_line(void)
{
	return DUMFB_BYTES_PER_PIXEL * dumfb_parameters.width;
}

static inline u32 dumfb_screen_size(void)
{
	return dumfb_bytes_per_line() * dumfb_parameters.height;
}

static int dumfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp, struct fb_info *info)
{
	u32 *palette = info->pseudo_palette;
	u32 r = red >> (16 - info->var.red.length);
	u32 g = green >> (16 - info->var.green.length);
	u32 b = blue >> (16 - info->var.blue.length);
	u32 color;

	if (regno >= DUMFB_PSEUDO_PALETTE_SIZE)
		return -EINVAL;

	color = (r << info->var.red.offset) |
	        (g << info->var.green.offset) |
	        (b << info->var.blue.offset);

	if (info->var.transp.length > 0) {
		u32 mask = (1 << info->var.transp.length) - 1;
		mask <<= info->var.transp.offset;
		color |= mask;
	}

	palette[regno] = color;

	return 0;
}

static int dumfb_mmap_vmalloc(void *addr, unsigned long addr_len,
			      struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pos = (unsigned long)addr + offset;

	pr_devel("%s\n", __FUNCTION__);

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) // TODO: Not verified earlier?
		return -EINVAL;

	if (offset > PAGE_ALIGN(addr_len))
		return -EINVAL;

	if (size > PAGE_ALIGN(addr_len) - offset)
		return -EINVAL;

	while (size > 0) {
		unsigned long pfn = vmalloc_to_pfn((void *)pos);
		int ret = remap_pfn_range(vma, start, pfn, PAGE_SIZE,
					  vma->vm_page_prot);
		if (ret < 0)
			return ret;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		size = size > PAGE_SIZE ? size - PAGE_SIZE : 0;
	}

	return 0;
}

static int dumfb_mmap_kmalloc(void *addr, unsigned long addr_len,
			      struct vm_area_struct *vma)
{
	unsigned long pfn = virt_to_phys(addr) >> PAGE_SHIFT;
	unsigned long pages = (addr_len + ~PAGE_MASK) >> PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;

	pr_devel("%s\n", __FUNCTION__);

	if (vma->vm_pgoff > pages)
		return -EINVAL;

	pfn += vma->vm_pgoff;
	pages -= vma->vm_pgoff;

	if (size >> PAGE_SHIFT > pages)
		return -EINVAL;

	return remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
}

static int dumfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	if (is_vmalloc_addr(info->screen_buffer))
		return dumfb_mmap_vmalloc(info->screen_buffer,
					  info->screen_size, vma);

	return dumfb_mmap_kmalloc(info->screen_buffer, info->screen_size, vma);
}

static ssize_t buffer_alloc_show(struct device *device,
				 struct device_attribute *attr, char *buf)
{
	struct fb_info *info = dev_get_drvdata(device);
	return scnprintf(buf, PAGE_SIZE, "%cmalloc\n",
			 is_vmalloc_addr(info->screen_buffer) ? 'v' : 'k');
}

static DEVICE_ATTR_RO(buffer_alloc);

static const struct fb_var_screeninfo dumfb_var = {
	.bits_per_pixel = DUMFB_BITS_PER_PIXEL,
	.red = { 16, 8, 0 },
	.green = { 8, 8, 0 },
	.blue = { 0, 8, 0 },
	.transp = { 0, 0, 0 },
	.activate = FB_ACTIVATE_NOW,
	.height = -1,
	.width = -1,
	.vmode = FB_VMODE_NONINTERLACED,
};

static const struct fb_fix_screeninfo dumfb_fix = {
	.id = "dumfb",
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_TRUECOLOR,
	.accel = FB_ACCEL_NONE,
};

static struct fb_ops dumfb_ops = {
	.owner = THIS_MODULE,
	.fb_setcolreg = dumfb_setcolreg,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
	.fb_mmap = dumfb_mmap,
};

static struct fb_info *dumfb_info = NULL;

static int __init dumfb_init(void)
{
	int ret = 0;
	struct fb_info *info;
	struct dumfb_par *par;
	void *screen_buffer;

	pr_devel("%s\n", __FUNCTION__);

	info = framebuffer_alloc(sizeof(struct dumfb_par), NULL);
	if (!info) {
		ret = -ENOMEM;
		goto framebuffer_alloc_error;
	}

	par = info->par;

	screen_buffer = kvzalloc(dumfb_screen_size(), GFP_USER);
	if (!screen_buffer) {
		ret = -ENOMEM;
		goto screen_buffer_alloc_error;
	}

	info->var = dumfb_var;
	info->var.xres = dumfb_parameters.width;
	info->var.yres = dumfb_parameters.height;
	info->var.xres_virtual = dumfb_parameters.width;
	info->var.yres_virtual = dumfb_parameters.height;

	info->fix = dumfb_fix;
	if (is_vmalloc_addr(screen_buffer)) {
		// This probably does not make any sense (only first page).
		info->fix.smem_start = PFN_PHYS(vmalloc_to_pfn(screen_buffer));
	} else {
		// This makes more sense but is probably meaningless.
		info->fix.smem_start = virt_to_phys(screen_buffer);
	}
	info->fix.smem_len = dumfb_screen_size();
	info->fix.line_length = dumfb_bytes_per_line();

	info->fbops = &dumfb_ops;
	info->screen_buffer = screen_buffer;
	info->screen_size = dumfb_screen_size();
	info->pseudo_palette = par->pseudo_palette;

	ret = register_framebuffer(info);
	if (ret < 0)
		goto register_framebuffer_error;

	ret = device_create_file(info->dev, &dev_attr_buffer_alloc);
	if (ret < 0)
		goto device_create_file_error; // Not a critical error but might as well fail.

	dumfb_info = info;

	return 0;

device_create_file_error:
	unregister_framebuffer(info);
register_framebuffer_error:
	kvfree(screen_buffer);
screen_buffer_alloc_error:
	framebuffer_release(info);
framebuffer_alloc_error:
	return ret;
}

static void __exit dumfb_exit(void)
{
	pr_devel("%s\n", __FUNCTION__);

	device_remove_file(dumfb_info->dev, &dev_attr_buffer_alloc);

	unregister_framebuffer(dumfb_info);

	kvfree(dumfb_info->screen_buffer);
	framebuffer_release(dumfb_info);
}

module_init(dumfb_init);
module_exit(dumfb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Ejdestig <marejde@gmail.com>");
MODULE_DESCRIPTION("Dumb framebuffer driver that reads/writes to memroy area");

module_param_named(width, dumfb_parameters.width, ushort, 0);
MODULE_PARM_DESC(width, "width of buffer");

module_param_named(height, dumfb_parameters.height, ushort, 0);
MODULE_PARM_DESC(height, "height of buffer");
