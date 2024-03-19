#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "storage.h"
#include "../lib/sfud/inc/sfud.h"
#include "../lib/sfud/inc/sfud_def.h"
#include "mode/spiflash.h"
#include "mem.h"
#include "fatfs/ff.h"


#define M_SPI_PORT spi1
#define M_SPI_CLK BIO6
#define M_SPI_CDO BIO7
#define M_SPI_CDI BIO4
#define M_SPI_CS BIO5

#define M_SPI_SELECT 0
#define M_SPI_DESELECT 1

uint32_t flash_read(void);
void flash_write_32(const uint32_t data, uint8_t count);
void flash_write(const uint32_t data);
void flash_start(void);
void flash_stop(void);
void flash_read_n(uint8_t *data, uint8_t count);

void flash_not_found()
{
    printf("not found\r\n");
}

static inline uint32_t 
spiflash_next_count(uint32_t current_address, uint32_t end_address, uint32_t buff_cnt)
{       
    if((end_address)>=((current_address)+buff_cnt))
    {
        return buff_cnt;
    }
    else
    {
        return (end_address)-(current_address);
    }
}

bool 
spiflash_init(sfud_flash *flash_info){
    flash_info->index = 0;
    sfud_err cur_flash_result = sfud_device_init(flash_info);
    if(cur_flash_result != SFUD_SUCCESS){
        printf("Error: device not detected\r\n");
        return false;
    }
    return true;
}

bool spiflash_erase(sfud_flash *flash_info)
{
    printf("Erasing flash...\r\n");
    if(sfud_chip_erase(flash_info)!=SFUD_SUCCESS)
    {
        printf("Error: erase failed\r\n");
        return false;
    }
    printf("Erase OK\r\n");
    return true;
}

bool spiflash_erase_verify(uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t *buf, sfud_flash *flash_info)
{
    uint32_t bytes_total=(end_address-start_address);
    uint32_t current_address=start_address;
    ui_term_progress_bar_t progress_bar;
    printf("Verify erase:\r\n");
    ui_term_progress_bar_draw(&progress_bar); 
    
    while(true){
        ui_term_progress_bar_update(bytes_total - (end_address-current_address), bytes_total, &progress_bar);
        uint32_t read_count=spiflash_next_count(current_address, end_address, buf_size);
        if(sfud_read(flash_info, current_address, read_count, buf)!=SFUD_SUCCESS)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("Error: read failed\r\n");
            return false;
        }

        for(uint32_t i=0; i<read_count; i++)
        {
            if(buf[i]!=0xff)
            {
                ui_term_progress_bar_cleanup(&progress_bar);
                printf("Error: erase failed at 0x%06x\r\n", (current_address+i));
                return false;
            }
        }   
        current_address+=read_count;
        if(current_address==end_address) break;//done!
    }

    ui_term_progress_bar_cleanup(&progress_bar);
    printf("Erase verified\r\n");
    return true;
}

// function to fill flash with the same data on each page for testing, typically 0...255
bool
spiflash_write_test(uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t *buf, sfud_flash *flash_info)
{
    uint32_t bytes_total=(end_address-start_address);
    uint32_t current_address=start_address;
    ui_term_progress_bar_t progress_bar;
    printf("Writing test data:\r\n");
    if(buf_size<256)
    {
        printf("Write buffer too small (%d), aborting\r\n", buf_size);
        return false;
    }  
    for(uint32_t i=0; i<256; i++)
    {
        buf[i]=(uint8_t)i;
    }
    ui_term_progress_bar_draw(&progress_bar); 
    while(true){
        ui_term_progress_bar_update(bytes_total - (end_address-current_address), bytes_total, &progress_bar);
        uint32_t write_count=spiflash_next_count(current_address, end_address,buf_size);
        if(sfud_write(flash_info, current_address, write_count, buf)!=SFUD_SUCCESS)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("Error: write failed\r\n");
            return false;
        }    
        current_address+=write_count;    
        if(current_address==end_address) break;//done!
    }

    ui_term_progress_bar_cleanup(&progress_bar);
    printf("Write complete\r\n");
    return true;
}

