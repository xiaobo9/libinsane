#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libinsane/util.h>

#include "../src/bmp.h"
#include "../src/endianess.h"

#define ANSI_RED "\033[38;91m"
#define ANSI_YELLOW "\033[38;93m"
#define ANSI_GREEN "\033[38;92m"
#define ANSI_RST "\033[0m"


union bmp
{
    struct bmp_header *header;
    unsigned char *whole;
};


static int print_hex(int offset, void *_data, size_t nb_bytes)
{
    unsigned char *data = _data;
    size_t i;

    if (offset >= 0) {
        printf("0x%08X | ", offset);
    } else {
        printf("           | ");
    }

    for (i = 0 ; i < nb_bytes ; i++) {
        printf("%02X ", data[i]);
    }
    if (nb_bytes <= 4) {
        for ( ; i < 4 ; i++) {
            printf("   ");
        }
        printf(" | ");
    } else {
        printf("\n");
    }

    return offset + nb_bytes;
}


static void *load_file(const char *filepath, unsigned int *file_size)
{
    struct stat st;
    FILE *fp;
    void *content;
    ssize_t r = 0, total = 0;

    if (stat(filepath, &st) < 0) {
        perror("stat()");
        return NULL;
    }
    *file_size = st.st_size;

    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("fopen()");
        return NULL;
    }

    content = malloc(st.st_size);
    if (content == NULL) {
        perror("malloc()");
        fclose(fp);
        return NULL;
    }

    while (total < st.st_size) {
        r = fread(content + total, sizeof(char), st.st_size - total, fp);
        if (r <= 0) {
            perror("fread()");
            fclose(fp);
            free(content);
            return NULL;
        }
        total += r;
    }

    fclose(fp);

    return content;
}


