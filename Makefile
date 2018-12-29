FB ?= fb1
DEV := /dev/$(FB)
SYSFS_DIR := /sys/class/graphics/$(FB)
SIZE_WXH := $(shell sed 's/,/x/' $(SYSFS_DIR)/virtual_size 2>/dev/null)

include makeinc/build.mk

.PHONY: all clean module info write_image grab

all: module

clean:
	$(RM) grab.png .clang-format
	$(MAKE) -C module clean
	$(RM) -r $(BUILD_DIR)

module:
	$(MAKE) -C $@

info:
	@echo $(SYSFS_DIR)
	@echo "name           :" $$(cat $(SYSFS_DIR)/name)
	@echo "virtual_size   :" $$(cat $(SYSFS_DIR)/virtual_size)
	@echo "bits_per_pixel :" $$(cat $(SYSFS_DIR)/bits_per_pixel)
	@echo "stride         :" $$(cat $(SYSFS_DIR)/stride)
	@echo "buffer_alloc   :" $$(cat $(SYSFS_DIR)/buffer_alloc)

write_image: IMAGE ?= image.png
write_image:
	ffmpeg -v error -y -i $(IMAGE) -vf scale=$(SIZE_WXH) -vcodec rawvideo -f rawvideo -pix_fmt rgb24 $(DEV)

grab:
	ffmpeg -v error -y -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -s $(SIZE_WXH) -i $(DEV) -f image2 -vcodec png $@.png

.clang-format:
	ln -s $(KERNEL_DIR)/.clang-format

MMAP_TEST = $(BUILD_DIR)/tests/mmap-test
$(MMAP_TEST): $(MMAP_TEST:=.o)
CC_EXES += $(MMAP_TEST)
OBJS += $(MMAP_TEST:=.o)

QT_TEST = $(BUILD_DIR)/tests/qt-test
$(QT_TEST): PKG_CONFIG_LIBS = Qt5Widgets >= 5.10
$(QT_TEST): CXXFLAGS += -fPIC -DQT_NO_KEYWORDS
$(QT_TEST): $(QT_TEST:=.o)
CXX_EXES += $(QT_TEST)
OBJS += $(QT_TEST:=.o)

all: $(MMAP_TEST) $(QT_TEST)

.PHONY: mmap-test qt-test

mmap-test: $(MMAP_TEST)
	$< $(DEV)

qt-test: $(QT_TEST)
	QT_QPA_PLATFORM=linuxfb:fb=$(DEV) $<

include makeinc/build_rules.mk
