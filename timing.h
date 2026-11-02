#ifndef RG34XX_TIMING_H
#define RG34XX_TIMING_H

/*
 * Read LCD timing properties from the actual boot package.
 * READ ONLY: this file never writes to the boot device.
 *
 * lcd_dclk_freq: MHz
 * lcd_ht:        horizontal total
 * lcd_vt:        vertical total
 */

static uint32_t timing_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

typedef struct {
    uint32_t clock_mhz;
    uint32_t ht;
    uint32_t vt;
    int got_clock;
    int got_ht;
    int got_vt;
} TimingNode;

static double timing_decode_package(const unsigned char *p) {
    /*
     * The LCD configuration lives in the second FDT.
     * These anchors are established by the known,
     * verified RG34XX boot packages.
     */
    const size_t dtb = 0x11b800;
    const size_t clock_anchor = 0x12b5b4;
    const size_t ht_anchor = 0x12b624;

    if (dtb + 40 > PKG_SIZE ||
        timing_be32(p + dtb) != 0xd00dfeed)
        return 0.0;

    uint32_t total = timing_be32(p + dtb + 4);
    uint32_t off_struct = timing_be32(p + dtb + 8);
    uint32_t off_strings = timing_be32(p + dtb + 12);
    uint32_t size_strings = timing_be32(p + dtb + 32);
    uint32_t size_struct = timing_be32(p + dtb + 36);

    if (total < 40 || total > PKG_SIZE - dtb ||
        off_struct > total ||
        size_struct > total - off_struct ||
        off_strings > total ||
        size_strings > total - off_strings)
        return 0.0;

    size_t cursor = dtb + off_struct;
    size_t end = cursor + size_struct;
    size_t strings = dtb + off_strings;

    TimingNode nodes[32] = {{0}};
    int depth = 0;

    while (cursor + 4 <= end) {
        uint32_t token = timing_be32(p + cursor);
        cursor += 4;

        if (token == 1) {
            /* FDT_BEGIN_NODE */
            while (cursor < end && p[cursor] != 0)
                cursor++;

            if (cursor >= end || depth >= 32)
                return 0.0;

            cursor = (cursor + 4) & ~(size_t)3;
            if (cursor > end)
                return 0.0;

            nodes[depth] = (TimingNode){0};
            depth++;

        } else if (token == 2) {
            /* FDT_END_NODE */
            if (depth == 0)
                return 0.0;

            TimingNode *n = &nodes[depth - 1];

            if (n->got_clock && n->got_ht && n->got_vt &&
                n->clock_mhz >= 1 && n->clock_mhz <= 300 &&
                n->ht >= 100 && n->vt >= 100) {

                double hz =
                    (double)n->clock_mhz * 1000000.0 /
                    ((double)n->ht * (double)n->vt);

                return hz >= 1.0 && hz <= 999.0
                    ? hz : 0.0;
            }

            depth--;

        } else if (token == 3) {
            /* FDT_PROP */
            if (depth == 0 || cursor + 8 > end)
                return 0.0;

            uint32_t len = timing_be32(p + cursor);
            uint32_t nameoff = timing_be32(p + cursor + 4);
            cursor += 8;

            if (len > end - cursor)
                return 0.0;

            size_t value_pos = cursor;
            cursor = (cursor + (size_t)len + 3) & ~(size_t)3;

            if (cursor > end || nameoff >= size_strings)
                return 0.0;

            const char *name =
                (const char *)(p + strings + nameoff);

            if (!memchr(name, 0, size_strings - nameoff))
                return 0.0;

            if (len != 4)
                continue;

            uint32_t value = timing_be32(p + value_pos);
            TimingNode *n = &nodes[depth - 1];

            if (strcmp(name, "lcd_dclk_freq") == 0 &&
                value_pos == clock_anchor) {
                n->clock_mhz = value;
                n->got_clock = 1;

            } else if (strcmp(name, "lcd_ht") == 0 &&
                       value_pos == ht_anchor) {
                n->ht = value;
                n->got_ht = 1;

            } else if (strcmp(name, "lcd_vt") == 0) {
                n->vt = value;
                n->got_vt = 1;
            }

        } else if (token == 4) {
            /* FDT_NOP */
            continue;

        } else if (token == 9) {
            /* FDT_END */
            break;

        } else {
            return 0.0;
        }
    }

    return 0.0;
}

static double timing_read_current_hz(void) {
    if (!flash_device_check())
        return 0.0;

    unsigned char *p = malloc(PKG_SIZE);
    if (!p)
        return 0.0;

    int fd = open(
        FLASH_DEVICE,
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW
    );

    struct stat st;
    int ok = fd >= 0 &&
             fstat(fd, &st) == 0 &&
             S_ISBLK(st.st_mode) &&
             read_exact(fd, BOOT_OFFSET, p, PKG_SIZE);

    if (fd >= 0)
        close(fd);

    double hz = ok ? timing_decode_package(p) : 0.0;

    free(p);
    return hz;
}

#endif
