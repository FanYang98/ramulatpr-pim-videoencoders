#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "stdint.h"
#include "/home/yangfan/zsim/ramulator-pim/zsim-ramulator/misc/hooks/zsim_hooks.h"
#if defined ( __cplusplus)
extern "C"
{
#include "x265.h"
};
#else
#include <x265.h>
#endif
 
int main(int argc, char** argv){
        
	int i,j;
	int y_size;
	int buff_size;
	char *buff=NULL;
	int ret;
	x265_nal *pNals=NULL;
	uint32_t iNal=0;
 
	x265_param* pParam=NULL;
	x265_encoder* pHandle=NULL;
	x265_picture *pPic_in=NULL;
 
	//Encode 50 frame
	//if set 0, encode all frame
	int frame_num=0;
	int csp=X265_CSP_I420;
	int width=720,height=480;
         
        if(argc<2) {
		printf("Error argvs\n");
		return 0;
	}
	printf("the src:%s\n",argv[1]);
	printf("the dst:%s\n",argv[2]);
        FILE* fp_src  = fopen(argv[1], "rb");

        FILE* fp_dst = fopen(argv[2], "wb");
        if(fp_src==NULL){
            printf("src open error\n");
            return 1;

        }
	if(fp_src==NULL||fp_dst==NULL){
		return -1;
	}
   
	pParam=x265_param_alloc();
	x265_param_default(pParam);
	pParam->internalCsp=csp;
	pParam->sourceWidth=width;
	pParam->sourceHeight=height;
	pParam->fpsNum=25;
	pParam->fpsDenom=1;
        //pParam->frameNumThreads=1;
	//Init
	pHandle=x265_encoder_open(pParam);
	if(pHandle==NULL){
		printf("x265_encoder_open err\n");
		return 0;
	}
	y_size = pParam->sourceWidth * pParam->sourceHeight;
 
	pPic_in = x265_picture_alloc();
	x265_picture_init(pParam,pPic_in);
	switch(csp){
	case X265_CSP_I444:{
		buff=(char *)malloc(y_size*3);
		pPic_in->planes[0]=buff;
		pPic_in->planes[1]=buff+y_size;
		pPic_in->planes[2]=buff+y_size*2;
		pPic_in->stride[0]=width;
		pPic_in->stride[1]=width;
		pPic_in->stride[2]=width;
		break;
					   }
	case X265_CSP_I420:{
		buff=(char *)malloc(y_size*3/2);
		pPic_in->planes[0]=buff;
		pPic_in->planes[1]=buff+y_size;
		pPic_in->planes[2]=buff+y_size*5/4;
		pPic_in->stride[0]=width;
		pPic_in->stride[1]=width/2;
		pPic_in->stride[2]=width/2;
		break;
					   }
	default:{
		printf("Colorspace Not Support.\n");
		return -1;
			}
	}
	
	//detect frame number
	if(frame_num==0){
		fseek(fp_src,0,SEEK_END);
		switch(csp){
		case X265_CSP_I444:frame_num=ftell(fp_src)/(y_size*3);break;
		case X265_CSP_I420:frame_num=ftell(fp_src)/(y_size*3/2);break;
		default:printf("Colorspace Not Support.\n");return -1;
		}
		fseek(fp_src,0,SEEK_SET);
	}
    zsim_roi_begin();
	//Loop to Encode
	for( i=0;i<26;i++){
		switch(csp){
		case X265_CSP_I444:{
			fread(pPic_in->planes[0],1,y_size,fp_src);		//Y
			fread(pPic_in->planes[1],1,y_size,fp_src);		//U
			fread(pPic_in->planes[2],1,y_size,fp_src);		//V
			break;}
		case X265_CSP_I420:{
			fread(pPic_in->planes[0],1,y_size,fp_src);		//Y
			fread(pPic_in->planes[1],1,y_size/4,fp_src);	//U
			fread(pPic_in->planes[2],1,y_size/4,fp_src);	//V
			break;}
		default:{
			printf("Colorspace Not Support.\n");
			return -1;}
		}
        zsim_PIM_function_begin();
		ret=x265_encoder_encode(pHandle,&pNals,&iNal,pPic_in,NULL);	
        zsim_PIM_function_end(); 
        printf("Succeed encode %5d frames\n",i);
                
		for(j=0;j<iNal;j++){
			fwrite(pNals[j].payload,1,pNals[j].sizeBytes,fp_dst);
		}	
        
	}
    zsim_roi_end(); 
	//Flush Decoder
    
	while(1){
		ret=x265_encoder_encode(pHandle,&pNals,&iNal,NULL,NULL);
		if(ret==0){
			break;
		}
		printf("Flush 1 frame.\n");
 
		for(j=0;j<iNal;j++){
	}
    
	x265_encoder_close(pHandle);
	x265_picture_free(pPic_in);
	x265_param_free(pParam);
	free(buff);
	fclose(fp_src);
	fclose(fp_dst);
	return 0;
	}
}
