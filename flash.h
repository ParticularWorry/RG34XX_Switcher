#ifndef RG34XX_FLASH_H
#define RG34XX_FLASH_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FLASH_DEVICE "/dev/mmcblk0"

/*
 * Known package:
 * Original SHA256:
 * a124aec8478e8d0e7f7fe9590a0d96b879e54d21c6c6d96989f94d8b8ac5567c
 *
 * 60 Hz SHA256:
 * bf4d9d800281a49f30a307880b547d2499c9d691c0996d9010b05b0b2fb41271
 *
 * 120 Hz is reconstructed from the verified 60 Hz package.
 */

static void flash_make_60(unsigned char *p) {
    p[0x16] = 0x3a;
    p[0x17] = 0xa2;

    p[0x12b5b7] = 0x36;

    p[0x12b626] = 0x06;
    p[0x12b627] = 0x8f;
}

static void flash_make_120(unsigned char *p) {
    p[0x16] = 0x37;
    p[0x17] = 0x5b;

    p[0x12b5b7] = 0x36;

    p[0x12b626] = 0x03;
    p[0x12b627] = 0x48;
}

static void flash_make_original(unsigned char *p) {
    p[0x16] = 0x37;
    p[0x17] = 0x40;

    p[0x12b5b7] = 0x1b;

    p[0x12b626] = 0x03;
    p[0x12b627] = 0x48;
}

static int flash_device_check(void) {
    FILE *f = fopen("/proc/cmdline", "r");
    if (!f)
        return 0;

    char cmdline[4096] = {0};
    int ok = fgets(cmdline, sizeof(cmdline), f) != NULL;
    fclose(f);

    if (!ok)
        return 0;

    return strstr(cmdline, "root=/dev/mmcblk0p5") != NULL &&
           strstr(cmdline,
                  "androidboot.hardware=sun50iw9p1") != NULL;
}

static int flash_write_exact(
    int fd,
    off_t offset,
    const unsigned char *data,
    size_t size
) {
    size_t done = 0;

    while (done < size) {
        ssize_t n = pwrite(
            fd,
            data + done,
            size - done,
            offset + (off_t)done
        );

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return 0;

        done += (size_t)n;
    }

    return 1;
}

/*
 * Convert a recognized original, 60 Hz or experimental
 * 120 Hz package into the exact known 60 Hz package.
 *
 * This prevents flashing over an unknown boot package.
 */
static int flash_get_known_60(
    const unsigned char *current,
    unsigned char *canonical,
    unsigned char *scratch
) {
    memcpy(canonical, current, PKG_SIZE);

    if (sha_matches(current, PKG_SIZE, PATCH60_SHA))
        return 1;

    if (valid_original(current)) {
        flash_make_60(canonical);

        return sha_matches(
            canonical, PKG_SIZE, PATCH60_SHA
        );
    }

    /*
     * Recognize our experimental 120 Hz package by
     * converting it back to the known 60 Hz package.
     */
    flash_make_60(canonical);

    if (!sha_matches(canonical, PKG_SIZE, PATCH60_SHA))
        return 0;

    memcpy(scratch, canonical, PKG_SIZE);
    flash_make_120(scratch);

    return memcmp(scratch, current, PKG_SIZE) == 0;
}

static int flash_verify_direct(
    int verify_fd,
    unsigned char *aligned_buffer,
    const unsigned char *expected
) {
    if (!read_exact(
            verify_fd,
            BOOT_OFFSET,
            aligned_buffer,
            PKG_SIZE
        ))
        return 0;

    return memcmp(
        aligned_buffer,
        expected,
        PKG_SIZE
    ) == 0;
}


/* A saved backup disables the backup menu entry. */
static const char *flash_backup_file(void) {
    struct stat st;

    if (lstat(ORIGINAL, &st) == 0 &&
        S_ISREG(st.st_mode) &&
        st.st_size == PKG_SIZE)
        return ORIGINAL;

    if (lstat(UNKNOWN_BACKUP, &st) == 0 &&
        S_ISREG(st.st_mode) &&
        st.st_size == PKG_SIZE)
        return UNKNOWN_BACKUP;

    return NULL;
}

static int flash_backup_exists(void) {
    return flash_backup_file() != NULL;
}

