/*
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
            CONNECTED_PROCESSING,
            CONNECTED_TRAINING_DONE
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
            
            //recvBuffer1 = 3;
            //recvBuffer[16] = 4;            
            //recvBuffer[0] = 1;
            //Array.Clear(recvBuffer, 0, recvBuffer.Length);
            // value cast success connectionState.Text = recvBuffer[0].ToString();
            globalBuffer.g_isThreadWorking = false;
            newThread.Start();
            arm_Info.shortTermError = new short[8] {0, 0, 0, 0, 0, 0, 0, 0};
            arm_Info.longTermError  = new short[8] {0, 0, 0, 0, 0, 0, 0, 0};
            arm_Info.food_left      = 0;
            arm_Info.trainState     = trainingStatus.STANDBY;
            arm_Info.netState       = connectionStatus.UNCONNECT;
            arm_Info.isDataReceived = false;
            DoRemoteIpInfoCast();   /* init variable since it cast a var which initialize with _new_, so with func call, we can guarantee that its been initialized */
            /* enable network timer and do a regular check to network state */
            networkTimer.Interval   = 100;
        }
 
        private void networkTimer_Tick(object sender, EventArgs e)
        {
           
           switch (arm_Info.netState)
           {
                case connectionStatus.UNCONNECT:    /* connection init here */                                      
                    try
                    {
                        globalBuffer.g_recvSocketfd = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                        globalBuffer.g_recvSocketfd.Bind(serverIpInfo);
                        globalBuffer.g_recvSocketfd.ReceiveTimeout = 2000;
                        //serverFd.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReceiveTimeout, 5);
                    }
                    catch (Exception ex)
                    {
                        errorBox.Text = ex.ToString();
                    }
                    connectionState.Text = "CONNECTED_KNOCK_DOOR";
                    arm_Info.netState = connectionStatus.CONNECTED_KNOCK_DOOR;
                    break;
                case connectionStatus.CONNECTED_KNOCK_DOOR:
                    try
                    {
                        globalBuffer.g_recvSocketfd.SendTo(sendBuffer, 6, SocketFlags.None, remoteIpInfo); /* send first packet to remote, which the data sent is "rd" */
                        //globalBuffer.g_recvSocketfd.ReceiveFrom(recvBuffer, 0, 3, SocketFlags.None, ref Remote);
                    }
                    catch (Exception ex)
                    {
                        errorBox.Text = ex.ToString();
                        break; /* re-knock the door */
                    }
                    Array.Clear(recvBuffer, 0, recvBuffer.Length);
                    connectionState.Text = "CONNECTED"; /* change color to green here */
                    globalBuffer.g_isThreadWorking = true;
                    arm_Info.netState = connectionStatus.CONNECTED;
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
                        resultStreamWriter.Close();
                        resultFileStream.Close();
                        trainingState.Text = "Training end";
                        timerTimeElapsed.Enabled = false;
                        startButton.BackColor = Color.LawnGreen;
                        break;
                    }
                    ushort ratPos, recordPos = 0;
                    trainingState.Text = "In Progress";
                    if (!globalBuffer.g_isDataReceive) /* data received, we go foward to parse data */
                        break;
                    if (!globalBuffer.g_dataNeedProcess)
                        break;
                    debugTextbox.Text = "received!";
                        // send corresponding DAC here, use following func to send 
                        /* C# is big-endian */
                        ratPos = globalBuffer.g_recvBuffer[0];
                    //Decimal.ToByte(((NumericUpDown)Controls.Find("arm1speed1".ToString(), true)[0]).Value);//arm1speed1
                    for (ushort i = 0; i < 3; i++)
                    {
                        switch (i)
                        {
                            case 0:
                                switch (globalBuffer.g_recvBuffer[i])
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
                                switch (globalBuffer.g_recvBuffer[i])
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
                                switch (globalBuffer.g_recvBuffer[i])
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
                    /*
                    ratPos--;
                    DACspeed[0] =  (byte)DACtable[ratPos];
                    DACspeed[1] = (byte) (DACtable[ratPos] >> 8);

                    ratPos = globalBuffer.g_recvBuffer[1];
                    ratPos--;
                    DACspeed[2] = (byte)DACtable[ratPos];
                    DACspeed[3] = (byte)(DACtable[ratPos] >> 8);

                    ratPos = globalBuffer.g_recvBuffer[2];
                    ratPos--;
                    DACspeed[4] = (byte)DACtable[ratPos];
                    DACspeed[5] = (byte)(DACtable[ratPos] >> 8);
                    */

                    /*----------Data Record-----------*/
                    ratPos = globalBuffer.g_recvBuffer[0];
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
                        case 3:
                            recordPos = 20;
                            break;
                    }
                    resultStreamWriter.WriteLine((ratPos + 1).ToString() + "  " + recordPos.ToString());
                    resultStreamWriter.Flush();
                    ratPos = globalBuffer.g_recvBuffer[1];
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
                        case 3:
                            recordPos = 20;
                            break;
                    }
                    resultStreamWriter.WriteLine((ratPos + 1).ToString() + "  " + recordPos.ToString());
                    resultStreamWriter.Flush();
                    ratPos = globalBuffer.g_recvBuffer[1];
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
                        case 3:
                            recordPos = 20;
                            break;
                    }
                    resultStreamWriter.WriteLine((ratPos + 1).ToString() + "  " + recordPos.ToString());
                    resultStreamWriter.Flush();
                    resultStreamWriter.WriteLine();
                    resultStreamWriter.Flush();
                    /*--------------------------------*/
                    globalBuffer.g_recvSocketfd.SendTo(DACspeed, 6, SocketFlags.None, remoteIpInfo); /* send ACK back */
                    Array.Clear(globalBuffer.g_recvBuffer, 0, globalBuffer.g_recvBuffer.Length);
                    globalBuffer.g_dataNeedProcess = false;
                    //arm_Iwnfo.isDataReceived = true; TODO maybe use in the future                    
                    //arm_Info.netState = connectionStatus.CONNECTED_PROCESSING;
                    //Array.Clear(dataBuffer, 0, dataBuffer.Length); use this after process the received data
                    break;
                case connectionStatus.CONNECTED_PROCESSING:
                    if (deadcode < 3)
                        deadcode++;
                    if (deadcode >= 3)
                    {
                        if (ratRouteIndex < (64 - 1))
                        {
                            if ((uint)globalBuffer.g_recvBuffer[ratRouteIndex] != 0)
                            {
                                ratRoute.Text += ((uint)globalBuffer.g_recvBuffer[ratRouteIndex]).ToString() + " ";
                                ratRouteIndex++;                                
                            }       
                        }
                    }
                    
                    for (short i = 1; i <= 16; i += 2)
                    {
                        switch (i)
                        {
                            case 1:
                                longTerm1.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm1.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 3:
                                longTerm2.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm2.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 5:
                                longTerm3.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm3.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 7:
                                longTerm4.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm4.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 9:
                                longTerm5.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm5.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 11:
                                longTerm6.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm6.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 13:
                                longTerm7.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm7.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                            case 15:
                                longTerm8.Text = globalBuffer.g_recvBuffer[i].ToString();
                                shortTerm8.Text = globalBuffer.g_recvBuffer[i + 1].ToString();
                                break;
                        }
                    }
                    short buffer = 0;
                    for (short i = 1; i < 16; i += 2)
                    {
                        buffer += globalBuffer.g_recvBuffer[i];
                    }
                    totalLongTerm.Text = buffer.ToString();
                    buffer = 0;
                    for (short i = 2; i <= 16; i += 2)
                    {
                        buffer += globalBuffer.g_recvBuffer[i];
                    }
                    totalShortTerm.Text = buffer.ToString();
                    foodAte.Text = globalBuffer.g_recvBuffer[17].ToString();                    
                    /* enter seq can record at the end of training */                                       
                    switch (globalBuffer.g_recvBuffer[0]) /* handle this since it's better to change the state */
                    {
                        case (byte)mazeStatus.WAIT_FOR_RAT:
                            mazeState.Text = "Wait for rat";
                            break;
                        case (byte)mazeStatus.RAT_ENTERED:
                            mazeState.Text = "Rat entered arm";
                            break;
                        case (byte)mazeStatus.RAT_NOT_ENTERED:
                            if (isTrainingFisrtTime)
                            {
                                isTrainingFisrtTime = false;
                                timerTimeElapsed.Enabled = true;
                            }
                            mazeState.Text = "Rat not entered";
                            break;
                        case (byte)mazeStatus.TRAINING_END:
                            mazeState.Text = "Training end";
                            timerTimeElapsed.Enabled = false;
                            startButton.BackColor = Color.LawnGreen;
                            arm_Info.netState = connectionStatus.CONNECTED_TRAINING_DONE;
                            break;
                    }
                    if (arm_Info.netState == connectionStatus.CONNECTED_TRAINING_DONE)
                    {
                        globalBuffer.g_isDataReceive = false;
                        break;
                    }
                    Array.Clear(globalBuffer.g_recvBuffer, 0, globalBuffer.g_recvBuffer.Length);
                    arm_Info.netState = connectionStatus.CONNECTED;
                    globalBuffer.g_isDataReceive = false;
                    break;
                case connectionStatus.CONNECTED_TRAINING_DONE:
                    /* implement seq record here */
                    /* after file write done, close file */
                    buffer = 18; /* improve overhead */
                    //while (globalBuffer.g_recvBuffer[buffer] != 0)
                    //    resultStreamWriter.WriteLine(globalBuffer.g_recvBuffer[buffer++].ToString());
                    //resultStreamWriter.Flush();
                    resultStreamWriter.Close();
                    resultFileStream.Close();
                    trainingState.BackColor = Color.LightGreen;
                    timeElapsed.BackColor = Color.LightGreen;
                    trainingState.Text = "Complete";
                    networkTimer.Enabled = false;

                    /* beep notification */
                    Console.Beep(1000, 1500);
                    Thread.Sleep(1000);
                    Console.Beep(1000, 1500);
                    Thread.Sleep(1000);
                    Console.Beep(1000, 1500);
                    Thread.Sleep(1000);
                    Console.Beep(1000, 1500);
                    Thread.Sleep(1000);
                    Console.Beep(1000, 1500);
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
            /* I dont know why this working so weried
            byte index = 0, bufferIndex = 2;
            foreach (CheckBox food in groupBoxFood.Controls)
            {
                if (index < 8)
                {
                    String str = "checkBoxArmLoc" + index;
                    food.Name = str;
                    
                    if (food.Checked)
                    {
                        sendBuffer[bufferIndex] = index;
                        bufferIndex += 1;
                    }
                    
                    food.Checked = true;
                }
                index++;
            }    
            */
            /*
            ushort bufferIndex = 2;
            while (true)    /* this part is extremly hard-coded 
            {
                if (checkBoxArmLoc0.Checked)
                {
                    sendBuffer[bufferIndex] = 0;                    
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc1.Checked)
                {
                    sendBuffer[bufferIndex] = 1;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc2.Checked)
                {
                    sendBuffer[bufferIndex] = 2;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc3.Checked)
                {
                    sendBuffer[bufferIndex] = 3;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc4.Checked)
                {
                    sendBuffer[bufferIndex] = 4;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc5.Checked)
                {
                    sendBuffer[bufferIndex] = 5;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc6.Checked)
                {
                    sendBuffer[bufferIndex] = 6;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                if (checkBoxArmLoc7.Checked)
                {
                    sendBuffer[bufferIndex] = 7;
                    bufferIndex++;
                    if (bufferIndex == 6)
                        break;
                }
                errorBox.ForeColor = Color.Red;
                errorBox.Text = "Arm with food didn't check properly";
                return;
            }
            */
            endTimestamp = getCurrentTimestamp() + long.Parse(trainTime.Text) * 60;
            resultFilePath.ForeColor = Color.Black;
            resultFilePath.Text = resultFileDialog.FileName;
            startButton.BackColor = Color.YellowGreen;
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