bool
spiflash_write_verify(uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t *buf, sfud_flash *flash_info)
{
    uint32_t bytes_total=(end_address-start_address);
    uint32_t current_address=start_address;
    //verify test data
    printf("Verifying write data:\r\n");
    if(buf_size<256)
    {
        printf("Read buffer too small (%d), aborting\r\n", buf_size);
        return false;
    }
    ui_term_progress_bar_t progress_bar;
    ui_term_progress_bar_draw(&progress_bar); 
    while(true){
        ui_term_progress_bar_update(bytes_total - (end_address-current_address), bytes_total, &progress_bar);
        uint32_t read_count=spiflash_next_count(current_address, end_address, buf_size);
        if(sfud_read(flash_info, current_address, read_count, buf)!=SFUD_SUCCESS)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("\r\nError: read failed\r\n");
            return false;
        }

        for(uint32_t i=0; i<read_count; i++)
        {
            if(buf[i]!=(uint8_t)(i&0xff))
            {
                ui_term_progress_bar_cleanup(&progress_bar);
                printf("\r\nError: verify failed at %06x [%02x != %02x]\r\n", (current_address)+i, i, buf[i]);
                return false;
            }
        }
        current_address+=read_count;         
        if(current_address==end_address) break;//done!
    }    

    ui_term_progress_bar_cleanup(&progress_bar);
    printf("Verify OK\r\n");
    return true;
}

bool 
spiflash_dump(uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t *buf, sfud_flash *flash_info, const char *file_name)
{
    uint32_t bytes_total=(end_address-start_address);
    uint32_t current_address=start_address;
    FIL fil;			/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    UINT bw;

    printf("Dumping to %s...\r\n", file_name);

    //open file
    fr = f_open(&fil, file_name, FA_WRITE | FA_CREATE_ALWAYS);	
    if (fr != FR_OK) {
        storage_file_error(fr);
        return false;
    }

    ui_term_progress_bar_t progress_bar;
    ui_term_progress_bar_draw(&progress_bar); 
    while(true){
        ui_term_progress_bar_update(bytes_total - (end_address-current_address), bytes_total, &progress_bar);
        uint32_t read_count=spiflash_next_count(current_address, end_address, buf_size);
        if(sfud_read(flash_info, current_address, read_count, buf)!=SFUD_SUCCESS)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("\r\nError: read failed\r\n");
            return false;
        }
        f_write(&fil, buf, read_count, &bw);	
        if(fr != FR_OK || bw!=read_count)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            storage_file_error(fr);
            return false;
        }       
        current_address+=read_count;         
        if(current_address==end_address) break;//done!
    }    
    f_close(&fil);

    ui_term_progress_bar_cleanup(&progress_bar);
    printf("Dump OK\r\n"); 
    return true;  
}

bool 
spiflash_load(uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t *buf, sfud_flash *flash_info, const char *file_name)
{
    uint32_t bytes_total=(end_address-start_address);
    uint32_t current_address=start_address;
    FIL fil;			/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    UINT bw;

    printf("Loading from %s...\r\n", file_name);

    //open file
    fr = f_open(&fil, file_name, FA_READ);	
    if (fr != FR_OK) {
        storage_file_error(fr);
        return false;
    }

    //match file size to chip capacity
    uint32_t file_size=f_size(&fil);
    printf("File size: %d, chip size: %d\r\n", file_size, end_address-start_address);    
    if(file_size>(end_address-start_address)){
        printf("Warning: file too large, writing first %d bytes\r\n", end_address-start_address);
    }else if(file_size<(end_address-start_address)){
        printf("Warning: file smaller than chip capacity, writing first %d bytes\r\n", file_size);
    }else{
        printf("File size matches chip capacity, writing %d bytes\r\n", file_size);
    }

    ui_term_progress_bar_t progress_bar;
    ui_term_progress_bar_draw(&progress_bar); 
    while(true){
        ui_term_progress_bar_update(bytes_total - (end_address-current_address), bytes_total, &progress_bar);
        uint32_t write_count=spiflash_next_count(current_address, end_address, buf_size);
        size_t file_read_count;
        fr = f_read(&fil, buf, write_count, &file_read_count); /* Read a chunk of data from the source file */
        if (file_read_count == 0)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("Warning: end of file, aborting");
            break; /* error or eof */
        }

        //zero out rest of array if file is smaller than buffer
        if(file_read_count<write_count)
        {
            memset(buf+file_read_count, 0xff, write_count-file_read_count);
        }

        if(sfud_write(flash_info, current_address, write_count, buf)!=SFUD_SUCCESS)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("\r\nError: write failed\r\n");
            return false;
        }
        current_address+=write_count;     
        if(current_address==end_address) break;//done!
    }    
    f_close(&fil);

    ui_term_progress_bar_cleanup(&progress_bar);
    printf("Program OK\r\n"); 
    return true;  
}

