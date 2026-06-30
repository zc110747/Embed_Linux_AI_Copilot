KERNELDIR := $(shell printenv AGENT_SDK_KERNEL_PATH)
CURRENT_PATH := $(shell pwd)

ARCH=arm
CROSS_COMPILE=arm-none-linux-gnueabihf-

build: kernel_modules

kernel_modules:
	echo "platform:${ARCH}, COMPILE:${CROSS_COMPILE}"
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
