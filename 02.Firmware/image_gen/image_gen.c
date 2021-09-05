#include <stdint.h>
#include <stdbool.h>
#include <app_md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_HEADER_LEN              16
#define MD5_LEN                     16
#define FILE_NAME_MAX_LEN           512

bool is_file_exist(char *file_name, uint32_t *size)
{
    if (access(file_name, F_OK) == 0) 
    {
        struct stat st;
        stat(file_name, &st);
        *size = st.st_size;
        return true;
    } 
    else 
    {
        return false;
    }
}

bool make_full_path(char *input, char *output, uint32_t max_size)
{
    // Case 1 : File name ./
    char *ptr = output;
    bool retval = true;
    if (input[0] == '.' && input[1] == '/')
    {
        if (getcwd(output, max_size) != NULL) 
        {
            ptr += strlen(output);
            sprintf((char*)ptr, "%s", &input[1]);
        } 
        else 
        {
            perror("getcwd() error");
            retval = false;
            goto exit;
        }
    }
    else
    {
        sprintf((char*)ptr, "%s", input);
    }

    exit:
    return retval;
}


int main(int argc, char **argv)
{
    int retval = 0;
    int header_len;
    /* Path + header + fw version + hardware version + output_file name */
    if (argc == 2)
    {
        if (strstr(argv[1], "help"))
        {
            printf("Please use format [input_file_name.bin header firmware_version hardware_version output_file_name]\r\n", argc);
            printf("Example mcu.bin DTG01 0.0.1 0.0.2 release.bin\r\n");
            return 0;
        }
    }
    if (argc != 6)
    {
        printf("Invalid %u arguments, please use format [input_file_name.bin header firmware_version hardware_version output_file_name]\r\n", argc);
        printf("Example mcu.bin DTG01 0.0.1 0.0.2 release.bin\r\n");
        goto exit;
    }

    header_len = strlen(argv[2]) + strlen(argv[3]) + strlen(argv[4]);

    if (header_len > MAX_HEADER_LEN)
    {
        printf("Invalid header size %u bytes, max size allowed %u bytes\r\n", header_len, MAX_HEADER_LEN);
        goto exit;
    }

    printf("\r\n==============================\r\n");
    printf("\r\nHeader                   : %s, firmware version %s, hardware version %s\r\n", argv[2], argv[3], argv[4]);

    // Get full path of input file name
    char input_file_name[FILE_NAME_MAX_LEN];
    char output_file_name[FILE_NAME_MAX_LEN];

    if (!make_full_path(argv[1], input_file_name, FILE_NAME_MAX_LEN-strlen(argv[1])))
    {
        goto exit;
    }

    printf("Input file name          : %s\r\n", input_file_name);

    // Check file exist or not
    uint32_t input_file_size = 0;
    uint32_t output_file_size = 0;

    if (is_file_exist(input_file_name, &input_file_size)) 
    {
        // file exists
        int read_size = -1;
        FILE *f_in = NULL;
        FILE *f_out = NULL;
        uint8_t *file_input_data = malloc(input_file_size);
        uint8_t *file_output_data = malloc(input_file_size+MAX_HEADER_LEN+MD5_LEN);
        app_md5_ctx md5_cxt;
        uint8_t md5_result[MD5_LEN];
        uint32_t checksum_addr;

        if (file_input_data == NULL || file_output_data == NULL)
        {
            printf("Failed to allocate memory for file %s, size %d\r\n", input_file_name, input_file_size);
            goto free_resource;
        }

        // Open file and read all data
        f_in = fopen(input_file_name, "rb");
        if (f_in == NULL)
        {
            printf("Failed to open file\r\n");
            goto free_resource;
        }
        read_size = fread(file_input_data, 1, input_file_size, f_in);
        printf("Read                     : %u/%u bytes\r\n", read_size, input_file_size);
        output_file_size = input_file_size + MAX_HEADER_LEN + MD5_LEN;

        // Calculate md5
        app_md5_init(&md5_cxt);
        app_md5_update(&md5_cxt, (uint8_t *)file_input_data, input_file_size);
        app_md5_final(md5_result, &md5_cxt);
        
        printf("MD5                      : ");
        for (uint32_t i = 0; i < MD5_LEN; i++)
        {
            printf("%02X", md5_result[i]);
        }
        printf("\r\n");

        // Make header
        sprintf((char*)file_output_data, "%s%s%s", argv[2], argv[3], argv[4]);
        memcpy(file_output_data + MAX_HEADER_LEN, file_input_data, input_file_size);
        memcpy(file_output_data + MAX_HEADER_LEN + input_file_size, md5_result, MD5_LEN);

        // Make path for output
        make_full_path(argv[5], output_file_name, FILE_NAME_MAX_LEN-strlen(argv[5]));

        f_out = fopen(output_file_name, "w");
        if (f_out == NULL)
        {
            printf("Make file %s failed\r\n", output_file_name);
        }

        if (fwrite(file_output_data, 1, output_file_size, f_out) != output_file_size)
        {
            printf("File output write failed\r\n");
        }

        retval = 1;

        free_resource:
            // Release resource
            if (f_in)
                fclose(f_in);
            if (f_out)
                fclose(f_out);
            if (file_input_data)
                free(file_input_data);
            if (file_output_data)
                free(file_output_data);
    } 
    else 
    {
        printf("File not exist\r\n");
        goto exit;
    }

exit:
    if (retval != 0)
    {
        printf("\r\n==============================\r\n\r\nImage create success at  : %s\r\nSize                     : %u bytes\r\n\r\n==============================\r\n", 
                output_file_name, 
                output_file_size);
    }
    else
    {
        printf("Image create failure\r\n");
    }
    return retval;
}