bool 
spiflash_verify(uint32_t start_address, uint32_t end_address, uint32_t buf_size, uint8_t *buf, uint8_t *buf2, sfud_flash *flash_info, const char *file_name)
{
    uint32_t bytes_total=(end_address-start_address);
    uint32_t current_address=start_address;
    FIL fil;			/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */
    UINT bw;

    //open file
    fr = f_open(&fil, file_name, FA_READ);	
    if (fr != FR_OK) {
        //printf("File error %d", fr);
        storage_file_error(fr);
        return false;
    }
    //match file size to chip capacity

    printf("Verifying from %s...\r\n", file_name);
    //match file size to chip capacity
    uint32_t file_size=f_size(&fil);
    printf("File size: %d, chip size:%d\r\n", file_size, end_address-start_address);
    if(file_size>(end_address-start_address)){
        printf("Warning: file larger than chip, verifying first %d bytes\r\n", end_address-start_address);
    }else if(file_size<(end_address-start_address)){
        printf("Warning: file smaller than chip capacity, verifying first %d bytes\r\n", file_size);
        //TODO: adjust bytes total and end address so progress indicators work
    }else{
        printf("File size matches chip capacity, verifying %d bytes\r\n", file_size);
    }    

    ui_term_progress_bar_t progress_bar;
    ui_term_progress_bar_draw(&progress_bar); 
    while(true){
        ui_term_progress_bar_update(bytes_total - (end_address-current_address), bytes_total, &progress_bar);
        uint32_t read_count=spiflash_next_count(current_address, end_address, buf_size);
        size_t file_read_count;
        fr = f_read(&fil, buf, read_count, &file_read_count); /* Read a chunk of data from the source file */
        if (file_read_count == 0)
        {
            ui_term_progress_bar_cleanup(&progress_bar);
            printf("Warning: end of file, aborting");
            break; /* error or eof */
        }

        if(sfud_read(flash_info, current_address, read_count, buf2)!=SFUD_SUCCESS)
        {
            ui_term_progress_bar_cleanup(&progress_bar);  
            printf("\r\nError: read failed\r\n");
            return false;
        }

        for(uint32_t i=0; i<file_read_count; i++)
        {
            if(buf[i]!=buf2[i])
            {
                ui_term_progress_bar_cleanup(&progress_bar);
                printf("\r\nError: verify failed at %06x [%02x != %02x]\r\n", (current_address)+i, buf[i], buf2[i]);
                return false;
            }
        }
        current_address+=read_count;     
        if(current_address==end_address) break;//done!
    }    
    f_close(&fil);

    ui_term_progress_bar_cleanup(&progress_bar);
    printf("Verify OK\r\n"); 
    return true;  
}

typedef struct __attribute__((packed)) ptp_head_struct {
    uint32_t signature:32;
    uint8_t revision_minor:8;
    uint8_t revision_major:8;
    uint8_t headers_count:8;
} ptp_head_t;

typedef struct __attribute__((packed)) ptp_record_struct {
    uint8_t id:8;
    uint8_t revision_minor:8;
    uint8_t revision_major:8;
    uint8_t length_dwords:8;
    uint32_t address:24;
} ptp_record_t;

