////////////////////////////////////////////////////////////////////////////
//  (c) copyright 2024-by Persional Inc.
//  All Rights Reserved
//
//  Name:
//      kernel_st7789.c
//
//  Purpose:
//      ST7789V SPI LCD DRM 驱动，支持局部刷新。
//      ST7789V 硬件特性:
//          - 240x320 像素, RGB565 (16-bit) 色彩
//          - 4线 SPI 接口 (SCLK, MOSI, CS, DC)
//          - 支持 CASET/RASET 局部窗口更新
//
// Author:
//     @听心跳的声音
//
//  Assumptions:
//
//  Revision History:
//      12/19/2022   Create New Version
/////////////////////////////////////////////////////////////////////////////
/*
设备树说明
1. ECSPI1控制器配置(SPI1, CS0):
&ecspi1 {
    fsl,spi-num-chipselects = <1>;
    pinctrl-names = "default";
    cs-gpios = <&gpio4 26 GPIO_ACTIVE_LOW>;
    pinctrl-0 = <&pinctrl_ecspi1>;
    status = "okay";

    st7789v: st7789v@0 {
        compatible = "rmk,st7789v";
        reg = <0>;
        spi-max-frequency = <40000000>;

        dc-gpios = <&gpio4 25 GPIO_ACTIVE_HIGH>;
        rst-gpios = <&gpio1 8 GPIO_ACTIVE_LOW>;
        backlight-gpios = <&gpio4 27 GPIO_ACTIVE_HIGH>;

        width = <240>;
        height = <320>;
        rotation = <0>;
    };
};

2. ECSPI1引脚配置:
pinctrl_ecspi1: ecspi1grp {
    fsl,pins = <
        MX6UL_PAD_CSI_DATA04__GPIO4_IO25     0x100b0   // DC
        MX6UL_PAD_CSI_DATA05__GPIO4_IO26     0x100b0   // CS
        MX6UL_PAD_CSI_DATA06__GPIO4_IO27     0x100b0   // BL
        MX6UL_PAD_GPIO1_IO08__GPIO1_IO08     0x100b0   // RST
        MX6UL_PAD_UART2_TX_DATA__ECSPI1_SCLK 0x100b1   // SCLK
        MX6UL_PAD_UART2_RX_DATA__ECSPI1_MOSI 0x100b1   // MOSI
    >;
};
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/version.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_framebuffer.h>
/*
 * GEM 内存管理: 5.12+ 重命名为 DMA (原 CMA)
 * 提供兼容宏: 5.12~5.x 映射到 DMA, 6.0+ 仅保留类型/基本函数映射
 *   (prime/vmap 相关在 6.0+ 已从 drm_driver 移除, 由 GEM object funcs 处理)
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
#include <drm/drm_gem_dma_helper.h>
/* 类型和基本操作 */
#define drm_gem_cma_object              drm_gem_dma_object
#define to_drm_gem_cma_obj(x)           to_drm_gem_dma_obj(x)
#define drm_gem_cma_dumb_create         drm_gem_dma_dumb_create
#define DEFINE_DRM_GEM_CMA_FOPS(name)   DEFINE_DRM_GEM_DMA_FOPS(name)
/* prime/vmap (5.12~5.x 存在, 6.0+ 的 #if 保证不会引用) */
#define drm_gem_cma_vm_ops              drm_gem_dma_vm_ops
#define drm_gem_cma_prime_get_sg_table  drm_gem_dma_prime_get_sg_table
#define drm_gem_cma_prime_import_sg_table drm_gem_dma_prime_import_sg_table
#define drm_gem_cma_prime_vmap          drm_gem_dma_prime_vmap
#define drm_gem_cma_prime_vunmap        drm_gem_dma_prime_vunmap
#define drm_gem_cma_prime_mmap          drm_gem_dma_prime_mmap
#else
#include <drm/drm_gem_cma_helper.h>
#endif
#include <drm/drm_connector.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_simple_kms_helper.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
#include <drm/drm_damage_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#endif

/*
 * fbdev emulation support.
 * 使用前向声明以避免不同BSP间头文件路径差异导致编译失败。
 * 函数实现位于 drm_fbdev_generic.c, 在 CONFIG_DRM_FBDEV_EMULATION=y 时
 * 由内核符号表解析。若链接报告 Unknown symbol, 请确保内核启用了该配置。
 */
