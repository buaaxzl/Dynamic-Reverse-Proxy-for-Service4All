<?php
	$pid="";
	$fp = fopen("/usr/local/nginx-1.4.2-mem/pid","r");
	while( !feof($fp) )
	{
		$line = fgets($fp);
		$pid.= $line;
	}
	$cmd = "kill -10 ".$pid;	
	//echo `whoami`;
	system($cmd);
	fclose($fp);

	$i=0;
	while($i<3000000){ $i=$i+1;}

	function getName($str)
	{
		$str = trim($str);
		$len = strlen($str);
		$i=0;

		if($len>0)
		{
			while($str[$i] != ' ')
				$i++;
			while($str[$i] == ' ')
				$i++;
		}

		$res = "";
		while($i<$len && $str[$i]!='{' && $str[$i]!=' ' )
		{
			$res.=$str[$i];
			$i++;
		}
		return $res;	
	}


	function process($str)
	{
		$res = ""; $flag = 0;
		for($i=0;$i<strlen($str);$i++)
		{
			if( $str[$i] == '{' ) $flag = 1;
			if( $str[$i] == '}' ) $flag = 0;

			if( $str[$i] == '{' ) continue;

			if( $str[$i] == ';' ) $res.="<br />";
			else if($flag) $res.=$str[$i];
		}
		return trim($res);
	}

	function getIP($str)
	{
		$res = "";
		$i=0;

		$str = trim($str);
		$len = strlen($str);

		$i=$i+6;
		while( $str[$i]===' '){$i++;}
		while($i<$len && $str[$i]!==' ' )
		{
			$res.=$str[$i];
			$i++;
		}

		return $res;
	}

	function Count_num($ip,$name)
	{
		$res = 0;
		$fp = fopen("/usr/local/nginx-1.4.2-mem/logs/my_log.log","r");

		if($name[0] == 'A' )
		{		
			$ip.="|/";
			$ip.=$name;
			$ip.="/";	
			$ip = trim($ip);
		}
		else
		{
			$ip.="|/";
			$ip.="axis2/services/";
			$ip.=$name;
			$ip.="?wsdl";
			$ip = trim($ip);
		}

		while(!feof($fp))
		{
			$ip_1 = fgets($fp);
			$ip_1 = trim($ip_1);

			if( ! strcmp($ip,$ip_1) )
				$res = $res + 1;
		}
		fclose($fp);
		return $res;
	}

	function solve($text,$name)
	{
		$cnt =0;
		$res = "";
		$temp = "";
		$text = str_replace("<br />", "#", $text);
		$len = strlen($text);
		for($i=0;$i<$len;$i++)
		{
			if($text[$i]!='#')
			{
				$temp.=$text[$i];
			}		
			else
			{
				$ip = getIP($temp);
				$res.=Count_num($ip,$name);
				
				$res.="<br />";
				$temp = "";
			}
		}
		return $res;
	}

	$fp = fopen("/usr/local/nginx-1.4.2-mem/applicationList.xml","r+");

	if( flock($fp,LOCK_SH))
	{

		$display = "<table id='result'>
					<tr>
						 <th>ID</th>
						 <th>Name</th>
						 <th>Back-end Node(s)</th>
						 <th>Invoke Counts</th>
					</tr>";

		$doc = new DOMDocument();
		$doc->load("/usr/local/nginx-1.4.2-mem/applicationList.xml");
		$applications = $doc->getElementsByTagName("application");
		foreach($applications as $application)
		{
			$id = $application->getAttribute("id");
			$name = $application->getAttribute("name");
			$upstreams = $application->getElementsByTagName("upstream");
			$upstream_text = $upstreams->item(0)->nodeValue;
			$name = getName($upstream_text);
			$text = process($upstream_text);
			$visit_num = solve($text,$name);

			$display.="
					<tr>
						<td>".$id."</td>
						<td>".$name."</td>
						<td>".$text."</td>
						<td>".$visit_num."</td>
					</tr>
			";
		}	

		$display.= "</table>";
	}
	else
	{
		echo "Couldn't get applicationList.xml lock!";
	}
	fclose($fp);	
?>

<html>

<head>
	<title> SHOW UPSTREAM FOR NGINX </title>
	<meta content="width=device-width, initial-scale=1.0" >
	<link href="css/bootstrap.min.css" rel="stylesheet" media="screen">
	<link href="table.css" rel="stylesheet">
</head>

<body style="text-align:center">

	<div> 
		<img src="img/logo.jpg"  alt="service4All"/> 
		<br />
		<br />
		<p class="text-center text-info" style="font-size:1.5em">Dynamic Reverse Proxy Configuration</p>
	</div>

	<?php echo $display; ?>
	<br />

	<button style="margin-top:100px;margin-bottom:100px;" class="btn btn-large btn-primary" type="button" onclick="window.location='conf_edit.php'">
		configure nginx
	</button>

	
	<script src="jquery-1.3.2.js"></script>
    <script src="js/bootstrap.min.js"></script>
</body>

</html>