typedef struct __attribute__((packed)) ptp_jedec_struct {
    //table part 1
    uint8_t erase_size:2; //Block / Sector Erase sizes, 01:4KB erase, 11:not supported 4KB erase, other reserve.
    bool write_granularity:1; //0:1Byte,1:64Byte or larger
    bool volatile_status_register_block_protect_bits:1;
    bool volatile_status_register_write_enable_instruction_select:1; //0:50h, 1:06h
    uint8_t unused_1:3;
    uint8_t erase_instruction_4k:8;
    bool fast_read_112:1;
    uint8_t address_bytes:2;  //00=3, 01=3 or 4, 10 =4bytes
    bool dtr_clocking:1; //Double Transfer Rate clocking
    bool fast_read_122:1; 
    bool fast_read_144:1; 
    bool fast_read_114:1; 
    bool unused_2:1;
    uint8_t unused_3:8;
    //table part 2
    uint32_t density:32; //2 gigabits or less: bit-31 is set to 0b. 30:0 defines the size in bits 2^n.
    //table part 3
    uint8_t fast_read_144_wait_states:4; //0/4/8 
    uint8_t fast_read_144_mode_clocks:4; //0 or 2
    uint8_t fast_read_144_read_instruction:8;
    uint8_t fast_read_114_wait_states:4; //0/4/8 
    uint8_t fast_read_114_mode_clocks:4; //0 or 2
    uint8_t fast_read_114_read_instruction:8;   
    //table part 4
    uint8_t fast_read_112_wait_states:4; //0/4/8 
    uint8_t fast_read_112_mode_clocks:4; //0 or 2
    uint8_t fast_read_112_read_instruction:8;
    uint8_t fast_read_122_wait_states:4; //0/4/8 
    uint8_t fast_read_122_mode_clocks:4; //0 or 2
    uint8_t fast_read_122_read_instruction:8;       
    //table part 5
    bool fast_read_222:1;
    uint8_t unused_4:3;
    bool fast_read_444:1;
    uint8_t unused_5:3;
    uint32_t unused_6:24;
    //table part 6
    uint16_t unused_7:16;
    uint8_t fast_read_222_wait_states:4; //0/4/8 
    uint8_t fast_read_222_mode_clocks:4; //0 or 2
    uint8_t fast_read_222_read_instruction:8;    
    //table part 7
    uint16_t unused_8:16;
    uint8_t fast_read_444_wait_states:4; //0/4/8 
    uint8_t fast_read_444_mode_clocks:4; //0 or 2
    uint8_t fast_read_444_read_instruction:8; 
    //table part 8
    uint8_t erase_1_size:8; //Sector/block size=2N bytes(4) 0Ch:4KB;0Fh:32KB;10h:64KB
    uint8_t erase_1_instruction:8;
    uint8_t erase_2_size:8; //Sector/block size=2N bytes 00h:NA;0Fh:32KB;10h:64KB
    uint8_t erase_2_instruction:8;    
    //table part 9
    uint8_t erase_3_size:8; //Sector/block size=2N bytes 00h:NA;0Fh:32KB;10h:64KB
    uint8_t erase_3_instruction:8;
    uint8_t erase_4_size:8; //Sector/block size=2N bytes 00h:NA;0Fh:32KB;10h:64KB
    uint8_t erase_4_instruction:8;    
} ptp_jedec_t;

typedef struct __attribute__((packed)) ptp_manuf_struct {
        uint16_t vcc_max :16;
        uint16_t vcc_min :16;
        bool reset_pin :1;
        bool hold_pin :1;
        bool deep_power_down_mode: 1;
        bool sw_reset:1;
        uint16_t sw_reset_instruction:8; //gcc doesn't like uint8_t bits across a boundary, using uint16
        bool program_suspend_resume:1;
        bool erase_suspend_resume:1;
        bool unused_1:1;
        bool wrap_read_mode:1;
        uint8_t wrap_read_instruction:8;
        uint8_t wrap_read_length:8;
        bool individual_block_lock:1;
        bool individual_block_lock_volatile:1;
        uint16_t individual_block_lock_instruction:8; //gcc doesn't like uint8_t bits across a boundary, using uint16
        bool individual_block_lock_volatile_default:1;
        bool secured_otp:1;
        bool read_lock:1;
        bool permanent_lock:1;
        bool unused_2:1;
        bool unused_3:1;
        uint16_t unused_4:16;
        uint32_t unused_5:32;
    } ptp_manuf_t;

uint32_t flash_erase_size(uint32_t size, char *unit)
{
    uint32_t erase_size=pow(2,size);
    *unit ='B';
    if(erase_size>=1024)
    {
        erase_size=erase_size/1024;
        *unit='K';
    }
    return erase_size;
}

bool flash_read_resid(uint8_t *res_id)
{
    //DP 0xB9: deep power down and then RDP 0xAB, 3 dummy bytes, 1 RES ID byte (release and read ID)
    //deep sleep command
    flash_start();
    flash_write(0xab);
    flash_stop();
    //resume and read ID
    busy_wait_ms(10);
    flash_start();
    flash_write_32(0xab000000, 4);
    *res_id = flash_read();
    flash_stop();
    if(*res_id==0x00 || *res_id==0xff)
    {
        return false;
    }
    return true;
}

