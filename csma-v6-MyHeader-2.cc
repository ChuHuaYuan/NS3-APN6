#include <fstream>
#include "ns3/core-module.h"
#include "ns3/header.h"
#include "ns3/ipv6-address.h"
#include "ns3/packet.h"
#include "ns3/ipv6-header.h"
#include "ns3/mac48-address.h"
#include "ns3/ethernet-header.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
// #include "ns3/mpi-interface.h"
#include "ns3/socket.h"
#include "ns3/callback.h"
#include "ns3/applications-module.h"
#include "ns3/ping6-helper.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/ripng.h"
#include "ns3/udp-header.h"
#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Ipv6_P2P_Example");


//myheader的定义

class MyHeader : public Header 
{
public:

  MyHeader ();
  virtual ~MyHeader ();

  /**
   * Set the header data.
   * \param data The data.
   */
  void SetData (uint16_t data);
  /**
   * Get the header data.
   * \return The data.
   */
  uint16_t GetData (void) const;
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t GetSerializedSize (void) const;
private:
  uint16_t m_data;  //!< Header data

};

MyHeader::MyHeader ()
{
  // we must provide a public default constructor, 
  // implicit or explicit, but never private.
}
MyHeader::~MyHeader ()
{
}

TypeId
MyHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyHeader")
    .SetParent<Header> ()
    .AddConstructor<MyHeader> ()
  ;
  return tid;
}
TypeId
MyHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
MyHeader::Print (std::ostream &os) const
{
  // This method is invoked by the packet printing
  // routines to print the content of my header.
  //os << "data=" << m_data << std::endl;
  os << "data=" << m_data;
}
uint32_t
MyHeader::GetSerializedSize (void) const
{
  // we reserve 2 bytes for our header.
  return 2;
}
void
MyHeader::Serialize (Buffer::Iterator start) const
{
  // we can serialize two bytes at the start of the buffer.
  // we write them in network byte order.
  start.WriteHtonU16 (m_data);
}
uint32_t
MyHeader::Deserialize (Buffer::Iterator start)
{
  // we can deserialize two bytes from the start of the buffer.
  // we read them in network byte order and store them
  // in host byte order.
  m_data = start.ReadNtohU16 ();

  // we return the number of bytes effectively read.
  return 2;
}

void 
MyHeader::SetData (uint16_t data)
{
  m_data = data;
}
uint16_t 
MyHeader::GetData (void) const
{
  return m_data;
}

//建立一个APN6报头内的类型约束
class APN6_type
{
  public:
    APN6_type();
    void setData();
    void reSetData();
    void setData(int,uint8_t);
    uint8_t getdate(int);

    uint8_t app_ID[3];//0-2
    uint8_t user_ID[5];//3-7
    // uint8_t SLA[0.5];
    // uint8_t session_ID[0.5];
    uint8_t sla_sessionID=0;//8
    uint8_t id_reserved=0;//9

    uint8_t bandwidth=0;//10
    uint8_t latency=0;//11
    uint8_t jitter=0;//12
    uint8_t loss_ratio=0;//13
    uint8_t par_reserved[2]={0,0};//14-15