#ifdef CONFIG_DRM_FBDEV_EMULATION
void drm_fbdev_generic_setup(struct drm_device *dev, unsigned int preferred_bpp);
#endif

/* ST7789V 寄存器命令定义 */
#define ST7789_NOP          0x00    /* 空操作 */
#define ST7789_SWRESET       0x01    /* 软件复位 */
#define ST7789_SLPIN         0x10    /* 睡眠模式 */
#define ST7789_SLPOUT        0x11    /* 退出睡眠 */
#define ST7789_PTLON         0x12    /* 局部显示模式开启 */
#define ST7789_NORON         0x13    /* 正常显示模式 */
#define ST7789_INVOFF        0x20    /* 关闭反转 */
#define ST7789_INVON         0x21    /* 开启反转 */
#define ST7789_DISPOFF       0x28    /* 关闭显示 */
#define ST7789_DISPON        0x29    /* 开启显示 */
#define ST7789_CASET         0x2A    /* 列地址设置 */
#define ST7789_RASET         0x2B    /* 行地址设置 */
#define ST7789_RAMWR         0x2C    /* 内存写入 */
#define ST7789_MADCTL        0x36    /* 内存数据访问控制 */
#define ST7789_COLMOD        0x3A    /* 接口像素格式 */
#define ST7789_PORCTRL       0xB2    /* 帧率控制 */
#define ST7789_GCTRL         0xB7    /* 网关控制 */
#define ST7789_VCOMS         0xBB    /* VCOM设置 */
#define ST7789_LCMCTRL       0xC0    /* LCM控制 */
#define ST7789_VDVVRHEN      0xC2    /* VDV和VRH命令使能 */
#define ST7789_VRHS          0xC3    /* VRH设置 */
#define ST7789_VDVS          0xC4    /* VDV设置 */
#define ST7789_FRCTRL2       0xC6    /* 帧率控制2(正常模式) */
#define ST7789_PWCTRL1       0xD0    /* 电源控制1 */
#define ST7789_PVGAMCTRL     0xE0    /* 正极性伽马控制 */
#define ST7789_NVGAMCTRL     0xE1    /* 负极性伽马控制 */

/* ST7789V MadCtl 位定义 */
#define MADCTL_MY            0x80    /* 行地址顺序 (上下翻转) */
#define MADCTL_MX            0x40    /* 列地址顺序 (左右翻转) */
#define MADCTL_MV            0x20    /* 行/列交换 (旋转90度) */
#define MADCTL_ML            0x10    /* 垂直刷新顺序 */
#define MADCTL_BGR           0x08    /* RGB/BGR顺序 */
#define MADCTL_MH            0x04    /* 水平刷新方向 */

/* 显示默认配置 */
#define ST7789_DEF_WIDTH     240
#define ST7789_DEF_HEIGHT    320

/* SPI传输行缓冲区大小 */
#define ST7789_ROW_BUF_SIZE   (ST7789_DEF_WIDTH * 2)  /* 480 bytes */

struct st7789_priv {
    /* SPI设备信息 */
    struct spi_device *spi;
    struct gpio_desc *dc_gpio;          /* Data/Command 引脚 */
    struct gpio_desc *rst_gpio;         /* 复位引脚 */
    struct gpio_desc *backlight_gpio;   /* 背光引脚 */

    /* DRM设备信息 */
    struct drm_device *drm;
    struct drm_simple_display_pipe pipe;

    /* SPI访问锁 (保护DC切换 + SPI传输的原子性) */
    struct mutex cmd_lock;

    /* 显示配置 */
    u8 madctl;                          /* MADCTL寄存器值 */
    u16 width;                          /* 显示宽度 */
    u16 height;                         /* 显示高度 */
    u32 rotation;                       /* 旋转角度 (0/90/180/270) */

    /* DMA安全的行缓冲区 (用于XRGB8888→RGB565转换) */
    u8 *row_buf;
};

/*
 * SPI通信辅助函数
 * DC引脚: 低电平 = 命令, 高电平 = 数据
 */

/* 发送命令字节 (DC = LOW) */
static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
    int ret;

    gpiod_set_value_cansleep(priv->dc_gpio, 0);
    ret = spi_write(priv->spi, &cmd, 1);
    if (ret)
        dev_err(&priv->spi->dev, "cmd 0x%02x write failed: %d\n", cmd, ret);
    return ret;
}

