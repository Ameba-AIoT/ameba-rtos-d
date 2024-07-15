#include <platform_opts.h>

#ifdef CONFIG_USBH_MSC

#include "scsi/scsi.h"
#include "us_intf.h"
#include "us_scsi.h"
#include "us_debug.h"

#include "usb.h"
_sema  us_sto_rdy_sema;

extern int usb_storage_init(void);
extern int usb_storage_deinit(void);

// todo
MSC_RESULT	us_format(void);
MSC_RESULT	us_mode_sense(void);

MSC_RESULT	us_init(void){
    rtw_init_sema(&us_sto_rdy_sema, 0);
	if(!usb_storage_init()){
		rtw_down_sema(&us_sto_rdy_sema);
        US_INFO("done");
		return MSC_OK;
	}
	return MSC_NOT_READY;
}

MSC_RESULT	us_deinit(void){
	usb_storage_deinit();
    rtw_free_sema(&us_sto_rdy_sema);
	return MSC_OK;
}

MSC_RESULT	us_isready (){
	if(!scsi_cmnd_execute(TEST_UNIT_READY, NULL, 0, 0)){
			return MSC_OK;
	}
	else{
		return MSC_NOT_READY;
	}
}

/* check disk status*/
MSC_RESULT	us_getStatus(){
	// query write protection status
	
	return MSC_OK;
}

MSC_RESULT	us_inquiry (u8 *pbuf){
	int res;
	res = scsi_cmnd_execute(INQUIRY,pbuf,0,0);
	if(!res)
		return MSC_OK;
	else
		return MSC_ERROR;
}

MSC_RESULT 	us_getcap(u32 *last_blk_addr, u32 *block_size){
	u8 buff_8[8];
	u8 buff_12[12];
	int res = 0;

	memset(buff_8, 0, 8);
	memset(buff_12, 0, 12);

	res = scsi_cmnd_execute(READ_CAPACITY, buff_8, 0, 0);
	if(!res){
		*last_blk_addr = ((buff_8[0] << 24) | (buff_8[1] << 16) | \
			                (buff_8[2] << 8) | buff_8[3]);
		if (0xffffffff != *last_blk_addr) {
			    *block_size = ((buff_8[4] << 24) | (buff_8[5] << 16) |
			                 (buff_8[6] << 8) | buff_8[7]);
		}
		else
			goto large;
	    return MSC_OK;
	}
	else
		return MSC_ERROR;
	large:
		//READ CAPACITY (10) indicates device capacity too large,now trying 16 byte cdb variant\n");
	res = scsi_cmnd_execute(SAI_READ_CAPACITY_16, buff_12, 0, 0);
	if(!res){
		u8 k;
		for (k = 0, *last_blk_addr = 0; k < 8; ++k) {
			*last_blk_addr <<= 8;
			*last_blk_addr |= buff_12[k];
		}
        *block_size = ((buff_12[8] << 24) | (buff_12[9] << 16) |
                      (buff_12[10] << 8) | buff_12[11]);
        return MSC_OK;
	}
	else
		return MSC_ERROR;
}

MSC_RESULT 	us_read_blocks( u8 *pbuf, u32 sector, u32 count){
	int res = 0;
	res = scsi_cmnd_execute(READ_10, pbuf, sector, count);
	if(!res)
		return MSC_OK;
	else
		return MSC_ERROR;
}

MSC_RESULT 	us_write_blocks( const u8 *pbuf, u32 sector, u32 count){
	int res = 0;
	res = scsi_cmnd_execute(WRITE_10, (unsigned char*)pbuf, sector, count);
	if(!res)
		return MSC_OK;
	else
		return MSC_ERROR;
}

#endif // CONFIG_USBH_MSC