static int dump_bmp_header(union bmp bmp, size_t file_size)
{
    int offset = 0;
    uint32_t expected_pixels = 0;
    uint32_t expected_pixels2 = 0;

    offset = print_hex(offset, &bmp.header->magic, sizeof(bmp.header->magic));
    if (le16toh(bmp.header->magic) == 0x4D42) {
        printf("%sValid magic%s\n", ANSI_GREEN, ANSI_RST);
    } else {
        printf("%sINVALID MAGIC%s\n", ANSI_RED, ANSI_RST);
    }

    offset = print_hex(offset, &bmp.header->file_size, sizeof(bmp.header->file_size));
    if (le32toh(bmp.header->file_size) == file_size) {
        printf(
            "File size: %s%u%s\n",
            ANSI_GREEN, le32toh(bmp.header->file_size), ANSI_RST
        );
    } else {
        printf(
            "INVALID File size: %s%u%s instead of %u\n",
            ANSI_RED, le32toh(bmp.header->file_size), ANSI_RST,
            file_size
        );
    }

    offset = print_hex(offset, &bmp.header->unused, sizeof(bmp.header->unused));
    if (bmp.header->unused == 0x0) {
        printf("Unused\n");
    } else {
        printf("%sINVALID Unused (!= 0)%s\n", ANSI_YELLOW, ANSI_RST);
    }

    offset = print_hex(offset, &bmp.header->offset_to_data, sizeof(bmp.header->offset_to_data));
    if (le32toh(bmp.header->offset_to_data) < file_size
            && le32toh(bmp.header->offset_to_data) >= BMP_HEADER_SIZE) {
        printf(
            "Offset to data: 0x%X (%d B)\n",
            le32toh(bmp.header->offset_to_data),
            le32toh(bmp.header->offset_to_data)
        );
    } else {
        printf(
            "INVALID Offset to data: %s0x%X%s (%d B)\n",
            ANSI_RED, le32toh(bmp.header->offset_to_data), ANSI_RST,
            le32toh(bmp.header->offset_to_data)
        );
    }
    expected_pixels = file_size - le32toh(bmp.header->offset_to_data);
    print_hex(-1, NULL, 0);
    printf(
        "Pixel data size based on file size and offset_to_data:"
        " %u B (%u KB)\n",
        expected_pixels, expected_pixels / 1024
    );

    offset = print_hex(offset, &bmp.header->remaining_header, sizeof(bmp.header->remaining_header));
    if (le32toh(bmp.header->remaining_header) == BMP_DIB_HEADER_SIZE) {
        printf(
            "Remaining header: %s%u%s\n",
            ANSI_GREEN, le32toh(bmp.header->remaining_header), ANSI_RST
        );
    } else {
        printf(
            "INVALID Remaining header: %s%u%s\n",
            ANSI_RED, le32toh(bmp.header->remaining_header), ANSI_RST
        );
    }

    offset = print_hex(offset, &bmp.header->width, sizeof(bmp.header->width));
    printf("Width: %u px\n", le32toh(bmp.header->width));
    offset = print_hex(offset, &bmp.header->height, sizeof(bmp.header->height));
    printf("Height: %u px\n", le32toh(bmp.header->height));

    offset = print_hex(offset, &bmp.header->nb_color_planes, sizeof(bmp.header->nb_color_planes));
    printf("Number of color planes: %u\n", le16toh(bmp.header->nb_color_planes));

    offset = print_hex(offset, &bmp.header->nb_bits_per_pixel, sizeof(bmp.header->nb_bits_per_pixel));
    printf("Number of bits per pixel: %u\n", le32toh(bmp.header->nb_bits_per_pixel));

    offset = print_hex(offset, &bmp.header->compression, sizeof(bmp.header->compression));
    if (le32toh(bmp.header->compression) == 0) {
        printf("Compression: %snone%s\n", ANSI_GREEN, ANSI_RST);
    } else {
        printf(
            "UNEXPECTED Compression: %s0x%X%s\n",
            ANSI_RED, le32toh(bmp.header->compression), ANSI_RST
        );
    }


    // compute expected line lengths
    expected_pixels2 = le32toh(bmp.header->width) * le16toh(bmp.header->nb_bits_per_pixel) / 8;
    if (bmp.header->nb_bits_per_pixel % 8 != 0) {
        expected_pixels2 += 1;
    }
    if (expected_pixels2 % 4 != 0) {
        expected_pixels2 += (4 - (expected_pixels2 % 4));
    }
    print_hex(-1, NULL, 0);
    printf(
        "Expected line length: %u px * %u bits => %u B (%u KB)\n",
        le32toh(bmp.header->width), le16toh(bmp.header->nb_bits_per_pixel),
        expected_pixels2, expected_pixels2 / 1024
    );

    // computer expected pixel data size
    print_hex(-1, NULL, 0);
    printf(
        "Pixel data size based on image size:"
        " %u B * %u lines => %u B (%u KB)\n",
        expected_pixels2, le32toh(bmp.header->height),
        expected_pixels2 * le32toh(bmp.header->height),
        expected_pixels2 * le32toh(bmp.header->height) / 1024
    );
    expected_pixels2 *= le32toh(bmp.header->height);

    print_hex(-1, NULL, 0);
    if (expected_pixels == expected_pixels2) {
        printf(
            "offset_to_data and image size do %smatch%s (pixels = %s%d%s B)\n",
            ANSI_GREEN, ANSI_RST,
            ANSI_GREEN, expected_pixels, ANSI_RST
        );
    } else {
        printf(
            "%sMISMATCH%s between"
            " offset_to_data (%s%d%s B)"
            " and image size (%s%d%s B) !\n",
            ANSI_RED, ANSI_RST,
            ANSI_RED, expected_pixels, ANSI_RST,
            ANSI_RED, expected_pixels2, ANSI_RST
        );
    }

    offset = print_hex(offset, &bmp.header->pixel_data_size, sizeof(bmp.header->pixel_data_size));
    if (le32toh(bmp.header->pixel_data_size) == 0) {
        printf(
            "Pixel data size: %s%u%s B (%u KB)\n",
            ANSI_YELLOW, le32toh(bmp.header->pixel_data_size), ANSI_RST,
            le32toh(bmp.header->pixel_data_size) / 1024
        );
    } else if (le32toh(bmp.header->pixel_data_size) == expected_pixels) {
        printf(
            "Pixel data size: %s%u%s B (%u KB)\n",
            ANSI_GREEN, le32toh(bmp.header->pixel_data_size), ANSI_RST,
            le32toh(bmp.header->pixel_data_size) / 1024
        );
    } else {
        printf(
            "Pixel data size: %s%u%s B (%u KB) != %s%u%s B (%u KB)\n",
            ANSI_RED, le32toh(bmp.header->pixel_data_size), ANSI_RST,
            le32toh(bmp.header->pixel_data_size) / 1024,
            ANSI_RED, expected_pixels, ANSI_RST, expected_pixels / 1024
        );
    }

    offset = print_hex(
        offset, &bmp.header->horizontal_resolution, sizeof(bmp.header->horizontal_resolution)
    );
    printf(
        "Horizontal resolution: %u pixels per meter\n",
        le32toh(bmp.header->horizontal_resolution)
    );

    offset = print_hex(
        offset, &bmp.header->vertical_resolution, sizeof(bmp.header->vertical_resolution)
    );
    printf(
        "Vertical resolution: %u pixels per meter\n",
        le32toh(bmp.header->vertical_resolution)
    );

    offset = print_hex(offset, &bmp.header->nb_colors_in_palette, sizeof(bmp.header->nb_colors_in_palette));
    printf("Number of colors in palette: %u\n", le32toh(bmp.header->nb_colors_in_palette));
    expected_pixels = le32toh(bmp.header->nb_colors_in_palette) * 4;
    expected_pixels2 = le32toh(bmp.header->offset_to_data) - BMP_HEADER_SIZE;
    print_hex(-1, NULL, 0);
    if (expected_pixels == expected_pixels2) {
        printf(
            "Expected palette size (%s%u%s B)"
            " and the space left for the palette (%s%u%s B) do %smatch%s\n",
            ANSI_GREEN, expected_pixels, ANSI_RST,
            ANSI_GREEN, expected_pixels2, ANSI_RST,
            ANSI_GREEN, ANSI_RST
        );
    } else {
        printf(
            "%sMISMATCH%s between"
            " the expected palette size (%s%u%s B)"
            " and the space left for the palette (%s%u%s B)\n",
            ANSI_RED, ANSI_RST,
            ANSI_RED, expected_pixels, ANSI_RST,
            ANSI_RED, expected_pixels2, ANSI_RST
        );
    }

    offset = print_hex(offset, &bmp.header->important_colors, sizeof(bmp.header->important_colors));
    printf("Important colors: %u\n", le32toh(bmp.header->important_colors));

    return offset;
}


int main(int argc, char **argv)
{
    union bmp bmp;
    unsigned int file_size;
    int offset;
    int remaining;

    if (argc != 2
            || strcasecmp(argv[1], "-h") == 0
            || strcasecmp(argv[1], "--help") == 0) {
        printf("Usage:\n");
        printf("\t%s <bmp file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    bmp.whole = load_file(argv[1], &file_size);
    if (bmp.whole == NULL) {
        printf("Failed to load file\n");
        return EXIT_FAILURE;
    }

    printf("------------ HEADER:\n");
    offset = dump_bmp_header(bmp, file_size);

    remaining = le32toh(bmp.header->offset_to_data) - offset;
    printf(
        "------------ REMAINING HEADER: %u - %u = %u B (%u KB)\n",
        le32toh(bmp.header->offset_to_data), offset,
        remaining, remaining / 1024
    );
    while (remaining > 0) {
        offset = print_hex(offset, bmp.whole + offset, MIN(remaining, 16));
        remaining -= MIN(remaining, 16);
    }

    printf(
        "------------ PIXELS: %u B (%u KB)\n",
        file_size - offset,
        (file_size - offset) / 1024
    );

    free(bmp.whole);
    return EXIT_SUCCESS;
}
