<?php
	session_start();	

	if( !isset($_COOKIE["userName"] ) )
	{
		echo "<script> alert('Please log in first'); </script>";
		echo "<script> location='http://192.168.3.220:8090/repository/testuser/login.jsp';</script>";
		exit;
	}
	else
	{
		if( $_COOKIE["userName"] != "user1")
		{
			echo "<script> alert('Permission denied!'); </script>";
			echo "<script> location='http://192.168.3.223:5800/drp/show.php';</script>";
			exit;
		}
	}

	$semID = sem_get(ftok("/usr/local/nginx-1.4.2-mem/applicationList.xml",'a'),1);

	$pid="";
	$fp = fopen("/usr/local/nginx-1.4.2-mem/pid","r");
	while( !feof($fp) )
	{
		$line = fgets($fp);
		$pid.= $line;
	}
	
	$cmd = "";
	$cmd = "kill -34 ".$pid;
	system($cmd);

	usleep(500000);

	sem_acquire($semID);

	function rcvData()
	{
		$message_queue = msg_get_queue(123, 0666);
		$message="";
		msg_receive($message_queue, 0, $message_type, 1024*8, $message, false, MSG_IPC_NOWAIT,$error);
		if( $error ==  MSG_ENOMSG)
		{
			echo "there is no msg in message_queue";
		}
		return $message;
	}
	
	$nodeNum = trim(rcvData());	
	$_SESSION["nodeNum"] = $nodeNum;
?>

<html>
<head>
	<title>Dynamic Configuration For nginx.conf</title>
	<link href="edit.css" rel="stylesheet" type="text/css" />
</head>

<body>
	<div id="service4all"> 
		<img src="img/logo.jpg"  alt="Service4All"/> 
	</div>

	<div id="bar">
		<p>
			Configuration Information
	    </p>
	</div>

	<form  method="POST" action="msg.php" id="form">
		<div id="options">
			<p id="user" > 
				user : &nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp
				<input size="9" type="text" name="user_name" value= <?php echo '"'.trim(rcvData()).'"'; ?> /> 
				<input  size="9" type="text" name="user_group" value=<?php echo '"'.trim(rcvData()).'"'; ?>  />
			</p>

			<p id="worker_processes">
				worker_processes : &nbsp&nbsp&nbsp&nbsp&nbsp&nbsp
				<input size="13" type="text" name="worker_processes" 
										value=<?php echo '"'.trim(rcvData()).'"'; ?> >
			</p>

			<p id="error_log">
				error_log : &nbsp&nbsp&nbsp&nbsp&nbsp&nbsp 
				<input type="text" name="error_log"  value=<?php echo '"'.trim(rcvData()).'"'; ?> >
			</p>

			<p id="pid">
				pid : &nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp
				<input size="20" type="text" name="pid" value=<?php echo '"'.trim(rcvData()).'"'; ?> >
			</p>

			<p id="worker_rlimit_nofile">
				worker_rlimit_nofile : &nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp 
				<input size="10" type="text" name="worker_rlimit_nofile" 
										value=<?php echo '"'.trim(rcvData()).'"'; ?> >
			</p>
		</div>

		<div id="events">
			<fieldset id="event_field" >
				<legend>Events</legend>
				<div id="event_in">
					<p id="use_what">
						use : 
						<select name="use_what">
							<?php $use_what = rcvData(); ?>
						    <option value="epoll" <?php if(strcmp($use_what,"epoll")==0) echo "selected"; ?> >epoll</option>
						    <option value="kqueue" <?php if(strcmp($use_what,"kqueue")==0) echo "selected"; ?> >kqueue</option>
						    <option value="rtsig" <?php if(strcmp($use_what,"rtsig")==0) echo "selected"; ?> >rtsig</option>
						    <option value="select" <?php if(strcmp($use_what,"select")==0) echo "selected"; ?> >select</option>
						    <option value="poll" <?php if(strcmp($use_what,"poll")==0) echo "selected"; ?> >poll</option>
						</select>
					</p>
					<p id="worker_connections">
						worker_connections : <input size="10" type="text" name="worker_connections" 
													value=<?php echo '"'.trim(rcvData()).'"'; ?>  >
					</p>
				</div>
			</fieldset>
		</div>

		<div id="http">
			<fieldset id="http_field">
				<legend>Http</legend>

					<div id="b_upstream">
						<p id="before_upstream_p">	
							<textarea name="before_upstream" rows="10" cols="60" wrap="virtual"> <?php echo trim(rcvData()); ?></textarea>
						</p>
					</div>
					
					<fieldset id="upstream_field">
						<legend>Upstream</legend>
						<?php 
							$display="
								<div id='upstreams'>	";
							for($i=1;$i<=(int)$nodeNum;$i++)
							{
								$upstream = trim(rcvData());
								$id = "upstream_".$i;
								$display.="
									<textarea name='".$id."' rows='10' cols='60' wrap='virtual'>".$upstream."</textarea>";
								$display.="<br />";
							}
							$display.="</div>";
							echo $display;
						?>
					</fieldset>

					<br />

					<fieldset id="server_field">
						<legend>Server</legend>
						<div id="server">

							<div id="server_fir">
								<br />listen 5800; <br /> server_name 127.0.0.1; <br /> access_log logs/my_log.log my_log; <br />

							</div>
							<br />
							<?php 
								$display="
									<div id='locations'>	";
								for($i=1;$i<=(int)$nodeNum;$i++)
								{
									$location = trim(rcvData());
									$id = "location_".$i;
									$display.="
										<textarea name='".$id."' rows='10' cols='60' wrap='virtual'>".$location."</textarea>";
									$display.="<br />";
								}
								$display.="</div>";
								echo $display;
							?>

							<?php 
								$display="<div id='php_location'>";
								$php_location = trim(rcvData());
								$display.="<textarea name='php_location' rows='10' cols='60' wrap='virtual'>".$php_location."</textarea>";
								$display.="</div>";
								echo $display;
							?>


							<?php 
								$display="<div id='location_added'>";
								$location_added = trim(rcvData());
								$display.="<p id='info'><i>(you can add \"location\" commands below)</i></p>
									<textarea name='location_added' rows='10' cols='60' wrap='virtual'>".$location_added."</textarea>";
								$display.="</div>";
								echo $display;
							?>
						</div>	
					</fieldset>

			</fieldset>
		</div>

		<div style="margin-top:50px;margin-bottom:100px;text-align: center;">
				<input type="submit" id="commit" value="SUBMIT" />	
		</div>

	</form> 

<?php
	sem_release($semID);
?>

</body>

</html>
