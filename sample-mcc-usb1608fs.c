#include <stdio.h>
#include <stdlib.h>

#include "pmd.h"
#include "usb-1608FS.h"

#define CHANNEL_MODE_ANALOG                0
#define CHANNEL_MODE_FREQUENCY_FROM_ANALOG 1

#define VOLTAGE_LOGIC_HIGH 3.0
#define VOLTAGE_LOGIC_LOW  1.0

typedef struct { 
	int mode;
	double m;	/* m value to scale voltage to get actual output */
	double b;	/* b value to add to voltage to get actual output */
	int gain;	/* USB-1608FS gain setting for this ADC input */

	int nSamples;

	/* state variables for counting edges */
	int lastLogicLevel;
	int nFallingEdges;

	/* stats for analog */
	double vMin;
	double vMax;
	double vSum;

	/* calculated after sampling is done */
	double frequency;
	double vAvg;

} struct_channel;
struct_channel ch[NCHAN_USB1608FS];

void init_channels(void) {
	for ( int i=0 ; i<NCHAN_USB1608FS ; i++ ) {
		ch[i].mode=CHANNEL_MODE_ANALOG;
		ch[i].m=1.0;
		ch[i].b=0.0;
		ch[i].gain=BP_10_00V;

		ch[i].nSamples=1.0;
		ch[i].lastLogicLevel=0;
		ch[i].vMin=100.0;
		ch[i].vMax=-100.0;
		ch[i].vSum=0.0;

		ch[i].frequency=0.0;
		ch[i].vAvg=0.0;
	}
}

