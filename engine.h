#ifndef RG34XX_ENGINE_H
#define RG34XX_ENGINE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define PKG_SIZE 1310720
#define BOOT_OFFSET 0x1004000LL

#define BACKUP_DIR "/run/muos/storage/backup/RG34XXSwitcher"
#define ORIGINAL BACKUP_DIR "/original.bin"
#define PATCH60  BACKUP_DIR "/60hz.fex"
#define PATCH120 BACKUP_DIR "/120hz.fex"

static const char *ORIGINAL_SHA =
    "a124aec8478e8d0e7f7fe9590a0d96b879e54d21c6c6d96989f94d8b8ac5567c";
static const char *PATCH60_SHA =
    "bf4d9d800281a49f30a307880b547d2499c9d691c0996d9010b05b0b2fb41271";

/* Self-contained SHA-256 */
typedef struct {
    uint32_t h[8];
    uint64_t bits;
    unsigned char block[64];
    unsigned used;
} SHA256;

static uint32_t rr(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

static void sha_block(SHA256 *s, const unsigned char *p) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
        0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
        0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
        0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
        0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
        0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    uint32_t w[64];

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i*4] << 24) |
               ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) |
               p[i*4+3];
    }

    for (int i = 16; i < 64; i++) {
        uint32_t x = w[i-15], y = w[i-2];
        uint32_t a = rr(x,7) ^ rr(x,18) ^ (x >> 3);
        uint32_t b = rr(y,17) ^ rr(y,19) ^ (y >> 10);
        w[i] = w[i-16] + a + w[i-7] + b;
    }

    uint32_t a=s->h[0], b=s->h[1], c=s->h[2], d=s->h[3];
    uint32_t e=s->h[4], f=s->h[5], g=s->h[6], h=s->h[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rr(e,6) ^ rr(e,11) ^ rr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = rr(a,2) ^ rr(a,13) ^ rr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;

        h=g; g=f; f=e; e=d+t1;
        d=c; c=b; b=a; a=t1+t2;
    }

    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
    s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}

static void sha_init(SHA256 *s) {
    static const uint32_t initial[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(s->h, initial, sizeof(initial));
    s->bits = 0;
    s->used = 0;
}

static void sha_update(SHA256 *s, const unsigned char *p, size_t n) {
    s->bits += (uint64_t)n * 8;

    while (n) {
        size_t take = 64 - s->used;
        if (take > n) take = n;

        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;

        if (s->used == 64) {
            sha_block(s, s->block);
            s->used = 0;
        }
    }
}

static void sha_finish(SHA256 *s, unsigned char out[32]) {
    uint64_t bits = s->bits;
    unsigned char one = 0x80, zero = 0;
    sha_update(s, &one, 1);

    while (s->used != 56)
        sha_update(s, &zero, 1);

    unsigned char len[8];
    for (int i = 0; i < 8; i++)
        len[i] = (unsigned char)(bits >> (56 - 8*i));

    sha_update(s, len, 8);

    for (int i = 0; i < 8; i++) {
        out[i*4]   = s->h[i] >> 24;
        out[i*4+1] = s->h[i] >> 16;
        out[i*4+2] = s->h[i] >> 8;
        out[i*4+3] = s->h[i];
    }
}

static int sha_matches(const unsigned char *p, size_t n,
                       const char *expected) {
    SHA256 s;
    unsigned char digest[32];
    char hex[65];

    sha_init(&s);
    sha_update(&s, p, n);
    sha_finish(&s, digest);

    for (int i = 0; i < 32; i++)
        sprintf(hex + i*2, "%02x", digest[i]);

    hex[64] = 0;
    return strcmp(hex, expected) == 0;
}

static int read_exact(int fd, off_t offset, unsigned char *p, size_t n) {
    size_t done = 0;

    while (done < n) {
        ssize_t r = pread(fd, p + done, n - done, offset + done);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return 0;
        done += (size_t)r;
    }
    return 1;
}

static int read_file(const char *path, unsigned char *p) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 0;

    struct stat st;
    int ok = fstat(fd, &st) == 0 &&
             S_ISREG(st.st_mode) &&
             st.st_size == PKG_SIZE &&
             read_exact(fd, 0, p, PKG_SIZE);

    close(fd);
    return ok;
}

static int valid_original(const unsigned char *p) {
    static const unsigned char dtb[] = {0xd0,0x0d,0xfe,0xed};

    return memcmp(p, "sunxi-package", 13) == 0 &&
           memcmp(p + 0x11ac00, dtb, 4) == 0 &&
           memcmp(p + 0x11b800, dtb, 4) == 0 &&
           p[0x12b5b7] == 0x1b &&
           p[0x12b626] == 0x03 &&
           p[0x12b627] == 0x48 &&
           sha_matches(p, PKG_SIZE, ORIGINAL_SHA);
}

static int backup_ok(void) {
    unsigned char *p = malloc(PKG_SIZE);
    if (!p) return 0;

    int ok = read_file(ORIGINAL, p) && valid_original(p);
    free(p);
    return ok;
}

