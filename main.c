#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

struct firmware_info {
    int32_t checksum;
    int32_t calculated_checksum;
};

//#define PRINT_OFFSETS
//#define PRINT_CHKSUM_PARTS
//#define PRINT_INTERMEDIATE_CHKSUM

char fileBuffer[0x4000];

int findSystemFile(const char* fileName, const char* buffer){
    int found;
    int offset;

    for(offset=0; offset != 0x20; offset++){
        found = strcasecmp(fileName,buffer);
        //printf("Offset: %d, Found: %d\n", offset, found);
        buffer = buffer + 0x20;
        if(found == 0) { 
            return offset << 5;
        }
    }

    return -1;
}

/*
Replicates the vfs_read function w/ fseek + fread :)
-> Not really needed, thats bogus anyways.
*/
int vfs_read(FILE *file, char *buffer, int count, int offset){
    fseek(file, offset, SEEK_SET);
    fread(buffer, 1, count, file);
}

int32_t cal_checksum(char *buffer, int32_t param_2){
    int32_t idx = 0;
    int32_t checksum = 0;

    //divide length by 5 bc. we are addressing 32 bit ints :)
    int32_t len = param_2 >> 2;

    int32_t* int_buffer = (int32_t*) buffer;
    for(idx=0; idx<len; idx++){
        checksum = checksum + *int_buffer;
        int_buffer++;
    }

    #ifdef PRINT_CHKSUM_PARTS
    printf("0x%08x\n", checksum);
    #endif
    return checksum;
}

struct firmware_info checksumInfo(FILE* f_ptr){
    fseek(f_ptr, 0, SEEK_SET);
    vfs_read(f_ptr, fileBuffer, 0x400, 0x00);

    int checksumOffset = findSystemFile("CHECKSUM", fileBuffer);
    printf("Checksum offset: 0x%x\n", checksumOffset);

    struct firmware_info info = {-1, -1};
    
    if (0 < checksumOffset) {
        printf("Checking Checksum. \n");

        char* Sys_Flag = fileBuffer + checksumOffset;
        uint32_t Sys_offset = *((uint32_t*) (Sys_Flag+0x10));
        uint32_t Sys_len = *((uint32_t*) (Sys_Flag+0x14));
        uint32_t Sys_para1 = *((uint32_t*) (Sys_Flag+0x18));
        uint32_t Sys_para2 = *((uint32_t*) (Sys_Flag+0x1c));
        
        printf("<INFO>: Sys_Flag:%s,\n",Sys_Flag);
        printf("<INFO>: Sys_offset:0x%x,\n",Sys_offset);
        printf("<INFO>: Sys_len:0x%x, %d KB, %d MB\n", Sys_len, Sys_len>>10, Sys_len>>20);
        printf("<INFO>: Sys_para1:0x%x,\n", Sys_para1);
        printf("<INFO>: Sys_para2:0x%x,\n", Sys_para2);

        //MAX sys-length is: 2^32 -> 4096 MB -> 4GB

        int32_t checksum = (int32_t) Sys_para2;
        printf("<INFO>: Checksum: %d,\n", Sys_para2);

        int32_t calculated_checksum = 0;

        // uVar2 = (uint)(Sys_Flag + 0x14) & 3;
        // checksumOffset = _filp;
        // retVar = *(undefined4 *)(Sys_Flag + 0x10);

        // This sets something in the file-pointer, what is the question.
        // *(undefined4 *)(_filp + 0x24) = 0;
        // *(undefined4 *)(checksumOffset + 0x20) = retVar;

        int32_t unaff_s3 = 0;

        // highest 3 bits of the sys_length
        uint32_t uVar2 = *(uint32_t*)(Sys_Flag + 0x14) & 3;

        //printf("uVar2: %u\n", uVar2);

        // Sys_Flag + 0x17 -> last byte of the Sys_len -> LE System -> ?
        /*
        uint32_t iterator = (*(uint32_t*)(Sys_Flag + 0x17)) & 3;
        iterator =
        (*(int32_t *)(Sys_Flag + 0x17 -iterator) << (3 - iterator) * 8 |    // encodes h
        //     
        unaff_s3 & 0xffffffffU >> (iterator + 1) * 8) & -1 << (4 - uVar2) * 8 |
        *(int32_t *)(Sys_Flag + 0x14 + - uVar2) >> uVar2 * 8;
        */

        int32_t iterator = Sys_len;

        //printf("iterator: %u | %d\n", iterator, iterator);
        
        // vfs_llseek(_filp,vfs_llseek,*(undefined4 *)(Sys_Flag + 0x10),0,0);
        fseek(f_ptr,Sys_offset, SEEK_SET);
        if(Sys_len > 0){
            calculated_checksum = 0;
            uint32_t read_length = 0x4000;
            do {
                if ((int)iterator < (int)uVar2) {
                    read_length = iterator;
                }
                int offset = ftell(f_ptr);
                fread(fileBuffer, 1, read_length, f_ptr);

                #ifdef PRINT_OFFSETS
                printf("offset: 0x%x\n", offset);
                #endif
                //vfs_read(f_ptr, fileBuffer, read_length,0x00);
                iterator = iterator - read_length;
                calculated_checksum = calculated_checksum + cal_checksum(fileBuffer, read_length);
                #ifdef PRINT_INTERMEDIATE_CHKSUM
                printf("0x%08x\n", calculated_checksum);
                #endif
            } while ((int)iterator > 0);
        }
        info.checksum = checksum;
        info.calculated_checksum = calculated_checksum;
        return info;
    }
    return info;
}

