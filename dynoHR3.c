#define _GNU_SOURCE

#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include <modbus.h>
#include "pmd.h"
#include "usb-1608FS.h"

#define modbus_host "192.168.10.220"
#define modbus_port 502
#define modbus_slave 1


typedef struct { 
	int mode;
	double m;	/* m value to scale voltage to get actual output */
	double b;	/* b value to add to voltage to get actual output */
	int gain;	/* USB-1608FS gain setting for this ADC input */
			/* PGA gain (10V, 5V, 2V, 1V) = { BP_10_00V, BP_5_00V, BP_2_00V, BP_1_00V) */


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


/* VFD modbus globals */
modbus_t *mb;
/* USB DAQ globals */
libusb_device_handle *udev = NULL;
Calibration_AIN table_AIN[NGAINS_USB1608FS][NCHAN_USB1608FS];
int commandedRPM=-1;


/* output file globals */
FILE *fp_raw;
FILE *fp_stats;

#define CHANNEL_MODE_ANALOG                0
#define CHANNEL_MODE_FREQUENCY_FROM_ANALOG 1

#define VOLTAGE_LOGIC_HIGH 3.0
#define VOLTAGE_LOGIC_LOW  1.0

/* Dyno and testing configuration */
/* physical dyno configuration */
#define DYNO_CONFIG_HZ_PER_RPM	(1.0/8.8)	/* Hz on drive motor per RPM on output shaft */

/* DAQ configuration */
#define DAQ_SAMPLE_FREQ 1000	/* Hz */
#define DAQ_SAMPLE_COUNT 1000	/* number of samples of each channel */

/* dyno sweep specifications */
#define DYNO_RPM_START	50	/* RPM */
#define DYNO_RPM_END	325	/* RPM to end at */
#define DYNO_RPM_STEP	5	/* RPM step size */
#define	DYNO_RPM_WAIT	2	/* seconds to wait for dyno to reach RPM */


void init_channel_stats(void) {
	fprintf(stderr,"# init_channel_stats()\n");
	for ( int i=0 ; i<NCHAN_USB1608FS ; i++ ) {
		ch[i].nSamples=0;
		ch[i].lastLogicLevel=0;
		ch[i].nFallingEdges=0;
		ch[i].vMin=100.0;
		ch[i].vMax=-100.0;
		ch[i].vSum=0.0;

		ch[i].frequency=0.0;
		ch[i].vAvg=0.0;
	}
}

void init_channels(void) {
	fprintf(stderr,"# init_channels()\n");
	for ( int i=0 ; i<NCHAN_USB1608FS ; i++ ) {
		ch[i].mode=CHANNEL_MODE_ANALOG;
		ch[i].m=1.0;
		ch[i].b=0.0;
		ch[i].gain=BP_10_00V;
	}

	/* init_channel_stats() needs to be called before each scan */
}

void set_channels() {
	/* configure our channels */
	init_channels();
	/* rectifier DC voltage */
	ch[0].mode=CHANNEL_MODE_ANALOG;
	ch[0].m=20.0;
	ch[0].b=0.0;
	ch[0].gain=BP_5_00V;

	/* rectifier DC current */
	ch[1].mode=CHANNEL_MODE_ANALOG;
	ch[1].m=175.0;
	ch[1].b=0.0;
	ch[1].gain=BP_1_00V;

	/* field DC voltage */
	ch[2].mode=CHANNEL_MODE_ANALOG;
	ch[2].m=20.0;
	ch[2].b=0.0;
	ch[2].gain=BP_5_00V;

	/* field DC current */
	ch[3].mode=CHANNEL_MODE_ANALOG;
	ch[3].m=175.0;
	ch[3].b=0.0;
	ch[3].gain=BP_1_00V;

	/* turbine output frequency */
	ch[4].mode=CHANNEL_MODE_FREQUENCY_FROM_ANALOG;
	ch[4].m=1.0;
	ch[4].b=0.0;
	ch[4].gain=BP_5_00V;

	/* dyno shaft RPM */
	ch[5].mode=CHANNEL_MODE_FREQUENCY_FROM_ANALOG;
	ch[5].m=1.0;
	ch[5].b=0.0;
	ch[5].gain=BP_5_00V;

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


}



int vfd_connect() {
	/* connect to modbus gateway */
	mb = modbus_new_tcp(modbus_host, modbus_port);

//	modbus_set_debug(mb,TRUE);
	modbus_set_error_recovery(mb,MODBUS_ERROR_RECOVERY_LINK);

	if ( NULL == mb ) {
		fprintf(stderr,"# error creating modbus instance: %s\n", modbus_strerror(errno));
		return -1;
	}

	if ( -1 == modbus_connect(mb) ) {
		fprintf(stderr,"# modbus connection failed: %s\n", modbus_strerror(errno));
		modbus_free(mb);
		return -2;
	}

	/* set slave address of device we want to talk to */
	if ( 0 != modbus_set_slave(mb,modbus_slave) ) {
		fprintf(stderr,"# modbus_set_slave() failed: %s\n", modbus_strerror(errno));
		modbus_free(mb);
		return -3;
	}


	return 0;
}


/* set parameters on VFD so we can control it programattically */
int vfd_gs3_set_automated_parameters() {
//	uint16_t value = 0;

	/* Set P3.00 "source of operation" to 0x03 "RS-485 with keypad STOP enabled"*/
	/* Set P4.00 "source of frequency command" to 0x05 "RS-485" */
	/* what else is needed? */

	return 0;
}

/* set parameters on VFD back to manual control through its front keypad interface */
int vfd_gs3_clear_automated_parameters() {
//	uint16_t value = 0;

	return 0;
}

/* set VFD output frequency corresponding to desired RPM */
int vfd_gs3_set_rpm(int rpm) {
	uint16_t value = 0;
	double d;

	commandedRPM=rpm;

	/* output shaft has reduction, so we use a constant to get from motor Hz to output RPM */
	d = rpm * DYNO_CONFIG_HZ_PER_RPM;
	/* VFD expects frequency in deciHz */
	d *= 10.0;
	d += 0.5; /* so we round */
	value=(uint16_t) d;


	fprintf(stderr,"# vfd_gs3_set_rpm() rpm=%d d=%f value to write=%d\n",rpm,d,value);

	/* Set P3.16 "desired frequency" to RPM scaled to Hz */
	/* forward direction */
	if ( -1 == modbus_write_register(mb,2330,value) ) {
		fprintf(stderr,"# modbus_write_register() failed in vfd_gs3_set_rpm() with:\n%s\n", modbus_strerror(errno));
		return -1;
	}

	return 0;

}

/* set VFD rotation direction to FWD and mode to run */
int vfd_gs3_command_run() {
	uint16_t value = 0;

	/* forward direction */
	value=0;
	if ( -1 == modbus_write_register(mb,2332,value) ) {
		fprintf(stderr,"# modbus_write_register() failed in vfd_gs3_command_run() setting forward direction with with:\n%s\n", modbus_strerror(errno));
		return -1;
	}

	/* run mode */
	value=1;
	if ( -1 == modbus_write_register(mb,2331,value) ) {
		fprintf(stderr,"# modbus_write_register() failed in vfd_gs3_command_run() setting mode run with: %s\n", modbus_strerror(errno));
		return -1;
	}

	return 0;
}

/* set VFD mode to stop */
int vfd_gs3_command_stop() {
	uint16_t value = 0;

	/* stop mode */
	value=0;
	if ( -1 == modbus_write_register(mb,2331,value) ) {
		fprintf(stderr,"# modbus_write_register() failed in vfd_gs3_command_run() setting mode stop with: %s\n", modbus_strerror(errno));
		return -1;
	}

	return 0;
}



/* acquire analog data from USB-1608FS DAQ */
int daq_acquire(void) {
	int i;
	int nSample;
	uint8_t gainArray[NCHAN_USB1608FS];
	uint16_t data[1000*NCHAN_USB1608FS*2];
	int options;
	struct timeval tv;

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


	/* get unix timestamp for this measurement */
	gettimeofday(&tv, NULL);

	/* start the scan */
	float freq = DAQ_SAMPLE_FREQ;
	fprintf(stderr,"# starting analog scan (frequency=%.f count=%d)\n",freq,DAQ_SAMPLE_COUNT);
	usbAInScan_USB1608FS(udev, 0, 7, DAQ_SAMPLE_COUNT, &freq, options, data);
	/* done with acquire, now we know how it actually did */
	fprintf(stderr,"# DAQ scan complete (actual frequency = %f)\n", freq);

	for ( nSample=0 ; nSample < DAQ_SAMPLE_COUNT ; nSample++ ) {
		/* timestamp is the beginning of each measurement */
		fprintf(fp_raw,"%ld,%f,%d",tv.tv_sec,freq,commandedRPM);

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


			/* write voltage to output file */
			fprintf(fp_raw,",%.4f",volts);

			ch[nChannel].nSamples++;
			ch[nChannel].vSum += volts;
			if ( volts < ch[nChannel].vMin ) {
				ch[nChannel].vMin=volts;
			}
			if ( volts > ch[nChannel].vMax ) {
				ch[nChannel].vMax=volts;
			}

//			double value = ch[nChannel].m * volts + ch[nChannel].b;
//			fprintf(stderr,"# value=%.4f raw data = %#x   data[%d] = %#hx  %.4fV\n", value, data[i], i, sdat, volts);
//			fprintf(stderr,"%d[%d], %.4f, %d, %d, %.4f\n",nSample,nChannel,value,data[nSample*8+nChannel],sdat,volts);

			if ( CHANNEL_MODE_FREQUENCY_FROM_ANALOG == ch[nChannel].mode ) {
//				printf("frequency_from_analog[%d] - nSample=%d - volts=%0.1f - ",nSample,nChannel,volts);
				if ( 1==ch[nChannel].lastLogicLevel && volts <= VOLTAGE_LOGIC_LOW ) {
					/* were logic level high, now are low ... got a falling edge */
					ch[nChannel].lastLogicLevel=0;
					ch[nChannel].nFallingEdges++;
//					printf("now low and was high ... falling edge\n");
				} else if ( volts >= VOLTAGE_LOGIC_HIGH ) {
					ch[nChannel].lastLogicLevel=1;
//					printf("high\n");
				} else {
//					printf("no action\n");
				}
			}
		}
		/* terminate raw data line */
		fprintf(fp_raw,"\n");
	}
	fprintf(stderr,"# sampleN, sensor value, raw data, scaled raw data, volts\n");

	/* each line gets timestamp of measurement, actual sample frequency, nSamples */
	fprintf(fp_stats,"%ld,%f,%d,%d",tv.tv_sec,freq,commandedRPM,ch[0].nSamples);

	/* calculate statistics now that we are done */
	fprintf(stderr,"# channel statistics:\n");
	for ( i=0 ; i<NCHAN_USB1608FS ; i++ ) {
		ch[i].vAvg=ch[i].vSum / ( (double) ch[i].nSamples);

		fprintf(stderr,"#\t[%d] vMin=%0.4f vMax=%0.4f vAvg=%0.4f nSamples=%d\n",i,ch[i].vMin,ch[i].vMax,ch[i].vAvg,ch[i].nSamples);

		if ( CHANNEL_MODE_FREQUENCY_FROM_ANALOG == ch[i].mode ) {
			/* calculate frequency based on our gate time */
			ch[i].frequency = ch[i].nFallingEdges / ( ch[i].nSamples / freq);

			fprintf(stderr,"#\t[%d] %d frequency=%0.1f\n",i,ch[i].nFallingEdges,ch[i].frequency);
		}

		double scaled = ch[i].m * ch[i].vAvg + ch[i].b;

		/* channel, vMin, vMax, vAvg, nFallingEdges, frequency */
		fprintf(fp_stats,", %d,%0.4f,%0.4f,%0.4f,%0.4f,%d,%0.1f",i,scaled,ch[i].vMin,ch[i].vMax,ch[i].vAvg,ch[i].nFallingEdges,ch[i].frequency);

	}
	fprintf(fp_stats,"\n");


	return ch[0].nSamples;
}

