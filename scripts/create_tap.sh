if [ $# -eq 0 ]
  then
    echo "No arguments supplied"
    exit
fi


i=$1;

sudo ip link add br$i type bridge;
sudo ip tuntap add mode tap tap$i;
sudo ip addr add 0.0.0.0 dev tap$i;
sudo ip link set dev tap$i master br$i;
sudo ip link set tap$i promisc on up;

# ns3
sudo ip tuntap add mode tap tap$i-ns;
sudo ip addr add 0.0.0.0 dev tap$i-ns;
sudo ip link set dev tap$i-ns master br$i;
sudo ip link set tap$i-ns promisc on up;

sudo ip link set dev br$i up


echo 'Remember to do, if not already done'
echo "echo \"allow br$i\" | sudo tee -a /etc/qemu/bridge.conf"