void writeChecksum(FILE* f_ptr, int32_t checksum){
    vfs_read(f_ptr, fileBuffer, 0x400, 0x00);
    
    printf("Locating Checksum structure for writing...");
    int checksumOffset = findSystemFile("CHECKSUM", fileBuffer);
    
    if (0 < checksumOffset) {
        char* Sys_Flag = fileBuffer + checksumOffset;
        int32_t old_checksum = *((uint32_t*) (Sys_Flag+0x1c));
        fseek(f_ptr, checksumOffset +0x1C, SEEK_SET);
        fwrite(&checksum, 4, 1, f_ptr);

        printf("Updated Checksum  0x%08x to 0x%08x", old_checksum, checksum);
    }
    printf("Could not locate checksum struct...");
}

int getFileSize(FILE* f_ptr){
    fseek(f_ptr, 0L, SEEK_END);
    int size = ftell(f_ptr);
    fseek(f_ptr, 0, SEEK_SET);
    return size;
}

struct firmware_info checksumInfoBuffer(char* buffer, int file_length){
    printf("Locating Checksum...\n");
    int checksumOffset = findSystemFile("CHECKSUM", buffer);
    printf("Checksum offset: 0x%x\n", checksumOffset);
    struct firmware_info info = {-1, -1};
    
    if (0 < checksumOffset) {
        char* Sys_Flag = buffer + checksumOffset;
        uint32_t Sys_offset = *((uint32_t*) (Sys_Flag+0x10));
        uint32_t Sys_len = *((uint32_t*) (Sys_Flag+0x14));
        uint32_t Sys_para1 = *((uint32_t*) (Sys_Flag+0x18));
        uint32_t Sys_para2 = *((uint32_t*) (Sys_Flag+0x1c));
        
        printf("<INFO>: Sys_Flag:%s,\n",Sys_Flag);
        printf("<INFO>: Sys_offset:0x%x,\n",Sys_offset);
        printf("<INFO>: Sys_len:0x%x, %d KB, %d MB\n", Sys_len, Sys_len>>10, Sys_len>>20);
        printf("<INFO>: Sys_para1:0x%x,\n", Sys_para1);
        printf("<INFO>: Sys_para2:0x%x,\n", Sys_para2);

        //MAX sys-length is: 2^32 -> 4096 MB -> 4GB

        int32_t checksum = (int32_t) Sys_para2;
        printf("<INFO>: Checksum: %d,\n", Sys_para2);

        int32_t calculated_checksum = 0;

        // uVar2 = (uint)(Sys_Flag + 0x14) & 3;
        // checksumOffset = _filp;
        // retVar = *(undefined4 *)(Sys_Flag + 0x10);

        // This sets something in the file-pointer, what is the question.
        // *(undefined4 *)(_filp + 0x24) = 0;
        // *(undefined4 *)(checksumOffset + 0x20) = retVar;

        int32_t unaff_s3 = 0;

        // highest 3 bits of the sys_length
        uint32_t uVar2 = *(uint32_t*)(Sys_Flag + 0x14) & 3;

        //printf("uVar2: %u\n", uVar2);

        int32_t iterator = Sys_len;
        printf("iterator: 0x%x\n", iterator);

        //fseek(f_ptr,Sys_offset, SEEK_SET);
        char * fbuf = buffer + Sys_offset;

        if(Sys_len > 0){
            calculated_checksum = 0;
            uint32_t read_length = 0x4000;
            do {
                if ((int)iterator < (int)uVar2) {
                    read_length = iterator;
                }
                //fread(fileBuffer, 1, read_length, f_ptr);
                int offset = fbuf - buffer;
                #ifdef PRINT_OFFSETS
                
                printf("offset: 0x%x\n", offset);
                #endif
                //vfs_read(f_ptr, fileBuffer, read_length,0x00);
                iterator = iterator - read_length;

                read_length = MIN(read_length, file_length-offset);
                calculated_checksum = calculated_checksum + cal_checksum(fbuf, read_length);

                #ifdef PRINT_INTERMEDIATE_CHKSUM
                printf("0x%08x\n", calculated_checksum);
                #endif
                fbuf += read_length;
            } while ((int)iterator > 1);
        }
        info.checksum = checksum;
        info.calculated_checksum = calculated_checksum;
        return info;
    }
    return info;
}

