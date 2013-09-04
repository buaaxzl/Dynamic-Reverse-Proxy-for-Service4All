<?php	
	session_start();

	$nodeNum = $_SESSION["nodeNum"];

	$message_queue = msg_get_queue(456,0666);

	msg_send($message_queue, 1, $_SESSION["nodeNum"],false,false,$error);  

	msg_send($message_queue, 1, $_POST["user_name"],false,false,$error);   

	msg_send($message_queue, 1, $_POST["user_group"],false,false,$error);            

	msg_send($message_queue, 1, $_POST["worker_processes"],false,false,$error);	

	msg_send($message_queue, 1, $_POST["error_log"],false,false,$error);	

	msg_send($message_queue, 1, $_POST["pid"],false,false,$error);		

	msg_send($message_queue, 1, $_POST["worker_rlimit_nofile"],false,false,$error);	

	msg_send($message_queue, 1, $_POST["use_what"],false,false,$error);		

	msg_send($message_queue, 1, $_POST["worker_connections"],false,false,$error);	

	msg_send($message_queue, 1, $_POST["before_upstream"],false,false,$error);	

	
	for($i=1;$i<=$nodeNum;$i++)
	{
		$name = "upstream_".$i;
		msg_send($message_queue, 1, $_POST[$name],false,false,$error);
	}

	for($i=1;$i<=$nodeNum;$i++)
	{
		$name = "location_".$i;
		msg_send($message_queue, 1, $_POST[$name],false,false,$error);
	}

	msg_send($message_queue, 1, $_POST["php_location"],false,false,$error);	

	msg_send($message_queue, 1, $_POST["location_added"],false,false,$error);


	$pid="";
	$fp = fopen("/usr/local/nginx-1.4.2-mem/pid","r");
	while( !feof($fp) )
	{
		$line = fgets($fp);
		$pid.= $line;
	}
	$cmd = "kill -12 ".$pid;
	system($cmd);
?> 

<html>

<head>
	<title>progress form</title>
	<meta content="width=device-width, initial-scale=1.0" >
	<link href="css/bootstrap.min.css" rel="stylesheet" media="screen">
	<meta http-equiv="refresh" content="1; url=show.php">
</head>

<body style="text-align:center">
	<p class="text-error lead"> Conf Infomation has been updated!.</p>
</body>

</html>
