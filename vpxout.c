#include <stdio.h>
#include <stdlib.h>

#define VPX_CODEC_DISABLE_COMPAT 1

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "stdint.h"
#include "/home/yangfan/zsim/ramulator-pim/zsim-ramulator/misc/hooks/zsim_hooks.h"
#define interface (&vpx_codec_vp8_cx_algo)

#define fourcc    0x30385056
 
#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)
 
static void mem_put_le16(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val>>8;
}
 
static void mem_put_le32(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val>>8;
    mem[2] = val>>16;
    mem[3] = val>>24;
}
 

static void write_ivf_file_header(FILE *outfile,
                                  const vpx_codec_enc_cfg_t *cfg,
                                  int frame_cnt) {
    char header[32];
 
    if(cfg->g_pass != VPX_RC_ONE_PASS && cfg->g_pass != VPX_RC_LAST_PASS)
        return;
    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header+4,  0);                   /* version */
    mem_put_le16(header+6,  32);                  /* headersize */
    mem_put_le32(header+8,  fourcc);              /* headersize */
    mem_put_le16(header+12, cfg->g_w);            /* width */
    mem_put_le16(header+14, cfg->g_h);            /* height */
    mem_put_le32(header+16, cfg->g_timebase.den); /* rate */
    mem_put_le32(header+20, cfg->g_timebase.num); /* scale */
    mem_put_le32(header+24, frame_cnt);           /* length */
    mem_put_le32(header+28, 0);                   /* unused */
 
    fwrite(header, 1, 32, outfile);
}
 
 
static void write_ivf_frame_header(FILE *outfile,
                                   const vpx_codec_cx_pkt_t *pkt)
{
    char             header[12];
    vpx_codec_pts_t  pts;
 
    if(pkt->kind != VPX_CODEC_CX_FRAME_PKT)
        return;
 
    pts = pkt->data.frame.pts;
    mem_put_le32(header, pkt->data.frame.sz);
    mem_put_le32(header+4, pts&0xFFFFFFFF);
    mem_put_le32(header+8, pts >> 32);
 
    fwrite(header, 1, 12, outfile);
}
 
int main(int argc, char **argv) {

    
    vpx_codec_ctx_t codec;
    vpx_codec_enc_cfg_t cfg;
    int frame_cnt = 0;
    unsigned char file_hdr[IVF_FILE_HDR_SZ];
    unsigned char frame_hdr[IVF_FRAME_HDR_SZ];
    vpx_image_t raw;
    vpx_codec_err_t ret;
    int width,height;
	int y_size;
    int frame_avail;
    int got_data;
    int flags = 0;
 
    width = 640;
    height = 360;
    if(argc<2) {
	   printf("Error argvs\n");
	   return 0;
    }
    printf("the src:%s\n",argv[1]);
    printf("the dst:%s\n",argv[2]);
    FILE* infile  = fopen(argv[1], "rb");

    FILE* outfile = fopen(argv[2], "wb");


	if(infile==NULL||outfile==NULL){
		printf("Error open files.\n");
		return -1;
	}

	if(!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 1)){
        printf("Fail to allocate image\n");
		return -1;
	}

    printf("Using %s\n",vpx_codec_iface_name(interface));

    //Populate encoder configuration 
    ret = vpx_codec_enc_config_default(interface, &cfg, 0);
    if(ret) {
        printf("Failed to get config: %s\n", vpx_codec_err_to_string(ret));
        return -1;                                                  
    }

    cfg.rc_target_bitrate =800;
    cfg.g_w = width;                                                          
    cfg.g_h = height;                                                         
    cfg.g_threads=1;
 
    write_ivf_file_header(outfile, &cfg, 0);
 
    //Initialize codec                                              
    if(vpx_codec_enc_init(&codec, interface, &cfg, 0)){
        printf("Failed to initialize encoder\n");
		return -1;
	}
 
    frame_avail = 1;
    got_data = 0;
    int   num=0;
    
    y_size=cfg.g_w*cfg.g_h;
    
    int num1=25;
    
    zsim_roi_begin();
    while((frame_avail || got_data)) {
        num1--;
        if(num1<0) break;
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t *pkt;
		
		if(fread(raw.planes[0], 1, y_size*3/2, infile)!=y_size*3/2){
			frame_avail=0;
		}

                zsim_PIM_function_begin();

		if(frame_avail){
			//Encode
                        
			ret=vpx_codec_encode(&codec,&raw,frame_cnt,1,flags,VPX_DL_REALTIME);
                        
                        
		}else{
			//Flush Encoder
			ret=vpx_codec_encode(&codec,NULL,frame_cnt,1,flags,VPX_DL_REALTIME);
		}
                zsim_PIM_function_end();
	/*ret=vpx_codec_encode(&codec,&raw,frame_cnt,1,flags,VPX_DL_REALTIME);*/

        

	if(ret){
            printf("Failed to encode frame\n");
	    return -1;
	}
        got_data = 0;
        
     
        while( (pkt = vpx_codec_get_cx_data(&codec, &iter)) ) {
            got_data = 1;
            switch(pkt->kind) {
            case VPX_CODEC_CX_FRAME_PKT:
                write_ivf_frame_header(outfile, pkt);
                fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,outfile); 
                break; 
            default:
                break;
            }
        }
        
	
        printf("Succeed encode frame: %5d\n",num);
        num++;
 
    }
    zsim_roi_end();
    fclose(infile);
 
    vpx_codec_destroy(&codec);
 
    //Try to rewrite the file header with the actual frame count
    
    if(!fseek(outfile, 0, SEEK_SET))
        write_ivf_file_header(outfile, &cfg, frame_cnt-1);
            
    fclose(outfile);

    return 0;
}