void writeChecksumIntoBuffer(char* buffer, int32_t checksum){    
    printf("Locating Checksum structure for writing...\n");
    int checksumOffset = findSystemFile("CHECKSUM", buffer);
    
    if (0 < checksumOffset) {
        char* Sys_Flag = buffer + checksumOffset;
        int32_t old_checksum = *((uint32_t*) (Sys_Flag+0x1c));
        *((int32_t*) (buffer + checksumOffset + 0x1C)) = checksum;
        printf("Updated Checksum  0x%08x to 0x%08x\n", old_checksum, checksum);
        return;
    }
    printf("Could not locate checksum struct...\n");
}

void writeFileSizeIntoBuffer(char* buffer, int32_t sys_len){    
    printf("Locating Checksum structure for writing...\n");
    int checksumOffset = findSystemFile("CHECKSUM", buffer);
    
    if (0 < checksumOffset) {
        char* Sys_Flag = buffer + checksumOffset;
        int32_t old_sys_len = *((uint32_t*) (Sys_Flag+0x1c));
        *((int32_t*) (buffer + checksumOffset + 0x14)) = sys_len;
        printf("Updated sys_len  0x%08x to 0x%08x\n", old_sys_len, sys_len);
        return;
    }
    printf("Could not locate checksum struct...\n");
}

int main(){
    FILE *original_binary_ptr, *modified_rfs_ptr, *new_binary_ptr;
    original_binary_ptr = fopen("../DOW_PX.bin", "rb");
    modified_rfs_ptr = fopen("../A28400.ext", "rb");

    new_binary_ptr = fopen("../DOW_PX_new.bin", "wb");


    if(!original_binary_ptr || !modified_rfs_ptr || !new_binary_ptr){
        printf("Unable to open Update Binary or rootFS");
        return 1;
    }

    int update_bin_size = getFileSize(original_binary_ptr);
    printf("UpdateBinary Size: %d bytes, %d kb\n", update_bin_size, update_bin_size>>10);

    int rootfs_size = getFileSize(modified_rfs_ptr);
    printf("RootFS Size: %d bytes, %d kb\n", rootfs_size, rootfs_size>>10);

    const int firmwareOffset = 0xA28400;
    int size_difference = rootfs_size - (update_bin_size - firmwareOffset);
    int new_binary_size = update_bin_size + size_difference;

    printf("New rootfs is %d bytes larger than the original.\n", size_difference);

    // Create the file-buffer we will be working in:
    int buffer_size = new_binary_size;
    char* buffer = (char*) malloc(sizeof(char) * buffer_size);
    memset(buffer, 0, buffer_size);


    // Read the original binary into the buffer
    fseek(original_binary_ptr, 0, SEEK_SET);
    fread(buffer, 1, update_bin_size, original_binary_ptr);
    // copy the modified rootfs to it's place.
    fseek(modified_rfs_ptr, 0, SEEK_SET);
    fread(buffer+firmwareOffset, 1, rootfs_size, modified_rfs_ptr);

    writeFileSizeIntoBuffer(buffer, new_binary_size - 0x40);

    struct firmware_info info = checksumInfoBuffer(buffer, buffer_size);
    printf("Calulated checksum: 0x%08x | 0x%08x \n", info.calculated_checksum, info.checksum);

    
    writeChecksumIntoBuffer(buffer, info.calculated_checksum);
    

    //Read the FW-Info once again, to make sure:
    info = checksumInfoBuffer(buffer, buffer_size);
    printf("Calulated checksum: 0x%08x | 0x%08x \n", info.calculated_checksum, info.checksum);
    
    //write the new, modified binary :)
    fwrite(buffer, new_binary_size, 1, new_binary_ptr);

    free(buffer);
}

