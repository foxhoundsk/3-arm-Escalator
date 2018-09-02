// ver 1.0.0
/*
 WARN: SINCE WRONG GIT BRANCH, HERE SHOULD ONLY CHENGE ONE SECTION WITH COMMENT BUG WHEN MERGE BACK TO MASTER
 1. after received data, ack didnt return properly.
 2. esp8266 still send its at+cipsend here, this occur same time with 1. .
 3. (maybe solved, this may caused by esp8266 didnt sent proper data since we use pared data to start the   timer)timer doesnt start even the training is started (sametime error with 1. 2.)

 
 
 
 */

using System;
using System.Windows.Forms;
using System.Text;
using System.Net;
using System.Net.Sockets;
using System.Linq;
using System.Drawing;
using System.IO;
using System.Threading;
using System.IO.Ports;
using System.Collections.Generic;

namespace Maze_3_arm
{
    
    
    
    public partial class Form1 : Form
    {
        //Socket serverFd;
        IPEndPoint serverIpInfo = new IPEndPoint(IPAddress.Parse("192.168.4.2"), 62222);
        IPEndPoint remoteIpInfo = new IPEndPoint(IPAddress.Parse("192.168.4.1"), 3232);
        EndPoint Remote;
        byte[] recvBuffer       = new byte[64]; 
        byte[] dataBuffer       = new byte[100];
        byte[] sendBuffer       = new byte[6] {114, 100, 0, 0, 0, 0};
        ushort[] DACtable = new ushort[3] { 0x020d, 0x041a, 0x0628 }; /* speed from low to high as index from 0 ~ max */
        byte[] DACspeed = new byte[6] { 0, 0, 0, 0, 0, 0 };
        FileStream resultFileStream;
        StreamWriter resultStreamWriter;
        static ThreadStart recvThread = new ThreadStart(Work.taskRecvThread);
        Thread newThread = new Thread(recvThread);
        bool isTrainingFisrtTime = true;
        uint ratRouteIndex = 18;
        uint deadcode = 0;
        long endTimestamp;

        SerialPort serialPort = new SerialPort();
        List<Byte> receiveDataList = new List<Byte>();
        char[] knockDoorConst = new char[3] { 'R', 'D', 'Y' };
        byte timeoutCount = 0;

        module_Info arm_Info;
        public void DoRemoteIpInfoCast()
        {
            Remote = (EndPoint)remoteIpInfo; /* receiveFrom use this as its arg */
        }
        public enum connectionStatus
        {
            UNCONNECT = 1,
            CONNECTED,
            CONNECTED_KNOCK_DOOR,
            CONNECTED_KNOCK_DOOR_WAIT, /* used in uart mode */           
            END_TRAINING_IN_PROGRESS, /* used in uart mode */
            END_TRAINING_WAIT_PROGRESS, /* used in uart mode */
            TRAINING_END /* used in uart mode */
        }
        public enum mazeStatus
        {
            WAIT_FOR_RAT = 1,
            RAT_NOT_ENTERED,
            RAT_ENTERED,
            TRAINING_END
        }
        public enum trainingStatus
        {
            STANDBY = 6,
            RUNNING,
            COMPLETE         
        }
        public struct module_Info
        {
            public short[]          shortTermError;
            public short[]          longTermError;
            public short            food_left;
            public trainingStatus   trainState;
            public connectionStatus netState;
            public bool isDataReceived; /* this flag determine wheather receive the incoming data */
        }
        
        public Form1()
        {
            InitializeComponent();
        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void Form1_Load(object sender, EventArgs e)
        {
            string[] ports = SerialPort.GetPortNames();
            serialPortSelect.Items.AddRange(ports);
            //recvBuffer1 = 3;
            //recvBuffer[16] = 4;            
            //recvBuffer[0] = 1;
            //Array.Clear(recvBuffer, 0, recvBuffer.Length);
            // value cast success connectionState.Text = recvBuffer[0].ToString();
            globalBuffer.g_isThreadWorking = false;
            //newThread.Start();            
            arm_Info.trainState     = trainingStatus.STANDBY;
            arm_Info.netState       = connectionStatus.CONNECTED_KNOCK_DOOR; /* CHANGEED FOR UART TRANSMISSION */
            arm_Info.isDataReceived = false;
            globalBuffer.g_dataNeedProcess = false;
            //DoRemoteIpInfoCast();   /* init variable since it cast a var which initialize with _new_, so with func call, we can guarantee that its been initialized */
            /* enable network timer and do a regular check to network state */
            networkTimer.Interval   = 100;
        }

        private void onSerialPortReceive(Object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                Byte[] buffer = new Byte[3]; /* WARN: once checksum added, the buffer size should increase */
                int length = (sender as SerialPort).Read(buffer, 0, buffer.Length);
                for (int i = 0; i < length; i++)
                {
                    receiveDataList.Add(buffer[i]);
                }
                globalBuffer.g_dataNeedProcess = true;
            }
            catch
            {
                serialPort.Close();
            }
        }

