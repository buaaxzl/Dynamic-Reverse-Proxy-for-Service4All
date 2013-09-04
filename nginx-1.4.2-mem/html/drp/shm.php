<?php
	$shm_id = shmop_open(1236,"w",0,0);
	if(empty($shm_id))
	{
		echo "fail";
	}
	else
	{
		echo $shm_id;
		echo "<br />success<br />";
		$shm_size = shmop_size($shm_id);
		echo "size:".$shm_size."<br/>";
		//$res = shmop_write($shm_id, "my shared memory block", 0);
		//echo "write bytes number is ".$res."<br/>";
		$my_string = shmop_read($shm_id, 0, $shm_size);
		echo $my_string;
		//shmop_delete($shm_id);
		shmop_close($shm_id);
	}
	//echo `whoami`;
?>