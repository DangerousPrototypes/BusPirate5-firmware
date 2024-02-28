#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "storage.h"
#include "../lib/sfud/inc/sfud.h"
#include "../lib/sfud/inc/sfud_def.h"

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



#define SFUD_DEMO_TEST_BUFFER_SIZE                     1024

static void sfud_demo(uint32_t addr, size_t size, uint8_t *data);

static uint8_t sfud_demo_test_buf[SFUD_DEMO_TEST_BUFFER_SIZE];

void sfud_test(void)
{
    /* SFUD initialize */
    if (sfud_init() == SFUD_SUCCESS) {
        printf("Success!\r\n");
        sfud_demo(0, sizeof(sfud_demo_test_buf), sfud_demo_test_buf);
    }
    else
    {
        printf("Failed!\r\n");
    }
}

/**
 * SFUD demo for the first flash device test.
 *
 * @param addr flash start address
 * @param size test flash size
 * @param size test flash data buffer
 */
static void sfud_demo(uint32_t addr, size_t size, uint8_t *data) {
    sfud_err result = SFUD_SUCCESS;
    const sfud_flash *flash = sfud_get_device_table() + 0;
    size_t i;
    /* prepare write data */
    for (i = 0; i < size; i++) {
        data[i] = i;
    }
    /* erase test */
    result = sfud_erase(flash, addr, size);
    if (result == SFUD_SUCCESS) {
        printf("Erase the %s flash data finish. Start from 0x%08X, size is %ld.\r\n", flash->name, addr,
                size);
    } else {
        printf("Erase the %s flash data failed.\r\n", flash->name);
        return;
    }
    /* write test */
    result = sfud_write(flash, addr, size, data);
    if (result == SFUD_SUCCESS) {
        printf("Write the %s flash data finish. Start from 0x%08X, size is %ld.\r\n", flash->name, addr,
                size);
    } else {
        printf("Write the %s flash data failed.\r\n", flash->name);
        return;
    }
    /* read test */
    result = sfud_read(flash, addr, size, data);
    if (result == SFUD_SUCCESS) {
        printf("Read the %s flash data success. Start from 0x%08X, size is %ld. The data is:\r\n", flash->name, addr,
                size);
        printf("Offset (h) 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n");
        for (i = 0; i < size; i++) {
            if (i % 16 == 0) {
                printf("[%08X] ", addr + i);
            }
            printf("%02X ", data[i]);
            if (((i + 1) % 16 == 0) || i == size - 1) {
                printf("\r\n");
            }
        }
        printf("\r\n");
    } else {
        printf("Read the %s flash data failed.\r\n", flash->name);
    }
    /* data check */
    for (i = 0; i < size; i++) {
        if (data[i] != i % 256) {
            printf("Read and check write data has an error. Write the %s flash data failed.\r\n", flash->name);
			break;
        }
    }
    if (i == size) {
        printf("The %s flash test is success.\r\n", flash->name);
    }
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

void flash_probe()
{
    //sfud_test();
    //return;
    //printf("Probing:\r\n\t\tRESID (0xAB)\tREMSID (0x90)\tRDID (0x9F)\r\n");  

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

/*    printf("Device ID\t");
    if(has_resid) printf("0x%02x\t\t", resid); else printf("--\t\t");
    if(has_remsid) printf("0x%02x", remsid_dev); else printf("--");
    printf("\r\nManuf ID\t\t\t");
    if(has_remsid) printf("0x%02x\t\t", remsid_manuf); else printf("--\t\t");
    if(has_rdid) printf("0x%02x", rdid_manuf); else printf("--");   
    printf("\r\nType ID\t\t\t\t\t\t");
    if(has_rdid) printf("0x%02x", rdid_type); else printf("--");   
    printf("\r\nCapacity ID\t\t\t\t\t");            
    if(has_rdid) printf("0x%02x", rdid_capacity); else printf("--");   
    printf("\r\n\r\n");*/

    //now grab Serial Flash Discoverable Parameter (SFDP)
    // 0x5a 3 byte address, dummy byte, read first 24bytes
    printf("\r\n\r\nSFDP (0x5A): ");
    flash_start();
    flash_write_32(0x5a000000, 4);
    flash_write(0xff); //dummy byte
    uint8_t sfdp[36];
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
                printf("Density: %dB\r\nAddress bytes: ", ptp_j->density); //todo: math   
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
                printf("/Reset pin: %d\r\n/Hold pin: %d\r\n", ptp->reset_pin, ptp->hold_pin);
                printf("Deep Power Down (DPDM): %d\r\n", ptp->deep_power_down_mode);
                printf("SW reset: %d (instruction 0x%02x)\r\n", ptp->sw_reset, ptp->sw_reset_instruction);
                printf("Suspend/Resume program %d\r\nSuspend/Resume erase %d\r\n", ptp->program_suspend_resume, ptp->erase_suspend_resume);
                printf("Wrap Read mode: %d (instruction 0x%02x, length %d)\r\n", ptp->wrap_read_mode, ptp->wrap_read_instruction, ptp->wrap_read_length);
                printf("Individual block lock: %d (nonvolatile %d, instruction 0x%02x, default %d)\r\n", ptp->individual_block_lock, ptp->individual_block_lock_volatile, ptp->individual_block_lock_instruction, ptp->individual_block_lock_volatile_default);
                printf("Secured OTP: %d\r\n", ptp->secured_otp);
                printf("Read lock: %d\r\n", ptp->read_lock);
                printf("Permanent lock: %d\r\n", ptp->permanent_lock);

                break;
        }

        

    }

      
 

    //version
    //JEDEC info and jump
    //Manuf info and jump
    // Read JEDEC location and length
    // Read Manuf location and length




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