        private void networkTimer_Tick(object sender, EventArgs e)
        {
           
           switch (arm_Info.netState)
           {                
                case connectionStatus.CONNECTED_KNOCK_DOOR:
                    /*
                    try
                    {
                        globalBuffer.g_recvSocketfd.SendTo(sendBuffer, 6, SocketFlags.None, remoteIpInfo); /* send first packet to remote, which the data sent is "rd" 
                        //globalBuffer.g_recvSocketfd.ReceiveFrom(recvBuffer, 0, 3, SocketFlags.None, ref Remote);
                    }
                    catch (Exception ex)
                    {
                        errorBox.Text = ex.ToString();
                        break; /* re-knock the door 
                    }
                    
                    Array.Clear(recvBuffer, 0, recvBuffer.Length);
                    */
                    serialPort.Write(knockDoorConst, 0, 3);
                    arm_Info.netState = connectionStatus.CONNECTED_KNOCK_DOOR_WAIT;
                    if (getCurrentTimestamp() >= endTimestamp) /* IF NEED RESTART WITHOUT RESTART THE APPM CONFIGUR HERE */
                    {
                        timerTimeElapsed.Enabled = false;
                        timeLeft.Text = "0 : 0";
                        arm_Info.netState = connectionStatus.END_TRAINING_IN_PROGRESS;
                        break;
                    }
                    //connectionState.Text = "CONNECTED"; /* change color to green here */
                    //globalBuffer.g_isThreadWorking = true;
                    // THIS CANT BE HERE, SHOULD AFTER KNOCK_DOOR CHECK arm_Info.netState = connectionStatus.CONNECTED;
                    break;
                case connectionStatus.CONNECTED_KNOCK_DOOR_WAIT:
                    if (!globalBuffer.g_dataNeedProcess)
                    {
                        timeoutCount++;
                        if (timeoutCount >= 3)
                        {
                            timeoutCount = 0;
                            arm_Info.netState = connectionStatus.CONNECTED_KNOCK_DOOR;
                        }
                    }
                    else
                    {
                        timeoutCount = 0; /* for next use */
                        connectionState.Text = "Connected";
                        arm_Info.netState = connectionStatus.CONNECTED;
                        globalBuffer.g_dataNeedProcess = false;
                        receiveDataList.Clear();
                    }
                    break;
                case connectionStatus.CONNECTED:
                    /*
                    try
                    {
                        serverFd.ReceiveFrom(recvBuffer, 0, 64, SocketFlags.None, ref Remote);
                    }
                    catch (Exception ex)
                    {
                        errorBox.Text = ex.ToString();
                        break;
                    }
                    */
                    if (getCurrentTimestamp() >= endTimestamp) /* IF NEED RESTART WITHOUT RESTART THE APPM CONFIGUR HERE */
                    {
                        timerTimeElapsed.Enabled = false;
                        timeLeft.Text = "0 : 0";                        
                        arm_Info.netState = connectionStatus.END_TRAINING_IN_PROGRESS;
                        break;
                    }
                    ushort ratPos, recordPos = 0;
                    trainingState.Text = "In Progress";                    
                    if (!globalBuffer.g_dataNeedProcess)
                        break;
                        // send corresponding DAC here, use following func to send 
                        /* C# is big-endian */
                    //Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed1".ToString(), true)[0]).Value);//arm1speed1
                    for (ushort i = 0; i < 3; i++)
                    {
                        switch (i)
                        {
                            case 0:
                                switch (receiveDataList[i])
                                {
                                    case 1:
                                        DACspeed[0] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed1".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[1] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed1".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                    case 2:
                                        DACspeed[0] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed2".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[1] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed2".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                    case 3:
                                        DACspeed[0] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed3".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[1] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed3".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;                                        
                                }
                                break;
                            case 1:
                                switch (receiveDataList[i])
                                {
                                    case 1:
                                        DACspeed[2] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm2speed1".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[3] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm2speed1".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                    case 2:
                                        DACspeed[2] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm2speed2".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[3] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm2speed2".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                    case 3:
                                        DACspeed[2] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm2speed3".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[3] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm2speed3".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                }                                
                                break;
                            case 2:
                                switch (receiveDataList[i])
                                {
                                    case 1:
                                        DACspeed[4] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm3speed1".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[5] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm3speed1".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                    case 2:
                                        DACspeed[4] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm3speed2".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[5] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm3speed2".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                    case 3:
                                        DACspeed[4] = (byte)DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm3speed3".ToString(), true)[0]).Value)) / 5) - 1)];
                                        DACspeed[5] = (byte)(DACtable[(((Decimal.ToByte(((NumericUpDown)Controls.Find("arm3speed3".ToString(), true)[0]).Value)) / 5) - 1)] >> 8);
                                        break;
                                }                                
                                break;
                        }                            
                    }

                    /*----------Data Record-----------*/
                    ratPos = receiveDataList[0];
                    switch (ratPos)
                    {
                        case 0:
                            recordPos = 5; /* 5 stands for 5 m/min, etc...*/
                            break;
                        case 1:
                            recordPos = 10;
                            break;
                        case 2:
                            recordPos = 15;
                            break;
                        /* POS case 3 deprecated since we changed total POS from 4 to 3 */
                    }
                    resultStreamWriter.WriteLine(ratPos.ToString() + "  " + recordPos.ToString());
                    resultStreamWriter.Flush();
                    ratPos = receiveDataList[1];
                    switch (ratPos)
                    {
                        case 0:
                            recordPos = 5;
                            break;
                        case 1:
                            recordPos = 10;
                            break;
                        case 2:
                            recordPos = 15;
                            break;
                    }
                    resultStreamWriter.WriteLine(ratPos.ToString() + "  " + recordPos.ToString());
                    resultStreamWriter.Flush();
                    ratPos = receiveDataList[2]; /* WARN: (MUST FIX IN MASTER BRANCH)bug fixed */
                    switch (ratPos)
                    {
                        case 0:
                            recordPos = 5;
                            break;
                        case 1:
                            recordPos = 10;
                            break;
                        case 2:
                            recordPos = 15;
                            break;
                    }
                    resultStreamWriter.WriteLine(ratPos.ToString() + "  " + recordPos.ToString());
                    resultStreamWriter.Flush();
                    resultStreamWriter.WriteLine();
                    resultStreamWriter.Flush();
                    /*--------------------------------*/
                    //this line and following line is used in wifi module. globalBuffer.g_recvSocketfd.SendTo(DACspeed, 6, SocketFlags.None, remoteIpInfo); /* send ACK back */
                    //Array.Clear(globalBuffer.g_recvBuffer, 0, globalBuffer.g_recvBuffer.Length);
                    /* uart transmission use-------------------------*/
                    serialPort.Write(DACspeed, 0, 6); /* we don't clean DACspeed is because that each time we write whole size of it with new data */
                    receiveDataList.Clear();
                    globalBuffer.g_dataNeedProcess = false;
                    /* --------------------------------------------- */
                    //arm_Iwnfo.isDataReceived = true; TODO maybe use in the future                    
                    //arm_Info.netState = connectionStatus.CONNECTED_PROCESSING;
                    //Array.Clear(dataBuffer, 0, dataBuffer.Length); use this after process the received data
                    break;
                case connectionStatus.END_TRAINING_IN_PROGRESS:
                    serialPort.Write(new char[6] {'E','N','D','O','F','T'}, 0, 6); // last 3 bytes are used to fill buffer to 6 byte, this is due to a infrastructure problem, it deserve better implementation TODO
                    arm_Info.netState = connectionStatus.END_TRAINING_WAIT_PROGRESS;
                    break;
                case connectionStatus.END_TRAINING_WAIT_PROGRESS:
                    if (!globalBuffer.g_dataNeedProcess)
                    {
                        timeoutCount++;
                        if (timeoutCount >= 3)
                        {
                            timeoutCount = 0;
                            arm_Info.netState = connectionStatus.END_TRAINING_IN_PROGRESS;
                        }
                    }
                    else
                    {
                        timeoutCount = 0;
                        if (receiveDataList[0] != 'A' || receiveDataList[1] != 'C')
                        {
                            arm_Info.netState = connectionStatus.END_TRAINING_IN_PROGRESS;
                            break;
                        }
                        arm_Info.netState = connectionStatus.TRAINING_END; /* training end state switched here */
                    }
                    break;
                case connectionStatus.TRAINING_END:
                    resultStreamWriter.Close();
                    resultFileStream.Close();
                    trainingState.Text = "Training end";
                    startButton.BackColor = Color.LawnGreen;
                    Console.Beep(1000, 1500);
                    Thread.Sleep(500);
                    Console.Beep(1000, 1500);
                    //Thread.Sleep(1000);
                    //Console.Beep(1000, 1500);
                    networkTimer.Enabled = false;
                    break;                                    
           }
        }

