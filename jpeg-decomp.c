/*
 * jpeg-decomp.c - decompile jpeg image into text form or vice-versa
 * Copyright (C) 2022 Alberto Maccioni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * or see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <ctype.h>
#include "MCU.h"

#define EOF_ERR 		-1000000
#define EOI_MARKER 		-2000000
#define HTAB_ERR 		-3000000
#define RESTART_MARKER 	-4000000
#define bufsize 128

int Rbitcount=0;
char MCUdef[32]="YYCC";		//default MCU composition
int restartInt=-1;
struct marker{ uint8_t type;
				char size;	//0=no segment; 1=segment
				char *shortname;
				char *name;
				} markers[]={ 	
					{0xC0,1,"SOF0","Start of Frame 0"},
					{0xC1,1,"SOF1","Start of Frame 1"},
					{0xC2,1,"SOF2","Start of Frame 2"},
					{0xC3,1,"SOF3","Start of Frame 3"},
					{0xC4,1,"DHT","Define Huffman Table"},
					{0xC5,1,"SOF5","Start of Frame 5"},
					{0xC6,1,"SOF6","Start of Frame 6"},
					{0xC7,1,"SOF7","Start of Frame 7"},
					{0xC8,1,"JPG","JPEG Extensions"},
					{0xC9,1,"SOF9","Start of Frame 9"},
					{0xCA,1,"SOF10","Start of Frame 10"},
					{0xCB,1,"SOF11","Start of Frame 11"},
					{0xCC,1,"DAC","Define Arithmetic Coding"},
					{0xCD,1,"SOF13","Start of Frame 13"},
					{0xCE,1,"SOF14","Start of Frame 14"},
					{0xCF,1,"SOF15","Start of Frame 15"},
					{0xD0,0,"RST0","Restart Marker 0"},
					{0xD1,0,"RST1","Restart Marker 1"},
					{0xD2,0,"RST2","Restart Marker 2"},
					{0xD3,0,"RST3","Restart Marker 3"},
					{0xD4,0,"RST4","Restart Marker 4"},
					{0xD5,0,"RST5","Restart Marker 5"},
					{0xD6,0,"RST6","Restart Marker 6"},
					{0xD7,0,"RST7","Restart Marker 7"},
					{0xD8,0,"SOI","Start of Image"},
					{0xD9,0,"EOI","End of Image"},
					{0xDA,1,"SOS","Start of Scan"},
					{0xDB,1,"DQT","Define Quantization Table"},
					{0xDC,1,"DNL","Define Number of Lines"},
					{0xDD,1,"DRI","Define Restart Interval"},
					{0xDE,1,"DHP","Define Hierarchical Progression"},
					{0xDF,1,"EXP","Expand Reference Component"},
					{0xE0,1,"APP0","Application Segment 0"},
					{0xE1,1,"APP1","Application Segment 1"},
					{0xE2,1,"APP2","Application Segment 2"},
					{0xE3,1,"APP3","Application Segment 3"},
					{0xE4,1,"APP4","Application Segment 4"},
					{0xE5,1,"APP5","Application Segment 5"},
					{0xE6,1,"APP6","Application Segment 6"},
					{0xE7,1,"APP7","Application Segment 7"},
					{0xE8,1,"APP8","Application Segment 8"},
					{0xE9,1,"APP9","Application Segment 9"},
					{0xEA,1,"APP10","Application Segment 10"},
					{0xEB,1,"APP11","Application Segment 11"},
					{0xEC,1,"APP12","Application Segment 12"},
					{0xED,1,"APP13","Application Segment 13"},
					{0xEE,1,"APP14","Application Segment 14"},
					{0xEF,1,"APP15","Application Segment 15"},
					{0xF0,1,"JPG0","JPEG Extension 0"},
					{0xF1,1,"JPG1","JPEG Extension 1"},
					{0xF2,1,"JPG2","JPEG Extension 2"},
					{0xF3,1,"JPG3","JPEG Extension 3"},
					{0xF4,1,"JPG4","JPEG Extension 4"},
					{0xF5,1,"JPG5","JPEG Extension 5"},
					{0xF6,1,"JPG6","JPEG Extension 6"},
					{0xF7,1,"JPG7","JPEG Extension 7"},
					{0xF8,1,"JPG8","JPEG Extension 8"},
					{0xF9,1,"JPG9","JPEG Extension 9"},
					{0xFA,1,"JPG10","JPEG Extension 10"},
					{0xFB,1,"JPG11","JPEG Extension 11"},
					{0xFC,1,"JPG12","JPEG Extension 12"},
					{0xFD,1,"JPG13","JPEG Extension 13"},
					{0xFE,1,"COM","Comment"},
				}; 

//read bit from file
//file=0: reset bitcount
//return value:
//0 	-> bit=0
//1 	-> bit=1
//-1	-> file=0 or EOF reached
//-2	-> EOI marker
//-0xD0..-0xD9: -> RESTART marker #0..9
int getbit(FILE* f){
	static int numbit=0;
	static int r=0;
	static int bitstuffing=0;
	int bit=0;
	if(!f){
		numbit=0;
		bitstuffing=0;
		return -1;
	}
	if(numbit==0){
		r=fgetc(f);
		if(r!=EOF){
			if(r==0xFF){	//bit stuffing or marker?
				int r2=fgetc(f);	//remove bit stuffing
				if(r2==0xD9){
					Rbitcount+=16;
					return -2; //EOI
				}
				if(restartInt>=0&&r2>=0xD0&&r2<=0xD7){
					Rbitcount+=16;
					return -r2; //RESTART marker
				}
				bitstuffing=1;		//increase bit count by 8 bit on next byte
			}
		}
		else return -1;
	}
	Rbitcount++;
	bit=(r&0x80)?1:0;
	r<<=1;
	numbit++;
	if(numbit==8){
		numbit=0;
		if(bitstuffing){	//increase offset when advancing next byte
			Rbitcount+=8;
			bitstuffing=0;
		}
	}
	//printf("r%c",bit?'1':'0');
	return bit;
}

//write bit to file
//file=0:	reset bit count
//bit=-1:	set remaining bits to 1 and force byte write
void putbit(int bit,FILE* f){
	static int numbit=0;
	static int c=0;
	if(!f){
		numbit=0;
		return;
	}
	//printf("w%c",bit?'1':'0');
	if(bit==-1){
		if(numbit!=0){	//force write
			c<<=(8-numbit);
			c|=(1<<(8-numbit))-1;	//fill with 1
			numbit=8;
		}
	}
	else{
		c<<=1;
		c+=bit&1;
		numbit++;
	}
	if(numbit==8){
		fputc(c,f);
		if(c==0xFF) fputc(0x00,f);	//add bit stuffing
		//printf("w%X ",c);
		c=numbit=0;
	}
}

//translate x expressed in n bits to integer according to
//table F.1 in CCIT Rec. T81 document
//size bits |   values
//     0    |     0
//     1    |    -1,1
//     2    |  -3,-2,2,3
//     3    | -7..-4,4..7
//    ...        ...
//    11    | -2047..-1024,1024..2047
int decodeInt(int x, int n){
		return x>=(1<<(n-1))?x:x-(1<<n)+1;
}

//encode x (DC value, max 11 bit) using Huffman table Htable
//result: 0xNNVVVVVV  (NN=total number of bits, VVVVVV=value)
//-1 if x requires more than 11 bits
int encodeH(int Htable[][3],int x){
	int val=x>0?x:-x;
	int n,i;
	for(n=0;val;val>>=1) n++;	//bits required
	if(n>11) return -1;
	for(i=0;Htable[i][2]!=n;i++);	//find prefix that encodes n bits
	if(x<0) x+=(1<<n)-1;			//adjust for negative numbers
	val=(Htable[i][1]<<Htable[i][2])+(x&((1<<n)-1)); //prefix + value
	return (val&0xFFFFFF)+((Htable[i][0]+Htable[i][2])<<24);
}

//decode DC value from file f using Huffman table Htable
//if f2!=0 write encoded value to f2 
//return value:
//EOF_ERR 			-> end of file
//EOI_MARKER 		-> EOI marker
//RESTART_MARKER-(0xD0..0xD7) -> restart marker
//HTAB_ERR 			-> can't find a coefficient; file bit index back to starting point
//[-2047..2047] 	-> DC value correctly decoded
int decodeHvalDC(int Htable[][3],FILE *f,FILE *f2){
	int startAddr=Rbitcount;
	int bit,h,i,j,x,y;
	bit=getbit(f); //printf("%d %X\n",Rbitcount,Rbitcount/8);
	if(bit==-1) return EOF_ERR;
	else if(bit==-2) return EOI_MARKER;	//EOI marker
	else if(bit<-2) return RESTART_MARKER+bit;	//RESTART marker
	x=bit;
	for(int n=1;n<16;){	//max 16 bit
		bit=getbit(f);
		if(bit==-1) return EOF_ERR;
		else if(bit==-2) return EOI_MARKER;	//EOI marker
		else if(bit<-2) return RESTART_MARKER+bit;	//RESTART marker
		x=(x<<1)+bit;	//printf("%d %X\n",Rbitcount,Rbitcount/8);
		h=0;
		n++;
		for(i=0;Htable[i][0]<=n&&h!=-1;i++){
			h=Htable[i][1];		//prefix
			if(n==Htable[i][0]&&x==h){ 	//right prefix
				if(f2) for(j=1<<(n-1);j;j>>=1) putbit(x&j?1:0,f2);
				if(Htable[i][2]==0) return 0;	//0 bit prefix
				y=0;
				for(j=Htable[i][2];j;j--){	//value on j bits
					bit=getbit(f);
					if(bit==-1) return EOF_ERR;
					else if(bit==-2) return EOI_MARKER;	//EOI marker
					else if(bit<-2) return RESTART_MARKER+bit;	//RESTART marker
					y<<=1;
					y+=bit;
				}
				if(f2) for(j=1<<(Htable[i][2]-1);j;j>>=1) putbit(y&j?1:0,f2);
				//printf("DC: %X.%X(%d) #bit:%d+%d = %d\n",h,y,y,Htable[i][0],Htable[i][2],decodeInt(y,Htable[i][2]));
				return decodeInt(y,Htable[i][2]);
			}
		}
	}
	int addr0=startAddr/8;
	fseek(f,addr0,0);
	Rbitcount=addr0*8;
	getbit(0);	//reset bitcount
	for(;Rbitcount<startAddr;getbit(f));	//back to start	
	return HTAB_ERR;
}

#define EOB 0x1000000
#define ZRL 0x2000000
//decode AC value from file f using Huffman table Htable
//if f2!=0 write encoded value to f2 
//return value:
//EOF_ERR 			-> end of file
//EOI_MARKER 		-> EOI marker
//RESTART_MARKER-(0xD0..0xD7) -> restart marker
//HTAB_ERR 			-> can't find a coefficient; file bit index back to starting point
//EOB 				-> end of block
//ZRL 				-> zero run length = 16 zeros
//AC coefficient	-> format: 0xZZXXXX
//		XXXX = coefficient
//		ZZ = number of zeros preceding the coefficient
int decodeHvalAC(int Htable[][3],FILE *f,FILE *f2){
	int startAddr=Rbitcount;
	int bit,h,i,j,x,y,nz;
	bit=getbit(f);
	if(bit==-1) return EOF_ERR;
	else if(bit==-2) return EOI_MARKER;	//EOI marker
	else if(bit<-2) return RESTART_MARKER+bit;	//RESTART marker
	x=bit;
	for(int n=1;n<17;){	//max 16 bit
		bit=getbit(f);
		if(bit==-1) return EOF_ERR;
		else if(bit==-2) return EOI_MARKER;	//EOI marker
		else if(bit<-2) return RESTART_MARKER+bit;	//RESTART marker
		x=(x<<1)+bit;
		h=0;
		n++;
		for(i=0;Htable[i][0]<=n&&h!=-1;i++){
			h=Htable[i][1];
			if(n==Htable[i][0]&&x==h){
				if(f2) for(j=1<<(n-1);j;j>>=1) putbit(x&j?1:0,f2);
				if(Htable[i][2]==0) return EOB; 
				if(Htable[i][2]==0xF0) return ZRL; //Zero run length = 16 zeros
				//code format: [# zeros][# bit]
				y=0;
				nz=Htable[i][2]>>4;
				for(j=Htable[i][2]&0xF;j;j--){
					bit=getbit(f);
					if(bit==-1) return EOF_ERR;
					else if(bit==-2) return EOI_MARKER;	//EOI marker
					else if(bit<-2) return RESTART_MARKER+bit;	//RESTART marker
					y<<=1;
					y+=bit;
				}
				if(f2) for(j=1<<((Htable[i][2]&0xF)-1);j;j>>=1) putbit(y&j?1:0,f2);
				y=decodeInt(y,Htable[i][2]&0xF);
				//printf("Z%d N%d\n",nz,y);
				return (nz<<16)+(y&0xFFFF);	//0xZZXXXX
			}
		}
	}
	int addr0=startAddr/8;
	fseek(f,addr0,0);
	Rbitcount=addr0*8;
	getbit(0);	//reset bitcount
	for(;Rbitcount<startAddr;getbit(f));	//back to start	
	return HTAB_ERR;
}

#define Y_BLOCK 0
#define C_BLOCK 1
#define DECODE_UNKNOWN -1
#define DECODE_ERR 0
#define DECODE_OK 1
#define DECODE_EOI 2
#define DECODE_RESTART 3
#define DECODE_PARTIAL_RESTART 4
//decodifica Y or C block (DC+AC)
//v=0 no messages
//v=1 out on console
//v=2 decoded output on f2
//type=0 Y
//type=1 C
//return value:
// DECODE_UNKNOWN	-> unknown code
// DECODE_ERR 		-> decode error
// DECODE_OK 		-> decode ok
// DECODE_EOI 		-> EOI marker
// DECODE_RESTART 	->RESTART marker (+ restart marker number <<8)
// DECODE_PARTIAL_RESTART 	->partial decoding + RESTART marker (+ restart marker number <<8)
int decodeBlock(FILE*f,FILE*f2,int v,int type){	
	int nz;
	int blockAddr=Rbitcount,endAddr;
	int dccoeff,rst;
	if(type==0)	dccoeff=decodeHvalDC(YDC,f,v==2?0:f2);
	else dccoeff=decodeHvalDC(CDC,f,v==2?0:f2);
	if(dccoeff<-10000||dccoeff>10000){
		if(dccoeff==HTAB_ERR){	//in case of error try advancing 1 bit
			dccoeff=0;
			getbit(f);	//advance 1 bit
			if(v==1) printf("Huffman error (DC)\n");
			if(v==2&&type==0) fprintf(f2,"\n<y>\n//[Y@0x%X.%d] Huffman error -> DC:0 \n0\n</y>",blockAddr>>3,blockAddr&7);
			if(v==2&&type==1) fprintf(f2,"\n<c>\n//[C@0x%X.%d] Huffman error -> DC:0 \n0\n</c>",blockAddr>>3,blockAddr&7);
			return DECODE_ERR;
		}
		if(dccoeff==EOI_MARKER){
			if(v==1) printf("EOI marker\n");
			if(v==2) fprintf(f2,"\n<EOI></EOI>\n");
			return DECODE_EOI;
		}
		if(dccoeff<RESTART_MARKER){
			rst=-dccoeff+RESTART_MARKER-0xD0;
			if(v==1) printf("RESTART marker %d\n",rst);
			if(v==2) fprintf(f2,"\n<restart>%d</restart>",rst);
			return DECODE_RESTART+(rst<<8);
		}
		else{ 
			//printf("Unknown code %X\n",dccoeff);
			return DECODE_UNKNOWN;
		}
	}
	int ACAddr=Rbitcount;
	int	coeff,ncoeff=1;
	char coeffstr[1024]="",str[8];
	if(v==1&&type==0) printf("0x%X.%d Y= %d",blockAddr>>3,blockAddr&7,dccoeff);
	if(v==1&&type==1) printf("0x%X.%d C= %d",blockAddr>>3,blockAddr&7,dccoeff);
	for(coeff=-1;coeff!=EOB&&ncoeff<64;){
		if(type==0)	coeff=decodeHvalAC(YAC,f,v==2?0:f2);
		else coeff=decodeHvalAC(CAC,f,v==2?0:f2);
		if(coeff<0||coeff>0x2000000){
			if(coeff==HTAB_ERR){
				if(v==1) printf("Huffman error (AC)\n");
				getbit(f);	//advance 1 bit
				if(v==1) printf("Huffman error (AC)\n");
				if(v==2&&type==0) fprintf(f2,"\n<y>\n//[Y@0x%X.%d] Huffman error -> DC:0 \n0\n</y>",blockAddr>>3,blockAddr&7);
				if(v==2&&type==1) fprintf(f2,"\n<c>\n//[C@0x%X.%d] Huffman error -> DC:0 \n0\n</c>",blockAddr>>3,blockAddr&7);
				return DECODE_ERR;
			}
			if(coeff==EOI_MARKER){
				if(v==1) printf("EOI marker\n");
				if(v==2) fprintf(f2,"\n<EOI></EOI>\n");
				return DECODE_EOI;
			}
			if(coeff<RESTART_MARKER){
				rst=-coeff+RESTART_MARKER-0xD0;
				if(v==1) printf("RESTART marker %d\n",rst);
				if(v==2){
					if(type==0)	fprintf(f2,"\n<y>\n//[Y@0x%X.%d] DC:%d AC: truncated by restart marker\n%d\n</y>",blockAddr>>3,blockAddr&7,dccoeff,dccoeff);
					else fprintf(f2,"\n<c>\n//[C@0x%X.%d] DC:%d AC: truncated by restart marker\n%d\n</c>",blockAddr>>3,blockAddr&7,dccoeff,dccoeff);
					fprintf(f2,"\n<restart>%d</restart>",rst);
				}
				return DECODE_PARTIAL_RESTART+(rst<<8);
			}
			else{ 
				//printf("Unknown code %X\n",coeff);
				return DECODE_UNKNOWN;
			}
		}
		if(coeff==EOB){
			if(v==1) printf(" EOB");
		}
		else if(coeff==ZRL){
			if(v==1) printf(" 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
			if(v==2) strcat(coeffstr," 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
			ncoeff+=16;
		}
		else{
			nz=(coeff&0xFF0000)>>16;
			ncoeff+=nz+1;
			for(;nz;nz--){
				if(v==1) printf(" 0");
				if(v==2) strcat(coeffstr," 0");
			}
			coeff&=0xFFFF;
			if(coeff&0x1000) coeff|=0xFFFF0000;	//sign extension
			if(v==1) printf(" %d",coeff);
			if(v==2){
				sprintf(str," %d",coeff);
				strcat(coeffstr,str);
			}
		}
	}
	endAddr=Rbitcount;
	if(v==1){
		printf(" (%d bit)\n",endAddr-blockAddr);
		if(ncoeff>64) printf("Too many AC coefficients! (%d)\n",ncoeff);
	}
	if(v==2){
		if(type==0)	fprintf(f2,"\n<y>\n//[Y@0x%X.%d] DC:%d AC:%s\n%d",blockAddr>>3,blockAddr&7,dccoeff,coeffstr,dccoeff);
		else fprintf(f2,"\n<c>\n//[C@0x%X.%d] DC:%d AC:%s\n%d",blockAddr>>3,blockAddr&7,dccoeff,coeffstr,dccoeff);
//		fprintf(f2,"\n//block %d %X, AC %d %X, end %d ",blockAddr,blockAddr/8,ACAddr,ACAddr/8,endAddr);
		int addr0=ACAddr/8;
		fseek(f,addr0,0);
		Rbitcount=addr0*8;
		getbit(0);	//reset bitcount
		for(;Rbitcount<ACAddr;getbit(f));	//start of AC data
		fprintf(f2," 0b");
		for(;Rbitcount<endAddr;) fprintf(f2,"%d",getbit(f));
		if(type==0)	fprintf(f2,"\n</y>");
		else fprintf(f2,"\n</c>");
	}
	return DECODE_OK;
}

//find "<tag>" in file f
//return the position of "<" and copy tag (without <>) in buf
int tag(FILE* f,char* buf,int size){
	if(!f) return -1;
	int r,n=0,pos;
	buf[0]=0;
	for(r=fgetc(f);r!=EOF&&r!='<';r=fgetc(f)){
		if(r=='#') for(r=fgetc(f);r!=EOF&&r!='\n';r=fgetc(f));
		else if(r=='/'){
			r=fgetc(f);
			if(r=='/') for(r=fgetc(f);r!=EOF&&r!='\n';r=fgetc(f));
		}
	}
	if(r=='<'){
		pos=ftell(f);
		for(r=fgetc(f);r!=EOF&&r!='>'&&n<size-2;r=fgetc(f)) buf[n++]=r;
		buf[n]=0;
		if(r=='>') return pos-1;
	}
	return -1;
}

//convert raw data and save it on file f
//returns number of extracted bits
int parseRaw(const char* inbuf,FILE* f){
//example:
//0xFFD8FFE1115C45786966000049492A00080000000C000001040001000000200A
//0b10111010000010011100101110100
	int s=0,n=0;
	uint8_t xx,c;
	for(int i=0;i<strlen(inbuf);i++){
		c=toupper(inbuf[i]);
		switch(s){
			case 0:
				if(c=='0') s=1; //0
				break;
			case 1:
				if(c=='X') s=2;	//0x
				else if(c=='B') s=4;	//0b
				else s=0;
				break;
			case 2:		//hex 1
				if((c>='0'&&c<='9')||(c>='A'&&c<='F')){
					if(c<='9') xx=c-'0';
					else xx=c-'A'+10;
					s=3;
				}
				else s=0;	// -> 0x
				break;
			case 3:		//hex 2
				if((c>='0'&&c<='9')||(c>='A'&&c<='F')){
					if(c<='9') xx=(xx<<4)+c-'0';
					else xx=(xx<<4)+c-'A'+10;
					s=2;
				}
				else s=0;	// -> 0x
				fputc(xx,f);
				n+=8;
				break;
			case 4:		//binary
				n++;
				if(c=='0') putbit(0,f);//printf("0");
				else if(c=='1') putbit(1,f);//printf("1");
				else{
					s=0;	// -> 0x/0b
					n--;
				}
				break;
			default:
				s=0;
		}
	}
	return n;
}

//parse buffer, read DC coefficient, encode it with Huffman table Htable, save on file f
//return the position of the first non-commented line
char* parseDC(char* inbuf,int Htable[][3],FILE* f){
//e.g.:
//[C@0x89C.3] DC:13 AC: -12 1 -1 -2 -2 0 -1 1
//13 0b11000001101101010001100011011001100
	int s=0,i=0,dccoeff=0;
	uint8_t xx,c;
	int len=strlen(inbuf);
	for(i=0;i<len;i++){
		c=inbuf[i];
		if(s==0){
			if(c==' '||c=='\t'||c=='\n') s=0;
			else if(c=='/') s=1;
			else if(c=='#') s=2;
			else{
				sscanf(inbuf+i,"%d",&dccoeff);
				break;
			}
		}
		else if(s==1){
			if(c=='/') s=2;
			else s=0;
		}
		else if(s==2){
			if(c=='\n') s=0;
		}
		else s=0;
	}
	//printf("parseDC: len%d i%d dc%d\n",len,i,dccoeff);
	int e=encodeH(Htable,dccoeff);
	int n=e>>24;	//tot bit
	for(int i=1<<(n-1);i;i>>=1) putbit(e&i?1:0,f);	//MSB first
	return inbuf+i;
	if(i<len+1) return 0;
}

void main (int argc, char **argv) {
	char filein[2000]="",fileout[2000]="",inschar[10000]="";
	int offset=0,bitoffset=0,remoffset=0,scanoffset=0,endoffset=0,sof0=0,drioffset=0;
	int rembit=0,insnum=0,insnumeff=0,ffrem=0,insmcu=0;
	int dc,nz,ncoeff;
	int deltaYDC=0,deltaCDC=0,decodeY=0,decodeC=0,decodeMCU=0,removeMCU=0;
	int prova=0,decode=0,encode=0;
	char c;
	int option_index=0;
	struct option long_options[] =
	{
		{"decode",       no_argument,   &decode, 1},
		{"encode",       no_argument,   &encode, 1},
		{"fin",    required_argument,       0, 'f'},
		{"fout",   required_argument,       0, 'F'},
		{0, 0, 0, 0}
	};
	while ((c = getopt_long_only (argc, argv, "",long_options,&option_index)) != -1)
		switch (c)
		{
			case 'f':	//fin
				strncpy(filein,optarg,sizeof(filein)-1);
				break;
			case 'F':	//fout
				strncpy(fileout,optarg,sizeof(fileout)-1);
				break;
			case '?':
				fprintf (stderr,"option error");
				return;
			default:

				break;
		}
	int bit;
	if(encode==0&&decode==0){
		printf("\
Usage:\n\
-decode or -encode -fin <file> -fout <file>\n");
		return;
	}
	if(!strcmp(filein,fileout)){ 	//in=out
		printf("fileout=filein");
		return;
	}
	FILE* f=fopen(filein,"rb");		//input file
	if(!f) return;
	FILE* f2=0;
	if(fileout[0]){
		f2=fopen(fileout,"wb");
		if(!f2) return;
	}
	char *buf=malloc(offset);
	int r=fread(buf,1,offset,f);
	Rbitcount=offset*8;
//text file tags
// <raw>0x  0b  </raw> <y>1 2 3 4  </y> <c> 1 2 3 4 </c>
// <restart>x<restart>
//<dht>1 2 3 4 </dht> 
	if(decode){				//jpeg -> txt
		int i=0,j,size,dht[]={-1,-1,-1,-1};
		int X=0,Y=0,Mx=0,My=0;
		int Nraw=0,Ny=0,Nc=0;
		printf("Addr     \tMarker\tType\n");
		for(r=fgetc(f);r!=EOF;r=fgetc(f)){
			if(r==0xFF){
				int r2=fgetc(f);
				if(r2!=0){ 
					if(restartInt==-1||(restartInt==0&&!(r2>=0xD0&&r2<=0xD7))) printf("@0x%04X \t0xFF%02X\t",i,r2);	//no RSTX
					for(j=0;j<sizeof(markers)/sizeof(struct marker);j++){
						if(r2==markers[j].type){
							if(markers[j].size==1){
								size=(fgetc(f)<<8)+fgetc(f);
								printf("%s (%d bytes)\n",markers[j].shortname,size);
								for(int k=size-2;k;k--) fgetc(f);
								i+=size;
							}
							else if(restartInt==-1||(restartInt==0&&!(r2>=0xD0&&r2<=0xD7))) printf("%s\n",markers[j].shortname);	//no RSTX
							if(markers[j].type==0xDA) scanoffset=i+2;	//SOS -> start of stream
							if(markers[j].type==0xD9) endoffset=i;		//EOI -> end of image
							if(markers[j].type==0xDD&&size==4){			//DRI define restart interval
								drioffset=i;
								restartInt=0;		
							}
							if(markers[j].type==0xC0) sof0=i-size+4;	//SOF0 P
							if(markers[j].type==0xC4){
								int h;
								for(h=0;h<4&&dht[h]!=-1;h++);
								dht[h]=i-size+2;				//DHT "Define Huffman Table"
							}
							break;
						}
					}
					if(j==sizeof(markers)/sizeof(struct marker)) printf("??\n");
				}
				i++;
			}
			i++;
		}
		if(endoffset==0) endoffset=ftell(f);
		fflush(stdout);
		if(f2){
			if(sof0){		//start of frame
				MCUdef[0]=0;
				fseek(f,sof0,0);
				printf("Precision=%d",fgetc(f));
				Y=(fgetc(f)<<8)+fgetc(f);
				X=(fgetc(f)<<8)+fgetc(f);
				int comp=fgetc(f);
				int mcuPixX=0,mcuPixY=0;
				printf(" %dx%d %d components:\n",X,Y,comp);
				fprintf(f2,"// %dx%d %d components:\n",X,Y,comp);
				for(;comp;comp--){
					int id=fgetc(f);
					int sfact=fgetc(f);
					int dest=fgetc(f);
					char type[2]={0,0};
					if(dest==0) type[0]='Y';
					if(dest==1) type[0]='C';
					printf("ID:%d [%02X] Dest:%d\n",id,sfact,dest);
					fprintf(f2,"//ID:%d [%02X] Dest:%d\n",id,sfact,dest);
					for(int n=(sfact>>4)*(sfact&0xF);n;n--) strncat(MCUdef,type,sizeof(MCUdef)-1);
					if((sfact>>4)>mcuPixX) mcuPixX=(sfact>>4);
					if((sfact&0xF)>mcuPixY) mcuPixY=(sfact&0xF);
				}
				mcuPixX*=8;
				mcuPixY*=8;
				printf("MCU: %s (%dx%d pixel)\n",MCUdef,mcuPixX,mcuPixY);
				fprintf(f2,"//MCU: %s (%dx%d pixel)\n",MCUdef,mcuPixX,mcuPixY);
				Mx=0.5+(float)X/(float)mcuPixX;
				My=0.5+(float)Y/(float)mcuPixY;
				printf("[%dx%d=%d MCU]\n",Mx,My,Mx*My);
				fprintf(f2,"//[%dx%d=%d MCU]\n",Mx,My,Mx*My);
			}
			if(drioffset){						//define restart interval
				fseek(f,drioffset,0);
				X=(fgetc(f)<<8)+fgetc(f);
				printf("Restart interval: %d\n",X);
				fprintf(f2,"//Restart interval: %d\n",X);
				restartInt=X;
			}
			for(int h=0;h<4&&dht[h]!=-1;h++){	//define huffman table
				fseek(f,dht[h],0);
				size=(fgetc(f)<<8)+fgetc(f)-2;	//2 bytes less to exclude size
				uint8_t table[size];
				fread(table,size,1,f);
				int (*HTX)[3]=0;
				if(size==sizeof(HT0)&&!memcmp(table,HT0,size)) printf("standard Huffman table @0x%X\n",dht[h]+2);
				else if(size==sizeof(HT1)&&!memcmp(table,HT1,size)) printf("standard Huffman table (Y DC) @0x%X\n",dht[h]+2);
				else if(size==sizeof(HT2)&&!memcmp(table,HT2,size)) printf("standard Huffman table (Y AC) @0x%X\n",dht[h]+2);
				else if(size==sizeof(HT3)&&!memcmp(table,HT3,size)) printf("standard Huffman table (C DC) @0x%X\n",dht[h]+2);
				else if(size==sizeof(HT4)&&!memcmp(table,HT4,size)) printf("standard Huffman table (C AC) @0x%X\n",dht[h]+2);
				else{
					//printf("DHT: %dB\n",size);
					int huffsize[256];
					int fullcode[256];
					for(int z=0;z<size;){
						//printf("Dest: %d %s\n",table[z]&0xF,(table[z]>>4)?"AC":"DC");
						if((table[z]&0xF)==0&&(table[z]>>4)==0) HTX=YDC;
						else if((table[z]&0xF)==0&&(table[z]>>4)==1) HTX=YAC;
						else if((table[z]&0xF)==1&&(table[z]>>4)==0) HTX=CDC;
						else if((table[z]&0xF)==1&&(table[z]>>4)==1) HTX=CAC;
						//printf("%p %p %p %p %p\n",HTX,YDC,YAC,CDC,CAC);
						int k=17,j,ncode=0;
						for(j=0;j<256;j++) huffsize[j]=0;
						for(int i=0;i<16;i++){
							for(j=0;j<table[z+1+i];j++){
								huffsize[k-17+j]=i+1;
								fullcode[k-17+j]=table[z+k+j];
								ncode++;
							}
							k+=j;
						}
						z+=k;
						//if(z>29) continue;
						//ISO/IEC 10918-1 : 1993(E) Figure C.2 – Generation of table of Huffman codes
						k=0;
						int code=0;
						int si=huffsize[0];
						int huffcode[256];
						do{
							do{
								huffcode[k]=code;
								code++;
								k++;
							} while (huffsize[k]==si);
							if(huffsize[k]==0) break;
							do{
								code<<=1;
								si++;
							} while (huffsize[k]!=si);
						} while (huffsize[k]);
						//end C.2
						//printf("[#bit,prefix,code]\n");
						for(int i=0;i<ncode;i++){
							//printf("%d,%X,%X\n",huffsize[i],huffcode[i],fullcode[i]);
							HTX[i][0]=huffsize[i];
							HTX[i][1]=huffcode[i];
							HTX[i][2]=fullcode[i];
						}
						HTX[ncode][0]=-1;
						HTX[ncode][1]=-1;
						HTX[ncode][2]=-1;
					}
					fprintf(f2,"<dht>\n");
					if(HTX==YDC) fprintf(f2,"YDC ");
					else if(HTX==YAC) fprintf(f2,"YAC ");
					else if(HTX==CDC) fprintf(f2,"CDC ");
					else if(HTX==CAC) fprintf(f2,"CAC ");
					if(HTX) for(int i=0;HTX[i][0]!=-1;i++) fprintf(f2,"[%X %X %X]",HTX[i][0],HTX[i][1],HTX[i][2]);
					fprintf(f2,"\n</dht>\n");
				}
			}
			fseek(f,0,0);
			if(scanoffset){	//copy first data as raw
				fprintf(f2,"<raw>");
				for(int p=0;p<scanoffset;p++){
					if(p%32==0) fprintf(f2,"\n0x");
					fprintf(f2,"%02X",fgetc(f));
				}
				fprintf(f2,"\n</raw>");
				fflush(stdout);
			}
			Rbitcount=scanoffset*8;
			int mcucount=0,decode_result,restartCount=0;
			int rstErrStat[50],rstErrStat_extra=0,errnum,next_rstnum=0,rst;
			int iblock=0;
			for(int i=0;i<50;i++) rstErrStat[i]=0;
			for(;Rbitcount<endoffset*8-16;){	//decode MCU (-2 bytes to end at last MCU)
				if(iblock==0) fprintf(f2,"\n//************ MCU %d (%d,%d) (@0x%X.%d):",mcucount,mcucount%Mx,mcucount/Mx,Rbitcount>>3,Rbitcount&7);
				if(MCUdef[iblock]=='Y')	decode_result=decodeBlock(f,f2,2,Y_BLOCK);
				else if(MCUdef[iblock]=='C')	decode_result=decodeBlock(f,f2,2,C_BLOCK);
				rst=decode_result>>8;
				if((decode_result&0xF)==DECODE_PARTIAL_RESTART){
					if(MCUdef[iblock]=='Y') Ny++;
					if(MCUdef[iblock]=='C') Nc++;
					iblock++;
				}					
				if((decode_result&0xF)==DECODE_RESTART||(decode_result&0xF)==DECODE_PARTIAL_RESTART){
					if(restartCount<restartInt){
						if(iblock<strlen(MCUdef)) fprintf(f2,"\n//Restart interval error (%d: %d MCU instead of %d)",restartCount-restartInt,restartCount,restartInt);
					}
					restartCount=0;
					if(iblock>0){
						mcucount++;
						if(iblock<strlen(MCUdef)) fprintf(f2,"\n//MCU error: missing component  (%d instead of %d)",iblock,strlen(MCUdef));
						iblock=0;
					}
					if(rst!=next_rstnum){
						fprintf(f2,"\n//Restart marker # error (%d instead of %d)",rst,next_rstnum);
					}
					next_rstnum=rst+1;
					if(next_rstnum>7) next_rstnum=0;
				}
				else if((decode_result&0xF)==DECODE_OK||(decode_result&0xF)==DECODE_ERR){
					if(restartInt>0&&iblock==0){	//on new MCU only
						restartCount++;
						errnum=restartCount-restartInt;
						if(errnum>0){
							fprintf(f2,"\n//Restart interval error (+%d: %d MCU instead of %d)",errnum,restartCount,restartInt);
							if(errnum<50) rstErrStat[errnum]++;
							else rstErrStat_extra=1;
						}
					}
					if(MCUdef[iblock]=='Y') Ny++;
					if(MCUdef[iblock]=='C') Nc++;
					iblock++;
					if(iblock>=strlen(MCUdef)){
						iblock=0;
						mcucount++;
					}					
				}
				else if(decode_result==DECODE_EOI);
				else{
					printf("MCU decoding error (0x%X)\n",decode_result);
					//break;
				}					
			}
			fprintf(f2,"\n<EOI></EOI>\n");
			printf("found %d MCU (%d Y + %d C)\n",mcucount,Ny,Nc);
			fprintf(f2,"//found %d MCU (%d Y + %d C)\n",mcucount,Ny,Nc);
			if(restartInt>0){
				int e=0;
				//convert to absolute chains
				for(int i=49;i>0;i--){
					for(int j=i-1;rstErrStat[i]&&j>0;j--){
						rstErrStat[j]-=rstErrStat[i];
					}
					if(rstErrStat[i]) e=1;
				}
				if(e){
					printf("Missing restart markers\nL \t#\n");
					for(int i=1;i<50;i++){
						if(rstErrStat[i]) printf("%d\t%d\n",i,rstErrStat[i]);
					}
					if(rstErrStat_extra) printf(">49\t>0\n");
				}
			}
		}
	}
	else if(encode&&f&&f2){			//text -> jpeg
		int tagstart,tagend;
		int Nraw=0,Ny=0,Nc=0;
		char tagbuf[128];
		#define tsize sizeof(tagbuf)
		int YAC_EOB_I=-1,CAC_EOB_I=-1;
		for(int i=0;YAC_EOB_I==-1&&YAC[i][2]!=-1;i++) if(YAC[i][2]==0) YAC_EOB_I=i;		//EOB code
		for(int i=0;CAC_EOB_I==-1&&CAC[i][2]!=-1;i++) if(CAC[i][2]==0) CAC_EOB_I=i;		//EOB code
		if(YAC_EOB_I==-1||CAC_EOB_I==-1) return; 
		//printf("Y eob: %d %X %X\n",YAC[YAC_EOB_I][0],YAC[YAC_EOB_I][1],YAC[YAC_EOB_I][2]);
		//printf("Y zrl: %d %X %X\n",YAC[YAC_ZRL_I][0],YAC[YAC_ZRL_I][1],YAC[YAC_ZRL_I][2]);
		putbit(0,0);	//reset bit count
		tagstart=tag(f,tagbuf,tsize);
		for(;tagstart>=0;tagstart=tag(f,tagbuf,tsize)){
			//printf("%d: tag= %s\n",tagstart,tagbuf);
			if(!strcmp(tagbuf,"raw")){		//<raw>
				tagend=tag(f,tagbuf,tsize);
				if(tagend&&!strcmp(tagbuf,"/raw")){
					Nraw++;
					//printf("R %d: tag= %s\n",tagend,tagbuf);
					int taglen=tagend-tagstart-5;
					fseek(f,tagstart+5,0);
					char* inbuf=malloc(taglen+1);
					fread(inbuf,taglen,1,f);
					inbuf[taglen]=0;
					//printf("R-->%s<--\n",inbuf);
					int n=parseRaw(inbuf,f2);
					//printf("Raw: %d byte\n",n/8);
					free(inbuf);
					fseek(f,tagend+5,0);
				}
			}
			else if(!strcmp(tagbuf,"y")){		//<y>
				tagend=tag(f,tagbuf,tsize);
				if(tagend&&!strcmp(tagbuf,"/y")){
					Ny++;
					//printf("Y %d: tag= %s\n",tagend,tagbuf);
					int taglen=tagend-tagstart-3;
					fseek(f,tagstart+3,0);
					char* inbuf=malloc(taglen+1);
					fread(inbuf,taglen,1,f);
					inbuf[taglen]=0;
					//printf("Y-->%s<--\n",inbuf);
					char* p=parseDC(inbuf,YDC,f2);
					int n=parseRaw(p,f2);
					//printf("p%p AC: %d bit\n",p,n);
					if(n==0){	//no AC data: EOB code
						//printf("%d Y EOB %d bit %X\n",Ny,YAC[YAC_EOB_I][0],YAC[YAC_EOB_I][1]);
						if(YAC_EOB_I!=-1) for(int j=1<<(YAC[YAC_EOB_I][0]-1);j;j>>=1) putbit(YAC[YAC_EOB_I][1]&j?1:0,f2);
					}
					free(inbuf);
					fseek(f,tagend+2,0);
				}
			}
			else if(!strcmp(tagbuf,"c")){		//<c>
				tagend=tag(f,tagbuf,tsize);
				if(tagend&&!strcmp(tagbuf,"/c")){
					Nc++;
					//printf("C %d: tag= %s\n",tagend,tagbuf);
					int taglen=tagend-tagstart-3;
					fseek(f,tagstart+3,0);
					char* inbuf=malloc(taglen+1);
					fread(inbuf,taglen,1,f);
					inbuf[taglen]=0;
					//printf("C-->%s<--\n",inbuf);
					char* p=parseDC(inbuf,CDC,f2);
					int n=parseRaw(p,f2);
					//printf(" AC: %d bit\n",n);
					if(n==0){	//no AC data: EOB code
						for(int j=1<<(CAC[CAC_EOB_I][0]-1);j;j>>=1) putbit(CAC[CAC_EOB_I][1]&j?1:0,f2);
					}
					free(inbuf);
					fseek(f,tagend+2,0);
				}
			}
			else if(!strcmp(tagbuf,"restart")){		//<restart>
				tagend=tag(f,tagbuf,tsize);
				int len=strlen(tagbuf);
				if(tagend&&!strcmp(tagbuf,"/restart")){
					int taglen=tagend-tagstart-len-1;
					fseek(f,tagstart+len+1,0);
					char* inbuf=malloc(taglen+1);
					fread(inbuf,taglen,1,f);
					inbuf[taglen]=0;
					int res_marker=0;
					sscanf(inbuf,"%d",&res_marker);
					putbit(-1,f2);	//fill byte
					fputc(0xFF,f2);
					fputc(0xD0+res_marker,f2);					
					free(inbuf);
					fseek(f,tagend+len+2,0);
				}
			}
			else if(!strcmp(tagbuf,"dht")){		//<dht> </dht> len=3
				tagend=tag(f,tagbuf,tsize);
				int len=strlen(tagbuf);
				if(tagend&&!strcmp(tagbuf,"/dht")){
					int taglen=tagend-tagstart-len-1;
					fseek(f,tagstart+len+1,0);
					char* inbuf=malloc(taglen+1);
					fread(inbuf,taglen,1,f);
					inbuf[taglen]=0;
					char* ht[128];
					int (*HTX)[3]=0;
					sscanf(inbuf,"%s",ht);
					if(!strcmp(ht,"YDC")) HTX=YDC; 
					if(!strcmp(ht,"YAC")) HTX=YAC;
					if(!strcmp(ht,"CDC")) HTX=CDC;
					if(!strcmp(ht,"CAC")) HTX=CAC;
					char *p;
					int x,y,z,j=0;
					for(p=strtok(inbuf,"[]");p!=NULL;p=strtok(NULL,"[]")){
						if(sscanf(p,"%x %x %x",&x,&y,&z)==3){
							//printf("[%X %X %X]",x,y,z);
							HTX[j][0]=x;
							HTX[j][1]=y;
							HTX[j][2]=z;
							j++;
						}
					}
					HTX[j][0]=-1;
					HTX[j][1]=-1;
					HTX[j][2]=-1;
					//printf("\n");
					free(inbuf);
					fseek(f,tagend+len+2,0);
				}
			}
		}
		putbit(-1,f2);	//fill byte with 1 and write to file
		fputc(0xFF,f2);
		fputc(0xD9,f2);
		printf("%d raw segments\n%d y segments\n%d c segments",Nraw,Ny,Nc);
	}
	return;
}