static int ensure_dir(void) {
    struct stat st;

    if (stat("/run/muos/storage", &st) != 0 ||
        !S_ISDIR(st.st_mode))
        return 0;

    if (mkdir("/run/muos/storage/backup", 0700) != 0 &&
        errno != EEXIST)
        return 0;

    if (mkdir(BACKUP_DIR, 0700) != 0 &&
        errno != EEXIST)
        return 0;

    return stat(BACKUP_DIR, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Create only. Never overwrite an existing file. */
static int save_new(const char *path, const unsigned char *p) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL |
                  O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return 0;

    size_t done = 0;
    int ok = 1;

    while (done < PKG_SIZE) {
        ssize_t n = write(fd, p + done, PKG_SIZE - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = 0; break; }
        done += (size_t)n;
    }

    if (ok && fsync(fd) != 0) ok = 0;
    if (close(fd) != 0) ok = 0;

    if (ok) {
        unsigned char *check = malloc(PKG_SIZE);
        ok = check && read_file(path, check) &&
             memcmp(check, p, PKG_SIZE) == 0;
        free(check);
    }

    if (!ok) unlink(path);
    return ok;
}


#define UNKNOWN_BACKUP BACKUP_DIR "/unknown-backup.bin"

static int pending_unknown_backup = 0;

static int looks_like_boot_package(const unsigned char *p) {
    static const unsigned char dtb[] = {
        0xd0, 0x0d, 0xfe, 0xed
    };

    return memcmp(p, "sunxi-package", 13) == 0 &&
           memcmp(p + 0x11ac00, dtb, 4) == 0 &&
           memcmp(p + 0x11b800, dtb, 4) == 0;
}

static const char *create_backup(void) {
    if (!ensure_dir())
        return "STORAGE NOT AVAILABLE";

    if (backup_ok())
        return "BACKUP ALREADY VERIFIED";

    if (access(ORIGINAL, F_OK) == 0)
        return "BACKUP EXISTS INVALID";

    unsigned char *p = malloc(PKG_SIZE);
    if (!p)
        return "OUT OF MEMORY";

    const char *devices[] = {
        "/dev/mmcblk0",
        "/dev/mmcblk1",
        "/dev/mmcblk2"
    };

    const char *source = NULL;
    int matches = 0;
    int known = 0;

    for (int i = 0; i < 3; i++) {
        int fd = open(devices[i],
                      O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0)
            continue;

        struct stat st;

        if (fstat(fd, &st) == 0 &&
            S_ISBLK(st.st_mode) &&
            read_exact(fd, BOOT_OFFSET, p, PKG_SIZE) &&
            looks_like_boot_package(p)) {
            matches++;
            source = devices[i];
            known = valid_original(p);
        }

        close(fd);
    }

    if (matches != 1) {
        pending_unknown_backup = 0;
        free(p);

        return matches > 1
            ? "MULTIPLE BOOT SOURCES"
            : "BOOT PACKAGE NOT FOUND";
    }

    if (!known && !pending_unknown_backup) {
        pending_unknown_backup = 1;
        free(p);
        return "CONFIRM UNKNOWN BACKUP";
    }

    const char *destination =
        known ? ORIGINAL : UNKNOWN_BACKUP;

    pending_unknown_backup = 0;

    if (access(destination, F_OK) == 0) {
        free(p);
        return "BACKUP FILE ALREADY EXISTS";
    }

    /* Re-read and verify before saving. */
    int fd = open(source,
                  O_RDONLY | O_CLOEXEC | O_NOFOLLOW);

    int ok = fd >= 0 &&
             read_exact(fd, BOOT_OFFSET, p, PKG_SIZE) &&
             looks_like_boot_package(p) &&
             (known ? valid_original(p) : !valid_original(p));

    if (fd >= 0)
        close(fd);

    if (ok)
        ok = save_new(destination, p);

    free(p);

    if (!ok)
        return "BACKUP FAILED";

    return known
        ? "ORIGINAL BACKUP VERIFIED"
        : "UNKNOWN BACKUP SAVED";
}

static const char *prepare_patch(int hz) {
    unsigned char *p = malloc(PKG_SIZE);
    if (!p) return "OUT OF MEMORY";

    if (!read_file(ORIGINAL, p) || !valid_original(p)) {
        free(p);
        return "BACKUP REQUIRED";
    }

    /* Display clock: 27 -> 54 MHz */
    p[0x12b5b7] = 0x36;

    if (hz == 60) {
        /* lcd_ht: 840 -> 1679 */
        p[0x12b626] = 0x06;
        p[0x12b627] = 0x8f;

        /* Observed checksum from working 60 Hz package */
        p[0x16] = 0x3a;
        p[0x17] = 0xa2;

        if (!sha_matches(p, PKG_SIZE, PATCH60_SHA)) {
            free(p);
            return "60 HZ VALIDATION FAILED";
        }
    } else {
        /* lcd_ht remains 840 */
        p[0x16] = 0x37;
        p[0x17] = 0x5b;
        /* 120 Hz checksum is calculated, not device-tested. */
    }

    const char *path = hz == 60 ? PATCH60 : PATCH120;

    if (!ensure_dir()) {
        free(p);
        return "STORAGE NOT AVAILABLE";
    }

    if (access(path, F_OK) == 0) {
        unsigned char *existing = malloc(PKG_SIZE);
        int same = existing && read_file(path, existing) &&
                   memcmp(existing, p, PKG_SIZE) == 0;
        free(existing);
        free(p);

        return same ? "PATCH ALREADY VERIFIED" :
                      "PATCH FILE EXISTS MISMATCH";
    }

    int ok = save_new(path, p);
    free(p);

    if (!ok) return "PATCH SAVE FAILED";

    return hz == 60 ? "60 HZ FILE READY NO FLASH" :
                      "120 HZ FILE READY NO FLASH";
}

#include "flash.h"

static const char *engine_action(int selected) {
    if (selected != 0) pending_unknown_backup = 0;
    switch (selected) {
        case 0: return create_backup();
        case 1: return flash_boot(60);
        case 2: return flash_boot(120);
        case 3: return flash_boot(0);
        default: return "INVALID ACTION";
    }
}

#endif