int main (int argc, char **argv) {
	int i,j;
	int nSample;
	uint8_t gainArray[NCHAN_USB1608FS];
	uint16_t data[1000*NCHAN_USB1608FS*2];
	int count;
	int options;
	float freq;

	libusb_device_handle *udev = NULL;
	int ret;
	Calibration_AIN table_AIN[NGAINS_USB1608FS][NCHAN_USB1608FS];


	/* configure our channels */
	init_channels();
	/* rectifier DC voltage */
	ch[0].mode=CHANNEL_MODE_ANALOG;
	ch[0].m=20.0;
	ch[0].b=0.0;
	ch[0].gain=BP_5_00V;

	/* rectifier DC current */
	ch[1].mode=CHANNEL_MODE_ANALOG;
	ch[1].m=1750.0;
	ch[1].b=0.0;
	ch[1].gain=BP_1_00V;

	/* field DC voltage */
	ch[2].mode=CHANNEL_MODE_ANALOG;
	ch[2].m=20.0;
	ch[2].b=0.0;
	ch[2].gain=BP_5_00V;

	/* field DC current */
	ch[3].mode=CHANNEL_MODE_ANALOG;
	ch[3].m=1750.0;
	ch[3].b=0.0;
	ch[3].gain=BP_1_00V;

	/* turbine output frequency */
	ch[4].mode=CHANNEL_MODE_FREQUENCY_FROM_ANALOG;
	ch[4].m=1.0;
	ch[4].b=0.0;
	ch[4].gain=BP_5_00V;
	ch[4].lastLogicLevel=0;
	ch[4].nFallingEdges=0;

	/* dyno shaft RPM */
	ch[5].mode=CHANNEL_MODE_FREQUENCY_FROM_ANALOG;
	ch[5].m=1.0;
	ch[5].b=0.0;
	ch[5].gain=BP_5_00V;
	ch[4].lastLogicLevel=0;
	ch[4].nFallingEdges=0;

	/* turbine temperature via IR thermocouple */
	ch[6].mode=CHANNEL_MODE_ANALOG;
	ch[6].m=100.0;
	ch[6].b=0.0;
	ch[6].gain=BP_2_00V;

	/* strain gauge on dyno output shaft */
	ch[7].mode=CHANNEL_MODE_ANALOG;
	ch[7].m=1.0;
	ch[7].b=0.0;
	ch[7].gain=BP_5_00V;


	udev = 0;
	ret = libusb_init(NULL);
	if ( ret < 0 ) {
		perror("libusb_init: Failed to initialize libusb");
		exit(1);
	}


	if ( (udev = usb_device_find_USB_MCC(USB1608FS_PID, NULL)) ) {
		printf("USB-1608FS Device is found!\n");
	} else {
		printf("No device found.\n");
		exit(0);
	}
 
	fprintf(stderr,"# Downloading and building calibration table from device ...\n");
	usbBuildCalTable_USB1608FS(udev, table_AIN);

	// print the table
	for (i = 0; i < NGAINS_USB1608FS; i++) {
		for (j = 0; j < NCHAN_USB1608FS; j++) {
			fprintf(stderr,"calibration table_AIN[%d][%d].slope = %f    table_AIN[%d][%d].offset = %f\n", i, j, table_AIN[i][j].slope, i, j, table_AIN[i][j].offset);
		}	
	}

	/* analog scan */
	/* sample rate */
	freq=1000.0;
	/* nSamples */
	count= 1000; // * NCHAN_USB1608FS;
	/* PGA gain (10V, 5V, 2V, 1V) */
	/*
        case 1: gain = BP_10_00V;
        case 2: gain = BP_5_00V;
        case 4: gain = BP_2_00V;
        case 6: gain = BP_1_00V;
	*/

	usbAInStop_USB1608FS(udev);

	/* build "gain queue" to send to USB-1608FS */
	for ( i=0 ; i<NCHAN_USB1608FS ; i++ ) {
		gainArray[i]=ch[i].gain;
	}
	

	/* load gains into DAQ (and program PGA?) */
	usbAInLoadQueue_USB1608FS(udev, gainArray);

	// configure options
	// options = AIN_EXECUTION | AIN_TRANSFER_MODE;
	options = AIN_EXECUTION;
	//options = AIN_EXECUTION | AIN_DEBUG_MODE;

	/* start the scan */
	fprintf(stderr,"# starting analog scan (frequency=%.f count=%d)\n",freq,count);
	usbAInScan_USB1608FS(udev, 0, 7, count, &freq, options, data);
	/* done with acquire, now we know how it actually did */
	fprintf(stderr,"# DAQ scan complete (actual frequency = %f)\n", freq);

	for ( nSample=0 ; nSample < count ; nSample++ ) {
		for ( int nChannel=0; nChannel < NCHAN_USB1608FS ; nChannel++ ) {
			uint16_t dat;	/* raw 16 bit value from ADC */
			int16_t sdat;	/* scaled using DAQ calibration and gain and sign extended */

			dat = data[ nSample*NCHAN_USB1608FS + nChannel];

			switch ( ch[nChannel].gain ) {
				case BP_10_00V:
					sdat = (int) (table_AIN[0][nChannel].slope*((float) dat) + table_AIN[0][nChannel].offset);
					break;
				case BP_5_00V:
					sdat = (int) (table_AIN[1][nChannel].slope*((float) dat) + table_AIN[1][nChannel].offset);
					break;
				case BP_2_00V:
					sdat = (int) (table_AIN[2][nChannel].slope*((float) dat) + table_AIN[2][nChannel].offset);
					break;
				case BP_1_00V:
					sdat = (int) (table_AIN[3][nChannel].slope*((float) dat) + table_AIN[3][nChannel].offset);
					break;
				default:
					break;
			}

			if (sdat >= 0x8000) {
				sdat -=  0x8000;
			} else {
				sdat = (0x8000 - sdat);
				sdat *= (-1);
			}
		
			double volts=volts_USB1608FS(ch[nChannel].gain, sdat);

			ch[nChannel].nSamples++;
			ch[nChannel].vSum += volts;
			if ( volts < ch[nChannel].vMin ) {
				ch[nChannel].vMin=volts;
			}
			if ( volts > ch[nChannel].vMax ) {
				ch[nChannel].vMax=volts;
			}

			double value = ch[nChannel].m * volts + ch[nChannel].b;
//			fprintf(stderr,"# value=%.4f raw data = %#x   data[%d] = %#hx  %.4fV\n", value, data[i], i, sdat, volts);
//			fprintf(stderr,"%d[%d], %.4f, %d, %d, %.4f\n",nSample,nChannel,value,data[nSample*8+nChannel],sdat,volts);

			if ( CHANNEL_MODE_FREQUENCY_FROM_ANALOG == ch[nChannel].mode ) {
				printf("frequency_from_analog[%d] - nSample=%d - volts=%0.1f - ",nSample,nChannel,volts);
				if ( 1==ch[nChannel].lastLogicLevel && volts <= VOLTAGE_LOGIC_LOW ) {
					/* were logic level high, now are low ... got a falling edge */
					ch[nChannel].lastLogicLevel=0;
					ch[nChannel].nFallingEdges++;
					printf("now low and was high ... falling edge\n");
				} else if ( volts >= VOLTAGE_LOGIC_HIGH ) {
					ch[nChannel].lastLogicLevel=1;
					printf("high\n");
				} else {
					printf("no action\n");
				}
			}
		}
	}
	fprintf(stderr,"# sampleN, sensor value, raw data, scaled raw data, volts\n");

	/* calculate statistics now that we are done */
	fprintf(stderr,"# channel statistics:\n");
	for ( i=0 ; i<NCHAN_USB1608FS ; i++ ) {
		if ( CHANNEL_MODE_FREQUENCY_FROM_ANALOG == ch[i].mode ) {
			/* calculate frequency based on our gate time */
			ch[i].frequency = ch[i].nFallingEdges / (count / freq);

			fprintf(stderr,"#\t[%d] %d frequency=%0.1f\n",i,ch[i].nFallingEdges,ch[i].frequency);
		}

		ch[i].vAvg=ch[i].vSum / ( (double) ch[i].nSamples);

		fprintf(stderr,"#\t[%d] vMin=%0.4f vMax=%0.4f vAvg=%0.4f nSamples=%d\n",i,ch[i].vMin,ch[i].vMax,ch[i].vAvg,ch[i].nSamples);
	}

	usbAInStop_USB1608FS(udev);
	usbReset_USB1608FS(udev);

	for (i = 2; i <= 6; i++ ) {
		libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | i);
	}

	for (i = 0; i <= 6; i++) {
		libusb_release_interface(udev, i);
	}
	libusb_close(udev);

	return 0;
}