  private:
    //一共应该是128位，也就是16字节，所以是16个uint8_t，8个uint16_t
    uint8_t apnContent[16];
};
//对APN6_type中的对象进行初始化
APN6_type::APN6_type(){
  for (int i = 0; i < 16; i++)
    apnContent[i]=0;
  for (int i = 0; i < 3; i++)
    app_ID[i]=0;
  for (int i = 0; i < 5; i++)
    user_ID[i]=0;
}
//对默认的对象进行赋值，也就是说，
//如果你完成了对APNtype对象内部数据的配置，就可以调用此方法直接生成最终数组
void APN6_type::setData(){//0-2
  for (int i = 0; i < 3; i++)
    apnContent[i]=app_ID[i];
  for (int i = 0; i < 5; i++)//3-7
    apnContent[i+3]=user_ID[i];
  
  apnContent[8]=sla_sessionID;//8
  apnContent[9]=id_reserved;//9
  apnContent[10]=bandwidth;//10
  apnContent[11]=latency;
  apnContent[12]=jitter;
  apnContent[13]=loss_ratio;
  for(int i=0;i<2;i++)
    apnContent[i+14]=par_reserved[i];
}
//和上面的方法相对应，作用是接收方把content摊开
void APN6_type::reSetData(){//0-2
  for (int i = 0; i < 3; i++)
    app_ID[i]=apnContent[i];
  for (int i = 0; i < 5; i++)//3-7
    user_ID[i]=apnContent[i+3];
  
  sla_sessionID=apnContent[8];//8
  id_reserved=apnContent[9];//9
  bandwidth=apnContent[10];//10
  latency=apnContent[11];
  jitter=apnContent[12];
  loss_ratio=apnContent[13];
  for(int i=0;i<2;i++)
    par_reserved[i]=apnContent[i+14];
}
void APN6_type::setData(int i,uint8_t data){
  apnContent[i]=data;
}
uint8_t APN6_type::getdate(int i){
  return apnContent[i];
}



//接包的设备模拟
void
ReceivePacket (Ptr<Socket> socket)
{
	std::cout<<"接受包"<<std::endl;
  Ptr<Packet> packet;
 	Address from;//设置起始地址
  APN6_type my_rec_type;

  while ((packet = socket->RecvFrom (from)))//循环接收
  {
    std::cout<<"数据包大小"<<packet->GetSize()<<std::endl;

    for (int i = 15; i >= 0; i--)
    {
	    MyHeader header1;
      // int header_size=packet->PeekHeader(header1);
      packet->PeekHeader(header1);  //很重要，如果不进行peek处理，就会造成报头错位
      // std::cout<<"报头大小"<<header_size<<std::endl;
      // std::cout<<"接收方报头内容"<<int(header1.GetData())<<std::endl;
      my_rec_type.setData(i,int(header1.GetData()));  //警告，具体是强转成int还是uint8还不确定
      packet->RemoveHeader(header1);
    }
    // my_rec_type.reSetData();

	  // std::cout << from << std::endl;
    if (packet->GetSize () > 0)//不为空
    {
        packet->EnablePrinting();

        std::cout<<"2数据包大小"<<packet->GetSize()<<std::endl;
        //以int类型输出packet内容
        uint8_t buffer[10];
        packet->CopyData(buffer,10);
        std::cout<<"packet content:";
        for(int i=0;i<10;i++)
          std::cout<<int(buffer[i])<<std::endl;//这里需要int强转一下


        /*
    	  uint8_t buffer[16];
    	 //	     	  s=packet->Serialize(buf,32);
    	 	     	  packet->CopyData(buffer,16);
    	 	    	  std::cout<<"溯源路由器地址段：";
    	 	    	  for(int i=0;i<8;i++){
    	 	    		  printf("%02X",buffer[i]);
    	 	    	  }
    	   printf("\n");
    	  Inet6SocketAddress iaddr = Inet6SocketAddress::ConvertFrom (from);
    	    std::ostringstream oss;
    	    oss << "Received one tracesource packet! Socket: " << iaddr.GetIpv6 ()
    	        << " port: " << iaddr.GetPort ()
    	        << " at time = " << Simulator::Now ().GetSeconds ()
    	        << "";

    	  std::string x = oss.str();
    	  std::cout << x << std::endl;
          */
         // NS_LOG_UNCOND (x);
        }
    }
}

/**
 * 发送分组回调方法
 */