int main (int argc, char **argv) {
	int i;
	int ret;
	char filename_raw[1024];
	char filename_stats[1024];
	struct timeval tv;

	if ( 2 != argc ) {
		fprintf(stderr,"dynoHR3 outputFilenamePrefix\n");
		exit(1);
	}

	gettimeofday(&tv, NULL);
	sprintf(filename_raw,"%s_%ld_raw.csv",argv[1],tv.tv_sec);
	sprintf(filename_stats,"%s_%ld_stats.csv",argv[1],tv.tv_sec);




	/* initialize and load values for the analog channels on the DAQ */
	set_channels();

	/* connect to USB-1608FS DAQ */
	fprintf(stderr,"# connecting to USB-1608FS DAQ\n");

	udev = 0;
	ret = libusb_init(NULL);
	if ( ret < 0 ) {
		perror("# libusb_init: Failed to initialize libusb");
		exit(1);
	}


	if ( (udev = usb_device_find_USB_MCC(USB1608FS_PID, NULL)) ) {
		fprintf(stderr,"# USB-1608FS device found\n");
	} else {
		fprintf(stderr,"# No USB-1608FS device found.\n");
		exit(1);
	}
 
	fprintf(stderr,"# Downloading and building calibration table from device ...\n");
	usbBuildCalTable_USB1608FS(udev, table_AIN);
	fprintf(stderr,"# calibration table downloaded and built.\n");

#if 0
	/* print the table */
	for ( i=0 ; i < NGAINS_USB1608FS; i++) {
		for ( int j = 0; j < NCHAN_USB1608FS; j++) {
			fprintf(stderr,"calibration table_AIN[%d][%d].slope,offset = %f,%f\n", i, j, table_AIN[i][j].slope, table_AIN[i][j].offset);
		}	
	}
#endif


	fprintf(stderr,"# USB-1608FS DAQ is ready to use\n");


	fprintf(stderr,"# connecting to GS3 VFD via Modbus (%s:%d address %d)\n",modbus_host,modbus_port,modbus_slave);
	if ( 0 != vfd_connect() ) {
		fprintf(stderr,"# unable to connect to VFD. Terminating.\n");
		exit(2);
	}
	fprintf(stderr,"# connected to GS3 VFD");
	vfd_gs3_set_automated_parameters();
	sleep(1);

	fprintf(stderr,"# commanding VFD to run\n");
	vfd_gs3_set_rpm(180);
	vfd_gs3_command_run();
	sleep(20); 	/* ramp up to initial speed */
	vfd_gs3_set_rpm(DYNO_RPM_END);
	sleep(15); 	/* ramp up to initial speed */


	fprintf(stderr,"# creating output files:\n");
	fprintf(stderr,"# %s   raw data log filename\n",filename_raw);
	fprintf(stderr,"# %s stats data log filename\n",filename_stats);
	fp_raw=fopen(filename_raw,"w");
	fp_stats=fopen(filename_stats,"w");

	int rpm;

//	/* step up */
//	for ( rpm=DYNO_RPM_START ; rpm<=DYNO_RPM_END ; rpm+=DYNO_RPM_STEP ) {
	/* step down */
	for ( rpm=DYNO_RPM_END ; rpm>=DYNO_RPM_START ; rpm-=DYNO_RPM_STEP ) {
		/* start new measurement */
		fprintf(stderr,"##################################################################################\n");
		init_channel_stats();

		/* command dyno RPM */
		fprintf(stderr,"# setting dyno RPM to %d\n",rpm);
		vfd_gs3_set_rpm(rpm);

		/* wait for dyno RPM to stabilize */
		fprintf(stderr,"# delay to allow RPM to be reached\n");
		sleep(DYNO_RPM_WAIT);

		/* acquire data */
		fprintf(stderr,"# acquiring data\n");
		daq_acquire();

		/* send / save data */
		fprintf(stderr,"# flushing logged data\n");
		fflush(fp_raw);
		fflush(fp_stats);
	}

#if 0
	/* step down */
	for ( ; rpm>=DYNO_RPM_START ; rpm-=DYNO_RPM_STEP ) {
		/* start new measurement */
		fprintf(stderr,"##################################################################################\n");
		init_channel_stats();

		/* command dyno RPM */
		fprintf(stderr,"# setting dyno RPM to %d\n",rpm);
		vfd_gs3_set_rpm(rpm);

		/* wait for dyno RPM to stabilize */
		fprintf(stderr,"# delay to allow RPM to be reached\n");
		sleep(DYNO_RPM_WAIT);

		/* acquire data */
		fprintf(stderr,"# acquiring data\n");
		daq_acquire();

		/* send / save data */
		fprintf(stderr,"# flushing logged data\n");
		fflush(fp_raw);
		fflush(fp_stats);
	}
#endif

stop:
	/* stop the motor */
	fprintf(stderr,"# commanding VFD to stop\n");
	vfd_gs3_command_stop();

	/* close output file */
	fprintf(stderr,"# closing output files\n");
	fclose(fp_raw);
	fclose(fp_stats);






	/* clean up MCC USB-1608FS DAQ */
	fprintf(stderr,"# USB DAQ shutdown / cleanup\n");
	usbAInStop_USB1608FS(udev);
	usbReset_USB1608FS(udev);

	for (i = 2; i <= 6; i++ ) {
		libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | i);
	}

	for (i = 0; i <= 6; i++) {
		libusb_release_interface(udev, i);
	}
	libusb_close(udev);

	/* clean up modbus */
	fprintf(stderr,"# restoring VFD to manual operation\n");
	vfd_gs3_clear_automated_parameters();

	fprintf(stderr,"# MODBUS shutdown / cleanup\n");
	modbus_close(mb);
	modbus_free(mb);


	return 0;
}
