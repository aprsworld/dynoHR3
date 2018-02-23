#!/usr/bin/php -q
<?
$outputDirectory='output';

if ( ! isset($_SERVER['argv'][1]) || ! file_exists($_SERVER['argv'][1]) ) {
	die("# Error opening input stats file or no filename specified.\n");
}
$dataFile=$_SERVER['argv'][1];

if ( ! isset($_SERVER['argv'][2]) ) {
	die("# Please specify DUT serial number as second argument\n");
}
$dut_serial=$_SERVER['argv'][2];

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



foreach ( $gnuplotFiles as &$gnuplotFile ) {
	printf("###########################################\n");

	$plot_name=basename($gnuplotFile,".gnuplot");
	printf("# processing: %s\n",$gnuplotFile);

	$outputPNG=sprintf("%s/%s_%d_%s.png",$outputDirectory,$dut_serial,$dyno_run_start_time,$plot_name);
	printf("# output PNG filename = '%s'\n",$outputPNG);

	$gnuplotVariables=sprintf("t=%s; f=%s; o=%s;",escapeshellarg($subtitle),escapeshellarg($dataFile),escapeshellarg($outputPNG));
	$gnuplotVariables=escapeshellarg($gnuplotVariables);
	printf("# gnuplot variables = %s\n",$gnuplotVariables);

	$cmd=sprintf("gnuplot -e %s %s",$gnuplotVariables,$gnuplotFile);
	exec($cmd);
//	printf("# cmd = %s\n",$cmd);

}

?>