/* 发送数据字节 (DC = HIGH) */
static int st7789_write_data(struct st7789_priv *priv, const void *data,
                              size_t len)
{
    int ret;

    gpiod_set_value_cansleep(priv->dc_gpio, 1);
    ret = spi_write(priv->spi, data, len);
    if (ret)
        dev_err(&priv->spi->dev, "data write failed(len=%zu): %d\n", len, ret);
    return ret;
}

/* 发送命令+数据组合 (DC先低后高) */
static int st7789_write_cmd_data(struct st7789_priv *priv, u8 cmd,
                                  const void *data, size_t len)
{
    int ret;

    ret = st7789_write_cmd(priv, cmd);
    if (ret)
        return ret;
    return st7789_write_data(priv, data, len);
}

/*
 * MADCTL 旋转角度计算
 *
 * 旋转映射:
 *   0°   (竖屏):        0x00
 *   90°  (横屏):        MY | MV = 0xA0
 *   180° (竖屏倒置):    MX | MY = 0xC0
 *   270° (横屏倒置):    MX | MV = 0x60
 */
static u8 st7789_compute_madctl(u32 rotation)
{
    switch (rotation % 360) {
    case 0:
        return 0x00;
    case 90:
        return MADCTL_MY | MADCTL_MV;
    case 180:
        return MADCTL_MX | MADCTL_MY;
    case 270:
        return MADCTL_MX | MADCTL_MV;
    default:
        return 0x00;
    }
}

/*
 * 硬件复位
 * 复位时序: RST低 >10ms → RST高 >120ms
 */
static void st7789_hw_reset(struct st7789_priv *priv)
{
    if (!priv->rst_gpio)
        return;

    gpiod_set_value_cansleep(priv->rst_gpio, 0);
    usleep_range(10000, 15000);

    gpiod_set_value_cansleep(priv->rst_gpio, 1);
    usleep_range(120000, 130000);

    dev_info(&priv->spi->dev, "hardware reset completed\n");
}

/*
 * ST7789V面板初始化序列
 */