bool flash_read_remsid(uint8_t *remsid_manuf, uint8_t *remsid_dev)
{
    //0x90: REMS  Read Electronic Manufacturer ID & Device ID (REMS)
    // 0x90, 0x00:3, 1 Manuf ID, 1 Device ID
    flash_start();
    flash_write_32(0x90000000,4);
    *remsid_manuf = flash_read();
    *remsid_dev = flash_read();
    flash_stop();
    if(*remsid_manuf==0x00 || *remsid_manuf==0xff) //TODO: manuf ID has a checksum bit or something, list of man and dev ids?    
    {
        return false;
    }
    return true;
}

bool flash_read_rdid(uint8_t *rdid_manuf, uint8_t *rdid_type, uint8_t *rdid_capacity)
{
    //0x9f: RDID  Read Identification (RDID)
    // 0x9f, 1 manuf ID, 1 memory type, 1 capacity
    flash_start();
    flash_write(0x9f);
    *rdid_manuf=flash_read();
    *rdid_type=flash_read();
    *rdid_capacity=flash_read();
    flash_stop();
    if(*rdid_manuf==0x00 || *rdid_manuf==0xff)//TODO: is there a standard coding?
    {
        return false;
    }
    return true;
}

void spiflash_probe(void)
{
    uint8_t resid;
    bool has_resid=flash_read_resid(&resid);
    uint8_t remsid_manuf, remsid_dev;
    bool has_remsid =flash_read_remsid(&remsid_manuf, &remsid_dev);
    uint8_t rdid_manuf, rdid_type, rdid_capacity;
    bool has_rdid = flash_read_rdid(&rdid_manuf, &rdid_type, &rdid_capacity);
    printf("Probing:\r\n\t\tDevice ID\tManuf ID\tType ID\t\tCapacity ID\r\n");  
    printf("RESID (0xAB)\t");
    if(has_resid) printf("0x%02x", resid); else printf("--");
    printf("\r\nREMSID (0x90)\t");   
    if(has_remsid) printf("0x%02x\t\t0x%02x", remsid_dev, remsid_manuf); else printf("--\t\t--");
    printf("\r\nRDID (0x9F)\t");
    if(has_rdid) printf("\t\t0x%02x\t\t0x%02x\t\t0x%02x", rdid_manuf, rdid_type, rdid_capacity); else printf("\t\t--\t\t--\t\t--");   

    //now grab Serial Flash Discoverable Parameter (SFDP)
    // 0x5a 3 byte address, dummy byte, read first 24bytes
    printf("\r\n\r\nSFDP (0x5A): ");
    flash_start();
    flash_write_32(0x5a000000, 4);
    flash_write(0xff); //dummy byte
    uint8_t sfdp[128];
    flash_read_n(sfdp,8);
    flash_stop();

    ptp_head_t *ptp_head;
    ptp_head = (ptp_head_t *)&sfdp;
               
    //check code
    if(ptp_head->signature == 0x50444653)
    {
        printf("found 0x50444653 \"PDFS\"\r\n");
    }
    else
    {
        flash_not_found();
        return;
    }

    printf(" Version: %d.%d\r\n", ptp_head->revision_major, ptp_head->revision_minor);
 
    uint8_t param_table_pointers=ptp_head->headers_count+1;
    printf(" Headers: %d\r\n", param_table_pointers); //n+1 parameter headers

    //loop over table pointers, usually 2?
    for(uint8_t i=0; i<param_table_pointers; i++)
    {
        printf("\r\n**Param Table %d**\r\n", i);
        flash_start();
        uint32_t address=0x5a000000 + 8 + (i*8);
        flash_write_32(address, 4);
        flash_write(0xff); //dummy byte
        flash_read_n(sfdp,8);
        flash_stop();
        const char ptp_jedec[]="JEDEC";
        const char ptp_manuf[]="manuf";
        ptp_record_t *ptp_rec;
        ptp_rec = (ptp_record_t *)&sfdp;

        uint8_t ptp_id = ptp_rec->id;
        uint8_t ptp_length=ptp_rec->length_dwords * 4;  
        uint32_t ptp_address=ptp_rec->address;     
        #define PTP_JEDEC 0
        printf("\t\tType\t\tVer.\tLength\tAddress\r\n");
        printf("Table %d\t\t%s (0x%02x)\t%d.%d\t%d\t0x%06x\r\n", i, ptp_id==PTP_JEDEC?ptp_jedec:ptp_manuf, ptp_id, ptp_rec->revision_major, ptp_rec->revision_minor, ptp_length, ptp_address );

        if(ptp_length>count_of(sfdp))
        {
            printf(" Error: record too long to fetch! Skipping...\r\n");
            continue;
        }    
          
        flash_start();
        flash_write_32(0x5a000000 + ptp_address, 4);
        flash_write(0xff); //dummy byte
        flash_read_n(sfdp,ptp_length);
        flash_stop();

        /* print JEDEC basic flash parameter table info */
        printf("\r\nMSB-LSB  3    2    1    0\r\n");
        for (uint8_t j = 0; j < ptp_length/4; j++) {
            printf("[%04d] 0x%02X 0x%02X 0x%02X 0x%02X\r\n", j + 1, sfdp[j * 4 + 3], sfdp[j * 4 + 2], sfdp[j * 4 + 1], sfdp[j * 4]);
        }

        switch(ptp_id)
        {
            case PTP_JEDEC:
                printf("\r\n");
                ptp_jedec_t *ptp_j;
                ptp_j = (ptp_jedec_t *)&sfdp;

                uint32_t capacity;
                if(!((ptp_j->density>>31) & 0b1)) 
                {
                        capacity = 1 + (ptp_j->density >> 3);
                }
                else
                {
                    capacity = ptp_j->density & 0x7FFFFFFF;
                    if (capacity > 4U * 8 + 3) {
                        printf("Error: The flash capacity is grater than 32 Gb/ 4 GB! Not Supported.\r\n");
                        capacity=0;
                    }
                    else
                    {
                        capacity = 1L << (capacity - 3);
                    }
                }
                printf("Density: %d bytes\r\nAddress bytes: ", capacity);  
                switch(ptp_j->address_bytes)
                {
                    case 0:
                        printf("3");
                        break;
                    case 1:
                        printf("3 or 4");
                        break;
                    case 2:
                        printf("4");
                        break;
                }            
                printf("\r\nWrite granularity:");
                if(!ptp_j->write_granularity) printf("1B"); else printf(">=64B");
                printf("\r\nWrite Enable Volatile: %d\r\nWrite Enable instruction: 0x%02x\r\n", ptp_j->volatile_status_register_block_protect_bits, (ptp_j->volatile_status_register_write_enable_instruction_select)?0x6:0x50);
                printf("4K erase instruction: ");
                if(ptp_j->erase_size==0b01) printf("0x%02x", ptp_j->erase_instruction_4k); else printf("--");


                printf("\r\n\r\nFast read:\t1-1-2\t1-1-4\t1-2-2\t1-4-4\t2-2-2\t4-4-4\r\n");
                printf("Instruction:");
                if(ptp_j->fast_read_112) printf("\t0x%02x",ptp_j->fast_read_112_read_instruction); else printf("\t--");
                if(ptp_j->fast_read_114) printf("\t0x%02x",ptp_j->fast_read_114_read_instruction); else printf("\t--");
                if(ptp_j->fast_read_122) printf("\t0x%02x",ptp_j->fast_read_122_read_instruction); else printf("\t--");
                if(ptp_j->fast_read_144) printf("\t0x%02x",ptp_j->fast_read_144_read_instruction); else printf("\t--");
                if(ptp_j->fast_read_222) printf("\t0x%02x",ptp_j->fast_read_222_read_instruction); else printf("\t--");
                if(ptp_j->fast_read_444) printf("\t0x%02x",ptp_j->fast_read_444_read_instruction); else printf("\t--");
                printf("\r\nWait states:\t%d\t%d\t%d\t%d\t%d\t%d",
                    ptp_j->fast_read_112_wait_states, ptp_j->fast_read_114_wait_states, ptp_j->fast_read_122_wait_states,
                    ptp_j->fast_read_144_wait_states, ptp_j->fast_read_222_wait_states, ptp_j->fast_read_444_wait_states 
                );
                printf("\r\nMode clocks:\t%d\t%d\t%d\t%d\t%d\t%d",               
                    ptp_j->fast_read_112_mode_clocks, ptp_j->fast_read_114_mode_clocks, ptp_j->fast_read_122_mode_clocks,
                    ptp_j->fast_read_144_mode_clocks, ptp_j->fast_read_222_mode_clocks, ptp_j->fast_read_444_mode_clocks
                );

                printf("\r\n\r\nErase:\t\t1\t2\t3\t4\r\n");
                printf("Instruction:\t0x%02x\t0x%02x\t0x%02x\t0x%02x\r\n", ptp_j->erase_1_instruction, ptp_j->erase_2_instruction, ptp_j->erase_3_instruction, ptp_j->erase_4_instruction);
                
                uint32_t erase_size;
                char unit;
                erase_size=flash_erase_size(ptp_j->erase_1_size, &unit);
                printf("Size:\t\t%d%c",erase_size, unit);
                erase_size=flash_erase_size(ptp_j->erase_2_size, &unit);
                printf("\t%d%c",erase_size, unit);
                erase_size=flash_erase_size(ptp_j->erase_3_size, &unit);
                printf("\t%d%c",erase_size, unit);
                erase_size=flash_erase_size(ptp_j->erase_4_size, &unit);
                printf("\t%d%c\r\n",erase_size, unit);                                
                break;
            default:
                printf("\r\n");
                ptp_manuf_t *ptp;  
                ptp = (ptp_manuf_t *)&sfdp;

                printf("VCC min: %04xmV\r\nVCC max: %04xmV\r\n", ptp->vcc_min, ptp->vcc_max);
                printf("/Reset pin: %c\r\n", ptp->reset_pin?'Y':'-');
                printf("/Hold pin: %c\r\n", ptp->hold_pin?'Y':'-');
                printf("Deep Power Down (DPDM): %c\r\n", ptp->deep_power_down_mode?'Y':'-');
                printf("SW reset: %c (instruction 0x%02x)\r\n", ptp->sw_reset?'Y':'-', ptp->sw_reset_instruction);
                printf("Suspend/Resume program: %c\r\nSuspend/Resume erase: %c\r\n", ptp->program_suspend_resume?'Y':'-', ptp->erase_suspend_resume?'Y':'-');
                printf("Wrap Read mode: %c (instruction 0x%02x, length %d)\r\n", ptp->wrap_read_mode?'Y':'-', ptp->wrap_read_instruction, ptp->wrap_read_length);
                printf("Individual block lock: %c (nonvolatile %c, instruction 0x%02x, default %d)\r\n", ptp->individual_block_lock?'Y':'-', ptp->individual_block_lock_volatile?'Y':'-', ptp->individual_block_lock_instruction, ptp->individual_block_lock_volatile_default);
                printf("Secured OTP: %c\r\n", ptp->secured_otp?'Y':'-');
                printf("Read lock: %c\r\n", ptp->read_lock?'Y':'-');
                printf("Permanent lock: %c\r\n", ptp->permanent_lock?'Y':'-');

                break;
        }    
    }
}

