CANbootloader.exe 
 |  
 +- cando.dll     
 | 
 +- Config.ini    

【使用方法】
【1】USB连接CANable Z Pro，WIN7以上系统自带驱动
【2】需要通过修改Config.ini文件中的 APP_PATH ，指定烧录文件的路径
     可以把需要烧录的.s19文件放到应用程序目录下，并填写名称或输入绝对路径
【3】确保USB CAN设备未被其他程序占用，单片机下上电，打开上位机软件，即可进行烧录