static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
                             uint32_t pktCount, Time pktInterval )
{
  NS_LOG_FUNCTION(socket<<pktSize<<pktCount<<pktInterval);
  if (pktCount > 0)
    {
      Time startTime=Simulator::Now();
      NS_LOG_LOGIC("start time:"<<startTime);


      //修改packet
      uint8_t fill[10]={1,2,3,4,5,6,7,8,9,0};
      Ptr<Packet> packet=Create<Packet>(fill,10);
      
      std::cout<<packet->ToString() <<std::endl;

      //添加IPv6Header
      // Ipv6ExtensionHeader iph6;  //知道有这么个类就行了
      Ipv6ExtensionHopByHopHeader iph6_hbh;
      iph6_hbh.SetLength(32);//单位是uint16_t
      
      std::cout<<"拓展报头大小"<<iph6_hbh.GetLength()<<std::endl;
      
      
      // packet->PeekHeader(iph6);
      // packet->RemoveHeader(iph6);
      // iph6.SetNextHeader(iph6.IPV6_EXT_HOP_BY_HOP);//添加逐跳选择报头
      packet->AddHeader(iph6_hbh);

      std::ostringstream ostt;
      std::cout<<"拓展报头内容"<<std::endl;
      iph6_hbh.Print(ostt) ;
      
      std::cout<<"包里加报头内容:"<<packet->ToString() <<std::endl;

/*
      //自定义要传输的报头内的文字
      APN6_type my_apn;

      my_apn.app_ID[2]=233;

      my_apn.user_ID[4]=134;
      //总赋值
      // my_apn.apn_all[2]=my_apn.App_ID[2];
      // my_apn.apn_all[7]=my_apn.User_ID[4];
      my_apn.setData();
        
      //导入
      for (int i = 0; i < 16; i++)
      {
        //添加header  
        MyHeader header0;
        header0.SetData(my_apn.getdate(i));
        std::cout<<"发送时的 header data:"<<header0.GetData()<<std::endl;
        packet->AddHeader(header0);
      }

      //打印包里内容
      std::cout<<packet->ToString() <<std::endl;
*/
      socket->Send(packet);
      // socket->Send (Create<Packet> (pktSize));
      //这是个回调函数，根据pktCount的大小进行重复的发送
      Simulator::Schedule (pktInterval, &GenerateTraffic,

                           socket, pktSize,pktCount-1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

int main(int argc,char **argv){
    CommandLine cmd;
    cmd.Parse(argc,argv);

    Ptr<Node> node0=CreateObject<Node>();
    Ptr<Node> node1=CreateObject<Node>();
    NodeContainer node(node0,node1);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(node);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate",StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay",StringValue("2ms"));
    NetDeviceContainer device=csma.Install(node);

    //此方法将向节点添加两个全局 IPv6 地址。请注意，与IPv6 一样，所有节点也将具有本地链路地址。通常，接口上的第一个地址是链路本地地址，下面的是全局地址。
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"),Ipv6Prefix(64));
    Ipv6InterfaceContainer i=ipv6.Assign(device);

    ns3::PacketMetadata::Enable();

    TypeId tid=UdpSocketFactory::GetTypeId();
    //socket的创建,作为服务端
    Ptr<Socket> recvSink = Socket::CreateSocket (node0, tid);
    Inet6SocketAddress local = Inet6SocketAddress (i.GetAddress(0,1),80);
    recvSink->Bind (local);//绑定地址，等待连接
    recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));//设置回调函数地址


    //创建socket，作为客户端
    Ptr<Socket> source = Socket::CreateSocket (node1, tid);
    Inet6SocketAddress remote = Inet6SocketAddress (i.GetAddress(1,1), 80);
    source->SetAllowBroadcast (false);
    source->Bind(remote);//绑定地址
    source->Connect (local);//发起连接
    // 连接成功，通过GenerateTraffic方法发packet,,,下面的四个参数是GenerateTraffic的参数
    Simulator::ScheduleWithContext (source->GetNode ()->GetId (),Seconds (10), &GenerateTraffic,
                                    source, 100, 5, Seconds(2));

    csma.EnablePcapAll("csma_v6");
    AnimationInterface anim ("csma_v6.xml");
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}