        private void pathSelectButton_Click(object sender, EventArgs e)
        {
            resultFileDialog.ShowDialog();
        }

        private void resultFileDialog_FileOk(object sender, System.ComponentModel.CancelEventArgs e)
        {
            resultFilePath.ForeColor = Color.Black;
            resultFilePath.Text = resultFileDialog.FileName;
        }

        private void startButton_Click(object sender, EventArgs e)
        {
            try
            {
                resultFileStream = (System.IO.FileStream)resultFileDialog.OpenFile();
                resultStreamWriter = new StreamWriter(resultFileStream);
            }
            catch
            {                
                resultFilePath.Text = "File path error";
                resultFilePath.ForeColor = Color.Red;
                return;
            }
            try
            {
                serialPortSelect.ForeColor = SystemColors.WindowText;
                serialPort.PortName = serialPortSelect.Text;
                serialPort.DataBits = 8;
                serialPort.Parity = Parity.None;
                serialPort.StopBits = StopBits.One;
                serialPort.BaudRate = 115200;
            }
            catch
            {
                serialPortSelect.ForeColor = Color.Red;
                serialPortSelect.Text = "Port error";
                return;
            }
            try
            {
                serialPortSelect.ForeColor = SystemColors.WindowText;
                serialPort.Open();
                serialPort.DiscardOutBuffer();
                serialPort.DiscardInBuffer();
                serialPort.DataReceived += new SerialDataReceivedEventHandler(onSerialPortReceive);
                serialPortSelect.ForeColor = Color.Black;
            }
            catch
            {
                serialPortSelect.ForeColor = Color.Red;
                serialPortSelect.Text = "Port error";
                return;
            }

            endTimestamp = getCurrentTimestamp() + long.Parse(trainTime.Text) * 60;
            resultFilePath.ForeColor = Color.Black;
            resultFilePath.Text = resultFileDialog.FileName;
            startButton.BackColor = Color.Orange;
            timerTimeElapsed.Enabled = true;
            networkTimer.Enabled = true;
            return;
        }