static int st7789_panel_init(struct st7789_priv *priv)
{
    int ret;
    u8 val;

    /* 1. 硬件复位 */
    st7789_hw_reset(priv);

    /* 2. 软件复位 */
    ret = st7789_write_cmd(priv, ST7789_SWRESET);
    if (ret)
        return ret;
    msleep(150);

    /* 3. 退出睡眠模式 */
    ret = st7789_write_cmd(priv, ST7789_SLPOUT);
    if (ret)
        return ret;
    msleep(120);

    /* 4. 设置像素格式: 16-bit/pixel (RGB565, 65K色) */
    val = 0x55;
    ret = st7789_write_cmd_data(priv, ST7789_COLMOD, &val, 1);
    if (ret)
        return ret;

    /* 5. 设置显示方向 */
    priv->madctl = st7789_compute_madctl(priv->rotation);
    ret = st7789_write_cmd_data(priv, ST7789_MADCTL, &priv->madctl, 1);
    if (ret)
        return ret;

    /* 6. 电源和显示配置 (ST7789V推荐初始化值) */
    val = 0x0C;
    ret = st7789_write_cmd_data(priv, ST7789_PORCTRL, &val, 1);
    if (ret)
        return ret;

    val = 0x05;
    ret = st7789_write_cmd_data(priv, ST7789_GCTRL, &val, 1);
    if (ret)
        return ret;

    val = 0x19;
    ret = st7789_write_cmd_data(priv, ST7789_VCOMS, &val, 1);
    if (ret)
        return ret;

    val = 0x2C;
    ret = st7789_write_cmd_data(priv, ST7789_LCMCTRL, &val, 1);
    if (ret)
        return ret;

    val = 0x01;
    ret = st7789_write_cmd_data(priv, ST7789_VDVVRHEN, &val, 1);
    if (ret)
        return ret;

    val = 0x12;
    ret = st7789_write_cmd_data(priv, ST7789_VRHS, &val, 1);
    if (ret)
        return ret;

    val = 0x20;
    ret = st7789_write_cmd_data(priv, ST7789_VDVS, &val, 1);
    if (ret)
        return ret;

    val = 0x0F;
    ret = st7789_write_cmd_data(priv, ST7789_FRCTRL2, &val, 1);
    if (ret)
        return ret;

    val = 0xA4;
    ret = st7789_write_cmd_data(priv, ST7789_PWCTRL1, &val, 1);
    if (ret)
        return ret;

    /* 正极性伽马 */
    {
        u8 pgamma[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,
                       0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
        ret = st7789_write_cmd_data(priv, ST7789_PVGAMCTRL, pgamma, 14);
        if (ret)
            return ret;
    }

    /* 负极性伽马 */
    {
        u8 ngamma[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,
                       0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
        ret = st7789_write_cmd_data(priv, ST7789_NVGAMCTRL, ngamma, 14);
        if (ret)
            return ret;
    }

    /* 7. 开启反转 (ST7789V标准配置) */
    ret = st7789_write_cmd(priv, ST7789_INVON);
    if (ret)
        return ret;

    /* 8. 正常显示模式 */
    ret = st7789_write_cmd(priv, ST7789_NORON);
    if (ret)
        return ret;
    msleep(10);

    /* 9. 开启显示 */
    ret = st7789_write_cmd(priv, ST7789_DISPON);
    if (ret)
        return ret;
    msleep(50);

    dev_info(&priv->spi->dev,
             "panel init done: %ux%u, madctl=0x%02x, rotation=%u\n",
             priv->width, priv->height, priv->madctl, priv->rotation);

    return 0;
}

/*
 * 关闭显示面板
 */
static void st7789_panel_disable(struct st7789_priv *priv)
{
    st7789_write_cmd(priv, ST7789_DISPOFF);
    msleep(20);
    st7789_write_cmd(priv, ST7789_SLPIN);
    msleep(120);
}

/*
 * 解析设备树配置
 * 获取 width, height, rotation 属性
 */
static int st7789_parse_dt(struct st7789_priv *priv)
{
    struct device *dev = &priv->spi->dev;

    if (of_property_read_u16(dev->of_node, "width", &priv->width))
        priv->width = ST7789_DEF_WIDTH;

    if (of_property_read_u16(dev->of_node, "height", &priv->height))
        priv->height = ST7789_DEF_HEIGHT;

    if (of_property_read_u32(dev->of_node, "rotation", &priv->rotation))
        priv->rotation = 0;
    priv->rotation %= 360;

    dev_info(dev, "dt parsed: %ux%u, rotation=%u\n",
             priv->width, priv->height, priv->rotation);

    return 0;
}

/*
 * 将DRM framebuffer中的脏区域写入ST7789V显示
 *
 * 局部刷新流程:
 * 1. CASET 设置列地址范围
 * 2. RASET 设置行地址范围
 * 3. RAMWR 写入像素数据 (逐行转换 XRGB8888 → RGB565 大端字节序)
 *
 * @param vaddr   framebuffer的CPU虚拟地址 (XRGB8888格式)
 * @param fb      DRM framebuffer
 * @param rect    需要更新的矩形区域(已裁剪到屏幕范围)
 */
static int st7789_fb_dirty(struct st7789_priv *priv, void *vaddr,
                            struct drm_framebuffer *fb,
                            const struct drm_rect *rect)
{
    u16 x_start = rect->x1;
    u16 x_end   = rect->x2 - 1;
    u16 y_start = rect->y1;
    u16 y_end   = rect->y2 - 1;
    u16 width   = drm_rect_width(rect);
    u16 height  = drm_rect_height(rect);
    u8 caset[4], raset[4];
    u32 src_stride;
    u32 *src;
    int y, ret;

    if (width == 0 || height == 0)
        return 0;

    /* CASET: 列地址 (X方向), 大端格式 */
    caset[0] = (x_start >> 8) & 0xFF;
    caset[1] = x_start & 0xFF;
    caset[2] = (x_end >> 8) & 0xFF;
    caset[3] = x_end & 0xFF;

    /* RASET: 行地址 (Y方向), 大端格式 */
    raset[0] = (y_start >> 8) & 0xFF;
    raset[1] = y_start & 0xFF;
    raset[2] = (y_end >> 8) & 0xFF;
    raset[3] = y_end & 0xFF;

    ret = st7789_write_cmd_data(priv, ST7789_CASET, caset, 4);
    if (ret)
        return ret;

    ret = st7789_write_cmd_data(priv, ST7789_RASET, raset, 4);
    if (ret)
        return ret;

    /* RAMWR: 内存写入 */
    ret = st7789_write_cmd(priv, ST7789_RAMWR);
    if (ret)
        return ret;

    /*
     * 逐行转换 XRGB8888 → RGB565 (大端字节序)
     *
     * XRGB8888: [X][R][G][B] (u32, 每通道8位)
     * RGB565:   高字节[RRRRRGGG] 低字节[GGGBBBBB]
     *
     * R[7:3] → bit[15:11], G[7:2] → bit[10:5], B[7:3] → bit[4:0]
     */
    src_stride = fb->pitches[0] / sizeof(u32);
    src = (u32 *)vaddr;

    for (y = 0; y < height; y++) {
        u32 *src_row = src + (y_start + y) * src_stride;
        int x;

        for (x = 0; x < width; x++) {
            u32 pixel = src_row[x_start + x];
            u16 r5, g6, b5, rgb565;

            r5 = (pixel >> 19) & 0x1F;
            g6 = (pixel >> 10) & 0x3F;
            b5 = (pixel >> 3) & 0x1F;
            rgb565 = (r5 << 11) | (g6 << 5) | b5;

            /* 大端序: 高字节在前 */
            priv->row_buf[x * 2]     = (rgb565 >> 8) & 0xFF;
            priv->row_buf[x * 2 + 1] = rgb565 & 0xFF;
        }

        ret = st7789_write_data(priv, priv->row_buf, width * 2);
        if (ret)
            return ret;
    }

    return 0;
}

/*
 * DRM 简单显示管道操作
 */

/* pipe.check: 校验plane state的格式和尺寸 */
static int st7789_pipe_check(struct drm_simple_display_pipe *pipe,
                              struct drm_plane_state *plane_state,
                              struct drm_crtc_state *crtc_state)
{
    const struct drm_display_mode *mode = &crtc_state->mode;
    struct st7789_priv *priv = container_of(pipe, struct st7789_priv, pipe);

    if (mode->hdisplay > priv->width || mode->vdisplay > priv->height) {
        dev_err(&priv->spi->dev, "mode %ux%u exceeds panel %ux%u\n",
                mode->hdisplay, mode->vdisplay, priv->width, priv->height);
        return -EINVAL;
    }

    return 0;
}

/*
 * pipe.update: 处理framebuffer更新及脏区域
 *
 * 局部刷新核心 — 使用drm_atomic_helper_damage_iter
 * (内核 ≥ 4.18) 迭代脏片段, 计算最小包围矩形, 仅刷新该区域。
 * 若无脏片段信息(全屏刷新或旧fbdev客户端), 则退化为全屏刷新。
 */
static void st7789_pipe_update(struct drm_simple_display_pipe *pipe,
                                struct drm_plane_state *old_state)
{
    struct st7789_priv *priv = container_of(pipe, struct st7789_priv, pipe);
    struct drm_plane_state *state = pipe->plane.state;
    struct drm_gem_cma_object *cma_obj;
    struct drm_framebuffer *fb = state->fb;
    struct drm_rect dirty_rect;
    bool has_damage = false;
    void *vaddr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    struct drm_atomic_helper_damage_iter iter;
    struct drm_rect damage;
#endif

    if (!fb)
        return;

    /* 直接获取GEM CMA对象 (兼容CMA/DMA命名变化) */
    cma_obj = to_drm_gem_cma_obj(fb->obj[0]);
    if (!cma_obj) {
        dev_err(&priv->spi->dev, "gem obj not found\n");
        return;
    }

    vaddr = cma_obj->vaddr;
    if (!vaddr) {
        dev_err(&priv->spi->dev, "gem vaddr is NULL\n");
        return;
    }

    /* 初始化脏矩形为 "空" 状态 */
    dirty_rect.x1 = priv->width;
    dirty_rect.y1 = priv->height;
    dirty_rect.x2 = 0;
    dirty_rect.y2 = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    /* 收集所有脏片段, 计算最小包围矩形 */
    drm_atomic_helper_damage_iter_init(&iter, old_state, state);
    while (drm_atomic_helper_damage_iter_next(&iter, &damage)) {
        dirty_rect.x1 = min(dirty_rect.x1, (int)damage.x1);
        dirty_rect.y1 = min(dirty_rect.y1, (int)damage.y1);
        dirty_rect.x2 = max(dirty_rect.x2, (int)damage.x2);
        dirty_rect.y2 = max(dirty_rect.y2, (int)damage.y2);
        has_damage = true;
    }
#endif

    /* 无脏片段信息 → 全屏刷新 */
    if (!has_damage) {
        dirty_rect.x1 = 0;
        dirty_rect.y1 = 0;
        dirty_rect.x2 = priv->width;
        dirty_rect.y2 = priv->height;
    }

    /* 裁剪到屏幕范围 */
    dirty_rect.x1 = max_t(int, dirty_rect.x1, 0);
    dirty_rect.y1 = max_t(int, dirty_rect.y1, 0);
    dirty_rect.x2 = min_t(int, dirty_rect.x2, (int)priv->width);
    dirty_rect.y2 = min_t(int, dirty_rect.y2, (int)priv->height);

    /* 跳过空的脏矩形 */
    if (dirty_rect.x1 >= dirty_rect.x2 || dirty_rect.y1 >= dirty_rect.y2)
        return;

    mutex_lock(&priv->cmd_lock);
    st7789_fb_dirty(priv, vaddr, fb, &dirty_rect);
    mutex_unlock(&priv->cmd_lock);
}

/* pipe.enable: 管道使能(开启背光) */
static void st7789_pipe_enable(struct drm_simple_display_pipe *pipe,
                                struct drm_crtc_state *crtc_state,
                                struct drm_plane_state *plane_state)
{
    struct st7789_priv *priv = container_of(pipe, struct st7789_priv, pipe);

    if (priv->backlight_gpio)
        gpiod_set_value_cansleep(priv->backlight_gpio, 1);
}

/* pipe.disable: 管道禁用(关闭背光) */
static void st7789_pipe_disable(struct drm_simple_display_pipe *pipe)
{
    struct st7789_priv *priv = container_of(pipe, struct st7789_priv, pipe);

    if (priv->backlight_gpio)
        gpiod_set_value_cansleep(priv->backlight_gpio, 0);
}

static const struct drm_simple_display_pipe_funcs st7789_pipe_funcs = {
    .check      = st7789_pipe_check,
    .update     = st7789_pipe_update,
    .enable     = st7789_pipe_enable,
    .disable    = st7789_pipe_disable,
};

/*
 * DRM mode 配置
 * 固定240x320模式
 */
static const struct drm_display_mode st7789_default_mode = {
    .clock          = 6400,
    .hdisplay       = ST7789_DEF_WIDTH,
    .hsync_start    = ST7789_DEF_WIDTH + 10,
    .hsync_end      = ST7789_DEF_WIDTH + 20,
    .htotal         = ST7789_DEF_WIDTH + 30,
    .vdisplay       = ST7789_DEF_HEIGHT,
    .vsync_start    = ST7789_DEF_HEIGHT + 10,
    .vsync_end      = ST7789_DEF_HEIGHT + 20,
    .vtotal         = ST7789_DEF_HEIGHT + 30,
    .flags          = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
    .width_mm       = 37,
    .height_mm      = 49,
};

/* get_modes: 提供固定显示模式 */
static int st7789_get_modes(struct drm_connector *connector)
{
    struct drm_device *drm = connector->dev;
    struct st7789_priv *priv = drm->dev_private;
    struct drm_display_mode *mode;

    mode = drm_mode_duplicate(drm, &st7789_default_mode);
    if (!mode)
        return 0;

    /* 根据设备树中的width/height调整模式 */
    mode->hdisplay = priv->width;
    mode->hsync_start = priv->width + 10;
    mode->hsync_end = priv->width + 20;
    mode->htotal = priv->width + 30;

    mode->vdisplay = priv->height;
    mode->vsync_start = priv->height + 10;
    mode->vsync_end = priv->height + 20;
    mode->vtotal = priv->height + 30;

    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

    drm_mode_set_name(mode);
    drm_mode_probed_add(connector, mode);

    return 1;
}

static const struct drm_connector_helper_funcs st7789_connector_hfuncs = {
    .get_modes = st7789_get_modes,
};

static const struct drm_connector_funcs st7789_connector_funcs = {
    .reset                  = drm_atomic_helper_connector_reset,
    .fill_modes             = drm_helper_probe_single_connector_modes,
    .destroy                = drm_connector_cleanup,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state   = drm_atomic_helper_connector_destroy_state,
};

/*
 * DRM mode_config_funcs
 * fb_create: 4.18+ 使用 drm_gem_fb_create_with_dirty (内建dirty支持),
 *           更早版本使用 drm_gem_fb_create (fallback全屏刷新)
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
static const struct drm_mode_config_funcs st7789_mode_config_funcs = {
    .fb_create              = drm_gem_fb_create_with_dirty,
    .atomic_check           = drm_atomic_helper_check,
    .atomic_commit          = drm_atomic_helper_commit,
};
#else
static struct drm_framebuffer *
st7789_fb_create(struct drm_device *dev, struct drm_file *file_priv,
                 const struct drm_mode_fb_cmd2 *mode_cmd)
{
    return drm_gem_fb_create(dev, file_priv, mode_cmd);
}

static const struct drm_mode_config_funcs st7789_mode_config_funcs = {
    .fb_create              = st7789_fb_create,
    .atomic_check           = drm_atomic_helper_check,
    .atomic_commit          = drm_atomic_helper_commit,
};
#endif

/* 支持的像素格式 */
static const u32 st7789_formats[] = {
    DRM_FORMAT_XRGB8888,
};

/*
 * DRM file operations 和 driver 定义
 */
DEFINE_DRM_GEM_CMA_FOPS(st7789_fops);

/*
 * DRM driver 结构体 — 根据内核版本配置不同的 GEM 操作
 *
 * 5.11-: gem_vm_ops 存在
 * 6.0-:  gem_prime_* / dumb_create 在 drm_driver 中显式设置
 * 6.0+:  这些操作全部移入 drm_gem_object_funcs, drm_driver 极简
 */
static const struct drm_driver st7789_drm_driver = {
    .driver_features        = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
    .fops                   = &st7789_fops,
    .name                   = "st7789",
    .desc                   = "ST7789V SPI Display Driver",
    .date                   = "20240329",
    .major                  = 1,
    .minor                  = 0,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
    .dumb_create            = drm_gem_cma_dumb_create,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
    .gem_vm_ops             = &drm_gem_cma_vm_ops,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
    .gem_prime_get_sg_table   = drm_gem_cma_prime_get_sg_table,
    .gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
    .gem_prime_vmap           = drm_gem_cma_prime_vmap,
    .gem_prime_vunmap         = drm_gem_cma_prime_vunmap,
    .gem_prime_mmap           = drm_gem_cma_prime_mmap,
#endif
};

/*
 * SPI driver probe / remove
 */

static int st7789_probe(struct spi_device *spi)
{
    struct device *dev = &spi->dev;
    struct st7789_priv *priv;
    struct drm_device *drm;
    struct drm_connector *connector;
    int ret;

    dev_info(dev, "st7789 probe start, max_speed=%u\n", spi->max_speed_hz);

    /* 1. 申请私有数据结构 */
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        dev_err(dev, "alloc private data failed!\n");
        return -ENOMEM;
    }
    priv->spi = spi;
    spi_set_drvdata(spi, priv);

    mutex_init(&priv->cmd_lock);

    /* 2. 配置SPI接口 */
    spi->mode = SPI_MODE_3;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret) {
        dev_err(dev, "spi_setup failed: %d\n", ret);
        return ret;
    }

    /* 3. 获取GPIO资源 */
    priv->dc_gpio = devm_gpiod_get(dev, "dc", GPIOD_OUT_LOW);
    if (IS_ERR(priv->dc_gpio)) {
        ret = PTR_ERR(priv->dc_gpio);
        dev_err(dev, "dc-gpios required, error: %d\n", ret);
        return ret;
    }

    priv->rst_gpio = devm_gpiod_get_optional(dev, "rst", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->rst_gpio)) {
        ret = PTR_ERR(priv->rst_gpio);
        dev_err(dev, "rst-gpios error: %d\n", ret);
        return ret;
    }

    priv->backlight_gpio = devm_gpiod_get_optional(dev, "backlight",
                                                    GPIOD_OUT_LOW);
    if (IS_ERR(priv->backlight_gpio)) {
        ret = PTR_ERR(priv->backlight_gpio);
        dev_err(dev, "backlight-gpios error: %d\n", ret);
        return ret;
    }

    /* 4. 解析设备树配置 */
    ret = st7789_parse_dt(priv);
    if (ret)
        return ret;

    /* 5. 分配DMA安全的行缓冲区 */
    priv->row_buf = devm_kmalloc(dev, ST7789_ROW_BUF_SIZE, GFP_KERNEL);
    if (!priv->row_buf) {
        dev_err(dev, "alloc row buffer failed!\n");
        return -ENOMEM;
    }

    /* 6. 初始化ST7789V面板 */
    ret = st7789_panel_init(priv);
    if (ret) {
        dev_err(dev, "panel init failed: %d\n", ret);
        return ret;
    }

    /* 7. 分配并初始化DRM设备 */
    drm = drm_dev_alloc(&st7789_drm_driver, dev);
    if (IS_ERR(drm)) {
        ret = PTR_ERR(drm);
        dev_err(dev, "drm_dev_alloc failed: %d\n", ret);
        goto err_panel_disable;
    }
    drm->dev_private = priv;
    priv->drm = drm;

    drm_mode_config_init(drm);
    drm->mode_config.min_width  = priv->width;
    drm->mode_config.max_width  = priv->width;
    drm->mode_config.min_height = priv->height;
    drm->mode_config.max_height = priv->height;
    drm->mode_config.preferred_depth = 24;
    drm->mode_config.funcs = &st7789_mode_config_funcs;

    /* 8. 创建connector (drm_connector_init 全版本兼容) */
    connector = devm_kzalloc(dev, sizeof(*connector), GFP_KERNEL);
    if (!connector) {
        ret = -ENOMEM;
        dev_err(dev, "connector alloc failed!\n");
        goto err_mode_config_cleanup;
    }
    ret = drm_connector_init(drm, connector, &st7789_connector_funcs,
                              DRM_MODE_CONNECTOR_SPI);
    if (ret) {
        dev_err(dev, "connector init failed: %d\n", ret);
        goto err_mode_config_cleanup;
    }
    drm_connector_helper_add(connector, &st7789_connector_hfuncs);

    /*
     * drm_simple_display_pipe_init 会自动:
     * - 创建primary plane + CRTC + encoder
     * - 将encoder连接到我们提供的connector
     * 因此不需要手动调用 drm_connector_attach_encoder
     */
    ret = drm_simple_display_pipe_init(drm, &priv->pipe, &st7789_pipe_funcs,
                                        st7789_formats,
                                        ARRAY_SIZE(st7789_formats),
                                        NULL, connector);
    if (ret) {
        dev_err(dev, "pipe init failed: %d\n", ret);
        goto err_connector_cleanup;
    }

    /* 9. 使能plane damage clips跟踪(局部刷新核心, 需要4.18+) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    drm_plane_enable_fb_damage_clips(&priv->pipe.plane);
#endif

    /* 10. 设置fbdev emulation (提供 /dev/fb0) */