static const char *flash_boot(int mode) {
    if (mode != 0 && mode != 60 && mode != 120)
        return "INVALID FLASH MODE";

    if (!flash_device_check())
        return "UNSUPPORTED BOOT DEVICE";

    unsigned char *current = malloc(PKG_SIZE);
    unsigned char *canonical = malloc(PKG_SIZE);
    unsigned char *target = malloc(PKG_SIZE);
    unsigned char *scratch = malloc(PKG_SIZE);
    void *aligned = NULL;

    if (!current || !canonical || !target || !scratch ||
        posix_memalign(&aligned, 4096, PKG_SIZE) != 0) {
        free(current);
        free(canonical);
        free(target);
        free(scratch);
        free(aligned);
        return "OUT OF MEMORY";
    }

    const char *result = "FLASH FAILED";

    int fd = -1;
    int verify_fd = -1;

    fd = open(
        FLASH_DEVICE,
        O_RDWR | O_SYNC | O_CLOEXEC | O_NOFOLLOW
    );

    if (fd < 0) {
        result = "CANNOT OPEN BOOT DEVICE";
        goto cleanup;
    }

    struct stat st;

    if (fstat(fd, &st) != 0 || !S_ISBLK(st.st_mode)) {
        result = "INVALID BLOCK DEVICE";
        goto cleanup;
    }

    if (!read_exact(fd, BOOT_OFFSET, current, PKG_SIZE)) {
        result = "BOOT READ FAILED";
        goto cleanup;
    }

    if (!flash_get_known_60(
            current, canonical, scratch
        )) {
        result = "UNKNOWN BOOT PACKAGE";
        goto cleanup;
    }

    memcpy(target, canonical, PKG_SIZE);

    if (mode == 60) {
        flash_make_60(target);

        if (!sha_matches(
                target, PKG_SIZE, PATCH60_SHA
            )) {
            result = "60 HZ HASH MISMATCH";
            goto cleanup;
        }
    } else if (mode == 120) {
        flash_make_120(target);

        memcpy(scratch, target, PKG_SIZE);
        flash_make_60(scratch);

        if (!sha_matches(
                scratch, PKG_SIZE, PATCH60_SHA
            )) {
            result = "120 HZ VALIDATION FAILED";
            goto cleanup;
        }
    } else {
        const char *backup = flash_backup_file();

        if (!backup || !read_file(backup, target)) {
            result = "BACKUP MISSING OR INVALID";
            goto cleanup;
        }

        /*
         * Restore only a recognized, complete boot package.
         * An unknown image must never be flashed blindly.
         */
        if (!flash_get_known_60(target, scratch, canonical)) {
            result = "UNKNOWN BACKUP REFUSED";
            goto cleanup;
        }
    }

    if (memcmp(current, target, PKG_SIZE) == 0) {
        result = "MODE ALREADY ACTIVE";
        goto cleanup;
    }

    /*
     * Require direct disk reads for verification.
     * Test BEFORE writing anything.
     */
    verify_fd = open(
        FLASH_DEVICE,
        O_RDONLY | O_DIRECT | O_CLOEXEC | O_NOFOLLOW
    );

    if (verify_fd < 0) {
        result = "DIRECT VERIFY UNAVAILABLE";
        goto cleanup;
    }

    if (!flash_verify_direct(
            verify_fd,
            aligned,
            current
        )) {
        result = "BOOT PREFLIGHT FAILED";
        goto cleanup;
    }

    /*
     * Re-read immediately before writing.
     * Refuse to continue if the boot area changed.
     */
    if (!read_exact(
            fd,
            BOOT_OFFSET,
            scratch,
            PKG_SIZE
        ) ||
        memcmp(scratch, current, PKG_SIZE) != 0) {
        result = "BOOT CHANGED ABORTED";
        goto cleanup;
    }

    /*
     * Actual flash.
     */
    int write_ok = flash_write_exact(
        fd,
        BOOT_OFFSET,
        target,
        PKG_SIZE
    );

    if (write_ok)
        write_ok = fsync(fd) == 0;

    if (write_ok) {
        sync();

        if (flash_verify_direct(
                verify_fd,
                aligned,
                target
            )) {
            result = "FLASH VERIFIED REBOOT REQUIRED";
            goto cleanup;
        }
    }

    /*
     * Best-effort rollback using the previous package
     * retained in RAM. This cannot protect against
     * power loss during writing.
     */
    if (flash_write_exact(
            fd,
            BOOT_OFFSET,
            current,
            PKG_SIZE
        ) &&
        fsync(fd) == 0) {
        sync();

        result = flash_verify_direct(
            verify_fd,
            aligned,
            current
        )
            ? "FLASH FAILED ROLLBACK OK"
            : "CRITICAL VERIFY FAILURE";
    } else {
        result = "CRITICAL ROLLBACK FAILURE";
    }

cleanup:
    if (verify_fd >= 0)
        close(verify_fd);

    if (fd >= 0)
        close(fd);

    free(current);
    free(canonical);
    free(target);
    free(scratch);
    free(aligned);

    return result;
}


/* Read the configured mode from the actual boot area. */
static const char *flash_current_mode(void) {
    if (!flash_device_check())
        return "CURRENT MODE: UNSUPPORTED";

    unsigned char *p = malloc(PKG_SIZE);
    if (!p)
        return "CURRENT MODE: READ ERROR";

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

    if (!ok) {
        free(p);
        return "CURRENT MODE: READ ERROR";
    }

    const char *mode = "CURRENT MODE: UNKNOWN";

    if (sha_matches(p, PKG_SIZE, ORIGINAL_SHA)) {
        mode = "CURRENT MODE: ORIGINAL";
    } else if (sha_matches(p, PKG_SIZE, PATCH60_SHA)) {
        mode = "CURRENT MODE: 60 HZ";
    } else if (
        p[0x16] == 0x37 &&
        p[0x17] == 0x5b &&
        p[0x12b5b7] == 0x36 &&
        p[0x12b626] == 0x03 &&
        p[0x12b627] == 0x48
    ) {
        /*
         * Recognize our experimental 120 Hz package
         * by converting it back to the verified 60 Hz
         * image and checking its complete SHA-256.
         */
        flash_make_60(p);

        if (sha_matches(p, PKG_SIZE, PATCH60_SHA))
            mode = "CURRENT MODE: 120 HZ";
    }

    free(p);
    return mode;
}

#endif
