////////////////////////////////////////////////////////////////////////////
//  (c) copyright 2024-by Persional Inc.
//  All Rights Reserved
//
//  Name:
//      st7789_fb_test.c
//
//  Purpose:
//      ST7789V DRM framebuffer 用户空间测试程序。
//      通过 /dev/fb0 操作显存, 测试显示输出。
//      测试项目:
//          1. 纯色填充 (红/绿/蓝/白/黑)
//          2. 彩条渐变
//          3. 棋盘格图案
//
// Author:
//     @听心跳的声音
//
//  Assumptions:
//
//  Revision History:
//      12/19/2022   Create New Version
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define FB_DEVICE       "/dev/fb0"

/* 测试图案颜色 (XRGB8888格式) */
#define COLOR_RED       0x00FF0000
#define COLOR_GREEN     0x0000FF00
#define COLOR_BLUE      0x000000FF
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_BLACK     0x00000000
#define COLOR_YELLOW    0x00FFFF00
#define COLOR_CYAN      0x0000FFFF
#define COLOR_MAGENTA   0x00FF00FF
#define COLOR_GRAY      0x00808080

/* 将 RGB 分量组合为 XRGB8888 像素值 */
#define RGB(r, g, b)    ((uint32_t)((r) << 16) | ((g) << 8) | (b))

static int fb_fd = -1;
static uint32_t *fb_mem = NULL;
static struct fb_fix_screeninfo fb_fix;
static struct fb_var_screeninfo fb_var;

/*
 * 打开framebuffer设备
 */
static int fb_open(void)
{
    fb_fd = open(FB_DEVICE, O_RDWR);
    if (fb_fd < 0) {
        perror("open " FB_DEVICE);
        return -1;
    }

    /* 获取固定屏幕信息 */
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
        perror("FBIOGET_FSCREENINFO");
        close(fb_fd);
        fb_fd = -1;
        return -1;
    }

    /* 获取可变屏幕信息 */
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
        perror("FBIOGET_VSCREENINFO");
        close(fb_fd);
        fb_fd = -1;
        return -1;
    }

    printf("Framebuffer info:\n");
    printf("  resolution: %ux%u\n", fb_var.xres, fb_var.yres);
    printf("  virtual:    %ux%u\n", fb_var.xres_virtual, fb_var.yres_virtual);
    printf("  bpp:        %u\n", fb_var.bits_per_pixel);
    printf("  pixel fmt:  R=%u/%u G=%u/%u B=%u/%u\n",
           fb_var.red.offset, fb_var.red.length,
           fb_var.green.offset, fb_var.green.length,
           fb_var.blue.offset, fb_var.blue.length);
    printf("  smem_len:   %u\n", fb_fix.smem_len);
    printf("  line_length: %u\n", fb_fix.line_length);

    /* mmap显存 */
    fb_mem = mmap(NULL, fb_fix.smem_len,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        perror("mmap");
        close(fb_fd);
        fb_fd = -1;
        return -1;
    }

    return 0;
}

/*
 * 关闭framebuffer设备
 */