        static public long getCurrentTimestamp()
        {
            return (long)(DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, 0)).TotalSeconds;
        }

        private void timerTimeElapsed_Tick(object sender, EventArgs e) /* max counting time should under 1 hour */
        {
            timeLeft.Text = ((endTimestamp - getCurrentTimestamp()) / 60).ToString() + " : " + ((endTimestamp - getCurrentTimestamp()) % 60).ToString();
            /*
            if ((endTimestamp - getCurrentTimestamp() / 60 == 0) && (endTimestamp - getCurrentTimestamp() % 60 <= 0))
            {
                //maybe it has something to de. networkTimer.Enabled = false;
                /* STOP ALL HERE 
                arm_Info.netState = connectionStatus.CONNECTED_TRAINING_DONE;
                timerTimeElapsed.Enabled = false;
            }
            */
        }

        private void checkBoxArmLoc0_CheckedChanged(object sender, EventArgs e)
        {
            
        }

        private void groupBox2_Enter(object sender, EventArgs e)
        {

        }

        private void groupBox2_Enter_1(object sender, EventArgs e)
        {

        }
    }
    static class globalBuffer
    {
        public static byte[] g_recvBuffer = new byte[64];
        public static Socket g_recvSocketfd;
        public static bool g_isDataReceive;
        public static bool g_dataNeedProcess;
        public static bool g_isThreadWorking;

    }
    class Work
    {
        static IPEndPoint remoteIpInfo = new IPEndPoint(IPAddress.Parse("192.168.4.1"), 3232);
        static EndPoint Remote;
        public static void doRemoteIPcast()
        {
            Remote = (EndPoint)remoteIpInfo;
        }
        public static void taskRecvThread()
        {
            //IPEndPoint serverIpInfo = new IPEndPoint(IPAddress.Any, 60138);
            Array.Clear(globalBuffer.g_recvBuffer, 0, globalBuffer.g_recvBuffer.Length);
            doRemoteIPcast();
            //globalBuffer.g_recvSocketfd = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            //globalBuffer.g_recvSocketfd.Bind(serverIpInfo);
            //globalBuffer.g_recvSocketfd.ReceiveTimeout = 1000;
            while (true)
            {
                if (!globalBuffer.g_isDataReceive && globalBuffer.g_isThreadWorking)
                {
                    try
                    {
                        globalBuffer.g_recvSocketfd.ReceiveFrom(globalBuffer.g_recvBuffer, 0, 64, SocketFlags.None, ref Remote);
                    }
                    catch
                    {

                    }
                    if (globalBuffer.g_recvBuffer[0] != 0) /* data received */
                    {
                        globalBuffer.g_isDataReceive = true; /* this var is buggy, it is suppose to break data parse when no data receive */
                        globalBuffer.g_dataNeedProcess = true;
                    }
                        
                }                
                Thread.Sleep(1);
            }
        }

    }
}