void flash_set_cs(uint8_t cs)
{

	if(cs==M_SPI_SELECT) // 'start'
	{
		if(true) bio_put(M_SPI_CS, 0);
			else bio_put(M_SPI_CS, 1);
	}
	else			// 'stop' 
	{
		if(true) bio_put(M_SPI_CS, 1);
			else bio_put(M_SPI_CS, 0);
	}
}

uint8_t flash_xfer(const uint8_t out)
{
	uint8_t spi_in;
	spi_write_read_blocking(M_SPI_PORT, &out,&spi_in, 1);
	return spi_in;
}

void flash_start(void)
{
	flash_set_cs(M_SPI_SELECT);
}

void flash_stop(void)
{

	flash_set_cs(M_SPI_DESELECT);
}

void flash_write_32(const uint32_t data, uint8_t count)
{
    uint8_t sent=0;
    for(uint8_t i=4; i>0; i--)
    {
        flash_write(data>>(8*(i-1)));
        sent++;
        if(sent==count) return;
    }
}

void flash_write(uint32_t data)
{
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
	while(!spi_is_writable(M_SPI_PORT))
	{
		tight_loop_contents();
	}

	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)data;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    while(spi_get_hw(M_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS)
	{
        tight_loop_contents();
	}

    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    // Don't leave overrun flag set
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

void flash_read_n(uint8_t *data, uint8_t count)
{
    for(uint8_t i=0; i<count; i++)
    {
        data[i]=flash_read();
    }
}

uint32_t flash_read(void)
{
	while(!spi_is_writable(M_SPI_PORT));
	
	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)0xff;

    while(!spi_is_readable(M_SPI_PORT));
	
	return (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}

