#!/usr/bin/php -q
<?
$outputDirectory='output';
$photoDirectory = 'photos'; /* appended to $outputDirectory for filesystem location */

if ( ! isset($_SERVER['argv'][1]) || ! file_exists($_SERVER['argv'][1]) ) {
	die("# Error opening input stats file or no filename specified.\n");
}
$dataFile=$_SERVER['argv'][1];

if ( ! isset($_SERVER['argv'][2]) ) {
	die("# Please specify DUT serial number as second argument\n");
}
$dut_serial=$_SERVER['argv'][2];

/* create serial number specific output directory */
$outputDirectory = sprintf("%s/%s",$outputDirectory,$dut_serial);
if ( ! is_dir($outputDirectory) ) { 
	printf("# creating %s\n",$outputDirectory);

	if ( ! mkdir($outputDirectory,0755,true) ) {
		die("# error creating output directory. Aborting...\n");
	}
}


$gnuplotFiles=array();
/* list of files we want to run gnuplot on, sorted in the order we want to appear in report */
$gnuplotFiles[]="dut_rect_power.gnuplot";
$gnuplotFiles[]="dut_rect_vdc.gnuplot";
$gnuplotFiles[]="dut_rect_idc.gnuplot";
$gnuplotFiles[]="dut_ac_frequency.gnuplot";
$gnuplotFiles[]="dut_field_power.gnuplot";
$gnuplotFiles[]="dut_field_vdc.gnuplot";
$gnuplotFiles[]="dut_field_idc.gnuplot";

/* example execution 
gnuplot -e "t=\"HR3A Serial Number 487 starting at 2018-01-05 13:52:10\"; f=\"../dataExample/487_full_run_backwards_1515185429_stats.csv\"; o=\"foo4.png\";" dut_rect_power.gnuplot
*/


/* split data filename by '_'. should be whatever_unixtimestamp_stats.csv */
$dataFile_parts=explode('_',$dataFile);
$dyno_run_start_time=$dataFile_parts[count($dataFile_parts)-2];

$dyno_run_start_date=gmdate("Y-m-d H:i:s e",$dyno_run_start_time);

printf("# dyno run start time = '%s' (from timestamp in filename)\n",$dyno_run_start_time);
printf("# dyno run start date = '%s'\n",$dyno_run_start_date);


$subtitle=sprintf("HR3 Serial Number %s starting at %s",$dut_serial,$dyno_run_start_date);
printf("# subtitle = '%s'\n",$subtitle);


$gnuplotOutputFiles=array();

foreach ( $gnuplotFiles as &$gnuplotFile ) {
	printf("###########################################\n");

	$plot_name=basename($gnuplotFile,".gnuplot");
	printf("# processing: %s\n",$gnuplotFile);
	$outputPNG=sprintf("%s/%s_%d_%s.png",$outputDirectory,$dut_serial,$dyno_run_start_time,$plot_name);
	printf("# output PNG filename = '%s'\n",$outputPNG);
	$gnuplotOutputFiles[]=basename($outputPNG);

	$gnuplotVariables=sprintf("t=%s; f=%s; o=%s;",escapeshellarg($subtitle),escapeshellarg($dataFile),escapeshellarg($outputPNG));
	$gnuplotVariables=escapeshellarg($gnuplotVariables);
	printf("# gnuplot variables = %s\n",$gnuplotVariables);

	$cmd=sprintf("gnuplot -e %s %s",$gnuplotVariables,$gnuplotFile);
	exec($cmd);
//	printf("# cmd = %s\n",$cmd);

}

print_r($gnuplotOutputFiles);

/* create report HTML file */
$reportHTMLFilename = sprintf("%s/report_%s_%s.html",$outputDirectory,$dut_serial,$dyno_run_start_time);
printf("# create HTML report named %s\n",$reportHTMLFilename);
$fp = fopen($reportHTMLFilename,'w');

fprintf($fp,"<!DOCTYPE html>\n");
fprintf($fp,"<html>\n");
fprintf($fp,"\t<head>\n");
fprintf($fp,"\t\t<title>Dynamometer Report for HR3 Serial Number %s @ %s</title>\n",$dut_serial,$dyno_run_start_date);
fprintf($fp,"\t</head>\n");
fprintf($fp,"\t<body>\n");

fprintf($fp,"<h1>Dynamometer Report for HR3 Serial Number %s @ %s</h1>\n",$dut_serial,$dyno_run_start_date);

fprintf($fp,"<p>This report was automatically generated on <i>%s</i> with the following command:</p><pre>%s</pre>\n",gmdate("Y-m-d H:i:s e"),implode(' ',$_SERVER['argv']));

fprintf($fp,"<p>The source code for this report generator and the dyno control software can be found at: <a href=\"https://github.com/aprsworld/dynoHR3/\">https://github.com/aprsworld/dynoHR3/</a></p>\n");

fprintf($fp,"<h2>Electrical Configuration</h2>\n");
copy('res/images/hr3_dut_simplified_electrical_release_20180223.png',$outputDirectory . '/hr3_dut_simplified_electrical_release_20180223.png');
fprintf($fp,"<img src=\"hr3_dut_simplified_electrical_release_20180223.png\" alt=\"Dyno Connection Schematic\" style=\"width: 1280px; height: auto; padding: 20px;\"/>\n");

