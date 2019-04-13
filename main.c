#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <png.h>

void abort_(const char * s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

void* malloc_(size_t size)
{
    void *p = malloc(size);
    if (!p)
        abort_("[malloc] cannot allocate %zu bytes", size);
    return p;
}

typedef struct
{
    int width, height;
    png_byte color_type;
    png_byte bit_depth;
    
    png_bytep * row_pointers;
} PNGData;

void load_data(PNGData* png_data)
{
    for (int y = 0; y < png_data->height; ++y)
    {
        png_byte* row = png_data->row_pointers[y];
        for (int x = 0; x < png_data->width; ++x)
        {
            /*row[4*x] = x;
            row[4*x+1] = y;
            row[4*x+2] = 0;
            row[4*x+3] = 255;*/
            png_save_uint_16(&row[8*x], x*255);
            png_save_uint_16(&row[8*x+2], y*255);
            png_save_uint_16(&row[8*x+4], 0);
            png_save_uint_16(&row[8*x+6], 256*256-1);
        }
    }
}

void write_png_file(const char* in_file_name, const char* out_file_name, PNGData* png_data)
{
    FILE *in = fopen(in_file_name, "rb");
    if (!in)
        abort_("[write_png_file] File %s could not be opened for reading", in_file_name);
    double * arr = malloc_(sizeof(arr[0]) * png_data->width*png_data->height);
    int res = fread(arr, sizeof(arr[0]), png_data->width*png_data->height, in);
    if (res != png_data->width*png_data->height)
        abort_("[write_png_file] Can't read requested data");
    fclose(in);
    
    double min = arr[0], max = arr[0];
    for (size_t idx = 0; idx < png_data->width*png_data->height; ++idx)
    {
        min = min < arr[idx] ? min : arr[idx];
        max = max > arr[idx] ? max : arr[idx];
    }
    
    /* create file */
    FILE *fp = fopen(out_file_name, "wb");
    if (!fp)
        abort_("[write_png_file] File %s could not be opened for writing", out_file_name);
    
    /* initialize stuff */
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        abort_("[write_png_file] png_create_write_struct failed");
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        abort_("[write_png_file] png_create_info_struct failed");
    if (setjmp(png_jmpbuf(png_ptr)))
        abort_("[write_png_file] Error during init_io");
    png_init_io(png_ptr, fp);
    
    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
        abort_("[write_png_file] Error during writing header");
    
    png_set_IHDR(png_ptr, info_ptr, png_data->width, png_data->height,
                 png_data->bit_depth, png_data->color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    
    /* allocate buffer and fill it */
    png_data->row_pointers = (png_bytep*) malloc_(sizeof(png_bytep) * png_data->height);
    for (int y = 0; y < png_data->height; ++y)
        png_data->row_pointers[y] = (png_byte*) malloc_(png_get_rowbytes(png_ptr, info_ptr));
    for (int y = 0; y < png_data->height; ++y)
    {
        png_byte* row = png_data->row_pointers[y];
        for (int x = 0; x < png_data->width; ++x)
        {
            unsigned int val = (unsigned int)(((arr[y*png_data->width+x] - min) / (max - min)) * 65535);
            png_save_uint_16(&row[8*x], val);
            png_save_uint_16(&row[8*x+2], val);
            png_save_uint_16(&row[8*x+4], val);
            png_save_uint_16(&row[8*x+6], 256*256-1);
        }
    }
    
    /* write bytes */
    if (setjmp(png_jmpbuf(png_ptr)))
        abort_("[write_png_file] Error during writing bytes");
    png_write_image(png_ptr, png_data->row_pointers);
    
    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
        abort_("[write_png_file] Error during end of write");
    png_write_end(png_ptr, NULL);
    
    /* cleanup heap allocation */
    for (int y = 0; y < png_data->height; ++y)
        free(png_data->row_pointers[y]);
    free(png_data->row_pointers);
    
    free(arr);
    fclose(fp);
}

int main(int argc, char **argv)
{
    if (argc != 3)
        abort_("Usage: program_name <binary file in> <png file out>");
    
    PNGData png_data;
    png_data.width = 3840;
    png_data.height = 3840;
    png_data.color_type = PNG_COLOR_TYPE_RGBA;
    png_data.bit_depth = 16;
    
    write_png_file(argv[1], argv[2], &png_data);
    
    return 0;
}