/*
undefined4 Firm_ChechsumFlow(void)

{
  char *pcVar1;
  int checksumOffset;
  uint systemTime_ms;
  int checksum;
  undefined4 retVar;
  char *Sys_Flag;
  uint uVar2;
  uint unaff_s3;
  int calculated_checksum;
  
  _Get_TimeOut('\0');
  printk("<INFO>: Start Time %x\n",temp_buf_cached);

  vfs_read(_filp,temp_buf_cached,0x400,_filp + 0x20);
  checksumOffset = FindSysFile("CHECKSUM",temp_buf_cached);
  pcVar1 = temp_buf_cached;
  retVar = 0xffffffff;
  if (0 < checksumOffset) {
    Sys_Flag = temp_buf_cached + checksumOffset;
    systemTime_ms = (uint)(Sys_Flag + 0x17) & 3;
    checksum = *(int *)(Sys_Flag + 0x1c);
    calculated_checksum = 0;
    uVar2 = (uint)(Sys_Flag + 0x14) & 3;
    systemTime_ms =
         (*(int *)(Sys_Flag + 0x17 + -systemTime_ms) << (3 - systemTime_ms) * 8 |
         unaff_s3 & 0xffffffffU >> (systemTime_ms + 1) * 8) & -1 << (4 - uVar2) * 8 |
         *(uint *)(Sys_Flag + 0x14 + -uVar2) >> uVar2 * 8;
    printk("<INFO>: Sys_Flag:%s,\n",Sys_Flag);
    printk("<INFO>: Sys_offset:0x%x,\n",*(undefined4 *)(Sys_Flag + 0x10));
    printk("<INFO>: Sys_len:0x%x, %d KB\n",*(uint *)(Sys_Flag + 0x14),
           *(uint *)(Sys_Flag + 0x14) >> 10);
    printk("<INFO>: Sys_para1:0x%x,\n",*(undefined4 *)(Sys_Flag + 0x18));
    printk("<INFO>: Sys_para2:0x%x,\n",*(undefined4 *)(Sys_Flag + 0x1c));
    vfs_llseek(_filp,vfs_llseek,*(undefined4 *)(Sys_Flag + 0x10),0,0);
    checksumOffset = _filp;
    retVar = *(undefined4 *)(Sys_Flag + 0x10);
    *(undefined4 *)(_filp + 0x24) = 0;
    *(undefined4 *)(checksumOffset + 0x20) = retVar;
    if (0 < (int)systemTime_ms) {
      uVar2 = 0x4000;
      calculated_checksum = 0;
      do {
        if ((int)systemTime_ms < (int)uVar2) {
          uVar2 = systemTime_ms;
        }
        vfs_read(_filp,temp_buf_cached,uVar2,_filp + 0x20);
        systemTime_ms = systemTime_ms - uVar2;
        checksumOffset = cal_checksum((int *)pcVar1,uVar2);
        calculated_checksum = calculated_checksum + checksumOffset;
      } while (0 < (int)systemTime_ms);
    }
    retVar = 0;
    if (checksum != calculated_checksum) {
      printk("<INFO>: firmware checksum error\n");
      printk("<INFO>: %x,%x\n",checksum,calculated_checksum);
      retVar = 0;
    }
  }
  systemTime_ms = _Get_TimeOut('\x02');
  printk("<INFO>: End of Time:%d ms\n",systemTime_ms);
  return retVar;
}

*/