fprintf($fp,"<h2>Graphs</h2>\n");
for ( $i=0 ; $i<count($gnuplotOutputFiles) ; $i++ ) {
	fprintf($fp,"<img src=\"%s\" alt=\"Graph\" /><br />\n",$gnuplotOutputFiles[$i]);
}

fprintf($fp,"<h2>Raw Data</h2>\n");

/* copy input stats CSV and raw file to report directory */
printf("# copying stats datafile='%s' to report directory\n",$dataFile);
copy($dataFile,$outputDirectory . '/' . basename($dataFile));

$dataFileRaw = str_replace('_stats.csv','_raw.csv',$dataFile);
printf("# copying raw datafile='%s' to report directory\n",$dataFileRaw);

fprintf($fp,"<ul>\n");

fprintf($fp,"\t<li><a href=\"%s\">%s</a> (%s kB) CSV file with summarized statistics<br />\n",basename($dataFile),basename($dataFile),number_format(filesize($dataFile)/1024));
fprintf($fp,"Column labels for CSV columns are:<br />\n");
fprintf($fp,"<pre>\"timetamp (unix)\", \"sample frequency\", \"commanded RPM\", \"nSamples\", \"ch[0] marker\", \"Rectifier DC volts (scaled)\", \"Rectifier DC volts (vMin)\", \"Rectifier DC volts (vMax)\", \"Rectifier DC volts (vAvg)\", \"Rectifier DC volts (nFallingEdges)\", \"Rectifier DC volts (frequency)\", \"ch[1] marker\", \"Rectifier DC amps (scaled)\", \"Rectifier DC amps (vMin)\", \"Rectifier DC amps (vMax)\", \"Rectifier DC amps (vAvg)\", \"Rectifier DC amps (nFallingEdges)\", \"Rectifier DC amps (frequency)\", \"ch[2] marker\", \"Field DC volts (scaled)\", \"Field DC volts (vMin)\", \"Field DC volts (vMax)\", \"Field DC volts (vAvg)\", \"Field DC volts (nFallingEdges)\", \"Field DC volts (frequency)\", \"ch[3] marker\", \"Field DC amps (scaled)\", \"Field DC amps (vMin)\", \"Field DC amps (vMax)\", \"Field DC amps (vAvg)\", \"Field DC amps (nFallingEdges)\", \"Field DC amps (frequency)\", \"ch[4] marker\", \"Turbine Output Hz (scaled)\", \"Turbine Output Hz (vMin)\", \"Turbine Output Hz (vMax)\", \"Turbine Output Hz (vAvg)\", \"Turbine Output Hz (nFallingEdges)\", \"Turbine Output Hz (frequency)\", \"ch[5] marker\", \"Dyno Shaft RPM (scaled)\", \"Dyno Shaft RPM (vMin)\", \"Dyno Shaft RPM (vMax)\", \"Dyno Shaft RPM (vAvg)\", \"Dyno Shaft RPM (nFallingEdges)\", \"Dyno Shaft RPM (frequency)\", \"ch[6] marker\", \"Turbine IR Temperature C (scaled)\", \"Turbine IR Temperature C (vMin)\", \"Turbine IR Temperature C (vMax)\", \"Turbine IR Temperature C (vAvg)\", \"Turbine IR Temperature C (nFallingEdges)\", \"Turbine IR Temperature C (frequency)\", \"ch[7] marker\", \"Dyno Strain unused (scaled)\", \"Dyno Strain unused (vMin)\", \"Dyno Strain unused (vMax)\", \"Dyno Strain unused (vAvg)\", \"Dyno Strain unused (nFallingEdges)\", \"Dyno Strain unused (frequency)\"</pre>\n");
fprintf($fp,"</li>");

fprintf($fp,"\t<li><a href=\"%s\">%s</a> (%s kB) CSV file with raw channel data<br />\n",basename($dataFileRaw),basename($dataFileRaw),number_format(filesize($dataFileRaw)/1024));

fprintf($fp,"</ul>\n");

fprintf($fp,"<h2>Device Under Test Photos</h2>\n");
fprintf($fp,"<p>These are manually captured visible and thermal photos of the device under test. Because they are captured on different devices, the time stamps may differ.</p>\n");

$dutPhotos=array();

if ( is_dir( $outputDirectory . '/' . $photoDirectory) ) {
	$dir = opendir($outputDirectory . '/' . $photoDirectory);
	while ( $file = readdir($dir) ) {
		if ( '.' == substr($file,0,1) ) {
			continue;
		}
		$dutPhotos[]=$photoDirectory . '/' . $file;
	}
	closedir($dir);
}

/* sort by filename */
sort($dutPhotos);

if ( 0 == count($dutPhotos) ) {
	fprintf($fp,"<p>No photos found.</p>\n");
} else {
	print_r($dutPhotos);

	fprintf($fp,"<ul>\n");
	for ( $i=0 ; $i<count($dutPhotos) ; $i++ ) {
		fprintf($fp,"<li><a href=\"%s\">%s</a> (%s Kb)<br />\n",$dutPhotos[$i],basename($dutPhotos[$i]),number_format(filesize($outputDirectory . '/' . $dutPhotos[$i])/1024));
		fprintf($fp,"<img src=\"%s\" alt=\"\" style=\"width: 800px; height: auto; padding: 20px;\" /><br />\n",$dutPhotos[$i]);
	}
	fprintf($fp,"</ul>\n");
}

fprintf($fp,"\t</body>\n");
fprintf($fp,"\t</html>\n");


/* close report HTML file */
fclose($fp);

?>