#ifdef CONFIG_DRM_FBDEV_EMULATION
    drm_fbdev_generic_setup(drm, 0);
#endif

    /* 11. 注册DRM设备 */
    ret = drm_dev_register(drm, 0);
    if (ret) {
        dev_err(dev, "drm_dev_register failed: %d\n", ret);
        goto err_connector_cleanup;
    }

    /* 12. 开启背光 */
    if (priv->backlight_gpio)
        gpiod_set_value_cansleep(priv->backlight_gpio, 1);

    dev_info(dev, "st7789 probe success! %ux%u, DRM device registered\n",
             priv->width, priv->height);

    return 0;

err_connector_cleanup:
    /* drm_mode_config_cleanup 会统一清理 connector, 不显式重复 */
err_mode_config_cleanup:
    drm_mode_config_cleanup(drm);
    drm_dev_put(drm);
err_panel_disable:
    st7789_panel_disable(priv);
    mutex_destroy(&priv->cmd_lock);
    return ret;
}

static void st7789_remove(struct spi_device *spi)
{
    struct st7789_priv *priv = spi_get_drvdata(spi);
    struct drm_device *drm = priv->drm;

    dev_info(&spi->dev, "st7789 remove start\n");

    /* 关闭背光 */
    if (priv->backlight_gpio)
        gpiod_set_value_cansleep(priv->backlight_gpio, 0);

    /* 卸载DRM设备 */
    drm_dev_unregister(drm);
    drm_atomic_helper_shutdown(drm);

    /* 关闭面板 */
    st7789_panel_disable(priv);

    /* 清理DRM资源 */
    drm_mode_config_cleanup(drm);
    drm_dev_put(drm);

    mutex_destroy(&priv->cmd_lock);

    dev_info(&spi->dev, "st7789 remove success!\n");
}

static const struct of_device_id st7789_of_match[] = {
    { .compatible = "rmk,st7789v" },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, st7789_of_match);

static struct spi_driver st7789_spi_driver = {
    .probe = st7789_probe,
    .remove = st7789_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "st7789",
        .of_match_table = st7789_of_match,
    },
};

module_spi_driver(st7789_spi_driver);

MODULE_AUTHOR("zc");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ST7789V SPI LCD DRM driver with partial refresh");
MODULE_ALIAS("spi_st7789_drm_driver");