static void fb_close(void)
{
    if (fb_mem && fb_mem != MAP_FAILED) {
        munmap(fb_mem, fb_fix.smem_len);
        fb_mem = NULL;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
}

/*
 * 画一个像素
 * @x, y: 坐标
 * @color: XRGB8888 颜色值
 */
static inline void fb_put_pixel(int x, int y, uint32_t color)
{
    uint32_t *pixel;
    uint32_t stride;

    if (x < 0 || y < 0 || x >= (int)fb_var.xres || y >= (int)fb_var.yres)
        return;

    stride = fb_fix.line_length / sizeof(uint32_t);
    pixel = fb_mem + y * stride + x;
    *pixel = color;
}

/*
 * 填充矩形区域 (测试局部刷新)
 * @x, y, w, h: 区域坐标和尺寸
 * @color: 填充颜色
 */
static void fb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    uint32_t stride;
    int row, col;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_var.xres) w = fb_var.xres - x;
    if (y + h > (int)fb_var.yres) h = fb_var.yres - y;
    if (w <= 0 || h <= 0) return;

    stride = fb_fix.line_length / sizeof(uint32_t);

    for (row = 0; row < h; row++) {
        uint32_t *line = fb_mem + (y + row) * stride + x;
        for (col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

/*
 * 测试1: 纯色填充
 * 从上到下依次填充: 红, 绿, 蓝, 白, 黑
 */
static void test_color_fill(void)
{
    int bar_h, y, i;
    uint32_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_WHITE, COLOR_BLACK
    };
    const char *names[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};

    bar_h = fb_var.yres / 5;

    for (i = 0; i < 5; i++) {
        y = i * bar_h;
        fb_fill_rect(0, y, fb_var.xres, bar_h, colors[i]);
        printf("  filling [%s] at y=%d, h=%d\n", names[i], y, bar_h);
    }

    sleep(2);
}

/*
 * 测试2: 垂直彩条渐变
 * 从左到右绘制8条彩条
 */
static void test_color_bars(void)
{
    int bar_w, x, y, i;
    uint32_t colors[] = {
        COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_GREEN,
        COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK
    };
    const char *names[] = {
        "WHT", "YEL", "CYN", "GRN", "MAG", "RED", "BLU", "BLK"
    };

    bar_w = fb_var.xres / 8;

    for (i = 0; i < 8; i++) {
        x = i * bar_w;
        fb_fill_rect(x, 0, bar_w, fb_var.yres, colors[i]);
        printf("  bar[%s] at x=%d, w=%d\n", names[i], x, bar_w);
    }

    sleep(2);
}

/*
 * 测试3: RGB渐变 (左上角区域)
 * 在屏幕左半部分绘制渐变, 验证局部刷新
 */
static void test_gradient(void)
{
    int x, y, w, h;
    uint32_t pixel;

    w = fb_var.xres / 2;
    h = fb_var.yres / 2;

    printf("  drawing RGB gradient (%dx%d)...\n", w, h);

    for (y = 0; y < h; y++) {
        uint8_t g = (y * 255) / h;   /* 垂直: 绿色渐变 */
        for (x = 0; x < w; x++) {
            uint8_t r = (x * 255) / w;   /* 水平: 红色渐变 */
            uint8_t b = 128;              /* 固定蓝色 */
            fb_put_pixel(x, y + h/2, RGB(r, g, b));
        }
    }

    sleep(2);
}

/*
 * 测试4: 棋盘格图案 (小方块, 验证显示器分辨率和局部刷新)
 */
static void test_checkerboard(void)
{
    int block_w, block_h, x, y, bx, by;

    block_w = 40;
    block_h = 40;

    printf("  drawing checkerboard (block=%dx%d)...\n", block_w, block_h);

    for (y = 0; y < (int)fb_var.yres; y += block_h) {
        for (x = 0; x < (int)fb_var.xres; x += block_w) {
            uint32_t color;
            /* 交替黑白 */
            if (((x / block_w) + (y / block_h)) & 1)
                color = COLOR_WHITE;
            else
                color = COLOR_GRAY;
            fb_fill_rect(x, y, block_w, block_h, color);
        }
    }

    sleep(2);
}

/*
 * 测试5: 逐行扫描填充 (全屏单色渐显)
 */
static void test_scanline_fill(void)
{
    int y;

    printf("  scanline fill: red...\n");
    for (y = 0; y < (int)fb_var.yres; y++) {
        fb_fill_rect(0, y, fb_var.xres, 1, COLOR_RED);
        usleep(5000);   /* 5ms/行 */
    }

    sleep(1);

    printf("  scanline fill: green...\n");
    for (y = 0; y < (int)fb_var.yres; y++) {
        fb_fill_rect(0, y, fb_var.xres, 1, COLOR_GREEN);
        usleep(5000);
    }

    sleep(1);

    printf("  scanline fill: blue...\n");
    for (y = 0; y < (int)fb_var.yres; y++) {
        fb_fill_rect(0, y, fb_var.xres, 1, COLOR_BLUE);
        usleep(5000);
    }

    sleep(1);
}

int main(int argc, const char *argv[])
{
    int test_sel = 0;

    if (argc > 1)
        test_sel = atoi(argv[1]);

    printf("=== ST7789V Framebuffer Test ===\n");

    /* 打开framebuffer */
    if (fb_open() < 0) {
        fprintf(stderr, "Failed to open framebuffer!\n");
        exit(1);
    }

    switch (test_sel) {
    case 0:
        /* 运行所有测试 */
        printf("\n--- Test 1: Solid Color Fill ---\n");
        test_color_fill();

        printf("\n--- Test 2: Color Bars ---\n");
        test_color_bars();

        printf("\n--- Test 3: RGB Gradient ---\n");
        test_gradient();

        printf("\n--- Test 4: Checkerboard ---\n");
        test_checkerboard();

        printf("\n--- Test 5: Scanline Fill ---\n");
        test_scanline_fill();

        break;
    case 1:
        test_color_fill();
        break;
    case 2:
        test_color_bars();
        break;
    case 3:
        test_gradient();
        break;
    case 4:
        test_checkerboard();
        break;
    case 5:
        test_scanline_fill();
        break;
    default:
        printf("Usage: %s [test_num 0-5]\n", argv[0]);
        printf("  0 = all tests (default)\n");
        printf("  1 = solid color fill\n");
        printf("  2 = color bars\n");
        printf("  3 = RGB gradient\n");
        printf("  4 = checkerboard\n");
        printf("  5 = scanline fill\n");
        break;
    }

    /* 清理: 黑屏 */
    printf("\nClearing screen...\n");
    fb_fill_rect(0, 0, fb_var.xres, fb_var.yres, COLOR_BLACK);

    fb_close();

    printf("=== Test Complete ===\n");
    return 0;
}
